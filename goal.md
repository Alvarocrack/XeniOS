# Metal Render-Run Planner Goal

Date: 2026-05-27
Checkout: `/Users/admin/Documents/xenia-edge-msc-clean-stack`
Branch observed: `pr/metal-msc-clean-stack...origin/edge [ahead 97, behind 3]`

This document is the concrete recovery and rewrite plan for the failed Metal
deferred draw queue experiment. The goal is to fix the original Apple Metal
encoder churn problem without preserving the CPU-exploding `PreparedDraw` queue.

## Initial Failure State Captured Before Patch 1

The failed experiment originally dirtied:

- `src/xenia/gpu/metal/metal_command_processor.cc`
- `src/xenia/gpu/metal/metal_command_processor.h`

Unrelated untracked item observed in this checkout:

- `hello-xenon/`

Do not touch or delete `hello-xenon/` unless separately asked. It is unrelated to
the Metal planner cleanup.

## Commit Discipline

Every patch in this plan must land as an individual, clean commit. Do not batch
multiple logical patches into one commit, and do not carry unrelated edits across
commit boundaries.

Required commit boundaries:

1. Documentation or plan updates, such as this file.
2. Failed-queue deletion / reset cleanup.
3. Conservative vertex-fetch residency restoration, if not already covered by
   the reset cleanup.
4. Planner telemetry and active-encoder miss accounting.
5. UploadPlan behavior.
6. StopPlan hard-boundary behavior.
7. AttachmentPlan load/store proof plumbing.
8. ResolvePlan tile-direct scheduling.
9. `StoreActionDontCare` liveness proof and enablement.

Each code commit must pass the static validation listed in this document before
the next behavior patch begins. Local scratch artifacts, including
`scratch/failed_prepared_draw_queue.patch`, must not be committed unless
explicitly requested.

The failed diff that must not survive in final code was:

```text
src/xenia/gpu/metal/metal_command_processor.cc | 1577 ++++++++++++++----------
src/xenia/gpu/metal/metal_command_processor.h  |  156 ++-
2 files changed, 1015 insertions(+), 718 deletions(-)
```

That diff has been saved locally as `scratch/failed_prepared_draw_queue.patch`
for reference only. The scratch patch is local evidence, not a commit artifact.

## Source Anchors

Use these anchors before editing, because the line numbers are from the current
dirty checkout and may shift after cleanup.

- Failed full-draw queue state:
  - `metal_command_processor.h:364-365`: `kDeferredDrawRunMaxDraws` and `kDeferredDrawRunMaxReadRanges`.
  - `metal_command_processor.h:378-414`: `PreparedDraw` copies the draw state.
  - `metal_command_processor.h:489-520`: deferred draw flush/reject enums.
  - `metal_command_processor.h:714-716`: deferred queue telemetry arrays.
  - `metal_command_processor.h:809-816`: `QueuePreparedDraw`, `FlushPendingDrawRun`, `EncodePreparedDraw`.
  - `metal_command_processor.h:938-942`: `pending_draw_run_` and associated state.
  - `metal_command_processor.cc:3290-3332`: current draw path builds `PreparedDraw`.
  - `metal_command_processor.cc:7131-7190`: `QueuePreparedDraw`.
  - `metal_command_processor.cc:7192-7290`: `EncodePreparedDraw`.
  - `metal_command_processor.cc:7292-7324`: `FlushPendingDrawRun`.
  - `metal_command_processor.cc:7326-7355`: `RequestTransferCommandBuffer`.

- Current resource and draw entry points:
  - `metal_command_processor.cc:2418`: `IssueDraw`.
  - `metal_command_processor.cc:2427-2432`: `EdramMode::kCopy` dispatches to `IssueCopy`.
  - `metal_command_processor.cc:2883-2901`: texture requests before render encoder.
  - `metal_command_processor.cc:2931-3068`: vertex fetch, memexport, and shared-memory range planning.
  - `metal_command_processor.cc:3072-3155`: guest index and shader primitive index range planning.
  - `metal_command_processor.cc:3226-3233`: vertex fetch residency request.
  - `metal_command_processor.cc:6820-6919`: `RequestSharedMemoryRanges` telemetry and active/no-active split.
  - `metal_command_processor.cc:7408`: shared-memory upload path requests a transfer command buffer.

