/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2025 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_METAL_METAL_COMMAND_PROCESSOR_H_
#define XENIA_GPU_METAL_METAL_COMMAND_PROCESSOR_H_

#include <dispatch/dispatch.h>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "xenia/base/platform.h"
#include "xenia/base/string_buffer.h"
#include "xenia/gpu/command_processor.h"
#include "xenia/gpu/draw_util.h"
#include "xenia/gpu/dxbc_shader_translator.h"
#include "xenia/gpu/metal/dxbc_to_dxil_converter.h"
#include "xenia/gpu/metal/metal_geometry_shader.h"
#include "xenia/gpu/metal/metal_pipeline_cache.h"
#include "xenia/gpu/metal/metal_primitive_processor.h"
#include "xenia/gpu/metal/metal_render_target_cache.h"
#include "xenia/gpu/metal/metal_shader.h"
#include "xenia/gpu/metal/metal_shader_converter.h"
#include "xenia/gpu/metal/metal_shared_memory.h"
#include "xenia/gpu/metal/metal_texture_cache.h"
#include "xenia/gpu/metal/metal_upload_buffer_pool.h"
// clang-format off
// Must come after metal_texture_cache.h which includes Metal.hpp
#include "third_party/metal-shader-converter/include/metal_irconverter_runtime.h"
// clang-format on
#include "xenia/ui/metal/metal_api.h"
#include "xenia/ui/metal/metal_provider.h"

namespace MTL {
class BlitCommandEncoder;
class ComputeCommandEncoder;
class Fence;
class Heap;
class SharedEvent;
}  // namespace MTL

namespace xe {
namespace gpu {
namespace metal {

class MetalGraphicsSystem;

class MetalCommandProcessor final : public CommandProcessor {
 protected:
#define OVERRIDING_BASE_CMDPROCESSOR
#include "../pm4_command_processor_declare.h"
#undef OVERRIDING_BASE_CMDPROCESSOR

 public:
  explicit MetalCommandProcessor(MetalGraphicsSystem* graphics_system,
                                 kernel::KernelState* kernel_state);
  ~MetalCommandProcessor();

  void TracePlaybackWroteMemory(uint32_t base_ptr, uint32_t length) override;
  void RestoreEdramSnapshot(const void* snapshot) override;
  void ClearCaches() override;
  void InvalidateGpuMemory() override;
  void ClearReadbackBuffers() override;

  ui::metal::MetalProvider& GetMetalProvider() const;

  // Get the Metal device and command queue
  MTL::Device* GetMetalDevice() const { return device_; }
  MTL::CommandQueue* GetMetalCommandQueue() const { return command_queue_; }
  MTL::CommandBuffer* GetCurrentCommandBuffer() const {
    return current_command_buffer_;
  }

  // Submission coordination helpers — callers use these to query or obtain
  // command buffers for transfer/upload work without reaching into internal
  // command-processor state.
  bool HasActiveSubmission() const {
    return current_command_buffer_ != nullptr;
  }
  // Returns true when upload/transfer work can join the current submission's
  // command buffer. This is the case when a command buffer exists but no render
  // encoder is open.
  bool CanJoinActiveSubmissionForTransfer() const {
    return current_command_buffer_ != nullptr &&
           current_render_encoder_ == nullptr;
  }
  // Returns a command buffer suitable for transfer (blit/compute) work.
  // If a render encoder is active it is ended first; if no command buffer
  // exists one is created. This is an encoder-lifetime break, not necessarily
  // a command-buffer submission break. Returns nullptr on failure.
  MTL::CommandBuffer* RequestTransferCommandBuffer();

  // Standalone (detached) transfer command-buffer helpers.
  // These create command buffers that are independent of the active submission
  // and are used by caches for upload/transfer work that cannot join the
  // current command buffer.  The returned CB is retained; ownership transfers
  // back via CommitStandaloneAsync or CommitStandaloneAndWait.
  MTL::CommandBuffer* CreateStandaloneTransferCommandBuffer(const char* label);
  // Commit a standalone command buffer asynchronously (fire-and-forget).
  // The CB is released via a completion handler.
  void CommitStandaloneAsync(MTL::CommandBuffer* cmd);
  // Commit a standalone command buffer synchronously and wait for completion.
  // The CB is released before returning.
  void CommitStandaloneAndWait(MTL::CommandBuffer* cmd);

  uint64_t GetCurrentSubmission() const;
  uint64_t GetCompletedSubmission() const override;
  uint64_t GetLatestSubmissionStarted() const { return submission_current_; }
  MTL::CommandBuffer* EnsureCommandBuffer();
  void EndRenderEncoder();
  void InvalidateRenderEncoderStateAfterDrawPassTransfers(
      MetalRenderTargetCache::DrawPassTransferEncoderMutationMask mutations);
  void ResetRenderEncoderResourceUsage();
  void MarkBindlessStableResourcesDirty();
  void UseRenderEncoderResource(MTL::Resource* resource,
                                MTL::ResourceUsage usage);
  void EnsureCommandBufferAutoreleasePool();
  void DrainCommandBufferAutoreleasePool();

  // Force issue a swap to push render target to presenter (for trace dumps)
  void ForceIssueSwap();
  bool HasSeenSwap() const { return saw_swap_; }
  void SetSwapDestSwap(uint32_t dest_base, bool swap);
  bool ConsumeSwapDestSwap(uint32_t dest_base, bool* swap_out);

  MetalSharedMemory* shared_memory() const { return shared_memory_.get(); }
  MetalRenderTargetCache* render_target_cache() const {
    return render_target_cache_.get();
  }
  MetalTextureCache* texture_cache() const { return texture_cache_.get(); }

