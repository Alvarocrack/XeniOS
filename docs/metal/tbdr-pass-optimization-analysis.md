# TBDR Pass Optimization Analysis for Xenia Metal Backend

This document classifies every render encoder restart site in the Metal backend
by whether the break is semantically necessary (guest requires externally
materialized state) or structurally necessary (artifact of the D3D12-shaped host
decomposition). Where a restart is structural, the document describes a concrete
Apple-TBDR-aware redesign path.

---

## Background: How Xenia's Render Pass Model Works

The Xbox 360 has 10 MB of embedded DRAM (EDRAM). Guest render targets are
regions of EDRAM tiles. The host emulates EDRAM by maintaining a map of tile
ownership ranges (`OwnershipRange` in `render_target_cache.h:607`). When a draw
targets EDRAM tiles currently owned by a different host render target, a
**transfer** is generated: the old owner's texture is read (via shader or blit)
and written to the new owner's texture so data is preserved.

This ownership-transfer system produces the bulk of the pass breaks in the Metal
backend, followed by the resolve (IssueCopy) path.

---

## Decision Table: Every Encoder Restart Site

### Site 1 -- Render target configuration change (Update / draw path)

**File:** `metal_command_processor.cc:6026-6028`
```
if (current_render_encoder_ &&
    current_render_pass_descriptor_ != pass_descriptor) {
  EndRenderEncoder();
}
```

**Trigger:** The guest changed which EDRAM regions or formats are bound as
render targets between draws. The render pass descriptor (attachments, formats,
sample counts) differs from the active encoder.

**Classification: Class A -- semantically necessary**

This is equivalent to a D3D12/Vulkan render pass end when attachments change.
Metal requires a new render command encoder when attachments change. No
TBDR workaround is possible -- different attachments = different pass.

**Possible TBDR win:** When only the *base tile offset* changes but format,
pitch, and sample count are identical, this is currently a full restart. On
TBDR, if the old and new targets could be aliased as memoryless with the same
format, the encoder restart could potentially be avoided by treating them as
the same attachment and adjusting tile addressing within the shader. This is
complex but would cover a common Xbox 360 pattern (multiple surfaces in EDRAM
at different base offsets with the same format).

---

### Site 2 -- Ownership transfers (PerformTransfersAndResolveClears)

**File:** `metal_render_target_cache.cc:4978`
```
command_processor_.EndRenderEncoder();
```

**Trigger:** `Update()` detected that the current draw's render targets overlap
EDRAM tiles owned by other host render targets. Data must be transferred before
drawing.

**What happens next:**
1. (Optional) Host depth store: a **compute encoder** reads the dest depth RT
   and writes to `edram_buffer_` (`cc:5032`).
2. (Optional) Blit transfers: a **blit encoder** copies between textures with
   compatible layouts (`cc:5562-5786`).
3. Shader-based transfers: a **render encoder** draws fullscreen quads reading
   source textures and writing to dest textures (`cc:5952-6149+`).
4. The main render encoder is then recreated for the actual draw.

**Classification: Class B/C -- partially structural**

The ownership transfer model itself is a **representation barrier**. The guest
never explicitly says "store this intermediate to external memory." The guest
just writes to EDRAM tiles and expects to read them later. The transfer chain
exists because the host represents EDRAM regions as separate textures.

**Sub-classification of transfer types:**