- Render target, attachment, and resolve anchors:
  - `metal_render_target_cache.h:352-361`: existing `ResolvePlan`.
  - `metal_render_target_cache.h:710-719`: `PendingDrawPassTransferPlan` with `full_overwrite` and `load_action_safe`.
  - `metal_render_target_cache.cc:399-422`: load/store action helper functions.
  - `metal_render_target_cache.cc:2520-2536`: pending draw-pass transfer plan creation and full-overwrite marking.
  - `metal_render_target_cache.cc:3090-3107`: `load_action_safe = full_overwrite`.
  - `metal_render_target_cache.cc:3727-3888`: render pass descriptor construction.
  - `metal_render_target_cache.cc:3777-3784`: `pending_draw_pass_load_dontcare_mask_`.
  - `metal_render_target_cache.cc:4084-4197`: render-pass descriptor compatibility.
  - `metal_render_target_cache.cc:6255-6275`: comment explicitly says store elision still needs liveness proof.
  - `metal_render_target_cache.cc:6283-6598`: tile direct host resolve execution.
  - `metal_render_target_cache.cc:6590-6592`: current experimental `setColorStoreAction(StoreActionDontCare)`.
  - `metal_render_target_cache.cc:6880-6955`: `PrepareResolvePlan`.
  - `metal_render_target_cache.cc:6956-7040`: `Resolve`.

- PM4 and Xenos semantic anchors:
  - `pm4_command_processor_declare.h:33-56`: special PM4 packet declarations.
  - `pm4_command_processor_implement.h:511-553`: type-3 packet dispatch, including indirect buffer, wait, memory write, events, and draw packets.
  - `pm4_command_processor_implement.h:798-833`: indirect buffer execution.
  - `pm4_command_processor_implement.h:833-965`: wait handling.
  - `pm4_command_processor_implement.h:965-1042`: memory write handling.
  - `pm4_command_processor_implement.h:1042-1194`: event and query-related handling.
  - `pm4_command_processor_implement.h:1368-1422`: draw packet execution into backend `IssueDraw`.
  - `xenos.h:223-280`: EDRAM layout, pitch, MSAA samples, depth tile-half swapping, circular addressing.
  - `xenos.h:950-1024`: resolve copy/conversion/clear/scissor/rectangle semantics.

## What Failed

The current deferred queue tried to avoid opening a render encoder before upload
needs were known. That high-level goal was correct, but the implementation was
the wrong shape.

The failed shape is:

1. `IssueDraw` still does draw analysis and preparation.
2. It copies a large draw record into `PreparedDraw`.
3. Later `FlushPendingDrawRun` swaps the vector and replays every draw through
   `EncodePreparedDraw`.
4. `RequestTransferCommandBuffer` still flushes the queued draws before transfer
   work.

The telemetry from the failed run proves the problem:

```text
deferred_draws queued=775816
flushes=20649
flush_draws=775816
transfer_command_buffer=6343
shared_memory_upload_before_draw_pass=6502
```

That means the experiment added a full CPU-side draw copy/replay machine while
still not eliminating the transfer path that ends render encoders.

The visual corruption is a separate correctness failure from tightened vertex
fetch residency ranges. Tightened ranges may under-upload data if a shader
computes addresses outside the modeled indexed span. Conservative full declared
fetch ranges must be restored for correctness until exact range analysis is
proved per shader/fetch pattern.

## Non-Negotiable Invariants

1. No full draw queue.
2. No draw replay.
3. No `PreparedDraw` replacement structure.
4. No copied `RegisterFile` per planned draw.
5. No per-draw `std::vector` allocations in planner state.
6. Every real draw is encoded exactly once by the normal `IssueDraw` path.
7. Planner scan may read PM4 and simulate register writes, but may not execute,
   retire, reorder, or replace PM4 commands.
8. Host-only materialization may move earlier only if all prior guest-visible
   writes have already happened and no semantic boundary is crossed.
9. Any host materialization transfer requested while a render encoder is active
   is a planner miss unless the current command is a deliberate semantic stop.
10. `StoreActionDontCare` is forbidden unless RT liveness proves no later host
    observer needs the attachment after the guest-visible export/resolve has
    happened.

## Correct Architecture

The target is a bounded `MetalRenderRunPreflight`, not a deferred draw queue.

The planner has four pieces:

```cpp
struct MetalRenderRunPreflight {
  StopPlan stop;
  UploadPlan uploads;
  AttachmentPlan attachments;
  ResolvePlan resolves;
};
```