  // D3D12-style shared-memory hazard tracking for GPU writes produced by
  // resolves and memexport draws, expressed with Metal fences/barriers.
  struct SharedMemoryRange {
    uint32_t start = 0;
    uint32_t length = 0;
  };

  struct SharedMemoryReadDependency {
    bool needs_fence_wait = false;
  };

  void MarkSharedMemoryComputeWritePending(
      uint32_t address, uint32_t length, MTL::ComputeCommandEncoder* encoder);
  bool PrepareSharedMemoryComputeReadDependency(
      const SharedMemoryRange* ranges, uint32_t range_count,
      bool consumer_can_join_current_submission,
      SharedMemoryReadDependency* dependency_out);
  bool EncodeSharedMemoryComputeReadDependency(
      MTL::ComputeCommandEncoder* encoder,
      const SharedMemoryReadDependency& dependency,
      const SharedMemoryRange* ranges, uint32_t range_count);

  // Persistent bindless descriptor heap allocators.
  uint32_t AllocateViewBindlessIndex();
  void ReleaseViewBindlessIndex(uint32_t index);
  void RetireViewBindlessIndex(uint32_t index);
  uint32_t GetViewBindlessHeapAvailableCount() const;
  uint32_t AllocateSamplerBindlessIndex();
  void ReleaseSamplerBindlessIndex(uint32_t index);
  IRDescriptorTableEntry* GetViewBindlessHeapEntry(uint32_t index);
  IRDescriptorTableEntry* GetSamplerBindlessHeapEntry(uint32_t index);

 protected:
  bool SetupContext() override;
  void ShutdownContext() override;
  void InitializeShaderStorage(
      const std::filesystem::path& cache_root, uint32_t title_id, bool blocking,
      std::function<void()> completion_callback = nullptr) override;

  // Flush pending GPU work before entering wait state.
  // This ensures Metal command buffers are submitted and completed before
  // the autorelease pool is drained, preventing hangs from deferred
  // deallocation.
  void PrepareForWait() override;

  // Use base class WriteRegister - don't override with empty implementation!
  // The base class stores values in register_file_->values[] which we need.
  void OnPrimaryBufferEnd() override;
  void OnGammaRamp256EntryTableValueWritten() override;
  void OnGammaRampPWLValueWritten() override;

  void IssueSwap(uint32_t frontbuffer_ptr, uint32_t frontbuffer_width,
                 uint32_t frontbuffer_height) override;

  Shader* LoadShader(xenos::ShaderType shader_type,
                     const uint32_t* host_address,
                     uint32_t dword_count) override;

  bool IssueDraw(xenos::PrimitiveType primitive_type, uint32_t index_count,
                 IndexBufferInfo* index_buffer_info,
                 bool major_mode_explicit) override;
  bool IssueCopy() override;
  void WriteRegister(uint32_t index, uint32_t value) override;
  void WriteRegistersFromMem(uint32_t start_index, uint32_t* base,
                             uint32_t num_registers) override;
  void WriteRegisterRangeFromRing(xe::RingBuffer* ring, uint32_t base,
                                  uint32_t num_registers) override;

  void WriteALURangeFromRing(xe::RingBuffer* ring, uint32_t base,
                             uint32_t num_times);
  void WriteFetchRangeFromRing(xe::RingBuffer* ring, uint32_t base,
                               uint32_t num_times);
  void WriteBoolRangeFromRing(xe::RingBuffer* ring, uint32_t base,
                              uint32_t num_times);
  void WriteLoopRangeFromRing(xe::RingBuffer* ring, uint32_t base,
                              uint32_t num_times);
  void WriteREGISTERSRangeFromRing(xe::RingBuffer* ring, uint32_t base,
                                   uint32_t num_times);

  void WriteALURangeFromMem(uint32_t start_index, uint32_t* base,
                            uint32_t num_registers);
  void WriteFetchRangeFromMem(uint32_t start_index, uint32_t* base,
                              uint32_t num_registers);
  void WriteBoolRangeFromMem(uint32_t start_index, uint32_t* base,
                             uint32_t num_registers);
  void WriteLoopRangeFromMem(uint32_t start_index, uint32_t* base,
                             uint32_t num_registers);
  void WriteREGISTERSRangeFromMem(uint32_t start_index, uint32_t* base,
                                  uint32_t num_registers);

  bool CanFastWriteRegisterRange(uint32_t start_index,
                                 uint32_t num_registers) const;
  bool TryWriteKnownRegisterRangeFromMem(uint32_t start_index, uint32_t* base,
                                         uint32_t num_registers);
  void WriteFastRegisterRangeFromRing(xe::RingBuffer* ring, uint32_t base,
                                      uint32_t num_registers);
  void WriteShaderConstantsFromMem(uint32_t start_index, uint32_t* base,
                                   uint32_t num_registers);
  void WriteBoolLoopConstantsFromMem(uint32_t start_index, uint32_t* base,
                                     uint32_t num_registers);
  void WriteFetchConstantsFromMem(uint32_t start_index, uint32_t* base,
                                  uint32_t num_registers);

  static constexpr size_t kStageVertex = 0;
  static constexpr size_t kStagePixel = 1;
  static constexpr size_t kStageCount = 2;  // Vertex + pixel.
  static constexpr size_t kCbvSlotCount = 5;
  enum CbvSlot : size_t {
    kCbvSlotSystem,
    kCbvSlotFloat,
    kCbvSlotBoolLoop,
    kCbvSlotFetch,
    kCbvSlotDescriptorIndices,
  };