| Transfer Mode | When it occurs | Semantic? | TBDR Opportunity |
|---|---|---|---|
| `kColorToColor` | Color RT reuses tiles previously owned by different color RT (same format family) | Structural | **High** -- If both surfaces have the same format and are truly sequential (no concurrent access), tile memory could persist between "virtual" targets via imageblock or programmatic tile storage. |
| `kColorToDepth` | Color RT data reinterpreted as depth | Semi-semantic | Medium -- format reinterpretation genuinely requires a shader, but could be a tile shader if both live in the same pass via imageblock. |
| `kDepthToColor` | Depth RT data reinterpreted as color | Semi-semantic | Medium -- same as above. |
| `kDepthToDepth` | Depth RT tiles reassigned to depth with different format | Structural | Medium -- format conversion shader needed but could be tile-local. |
| `kColorToStencilBit` / `kDepthToStencilBit` | Stencil reconstruction from color/depth data | Structural | Medium -- currently 8 separate draws (one per stencil bit). Could potentially be a single tile shader. |
| `kColorAndHostDepthToDepth` / `kDepthAndHostDepthToDepth` | Depth transfer where host precision must be recovered from a float32 backup | Structural | Low -- requires reading a separate host depth texture. The host depth texture itself is a representation artifact, but removing it requires rethinking depth precision management. |

**Concrete TBDR redesign path:**

The highest-value target is **kColorToColor** transfers where source and dest
have the same format. These happen frequently when games reuse EDRAM tiles
across passes (extremely common on Xbox 360). On Apple TBDR:

1. Instead of separate host textures per EDRAM region, maintain a single
   **tile-addressed render target** per unique format+pitch+MSAA config.
2. Use Metal's **programmable tile shading** (tile functions, imageblock) to
   keep data resident in tile memory across what are currently separate "passes."
3. When a game binds new EDRAM base offsets with the same format/pitch, do NOT
   end the encoder. Instead, update the tile address offset as a shader constant.
4. This eliminates the store-from-old-RT + load-into-new-RT cycle entirely.

Estimated bandwidth savings: **Very large** for games that rapidly cycle
EDRAM allocations (most Xbox 360 titles).

---

### Site 3 -- IssueCopy (Resolve) path

**File:** `metal_command_processor.cc:5589`
```
EndRenderEncoder();
```

**What happens next (full chain):**
1. `EndRenderEncoder()` -- store all render target contents to memory.
2. `DumpRenderTargets()` -- compute encoder reads host RTs and writes packed
   tile data to `edram_buffer_` (the 10 MB EDRAM shadow).
3. Resolve compute shader -- reads from `edram_buffer_` and writes to shared
   memory (guest-visible RAM).
4. (Optional) Resolve clear -- transfers + clear of the resolved render targets.

**Classification: Class A (partially) + Class C**

The resolve itself is **semantically necessary** -- the guest explicitly
requests copying render target contents to main memory (`RB_COPY_DEST_BASE`).
This is a true externally-visible memory boundary.

However, the intermediate step through `edram_buffer_` is **purely structural**:

```
Host RT texture → [compute: dump to EDRAM buffer] → [compute: resolve from EDRAM buffer to shared memory]
```

This two-hop path exists because the D3D12/Vulkan backends use it. On Metal
TBDR, the resolve could potentially be:

```
Host RT texture → [compute: resolve directly to shared memory]
```

Or even better, if the resolve happens at the end of the render pass:

```
Tile memory → [tile end action / tile shader: resolve + format-convert to shared memory]
```

**Concrete TBDR redesign path:**

1. **Short term:** Fuse DumpRenderTargets + resolve into a single compute
   dispatch that reads the host RT texture and writes directly to shared memory.
   Eliminates one full read+write of the EDRAM buffer (up to 10 MB per resolve).

2. **Medium term:** When the resolve immediately follows drawing (common case),
   use Metal's `MTLStoreActionCustomSampleDepthStore` or a tile end-action to
   perform the resolve as part of the render pass store, avoiding the
   render-encoder restart entirely.

3. **Long term:** Use memoryless render targets for surfaces whose only
   consumer is an immediately-following resolve. The render target never needs
   to leave tile memory -- it goes directly from tile to shared memory.

---

### Site 4 -- Resolve clears

**File:** `metal_render_target_cache.cc:4899` (within Resolve)
```
PrepareHostRenderTargetsResolveClear(...)
→ PerformTransfersAndResolveClears(...)  // another encoder restart
```