The scanner state is intentionally small:

```cpp
RegisterFile scan_regs = regs;  // One clone per run, not one clone per draw.
Pm4Cursor scan = current_pm4_cursor;
```

The plan stores resource needs and proof state only. It must not store:

- `PreparedDraw`
- per-draw `RegisterFile`
- `PrimitiveProcessor::ProcessingResult`
- copied texture vectors
- copied vertex binding vectors
- pipeline objects as replay state
- draw-call records

The execution shape must be:

```text
first draw reached while no render encoder is active
  build bounded read-only preflight
  materialize UploadPlan while no render encoder is active
  open render encoder
  execute real PM4 normally
  each draw reaches IssueDraw and encodes once
  resolve/tile work executes only at the real Xenos resolve point
  late store action is chosen before endEncoding
```

## Patch Plan

### Patch 0: Save and inspect the current failure

Purpose: keep the failed diff available for reference without preserving it in
the final implementation.

Actions:

1. Save a patch of the current dirty command-processor diff for local reference.
2. Do not commit it.
3. Do not include `hello-xenon/`.

Suggested local command:

```bash
git diff -- src/xenia/gpu/metal/metal_command_processor.cc src/xenia/gpu/metal/metal_command_processor.h > scratch/failed_prepared_draw_queue.patch
```

Why this is correct:

- The failed diff contains useful telemetry names and comments, but the code
  shape must not remain in the implementation.

Watch out:

- `scratch/` may be local-only workflow material. Do not commit the saved patch
  unless explicitly asked.

### Patch 1: Delete the full draw queue and restore the pre-experiment draw path

Purpose: remove the CPU blow-up class before adding any new planner.

Preferred implementation:

1. Restore `metal_command_processor.cc` and `metal_command_processor.h` to
   `HEAD`.
2. Reapply only narrowly selected telemetry or helper code that is still needed
   and is not tied to `PreparedDraw`.

Explicit deletions if doing this manually:

- Delete `kDeferredDrawRunMaxDraws`.
- Delete `kDeferredDrawRunMaxReadRanges`.
- Delete `DeferredDrawRenderTargetKey`.
- Delete `PreparedDraw`.
- Delete `DeferredDrawFlushReason`.
- Delete `DeferredDrawRejectReason`.
- Delete `DeferredDrawFlushReasonName`.
- Delete `DeferredDrawRejectReasonName`.
- Delete all deferred draw telemetry fields.
- Delete `GetDeferredDrawRenderTargetKey`.
- Delete `PendingDrawRunReadRangesOverlap`.
- Delete `PendingDrawRunReadRangesOverlapInvalid`.
- Delete `RecordDeferredDrawReject`.
- Delete `CanDeferPreparedDraw`.
- Delete `QueuePreparedDraw`.
- Delete `EncodePreparedDraw`.
- Delete `FlushPendingDrawRun`.
- Delete `pending_draw_run_`.
- Delete `pending_draw_run_render_target_key_`.
- Delete `pending_draw_run_has_render_target_key_`.
- Delete `pending_draw_run_read_ranges_`.
- Delete `flushing_pending_draw_run_`.

Then remove every call to `FlushPendingDrawRun(...)`. These calls currently
appear in memory invalidation, explicit flush, wait, shutdown, swap, copy,
texture upload, shared-memory overlap, transfer command buffer, and command
buffer end paths. They should not be replaced by another draw replay path.

Why this is correct:

- The telemetry proves full draw copy/replay is the CPU explosion.
- Removing the queue makes `queued=775816` and `flush_draws=775816`
  mechanically impossible.
- The original draw path already encodes draws once through `IssueDraw`,
  `BeginRenderEncoderForDraw`, `PopulateBindlessTables`, and `DispatchDraw`.

Watch out:

- Do not accidentally delete unrelated shared-memory request telemetry if it
  already exists outside the queue experiment.
- Do not change `MetalRenderTargetCache` pending draw-pass transfer machinery in
  this patch.

Validation:

- `rg -n "PreparedDraw|pending_draw_run_|FlushPendingDrawRun|QueuePreparedDraw|EncodePreparedDraw|DeferredDraw" src/xenia/gpu/metal`
  must return no matches.
- `git diff --check` must pass.

### Patch 2: Restore conservative vertex-fetch residency

Purpose: remove the suspected top-left corruption cause before performance work.

Actions:

1. In the vertex fetch residency request path, request full declared fetch ranges
   from fetch constants.