  // Per-draw uniform buffer coordinates passed between IssueDraw sub-methods.
  struct UniformBufferInfo {
    struct Cbv {
      MTL::Buffer* buffer = nullptr;
      NS::UInteger offset = 0;
      uint64_t gpu_address = 0;
      size_t size = 0;
      bool active = false;
    };

    std::array<std::array<Cbv, kCbvSlotCount>, kStageCount> cbvs = {};
    std::array<uint32_t, kStageCount> active_cbv_masks = {};
    std::array<DxbcShader::FetchConstantDwordMask, kStageCount>
        fetch_constant_dword_masks = {};
  };

  struct DrawDynamicState {
    MTL::Viewport viewport = {};
    MTL::ScissorRect scissor = {};
    draw_util::ViewportInfo viewport_info = {};
    reg::RB_DEPTHCONTROL depth_control = {};
    bool primitive_polygonal = false;
    bool rasterization_enabled = true;
    float blend_constants[4] = {};
  };

  // Vertex binding range for stage-in / geometry emulation.
  struct VertexBindingRange {
    uint32_t binding_index = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint32_t stride = 0;
  };

  struct PreparedIndexBuffer {
    MTL::Buffer* buffer = nullptr;
    uint64_t offset = 0;
  };

  bool PrepareGuestDMAIndexBufferForMemexport(
      const PrimitiveProcessor::ProcessingResult& primitive_processing_result,
      PreparedIndexBuffer& prepared_index_buffer_out);

  // Host draw path — prepare per-draw dynamic state and upload constant buffers
  // before entering the Metal render encoder.
  bool PrepareDrawConstants(
      const RegisterFile& regs, Shader* vertex_shader, Shader* pixel_shader,
      MetalShader* metal_vertex_shader, MetalShader* metal_pixel_shader,
      bool shared_memory_is_uav,
      bool is_rasterization_done,
      const PrimitiveProcessor::ProcessingResult& primitive_processing_result,
      uint32_t used_texture_mask, uint32_t normalized_color_mask,
      MTL::RenderPassDescriptor* render_pass_descriptor,
      UniformBufferInfo& uniforms_out, DrawDynamicState& dynamic_state_out);

  void ApplyDrawDynamicState(const DrawDynamicState& dynamic_state);

  // Host draw path — refresh top-level argument buffers when root CBV state
  // changes and bind them plus the stable descriptor heap buffers to the
  // render encoder when encoder state requires it.
  bool PopulateBindlessTables(MetalShader* metal_vertex_shader,
                              MetalShader* metal_pixel_shader,
                              bool shared_memory_is_uav,
                              MTL::ResourceUsage shared_memory_usage,
                              bool use_geometry_emulation,
                              bool use_tessellation_emulation,
                              const UniformBufferInfo& uniforms);

  // Host draw path — bind vertex buffers and dispatch the actual draw call
  // (tessellation, geometry emulation, or standard path), then track
  // memexport writes.
  bool DispatchDraw(
      const RegisterFile& regs,
      const PrimitiveProcessor::ProcessingResult& primitive_processing_result,
      bool use_tessellation_emulation,
      MetalPipelineCache::TessellationPipelineState*
          tessellation_pipeline_state,
      bool use_geometry_emulation,
      MetalPipelineCache::GeometryPipelineState* geometry_pipeline_state,
      bool shared_memory_is_uav, MTL::ResourceUsage shared_memory_usage,
      bool memexport_used, MTL::RenderStages memexport_write_stages,
      bool uses_vertex_fetch,
      const PreparedIndexBuffer* prepared_guest_dma_index_buffer,
      const std::vector<Shader::VertexBinding>& vb_bindings,
      const VertexBindingRange* vertex_ranges, uint32_t vertex_range_count,
      IndexBufferInfo* index_buffer_info);

 private:
  // Command buffer management
  enum class RenderEncoderEndReason : uint32_t {
    kUnknown,
    kPrepareForWait,
    kSwap,
    kCommandBufferEnd,
    kRequestTransferCommandBuffer,
    kSharedMemoryReadDependency,
    kRenderTargetUpdateDescriptorDirty,
    kPipelineDescriptorIncompatible,
    kTextureUploadBeforeDrawPass,
    kResolveNeedsBoundary,
    kBeginRenderEncoderDescriptorChanged,
    kCount,
  };

  static constexpr size_t kRenderEncoderEndReasonCount =
      static_cast<size_t>(RenderEncoderEndReason::kCount);

  struct BackendTelemetryStats {
    static constexpr size_t kBindlessTelemetryStageCount = 2;
    static constexpr size_t kBindlessTelemetryCbvSlotsPerStage = 5;
    static constexpr size_t kRenderEncoderBufferTelemetryStageCount = 4;

    uint64_t swaps = 0;
    uint64_t draw_calls = 0;
    uint64_t prepare_draw_constants = 0;
    uint64_t pipeline_sets = 0;
    uint64_t pipeline_set_skips = 0;
    uint64_t texture_request_work_draws = 0;
    uint64_t texture_request_work_active_encoder = 0;
    uint64_t texture_request_work_no_active_encoder = 0;
    uint64_t texture_request_work_pass_compatible = 0;
    uint64_t texture_request_work_pass_changing = 0;
    uint64_t texture_request_work_no_descriptor = 0;
    uint64_t texture_request_work_mask_or = 0;
    uint64_t texture_requests_before_encoder = 0;
    uint64_t texture_requests_after_encoder_begin = 0;