**Trigger:** The guest resolve operation includes a clear of the source render
target (very common -- Xbox 360 games clear after resolve to prepare for the
next frame).

**Classification: Class C -- almost certainly architectural debt**

The clear is semantically necessary (guest requested it), but the way it's
implemented requires:
1. Ending the current encoder.
2. Running transfers to ensure ownership is correct.
3. Starting a new render encoder with `LoadActionClear`.
4. Ending that encoder.
5. Starting yet another render encoder for subsequent draws.

**TBDR redesign:**
The existing TODO in `render_target_cache.h:571` already identifies this:
```
// TODO(Triang3l): Try to defer clears until the first draw in the next pass
// (if it uses one or both render targets being cleared) for tile-based GPUs.
```

On TBDR, a clear should be deferred to the `LoadAction` of the next render pass
that uses the target. This is a well-understood optimization:
- Track "pending clear" state per render target.
- When starting a new encoder that uses a target with a pending clear, use
  `MTLLoadActionClear` instead of `MTLLoadActionLoad`.
- This eliminates an entire encoder restart cycle per resolve-clear.

Already partially implemented: `resolve_clear_via_load_action` logic exists in
`PerformTransfersAndResolveClears` (`cc:5830-5950`), but it only applies within
the transfer pass itself, not across passes.

---

### Site 5 -- Host depth store (compute dispatch within transfers)

**File:** `metal_render_target_cache.cc:5032`
```
MTL::ComputeCommandEncoder* encoder = cmd->computeCommandEncoder();
```

**Trigger:** When a depth render target is being transferred and the host
stores depth at higher precision (float32) than the guest (float24/unorm24),
the current host depth values must be saved to `edram_buffer_` before the
transfer overwrites them.

**Classification: Class B -- suspicious**

This exists to preserve host-side depth precision that would otherwise be lost
during transfer. The question is whether that precision is actually observed by
the guest. If the guest only sees float24/unorm24 precision, the extra float32
precision is a host optimization that might not justify the bandwidth cost of an
extra compute pass on TBDR.

**TBDR redesign:**
- Profile whether the float32 depth precision recovery actually improves visual
  quality enough to justify the bandwidth. On TBDR, the store to `edram_buffer_`
  and reload may cost more than the precision gain.
- If kept, could potentially be fused into the transfer render pass as a tile
  shader that reads depth from the imageblock.

---

### Site 6 -- IssueSwap

**File:** `metal_command_processor.cc:2531`
```
EndRenderEncoder();
```

**Classification: Class A -- semantically necessary**

Frame presentation requires submitting the command buffer. No optimization
possible -- this is a hard boundary.

---

### Site 7 -- PrepareForWait (CPU sync)

**File:** `metal_command_processor.cc:1718`
```
EndRenderEncoder();
```

**Classification: Class A -- semantically necessary**

CPU needs to wait for GPU completion. Hard boundary.

---

### Site 8 -- EndCommandBuffer

**File:** `metal_command_processor.cc:6581`
```
EndRenderEncoder();
```

**Classification: Class A -- semantically necessary**

Command buffer granularity boundary.

---

### Site 9 -- EDRAM load/store compute dispatches

**Files:** `metal_render_target_cache.cc:4008` (LoadTiledData),
`metal_render_target_cache.cc:4115` (DumpRenderTargets)

**Classification: Class C for TBDR**

These compute dispatches convert between the host's linear texture layout and
the guest's tiled EDRAM layout. Each one requires ending any active render
encoder, dispatching compute, then potentially starting a new render encoder.

On TBDR, the tiled layout of EDRAM is conceptually similar to how tile memory
already works. The ideal path would bypass the linear↔tiled conversion entirely
by mapping EDRAM tiles directly to Metal tile memory.

---

## Priority-Ranked Optimization Opportunities

### Tier 1: High impact, moderate complexity