2. Do not use tightened indexed ranges for correctness.
3. If the exact range code is useful, keep it only as telemetry behind an
   explicitly disabled flag and do not feed it into `RequestSharedMemoryRanges`.

Grounding:

- Current exact/conservative logic lives around
  `metal_command_processor.cc:2931-3028`.
- The actual residency request is currently at
  `metal_command_processor.cc:3226-3233`.

Why this is correct:

- Xenos vertex fetch can be shader-addressed. Unless the shader/fetch pattern is
  fully modeled, narrowing the upload range can under-upload bytes.
- Full declared fetch ranges match the existing conservative correctness model.

Watch out:

- The draw binding ranges used by `DispatchDraw` are separate from residency
  upload ranges. Do not break actual vertex buffer binding.
- Conservative ranges may upload more bytes, but correctness comes first.

Validation:

- Visual corruption observed after the tight range patch must disappear.
- Add or retain telemetry for exact-range opportunities, but require
  correctness path to report conservative uploads.

### Patch 3: Replace deferred queue telemetry with planner telemetry

Purpose: keep proof counters without preserving dead queue counters.

Delete deferred-queue counters:

- `deferred_draw_queue_attempts`
- `deferred_draw_queued`
- `deferred_draw_immediate`
- `deferred_draw_flushes`
- `deferred_draw_flush_draws`
- `deferred_draw_flush_max_draws`
- `deferred_draw_read_ranges`
- deferred overlap counters
- deferred reject counters
- deferred flush reason arrays

Add planner counters:

```cpp
uint64_t render_run_planner_runs = 0;
uint64_t render_run_planner_draws_covered = 0;
uint64_t render_run_planner_pm4_dwords_scanned = 0;
uint64_t render_run_planner_smem_ranges_collected = 0;
uint64_t render_run_planner_smem_bytes_collected = 0;
uint64_t render_run_planner_texture_requests_collected = 0;
uint64_t render_run_planner_upload_smem_before_encoder = 0;
uint64_t render_run_planner_upload_texture_before_encoder = 0;
uint64_t render_run_planner_miss_active_smem_upload = 0;
uint64_t render_run_planner_miss_active_texture_upload = 0;
uint64_t render_run_planner_miss_active_guest_index_copy = 0;
uint64_t render_run_planner_resolve_tile_attempt = 0;
uint64_t render_run_planner_resolve_tile_success = 0;
uint64_t render_run_planner_store_dontcare_eligible = 0;
uint64_t render_run_planner_store_dontcare_proven = 0;
uint64_t render_run_planner_store_dontcare_rejected = 0;
std::array<uint64_t, kTransferRequestSourceCount>
    render_run_planner_host_materialization_miss_sources = {};
std::array<uint64_t, kRenderRunPlannerStopReasonCount>
    render_run_planner_stop_reasons = {};
```

Suggested stop reasons:

```cpp
enum class RenderRunPlannerStopReason : uint32_t {
  kNoWork,
  kAlreadyActiveEncoder,
  kActiveSharedMemoryWrite,
  kNoSharedMemory,
  kBudgetDraws,
  kBudgetDwords,
  kBudgetSharedMemoryRanges,
  kBudgetTextureRequests,
  kUnsupportedPacket,
  kIndirectBuffer,
  kWaitRegMem,
  kMemWrite,
  kEventOrQuery,
  kShaderLoadOrUnknownShaderState,
  kMemoryOrWaitPacket,  // Temporary bucket for the existing warmer until StopPlan splits it.
  kRenderTargetIncompatible,
  kResolveCopy,
  kResolveClear,
  kMemexport,
  kUnknownGuestMemoryWrite,
  kRingEnd,
  kRequestFailed,
  kCount,
};
```

Keep existing non-queue telemetry:

- shared-memory request active/no-active counters
- shared-memory upload range counters
- texture request/load counters
- render encoder end reasons
- transfer request source counters
- render-pass descriptor dirty/compatibility counters
- pending draw-pass transfer counters
- resolve direct-host and tile direct-host counters

Why this is correct:

- The new counters prove the actual invariant: uploads happen before the render
  encoder opens, and active-encoder upload misses go down.
- Queue counters would be dead code after `PreparedDraw` deletion.

Watch out:

- Do not report "success" based only on lower encoder count. Require active
  upload misses and image correctness too.

Validation:

- The telemetry line should no longer contain `deferred_draws`.
- It should contain planner run/stops/miss counters.

### Patch 4: Add the no-active host-materialization invariant

Purpose: prevent the same failure mode from re-entering through transfer paths.

Rule:

```cpp
// No transfer request may first encode queued or planned draws.
// If a host materialization transfer is needed, it must happen before
// BeginRenderEncoderForDraw unless the planner hit a real semantic stop.
```

Implementation:

1. In `RequestTransferCommandBuffer`, delete any draw flushing behavior.
2. Classify transfer request sources:
   - host materialization: shared-memory upload, texture upload, guest index copy.
   - semantic boundary: resolve/copy, readback, query, wait, explicit flush.
3. If `current_render_encoder_` is non-null and the source is host
   materialization, increment planner miss telemetry.
4. In debug builds, log loudly or assert once the upload planner is expected to
   cover that source.

Why this is correct:

- The original issue was "render encoder open -> discover upload -> end encoder
  for blit".
- The failed queue still had `transfer_command_buffer=6343` because
  `RequestTransferCommandBuffer` flushed queued draws first and then opened the
  transfer path.

Watch out:

- Some semantic boundaries legitimately end render encoders. Do not turn those
  into fatal planner misses.
- Shared memory writes from memexport or resolve are not the same as host
  materialization uploads.

Validation:

- `transfer_request shared_memory_upload active` should approach zero.
- `render_encoder end shared_memory_upload_before_draw_pass` should approach
  zero.
- Any remaining active upload must have a stop reason explaining why it was a
  true semantic boundary or planner miss.

### Patch 5: Implement UploadPlan v1

Purpose: solve the original upload-before-render-pass problem without a draw
queue.

Immediate correction from the May 27 Gears telemetry:

- The `PM4_EVENT_WRITE_EXT` barrier was successfully narrowed from a generic stop
  to a fixed 12-byte crossed-write range, but render-pass churn did not improve
  because the next deterministic scanner wall became shader loads.
- The measured blocker is `shader_load_or_unknown_shader_state`, dominated by
  `PM4_IM_LOAD` (`0x27`) and `PM4_IM_LOAD_IMMEDIATE` (`0x2B`).
- UploadPlan v1 must therefore carry scan-local active shader state:
  `Shader* scan_vertex_shader` and `Shader* scan_pixel_shader`. These are
  pointers to shader-cache objects only, not copied draw records.
- `PM4_IM_LOAD` and `PM4_IM_LOAD_IMMEDIATE` may be crossed only by loading the
  shader through the existing shader cache, updating the scan-local active shader
  pointer, and using the scan-local vertex shader for later vertex-fetch range
  collection.
- Pointer shader loads are guest-memory reads. They may not be crossed if their
  source range overlaps a guest-memory write range crossed earlier by the scanner.
- Embedded shader loads (`PM4_IM_LOAD_IMMEDIATE`) do not add a guest-memory read
  dependency, but they still update only scan-local active shader state.
- After any scan-local shader change, speculative texture preload must not reuse
  the current draw's texture mask. Texture work should be skipped or collected
  only when the future shader texture mask is proven separately.
- If a scan-local future shader has memexport, the scanner must stop before
  planning across that draw. UploadPlan cannot move host materialization across a
  future render-pass shared-memory write.

Scope:

- Start with upload/materialization only.
- Do not add `StoreActionDontCare` proof in this patch.
- Do not change resolve execution yet.

Plan structure:

```cpp
struct RenderRunUploadPlan {
  std::array<SharedMemory::Range, 256> smem_ranges;
  uint32_t smem_range_count = 0;
  uint64_t texture_request_mask = 0;
  bool may_load_texture_data = false;
};
```

Build path:

1. Run only when `current_render_encoder_ == nullptr`.
2. Start at the first draw.
3. Scan a bounded PM4 region with one temporary `RegisterFile`.
4. Accept register writes that are safe to simulate.
5. Stop on hard boundaries from `StopPlan`.
6. For each planned draw, collect:
   - conservative vertex fetch ranges
   - guest DMA index range
   - shader primitive index range
   - texture request mask only when it is proven for the scan-local shader state
7. Coalesce/deduplicate shared-memory ranges.
8. Call `RequestSharedMemoryRanges` before `BeginRenderEncoderForDraw`.
9. Call `texture_cache_->RequestTextures` before `BeginRenderEncoderForDraw`.
10. Return to normal PM4 execution. Do not encode from the plan.