    uint64_t constant_upload_system = 0;
    uint64_t constant_upload_float_vertex = 0;
    uint64_t constant_upload_float_pixel = 0;
    uint64_t constant_upload_bool_loop = 0;
    uint64_t constant_upload_fetch = 0;
    uint64_t constant_upload_descriptor_indices_vertex = 0;
    uint64_t constant_upload_descriptor_indices_pixel = 0;
    uint64_t constant_upload_bytes = 0;
    uint64_t constant_dirty_float_layout_vertex = 0;
    uint64_t constant_dirty_float_layout_pixel = 0;
    uint64_t descriptor_dirty_vertex_sampler_layout = 0;
    uint64_t descriptor_dirty_vertex_sampler_params = 0;
    uint64_t descriptor_dirty_vertex_texture_layout = 0;
    uint64_t descriptor_dirty_vertex_texture_srv = 0;
    uint64_t descriptor_dirty_pixel_sampler_layout = 0;
    uint64_t descriptor_dirty_pixel_sampler_params = 0;
    uint64_t descriptor_dirty_pixel_texture_layout = 0;
    uint64_t descriptor_dirty_pixel_texture_srv = 0;
    uint64_t descriptor_index_texture_lookups_vertex = 0;
    uint64_t descriptor_index_sampler_lookups_vertex = 0;
    uint64_t descriptor_index_texture_lookups_pixel = 0;
    uint64_t descriptor_index_sampler_lookups_pixel = 0;
    uint64_t register_write_float_total = 0;
    uint64_t register_write_float_changed = 0;
    uint64_t register_write_float_unchanged = 0;
    uint64_t register_write_float_dirty = 0;
    uint64_t register_write_float_dwords_copied = 0;
    uint64_t register_write_float_dwords_compared = 0;
    uint64_t register_write_float_dirty_vertex = 0;
    uint64_t register_write_float_dirty_pixel = 0;
    uint64_t register_write_float_stage_already_dirty_vertex = 0;
    uint64_t register_write_float_stage_already_dirty_pixel = 0;
    uint64_t register_write_float_range_unused_vertex = 0;
    uint64_t register_write_float_range_unused_pixel = 0;
    uint64_t register_write_bool_loop_total = 0;
    uint64_t register_write_bool_loop_changed = 0;
    uint64_t register_write_bool_loop_unchanged = 0;
    uint64_t register_write_bool_loop_dirty = 0;
    uint64_t register_write_fetch_total = 0;
    uint64_t register_write_fetch_changed = 0;
    uint64_t register_write_fetch_unchanged = 0;
    uint64_t register_write_fetch_dirty = 0;
    uint64_t register_write_fetch_dwords_copied = 0;
    uint64_t register_write_fetch_slots_tested = 0;
    uint64_t register_write_fetch_dwords_compared = 0;
    uint64_t register_write_fetch_changed_slots = 0;
    uint64_t register_write_fetch_dirty_vertex = 0;
    uint64_t register_write_fetch_dirty_pixel = 0;
    uint64_t texture_fetch_constant_invalidations = 0;
    uint64_t register_range_mem_calls = 0;
    uint64_t register_range_ring_calls = 0;
    uint64_t register_range_ring_wraparound = 0;
    uint64_t register_range_fallback_calls = 0;
    uint64_t register_range_fast_float_dwords = 0;
    uint64_t register_range_fast_fetch_dwords = 0;
    uint64_t register_range_fast_bool_loop_dwords = 0;
    uint64_t register_range_fast_regular_dwords = 0;

    uint64_t begin_encoder_calls = 0;
    uint64_t begin_encoder_reused_compatible = 0;
    uint64_t begin_encoder_created = 0;
    uint64_t begin_encoder_descriptor_restarts = 0;
    uint64_t begin_encoder_resource_usage_resets = 0;
    uint64_t begin_encoder_descriptor_failures = 0;
    uint64_t begin_encoder_creation_failures = 0;

    uint64_t end_encoder_calls = 0;
    uint64_t end_encoder_active = 0;
    uint64_t end_encoder_no_active = 0;
    std::array<uint64_t, kRenderEncoderEndReasonCount> end_reasons = {};

    uint64_t pending_transfer_encode_attempts = 0;
    uint64_t pending_transfer_encode_successes = 0;
    uint64_t pending_transfer_encode_failures = 0;
    uint64_t pending_transfer_fallback_flush_successes = 0;
    uint64_t pending_transfer_fallback_flush_failures = 0;
    uint64_t pending_transfer_state_invalidations = 0;
    uint64_t pending_transfer_mutation_mask_or = 0;

    uint64_t bindless_populate_calls = 0;
    uint64_t bindless_table_reuse_hits = 0;
    uint64_t bindless_table_reuse_misses = 0;
    uint64_t bindless_table_miss_invalid = 0;
    uint64_t bindless_table_miss_cbv = 0;
    uint64_t bindless_table_miss_shared_memory_uav = 0;
    uint64_t bindless_table_allocations = 0;
    uint64_t bindless_table_bytes = 0;
    uint64_t bindless_root_cbv_pointer_writes = 0;
    uint64_t bindless_root_argument_bind_updates = 0;
    uint64_t bindless_root_argument_bind_skips = 0;
    std::array<uint64_t, kBindlessTelemetryStageCount>
        bindless_stage_cbv_same = {};
    std::array<uint64_t, kBindlessTelemetryStageCount>
        bindless_stage_cbv_changed = {};
    std::array<uint64_t, kBindlessTelemetryStageCount>
        bindless_stage_top_level_allocations = {};
    std::array<uint64_t, kBindlessTelemetryStageCount>
        bindless_stage_top_level_bytes = {};
    std::array<uint64_t, kBindlessTelemetryStageCount>
        bindless_stage_root_cbv_pointer_writes = {};
    std::array<uint64_t, kBindlessTelemetryStageCount>
        bindless_stage_active_cbv_mask_or = {};
    std::array<std::array<uint64_t, kBindlessTelemetryCbvSlotsPerStage>,
               kBindlessTelemetryStageCount>
        bindless_table_miss_cbv_slots = {};
    std::array<std::array<uint64_t, kBindlessTelemetryCbvSlotsPerStage>,
               kBindlessTelemetryStageCount>
        bindless_table_miss_active_cbv_slots = {};
    std::array<std::array<uint64_t, kBindlessTelemetryCbvSlotsPerStage>,
               kBindlessTelemetryStageCount>
        bindless_table_miss_inactive_cbv_slots = {};
    uint64_t bindless_resource_serial_hits = 0;
    uint64_t bindless_resource_serial_misses = 0;
    uint64_t bindless_resource_miss_invalid = 0;
    uint64_t bindless_resource_miss_shared_memory_uav = 0;
    uint64_t bindless_resource_miss_usage = 0;
    uint64_t bindless_resource_textures_tracked = 0;
    uint64_t bindless_resource_uniform_buffers_tracked = 0;