1. **Defer resolve clears to next pass LoadAction** (Site 4)
   - Impact: Eliminates 1-2 encoder restarts per resolve.
   - Games affected: Nearly all (resolve+clear is ubiquitous).
   - Complexity: Moderate -- need "pending clear" tracking per RT.
   - The TODO already exists in the codebase.

2. **Fuse DumpRenderTargets + resolve compute** (Site 3)
   - Impact: Eliminates 1 full EDRAM buffer read+write (up to 10 MB) per
     resolve.
   - Complexity: Moderate -- merge two compute shaders into one.

3. **Use LoadActionDontCare / memoryless for scratch RTs** (Site 2)
   - Impact: Eliminates load bandwidth for targets that are fully overwritten.
   - The code already partially detects this (`transfer_pass_load_dontcare`
     at `cc:5937`), but could be more aggressive.

### Tier 2: High impact, high complexity

4. **Tile-addressed render targets for same-format EDRAM reuse** (Site 2)
   - Impact: Eliminates the most common transfer type (kColorToColor).
   - Complexity: High -- requires rethinking how EDRAM regions map to host
     textures. Would use Metal programmable tile functions.

5. **Resolve as tile end-action** (Site 3)
   - Impact: Eliminates the encoder restart for resolve entirely when resolve
     follows drawing.
   - Complexity: High -- requires integrating resolve logic into the render
     pass store actions.

### Tier 3: Speculative, requires profiling

6. **Re-evaluate host depth precision preservation** (Site 5)
   - Measure whether float32→float24 precision recovery is worth the extra
     compute pass on Apple Silicon.

7. **Programmable tile shading for format reinterpretation transfers** (Site 2)
   - kColorToDepth, kDepthToColor could theoretically be done via tile shaders
     without leaving tile memory.

---

## Correctness Invariants That Must Be Maintained

Regardless of optimization, these guest-semantic guarantees must hold:

1. **EDRAM tile aliasing correctness:** When the guest rebinds EDRAM tiles to a
   different RT config, subsequent draws must see data from the previous owner
   (correctly reinterpreted for the new format if formats differ).

2. **Resolve produces correct shared memory contents:** The resolve path must
   write guest-format-correct data to the guest-specified memory address.
   Format conversion (host→guest bit packing) must be exact.

3. **Depth precision:** Where the host uses float32 for guest float24/unorm24,
   the values when converted back to guest format must match what the guest
   would have computed. (This is the invariant that host depth store protects.)

4. **MSAA sample correctness:** Transfers between different MSAA sample counts
   must correctly map samples. The current per-sample-id transfer logic must
   be preserved.

5. **Resolve clear timing:** The clear must take effect at the correct point
   in the command stream. Deferring to LoadAction is safe only if no
   intervening operation reads the "cleared" state before the next draw.

---

## Summary

| Site | Location | Class | TBDR Opportunity | Priority |
|------|----------|-------|------------------|----------|
| RT config change | `metal_command_processor.cc:6028` | A | Low (except same-format base offset changes) | -- |
| Ownership transfers | `metal_render_target_cache.cc:4978` | B/C | **Very High** (tile-addressed RTs, imageblock) | Tier 2 |
| IssueCopy/Resolve | `metal_command_processor.cc:5589` | A+C | **High** (fuse dump+resolve, tile end-action) | Tier 1-2 |
| Resolve clears | `render_target_cache.cc:572` | C | **High** (defer to LoadAction) | Tier 1 |
| Host depth store | `metal_render_target_cache.cc:5032` | B | Medium (profile, possibly fuse) | Tier 3 |
| IssueSwap | `metal_command_processor.cc:2531` | A | None | -- |
| CPU sync | `metal_command_processor.cc:1718` | A | None | -- |
| Command buffer end | `metal_command_processor.cc:6581` | A | None | -- |
| EDRAM load/store | `metal_render_target_cache.cc:4008,4115` | C | High (bypass for TBDR) | Tier 2 |