Hard budgets:

- max draws: 256 initially
- max PM4 dwords: fixed cap, for example 4096
- max shared-memory ranges: 256
- max texture request groups: 128 or current texture mask capacity

Why this is correct:

- Moving host materialization earlier does not execute Xbox GPU work.
- The real draw still reaches `IssueDraw` in PM4 order and sees the same guest
  memory bytes.
- Existing per-draw residency checks remain as a final fallback.

Watch out:

- Do not call mutating `RenderTargetCache::Update` from the scanner.
- Do not call `PrimitiveProcessor::Process` in a way that allocates persistent
  draw replay state.
- Do not synchronously compile pipelines as part of UploadPlan.
- Do not upload into a mutable texture or shared-memory resource that an older
  already encoded draw may still observe.

Validation:

- `render_run_planner_runs > 0`.
- `render_run_planner_draws_covered > 0`.
- `render_run_planner_smem_ranges_collected > 0` on the Gears trace.
- `shared_memory_request_upload_calls_no_active` increases.
- `shared_memory_request_upload_calls_active` decreases sharply.
- No visual corruption.

### Patch 6: Add StopPlan and hard semantic boundaries

Purpose: ensure the scanner cannot cross guest-visible execution boundaries.

Hard stops:

- `PM4_INDIRECT_BUFFER` and `PM4_INDIRECT_BUFFER_PFD`
- `PM4_WAIT_REG_MEM`
- `PM4_MEM_WRITE`
- `PM4_EVENT_WRITE`
- `PM4_EVENT_WRITE_SHD`
- `PM4_EVENT_WRITE_EXT`
- `PM4_EVENT_WRITE_ZPD`
- query begin/end/resolve boundaries
- unknown type-3 packet
- shader load or unknown shader state mutation, except `PM4_IM_LOAD` and
  `PM4_IM_LOAD_IMMEDIATE` after the scan-local shader-state rules in Patch 5 are
  implemented
- draw with memexport
- `RB_MODECONTROL` with `EdramMode::kCopy`
- render target/depth state change not proven compatible
- any packet that may write guest memory
- budget exhaustion

StopPlan must expose the remaining wall directly in telemetry. At minimum,
shader-state stops must report opcode counts, and separately tracked shader-load
crossing telemetry must report:

- pointer shader loads crossed
- immediate shader loads crossed
- pointer shader load overlap stops
- shader load out-of-bounds stops
- future memexport stops
- texture preload skips after shader state changes

Why this is correct:

- `pm4_command_processor_implement.h` already dispatches these packet families
  separately. They are visible boundaries, not hidden heuristics.
- Xenos resolve, events, waits, memory writes, and memexport can affect what
  later draws observe.

Watch out:

- Register writes are not all equivalent. Viewport/scissor/blend constants can
  be simulated; render target identity, shader address, resolve state, and
  memory write controls need explicit treatment.
- If unsure, stop. A stopped planner is slower, but still correct.

Validation:

- Planner stop telemetry should show why runs end.
- No unknown stop reason should be collapsed into "compatible".

### Patch 7: Add AttachmentPlan without enabling new store elision

Purpose: use the existing render-target semantics to plan load actions and pass
compatibility without changing store semantics yet.

Actions:

1. Factor a read-only render-target preflight helper out of the logic currently
   buried in `RenderTargetCache::Update` and render-pass descriptor construction.
2. The helper must report:
   - color/depth attachment identities
   - formats
   - sample count
   - fallback depth requirement
   - current ownership transfer needs
   - full-overwrite status
   - load-action-safe status
   - whether previous contents are needed
3. Reuse existing `PendingDrawPassTransferPlan` facts where possible:
   - `full_overwrite`
   - `load_action_safe`
   - transfer rectangle proof
4. Keep store action defaulted to `Store`.

Why this is correct:

- Existing code already uses `load_action_safe` to build
  `pending_draw_pass_load_dontcare_mask_`.
- Load action optimization is safe only when previous contents are not needed or
  a full clear/full overwrite is proven.

Watch out:

- Do not call mutating `Update` from the scanner.
- Partial guest resolve clears are not full attachment clears. A guest
  `1280x256` clear on a larger attachment cannot become full attachment
  `LoadActionClear`.
- AttachmentPlan must include depth/stencil attachment identity and fallback
  depth state, not just color RTs.

Validation:

- Existing pending draw-pass transfer telemetry remains valid:
  - update lists/transfers
  - accepted/fallback
  - full_overwrite
  - preflight attempt/success/fail
  - encode attempt/success/fail
- Render-pass compatibility rejects remain explainable by existing compatibility
  counters.

### Patch 8: Add ResolvePlan integration and tile-direct resolve scheduling

Purpose: preserve Xenos resolve semantics while avoiding unnecessary pass
breaks for proven in-pass tile direct host resolve cases.

Actions:

1. Reuse existing `MetalRenderTargetCache::ResolvePlan`.
2. Use `PrepareResolvePlan` at the real `EdramMode::kCopy` point.
3. Classify:
   - noop
   - copy-only
   - clear-only
   - copy+clear
4. For copy-only resolves, attempt tile-direct only if the source attachment is
   still active and all current `TryTileDirectHostResolveCopy` gates pass:
   - host render target path
   - valid plan
   - needs copy export
   - active render encoder and descriptor
   - not copy-clear
   - not scaled
   - not depth unless a proven depth path exists
   - not gamma until conversion semantics match guest PWL gamma
   - format/sample/rect/source/current-RT checks pass
5. For copy+clear and clear-only, stop unless a separate clear ownership proof is
   implemented.

Why this is correct:

- `xenos.h` defines resolve as guest-visible EDRAM copy/conversion/clear work.
  It must execute at the real resolve point, not during upload preflight.
- Tile direct resolve is a replacement for host RT dump + compute copy only when
  it writes the same guest-visible shared-memory result.

Watch out:

- Gamma direct host resolve is still unsafe unless direct resolve mirrors the
  existing dump-path gamma conversion.
- `StoreActionDontCare` after tile direct resolve requires separate AttachmentPlan
  liveness proof.
- `tile_execute_reject_no_active` means scheduling failed; shader capability is
  not necessarily the blocker.

Validation:

- `resolve_direct_host tile_execute attempt/success/reject` must move in the
  expected direction.
- `tile_execute_reject no_active` should decrease.
- `direct_host attempt/success` should not claim gamma/depth/format cases that
  are still rejected.

### Patch 9: Prove and enable StoreActionDontCare only under liveness proof

Purpose: get the Apple TBDR bandwidth win without discarding host RT contents
that Xenos semantics still require.

Apple API rules:

- Apple load/store actions define how render targets are loaded and stored.
  Choosing the right action can avoid unnecessary load/store work.
- `LoadActionDontCare` and `StoreActionDontCare` leave contents undefined where
  used.
- `StoreActionUnknown` is temporary only; a real store action must be specified
  before render encoding finishes.
- Memoryless render targets cannot use `LoadActionLoad`, `StoreActionStore`, or
  `StoreActionStoreAndMultisampleResolve`.

References:

- https://sosumi.ai/documentation/metal/setting-load-and-store-actions
- https://sosumi.ai/documentation/metal/mtlstoreaction/unknown
- https://sosumi.ai/documentation/metal/mtlrendercommandencoder

Liveness proof for color `StoreActionDontCare`:

Store may be `DontCare` only if all are true:

1. The current attachment is not a drawable.
2. The guest-visible result has already been exported/resolved if the guest can
   observe it through shared memory.
3. No later host RT load needs the attachment.
4. No later host RT sampling path needs the attachment.
5. No later ownership transfer/dump/readback/trace path needs the attachment.
6. The next use is either a full overwrite/full clear or a different RT.
7. Depth/stencil dependencies are handled independently.
8. The resolve/clear case is not copy+clear unless clear semantics are also
   proven.

Implementation:

1. Keep `StoreActionStore` as default.
2. Track `store_dontcare_eligible` as source-side potential only.
3. Add `store_dontcare_proven` only when liveness proof passes.
4. Call `setColorStoreAction(MTL::StoreActionDontCare, index)` only in the
   proven case.
5. If `StoreActionUnknown` is used at descriptor creation, always set final
   store action before `EndRenderEncoder`.

Why this is correct:

- Discarding a host RT is not guest-visible only when the guest-visible result
  has already been written and no later host observer needs the attachment.
- This matches the comment already in `metal_render_target_cache.cc:6255-6275`.

Watch out:

- "Resolved to shared memory" is not sufficient if a later pass still expects to
  load or sample the host RT.
- Partial clears and predicated tiling can keep old attachment contents relevant.
- Depth and stencil require separate handling.