    uint64_t render_encoder_use_resource_calls = 0;
    uint64_t render_encoder_use_resource_redundant = 0;
    uint64_t render_encoder_use_resource_driver_calls = 0;
    uint64_t render_encoder_use_heap_calls = 0;
    uint64_t render_encoder_use_heap_redundant = 0;
    uint64_t render_encoder_use_heap_driver_calls = 0;
    std::array<uint64_t, kRenderEncoderBufferTelemetryStageCount>
        render_encoder_buffer_full_binds = {};
    std::array<uint64_t, kRenderEncoderBufferTelemetryStageCount>
        render_encoder_buffer_offset_binds = {};
    std::array<uint64_t, kRenderEncoderBufferTelemetryStageCount>
        render_encoder_buffer_skipped_binds = {};
    std::array<uint64_t, kRenderEncoderBufferTelemetryStageCount>
        render_encoder_buffer_null_binds = {};
    std::array<uint64_t, kRenderEncoderBufferTelemetryStageCount>
        render_encoder_buffer_untracked_full_binds = {};
  };

  void FlushCommandBufferAndWait(uint64_t timeout_ns, const char* context);
  MTL::RenderPassDescriptor* GetDrawRenderPassDescriptor(
      bool fallback_depth_attachment_required = false);
  bool BeginRenderEncoderForDraw(
      bool fallback_depth_attachment_required = false);
  void EndRenderEncoder(RenderEncoderEndReason reason);
  void EndCommandBuffer();
  bool CanEndSubmissionImmediately();
  void WaitForPendingCompletionHandlers();
  void ProcessCompletedSubmissions();
  void MaybeDumpBackendTelemetry(const char* reason, bool force = false);
  void ResetBackendTelemetry();

  void UseRenderEncoderAttachmentHeaps(MTL::RenderPassDescriptor* descriptor);
  void UseRenderEncoderHeap(MTL::Heap* heap);
  uint64_t GetBindlessDescriptorRetirementSubmission() const;
  void FreeViewBindlessIndexNow(uint32_t index);
  void FreeSamplerBindlessIndexNow(uint32_t index);

  struct PendingSharedMemoryWrite {
    uint32_t start = 0;
    uint32_t end = 0;
    uint64_t submission_id = 0;
    MTL::RenderStages producer_stages = MTL::RenderStages(0);
    bool active_render_encoder = false;
    bool fence_updated = false;
  };

  void MarkSharedMemoryWritePending(uint32_t address, uint32_t length,
                                    MTL::RenderStages producer_stages,
                                    bool active_render_encoder,
                                    bool fence_updated);
  bool PendingSharedMemoryWritesOverlapRange(uint32_t start,
                                             uint32_t length) const;
  bool PendingSharedMemoryWriteOverlapsRanges(
      const PendingSharedMemoryWrite& pending, const SharedMemoryRange* ranges,
      uint32_t range_count) const;
  bool PendingSharedMemoryWritesOverlapRanges(
      const SharedMemoryRange* ranges, uint32_t range_count) const;
  void UpdateSharedMemoryFenceForActiveRenderEncoder();
  void PruneCompletedSharedMemoryWrites(uint64_t completed_submission);
  void RetireFenceWaitedSharedMemoryWrites(const SharedMemoryRange* ranges,
                                           uint32_t range_count);
  bool EncodeSharedMemoryRenderReadDependencies(
      const SharedMemoryRange* ranges, uint32_t range_count,
      MTL::RenderStages consumer_stages);
  bool EncodeSharedMemoryBlitReadDependency(MTL::BlitCommandEncoder* encoder,
                                            uint32_t start, uint32_t length);

  // Fixed-function depth/stencil state (mirrors Vulkan/D3D12 dynamic state).
  void ApplyDepthStencilState(bool primitive_polygonal,
                              reg::RB_DEPTHCONTROL normalized_depth_control);
  void ApplyRasterizerState(bool primitive_polygonal);

  // Constants for the MSC path.
  static constexpr size_t kNullBufferSize = 4096;
  static constexpr size_t kCbvSizeBytes = 4096;

  // Constants for MSC descriptor heap sizes.
  static constexpr size_t kResourceHeapSlotsPerTable = 1025 + 2;
  static constexpr size_t kSamplerHeapSlotsPerTable = 257 + 2;
  static constexpr size_t kTopLevelABSlotsPerTable = 32;
  static constexpr size_t kTopLevelABBytesPerTable =
      kTopLevelABSlotsPerTable * sizeof(uint64_t);
  // MSC explicit root signatures encode descriptor-table pointers and root
  // resource pointers as 64-bit entries in the top-level argument buffer.
  // Keep these in the same order as MetalShaderConverter's root signature.
  enum TopLevelABSlot : uint32_t {
    kTopLevelABSlotSRVSpace0,
    kTopLevelABSlotSRVSpace1,
    kTopLevelABSlotSRVSpace2,
    kTopLevelABSlotSRVSpace3,
    kTopLevelABSlotSRVSpace10,
    kTopLevelABSlotUAVSpace0,
    kTopLevelABSlotUAVSpace1,
    kTopLevelABSlotUAVSpace2,
    kTopLevelABSlotUAVSpace3,
    kTopLevelABSlotSamplerSpace0,
    kTopLevelABSlotCBVSystem,
    kTopLevelABSlotCBVFloat,
    kTopLevelABSlotCBVBoolLoop,
    kTopLevelABSlotCBVFetch,
    kTopLevelABSlotCBVDescriptorIndices,
  };

  // System constants population (mirrors D3D12 implementation)
  void UpdateSystemConstantValues(bool shared_memory_is_uav,
                                  bool primitive_polygonal,
                                  uint32_t line_loop_closing_index,
                                  xenos::Endian index_endian,
                                  const draw_util::ViewportInfo& viewport_info,
                                  uint32_t used_texture_mask,
                                  reg::RB_DEPTHCONTROL normalized_depth_control,
                                  uint32_t normalized_color_mask);

  // Metal device and command queue (from provider)
  MTL::Device* device_ = nullptr;
  MTL::CommandQueue* command_queue_ = nullptr;
  MTL::SharedEvent* wait_shared_event_ = nullptr;
  uint64_t wait_shared_event_value_ = 0;
  MTL::Fence* shared_memory_fence_ = nullptr;

  // Current command buffer and encoder
  MTL::CommandBuffer* current_command_buffer_ = nullptr;
  MTL::RenderCommandEncoder* current_render_encoder_ = nullptr;
  MTL::RenderPassDescriptor* current_render_pass_descriptor_ = nullptr;
  NS::AutoreleasePool* command_buffer_autorelease_pool_ = nullptr;

  struct EncoderResourceUsage {
    MTL::Resource* resource = nullptr;
    uint32_t usage_bits = 0;
  };
  // Tracks resources marked via useResource for the current render encoder
  // to avoid redundant driver calls across draws within the same encoder.
  std::vector<EncoderResourceUsage> render_encoder_resource_usage_;
  std::unordered_map<MTL::Resource*, uint32_t>
      render_encoder_resource_usage_map_;
  std::vector<MTL::Heap*> render_encoder_heap_usage_;
  std::unordered_set<MTL::Heap*> render_encoder_heap_usage_set_;
  BackendTelemetryStats backend_telemetry_;
  uint64_t backend_telemetry_last_dump_swap_ = 0;

  // Shared memory for Xbox 360 memory access
  std::unique_ptr<MetalSharedMemory> shared_memory_;
  std::unique_ptr<MetalPrimitiveProcessor> primitive_processor_;
  bool frame_open_ = false;

  bool saw_swap_ = false;
  uint32_t last_swap_ptr_ = 0;
  uint32_t last_swap_width_ = 0;
  uint32_t last_swap_height_ = 0;
  std::unordered_map<uint32_t, bool> swap_dest_swaps_by_base_;

  // Pipeline cache (owns shaders, pipelines, shader translation components).
  std::unique_ptr<MetalPipelineCache> pipeline_cache_;

  struct DepthStencilStateKey {
    uint32_t depth_control;
    uint32_t stencil_ref_mask_front;
    uint32_t stencil_ref_mask_back;
    uint32_t polygonal_and_backface;
    bool operator==(const DepthStencilStateKey& other) const {
      return depth_control == other.depth_control &&
             stencil_ref_mask_front == other.stencil_ref_mask_front &&
             stencil_ref_mask_back == other.stencil_ref_mask_back &&
             polygonal_and_backface == other.polygonal_and_backface;
    }
    struct Hasher {
      size_t operator()(const DepthStencilStateKey& key) const {
        size_t h = size_t(key.depth_control);
        h ^= size_t(key.stencil_ref_mask_front) << 1;
        h ^= size_t(key.stencil_ref_mask_back) << 2;
        h ^= size_t(key.polygonal_and_backface) << 3;
        return h;
      }
    };
  };

  std::unordered_map<DepthStencilStateKey, MTL::DepthStencilState*,
                     DepthStencilStateKey::Hasher>
      depth_stencil_state_cache_;

  bool mesh_shader_supported_ = false;

  // Texture cache for guest texture uploads
  std::unique_ptr<MetalTextureCache> texture_cache_;

  // Render target cache for framebuffer management
  std::unique_ptr<MetalRenderTargetCache> render_target_cache_;

  // Null resources for unbound slots
  MTL::Buffer* null_buffer_ = nullptr;
  MTL::Texture* null_texture_ = nullptr;
  MTL::SamplerState* null_sampler_ = nullptr;