Validation:

- `store_dontcare_applied` must be renamed or paired with
  `store_dontcare_proven`.
- In debug telemetry:
  - `store_dontcare_proven <= store_dontcare_eligible`
  - `store_dontcare_rejected` explains the unproven remainder
- Metal API validation must not report an unresolved `StoreActionUnknown`.

## Metal API Validation Checklist

Before considering the planner correct:

1. Enable Metal API Validation in an Xcode scheme or equivalent local launch
   configuration.
2. Run with Metal HUD/counters if available to compare encoder/store/load churn.
3. Capture at least one frame from the problem scene.
4. Verify no render pass ends with `MTLStoreActionUnknown`.
5. Verify no memoryless attachment is configured with invalid load/store actions.
6. Verify tile shader dispatch occurs only inside an active render command
   encoder.
7. Verify `setColorStoreAction(DontCare)` is called only after liveness proof.
8. Verify blit/compute encoders are not opened while a render encoder is active
   except at deliberate semantic boundaries.

## Telemetry Validation Contract

Use the same scene/trace before and after each patch. Do not compare unrelated
gameplay windows.

Failure baseline from the current experiment:

```text
deferred_draws queued=775816
deferred_draws flushes=20649
deferred_draws flush_draws=775816
transfer_command_buffer=6343
shared_memory_upload_before_draw_pass=6502
render_encoder created=18172
tile_execute_reject_no_active=240
store_dontcare eligible/attempt/applied=360/360/360
```

Expected after Patch 1:

- No deferred queue telemetry line exists.
- No `PreparedDraw` symbols exist.
- CPU time should drop back near pre-experiment baseline.
- Visual corruption may still exist until Patch 2 if tight vertex ranges remain.

Expected after Patch 2:

- Visual corruption from under-uploaded vertex fetch data should disappear.
- Conservative vertex fetch upload bytes may rise. That is acceptable.

Expected after UploadPlan v1:

- `render_run_planner_runs > 0`
- `render_run_planner_draws_covered > 0`
- `render_run_planner_smem_ranges_collected > 0`
- `shared_memory_request_upload_calls_no_active` rises
- `shared_memory_request_upload_calls_active` falls
- `render_encoder end shared_memory_upload_before_draw_pass` approaches zero
- `transfer_request shared_memory_upload active` approaches zero
- no new draw replay counters exist

Expected after ResolvePlan integration:

- `tile_execute_reject_no_active` falls
- `tile_execute_success` rises only for proven cases
- gamma/depth/uint/format reject buckets remain honest
- shared-memory write ranges from tile resolve still invalidate/mark texture
  cache correctly

Expected after StoreActionDontCare liveness:

- `store_dontcare_eligible` may be high
- `store_dontcare_proven` must be less than or equal to eligible
- `store_dontcare_applied` must equal proven, not merely eligible
- no visual regression
- no Metal API validation error

## Build And Static Validation

Minimum checks after each patch:

```bash
git status --short --branch
git diff --check
rg -n "PreparedDraw|pending_draw_run_|FlushPendingDrawRun|QueuePreparedDraw|EncodePreparedDraw|DeferredDraw" src/xenia/gpu/metal
./xb build --config release --disable-lto
```

Use only `./xb build --config release --disable-lto` for build validation unless
the user explicitly gives a new build command later. Do not substitute
target-specific Metal builds.

Runtime checks:

1. Run the same Gears scene or trace used for the failure telemetry.
2. Dump `MetalTelemetry[swap]` before and after.
3. Capture a screenshot or frame capture for the top-left corruption check.
4. Compare CPU time, render encoder count, transfer command buffer count, active
   upload counts, and tile direct resolve counters.

## Final Success Criteria

The rewrite is successful only when all are true:

1. No `PreparedDraw` queue remains.
2. Draws are encoded once through normal `IssueDraw`.
3. Conservative vertex-fetch residency is restored.
4. Host materialization uploads happen before render encoder creation for planned
   runs.
5. Active-encoder upload misses are explicitly counted and near zero in the
   target scene.
6. Xenos resolve/copy/clear semantics remain grounded in `xenos.h`.
7. Tile direct resolve executes only for proven copy-compatible cases.
8. `StoreActionDontCare` is applied only after attachment liveness proof.
9. Metal API validation is clean.
10. Telemetry shows reduced encoder/transfer churn without increased CPU draw
    replay cost.