  // Persistent bindless descriptor heaps.
  // Canonical texture views and samplers get stable slot indices allocated on
  // demand and freed on destruction. Non-canonical texture views may use
  // submission-lifetime slots from the same heap. The heaps are bound once per
  // encoder.
  // 1M entries (24 MiB). Metal has no API-side descriptor heap cap — the heap
  // is just an MTLBuffer. D3D12 uses 262144 but doesn't need per-swizzle
  // texture views; 4x headroom covers the swizzled-view multiplier and avoids
  // exhausting the heap before the first submission completes.
  static constexpr uint32_t kViewBindlessHeapSize = 1048576;
  static constexpr uint32_t kSamplerBindlessHeapSize = 2048;
  MTL::Buffer* view_bindless_heap_ = nullptr;
  MTL::Buffer* sampler_bindless_heap_ = nullptr;
  // Explicit-layout top-level bindings for shared memory / EDRAM use small
  // dedicated system tables rather than overlapping the bindless texture heap.
  MTL::Buffer* system_view_tables_ = nullptr;
  static constexpr uint32_t kSystemViewTableSRVSharedMemory = 0;
  static constexpr uint32_t kSystemViewTableSRVNull = 1;
  static constexpr uint32_t kSystemViewTableUAVNullStart = 2;
  static constexpr uint32_t kSystemViewTableUAVSharedMemoryStart = 4;
  static constexpr uint32_t kSystemViewTableEntryCount = 6;

  // Simple bump allocator with free list for persistent heap slots.
  uint32_t view_bindless_heap_next_ = 0;
  std::vector<uint32_t> view_bindless_heap_free_;
  bool view_bindless_heap_exhausted_logged_ = false;
  uint32_t sampler_bindless_heap_next_ = 0;
  std::vector<uint32_t> sampler_bindless_heap_free_;
  bool sampler_bindless_heap_exhausted_logged_ = false;
  struct RetiredBindlessDescriptor {
    uint32_t index;
    uint64_t submission_id;
  };
  std::deque<RetiredBindlessDescriptor> retired_view_bindless_indices_;
  std::deque<RetiredBindlessDescriptor> retired_sampler_bindless_indices_;

  struct RetiredMetalBuffer {
    MTL::Buffer* buffer = nullptr;
    uint64_t submission_id = 0;
  };
  std::deque<RetiredMetalBuffer> retired_memexport_index_buffers_;

  MTL::Buffer* tessellator_tables_buffer_ = nullptr;

  // System constants - matches DxbcShaderTranslator::SystemConstants layout
  DxbcShaderTranslator::SystemConstants system_constants_;

  // Fixed-function dynamic state cached per render encoder.
  MTL::RenderPipelineState* current_render_pipeline_state_ = nullptr;
  float ff_blend_factor_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  bool ff_blend_factor_valid_ = false;
  bool rasterizer_state_valid_ = false;
  MTL::CullMode current_cull_mode_ = MTL::CullModeNone;
  MTL::Winding current_front_facing_winding_ = MTL::WindingCounterClockwise;
  MTL::TriangleFillMode current_triangle_fill_mode_ = MTL::TriangleFillModeFill;
  float current_depth_bias_values_[3] = {0.0f, 0.0f, 0.0f};
  MTL::DepthClipMode current_depth_clip_mode_ = MTL::DepthClipModeClip;
  MTL::DepthStencilState* current_depth_stencil_state_ = nullptr;
  bool stencil_reference_valid_ = false;
  uint32_t current_stencil_reference_ = 0;
  bool viewport_dirty_ = true;
  MTL::Viewport cached_viewport_ = {};
  bool scissor_dirty_ = true;
  MTL::ScissorRect cached_scissor_ = {};

  // Constant buffer dirty tracking (D3D12 pattern).
  // Each binding records the pool-allocated buffer, offset, and GPU address
  // for the most recent constant upload.  When up_to_date is true, the draw
  // reuses that upload through the top-level argument buffer instead of
  // gathering and uploading the same CBV again.
  struct ConstantBufferBinding {
    MTL::Buffer* buffer = nullptr;
    NS::UInteger offset = 0;
    uint64_t gpu_address = 0;
    size_t size = 0;
    bool up_to_date = false;
  };
  struct StageRootArgumentKey {
    std::array<uint64_t, kTopLevelABSlotsPerTable> pointers = {};
    std::array<size_t, kCbvSlotCount> cbv_sizes = {};
  };
  struct StageRootArgumentAllocation {
    MTL::Buffer* buffer = nullptr;
    NS::UInteger offset = 0;
    uint64_t gpu_address = 0;
    bool valid = false;
  };
  // MSC root arguments are one small top-level argument buffer per shader
  // stage.  The current tuple is carried forward while the CBV addresses,
  // sizes, shared-memory mode, and mesh/tessellation path stay unchanged;
  // otherwise a fresh table is written from the bump-allocated upload pool.
  StageRootArgumentKey BuildStageRootArgumentKey(
      const std::array<UniformBufferInfo::Cbv, kCbvSlotCount>& uniform_cbvs,
      bool shared_memory_is_uav) const;
  void WriteStageRootArgumentTable(uint64_t* top_level_ptrs,
                                   const StageRootArgumentKey& key) const;
  bool AllocateStageRootArgument(size_t stage_index,
                                 const StageRootArgumentKey& key,
                                 StageRootArgumentAllocation& allocation_out);
  ConstantBufferBinding cbuffer_binding_system_;
  ConstantBufferBinding cbuffer_binding_float_vertex_;
  ConstantBufferBinding cbuffer_binding_float_pixel_;
  ConstantBufferBinding cbuffer_binding_bool_loop_;
  std::array<ConstantBufferBinding, kStageCount> cbuffer_binding_fetch_stage_;
  ConstantBufferBinding cbuffer_binding_descriptor_indices_vertex_;
  ConstantBufferBinding cbuffer_binding_descriptor_indices_pixel_;
  std::array<DxbcShader::FetchConstantDwordMask, kStageCount>
      fetch_constant_dirty_masks_ = {};

  // Float constant usage bitmaps for the current shader pair.
  // Used to gate WriteRegister invalidation: only dirty the float CBV
  // when the written register is in the current shader's bitmap.
  // Matches D3D12's current_float_constant_map_vertex_/pixel_ at
  // d3d12_command_processor.h:829-830.
  uint64_t current_float_constant_map_vertex_[4] = {};
  uint64_t current_float_constant_map_pixel_[4] = {};
  size_t current_texture_layout_uid_vertex_ = 0;
  size_t current_texture_layout_uid_pixel_ = 0;
  size_t current_sampler_layout_uid_vertex_ = 0;
  size_t current_sampler_layout_uid_pixel_ = 0;
  std::vector<MetalTextureCache::TextureSRVKey>
      current_texture_srv_keys_vertex_;
  std::vector<MetalTextureCache::TextureSRVKey> current_texture_srv_keys_pixel_;
  std::vector<MetalTextureCache::SamplerParameters> current_samplers_vertex_;
  std::vector<MetalTextureCache::SamplerParameters> current_samplers_pixel_;
  std::vector<uint32_t> current_texture_bindless_indices_vertex_;
  std::vector<uint32_t> current_texture_bindless_indices_pixel_;
  std::vector<uint32_t> current_sampler_bindless_indices_vertex_;
  std::vector<uint32_t> current_sampler_bindless_indices_pixel_;
  std::vector<MTL::Texture*> current_texture_bindless_resources_vertex_;
  std::vector<MTL::Texture*> current_texture_bindless_resources_pixel_;
  std::array<bool, kStageCount> current_bindless_stage_root_valid_ = {};
  std::array<uint64_t, kStageCount> current_bindless_stage_root_serials_ = {};
  uint64_t current_bindless_stable_resources_serial_ = 0;
  std::array<uint64_t, kStageCount>
      render_encoder_bindless_stage_root_resource_serials_ = {};
  uint64_t render_encoder_bindless_stable_resources_serial_ = 0;
  std::array<uint64_t, kStageCount>
      render_encoder_bindless_stage_root_bind_serials_ = {};
  bool render_encoder_bindless_table_bind_mesh_path_ = false;
  bool render_encoder_bindless_table_bind_tessellation_ = false;
  std::array<StageRootArgumentAllocation, kStageCount>
      current_bindless_stage_root_arguments_ = {};
  std::array<std::array<uint64_t, kCbvSlotCount>, kStageCount>
      current_bindless_cbv_gpu_addresses_ = {};
  std::array<std::array<size_t, kCbvSlotCount>, kStageCount>
      current_bindless_cbv_sizes_ = {};
  std::array<DxbcShader::FetchConstantDwordMask, kStageCount>
      current_fetch_constant_dword_masks_ = {};
  bool current_bindless_shared_memory_is_uav_ = false;
  bool current_bindless_stable_resources_valid_ = false;
  bool current_bindless_stable_shared_memory_is_uav_ = false;
  uint32_t current_bindless_stable_shared_memory_usage_bits_ = 0;

  // Pool for per-draw constant buffer allocations (replaces the ring's
  // uniforms_buffer_ for constant data).
  std::unique_ptr<MetalUploadBufferPool> constant_buffer_pool_;
  enum class RenderEncoderBufferStage : uint32_t {
    kVertex,
    kFragment,
    kObject,
    kMesh,
    kCount,
  };
  struct RenderEncoderBufferBinding {
    MTL::Buffer* buffer = nullptr;
    NS::UInteger offset = 0;
    bool valid = false;
  };
  static constexpr size_t kTrackedRenderEncoderBufferBindingCount = 32;
  std::array<std::array<RenderEncoderBufferBinding,
                        kTrackedRenderEncoderBufferBindingCount>,
             size_t(RenderEncoderBufferStage::kCount)>
      render_encoder_buffer_bindings_ = {};
  void ResetRenderEncoderBufferBindings();
  void InvalidateRenderEncoderBufferBinding(RenderEncoderBufferStage stage,
                                            NS::UInteger index);
  void SetRenderEncoderBuffer(RenderEncoderBufferStage stage,
                              MTL::Buffer* buffer, NS::UInteger offset,
                              NS::UInteger index);
  void SetRenderEncoderVertexBuffer(MTL::Buffer* buffer, NS::UInteger offset,
                                    NS::UInteger index);
  void SetRenderEncoderFragmentBuffer(MTL::Buffer* buffer, NS::UInteger offset,
                                      NS::UInteger index);
  void SetRenderEncoderObjectBuffer(MTL::Buffer* buffer, NS::UInteger offset,
                                    NS::UInteger index);
  void SetRenderEncoderMeshBuffer(MTL::Buffer* buffer, NS::UInteger offset,
                                  NS::UInteger index);
  // Track which heap buffer binds have been set on the current encoder.
  bool heap_binds_set_on_encoder_ = false;

  std::atomic<uint64_t> completed_command_buffers_{0};
  std::atomic<uint32_t> pending_completion_handlers_{0};
  uint64_t submission_current_ = 0;
  uint64_t submission_completed_processed_ = 0;

  bool submission_has_draws_ = false;
  std::vector<PendingSharedMemoryWrite> pending_shared_memory_writes_;
  MTL::RenderStages active_render_encoder_shared_memory_write_stages_ =
      MTL::RenderStages(0);

  // Memexport tracking for shared memory invalidation.
  std::vector<draw_util::MemExportRange> memexport_ranges_;

  bool gamma_ramp_256_entry_table_up_to_date_ = false;
  bool gamma_ramp_pwl_up_to_date_ = false;

};

}  // namespace metal
}  // namespace gpu
}  // namespace xe

#endif  // XENIA_GPU_METAL_METAL_COMMAND_PROCESSOR_H_
