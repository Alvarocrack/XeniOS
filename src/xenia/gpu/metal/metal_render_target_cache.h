/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_METAL_METAL_RENDER_TARGET_CACHE_H_
#define XENIA_GPU_METAL_METAL_RENDER_TARGET_CACHE_H_

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "xenia/gpu/draw_util.h"
#include "xenia/gpu/register_file.h"
#include "xenia/gpu/render_target_cache.h"
#include "xenia/gpu/trace_writer.h"
#include "xenia/gpu/xenos.h"
#include "xenia/memory.h"

#include "third_party/metal-cpp/Metal/Metal.hpp"

struct IRDescriptorTableEntry;

namespace xe {
namespace gpu {
namespace metal {

class MetalCommandProcessor;
class MetalHeapPool;

class MetalRenderTargetCache final : public gpu::RenderTargetCache {
 public:
  // Metal-specific render target - defined inside cache class to access
  // protected RenderTarget
  class MetalRenderTarget final : public RenderTarget {
   public:
    ~MetalRenderTarget() override;

    MTL::Texture* texture() const { return texture_; }
    MTL::Texture* msaa_texture() const { return msaa_texture_; }
    MTL::Texture* draw_texture() const {
      return draw_texture_ ? draw_texture_ : texture_;
    }
    MTL::Texture* transfer_texture() const {
      return transfer_texture_ ? transfer_texture_ : texture_;
    }
    MTL::Texture* msaa_draw_texture() const {
      return msaa_draw_texture_ ? msaa_draw_texture_ : msaa_texture_;
    }
    MTL::Texture* msaa_transfer_texture() const {
      return msaa_transfer_texture_ ? msaa_transfer_texture_ : msaa_texture_;
    }
    MTL::Texture* stencil_view() const { return stencil_view_; }
    void SetStencilView(MTL::Texture* view) { stencil_view_ = view; }

    void SetTemporarySortIndex(uint32_t index) {
      temporary_sort_index_ = index;
    }
    uint32_t temporary_sort_index() const { return temporary_sort_index_; }

    void SetTexture(MTL::Texture* texture) {
      if (texture_ != texture) {
        if (stencil_view_) {
          stencil_view_->release();
          stencil_view_ = nullptr;
        }
        texture_ = texture;
      }
    }
    void SetMsaaTexture(MTL::Texture* texture) { msaa_texture_ = texture; }
    void SetDrawTexture(MTL::Texture* texture) { draw_texture_ = texture; }
    void SetTransferTexture(MTL::Texture* texture) {
      transfer_texture_ = texture;
    }
    void SetMsaaDrawTexture(MTL::Texture* texture) {
      msaa_draw_texture_ = texture;
    }
    void SetMsaaTransferTexture(MTL::Texture* texture) {
      msaa_transfer_texture_ = texture;
    }
    bool needs_initial_clear() const { return needs_initial_clear_; }
    void SetNeedsInitialClear(bool needs_initial_clear) {
      needs_initial_clear_ = needs_initial_clear;
    }

    // Public constructor for creating render targets
    MetalRenderTarget(RenderTargetKey key) : RenderTarget(key) {}

   private:
    MTL::Texture* texture_ = nullptr;
    MTL::Texture* msaa_texture_ = nullptr;  // If MSAA is enabled
    MTL::Texture* draw_texture_ = nullptr;
    MTL::Texture* transfer_texture_ = nullptr;
    MTL::Texture* msaa_draw_texture_ = nullptr;
    MTL::Texture* msaa_transfer_texture_ = nullptr;
    MTL::Texture* stencil_view_ = nullptr;
    uint32_t temporary_sort_index_ = UINT32_MAX;
    bool needs_initial_clear_ = true;
  };

 public:
  MetalRenderTargetCache(const RegisterFile& register_file,
                         const Memory& memory, TraceWriter* trace_writer,
                         uint32_t draw_resolution_scale_x,
                         uint32_t draw_resolution_scale_y,
                         MetalCommandProcessor& command_processor);
  ~MetalRenderTargetCache() override;

  bool Initialize();
  void Shutdown(bool from_destructor = false);

  // RenderTargetCache implementation
  Path GetPath() const override;

  // Fixed-point render targets (k_16_16 / k_16_16_16_16) are backed by *_SNORM
  // formats in the host render targets path, which are -1...1 rather than the
  // Xbox 360's -32...32 range. When this is true, resolve/copy must compensate
  // to match the guest packing expectations.
  bool IsFixedRG16TruncatedToMinus1To1() const {
    return !cvars::snorm16_render_target_full_range;
  }
  bool IsFixedRGBA16TruncatedToMinus1To1() const {
    return !cvars::snorm16_render_target_full_range;
  }

  // Whether 2x MSAA is supported on this device.
  bool msaa_2x_supported() const { return msaa_2x_supported_; }

  // Whether gamma render targets use UNORM16 storage (separate from sRGB).
  // When true, gamma correction is done in shaders rather than via sRGB format.
  bool gamma_render_target_as_unorm16() const {
    return gamma_render_target_as_unorm16_;
  }

  bool IsGammaFormatHostStorageSeparate() const override;

  // Check if the render target key uses a 64bpp format.
  bool IsKey64bpp(RenderTargetKey key) const;

  void ClearCache() override;
  void BeginFrame() override;

  bool Update(bool is_rasterization_done,
              reg::RB_DEPTHCONTROL normalized_depth_control,
              uint32_t normalized_color_mask,
              const Shader& vertex_shader) override;

  // Metal-specific methods
  MTL::RenderPassDescriptor* GetRenderPassDescriptor(
      uint32_t expected_sample_count = 1,
      bool fallback_depth_attachment_required = false);
  bool IsRenderPassDescriptorCompatible(
      MTL::RenderPassDescriptor* pass_descriptor,
      uint32_t expected_sample_count = 1,
      bool fallback_depth_attachment_required = false) const;
  bool HasPendingDrawPassTransfers() const {
    return pending_draw_pass_transfer_mask_ != 0;
  }
  enum DrawPassTransferEncoderMutation : uint32_t {
    kDrawPassTransferEncoderMutationNone = 0,
    kDrawPassTransferEncoderMutationPipeline = 1u << 0,
    kDrawPassTransferEncoderMutationDepthStencil = 1u << 1,
    kDrawPassTransferEncoderMutationStencilReference = 1u << 2,
    kDrawPassTransferEncoderMutationViewport = 1u << 3,
    kDrawPassTransferEncoderMutationScissor = 1u << 4,
    kDrawPassTransferEncoderMutationVertexSlot0 = 1u << 5,
    kDrawPassTransferEncoderMutationVertexSlot1 = 1u << 6,
    kDrawPassTransferEncoderMutationFragmentSlot0 = 1u << 7,
    kDrawPassTransferEncoderMutationFragmentSlot1 = 1u << 8,
    kDrawPassTransferEncoderMutationFragmentTextures = 1u << 9,
  };
  using DrawPassTransferEncoderMutationMask = uint32_t;
  bool EncodePendingDrawPassTransfers(
      MTL::RenderCommandEncoder* encoder,
      MTL::RenderPassDescriptor* pass_descriptor,
      DrawPassTransferEncoderMutationMask* mutations_out = nullptr);
  bool FlushPendingDrawPassTransfers();

  static constexpr size_t kDrawPassTransferRejectionReasonCount = 14;
  enum class RenderPassDescriptorDirtyReason : uint32_t {
    kClearCache,
    kSampleCountOrFallbackChanged,
    kPendingFullOverwriteTransfer,
    kPendingInitialClearConsumed,
    kTargetsChanged,
    kLoadDontCareDescriptorConsumed,
    kRestoreInitialClearConsumed,
    kFallbackDepthCreateFailed,
    kStandaloneTransferInitialClearConsumed,
    kDescriptorDepthInitialClearConsumed,
    kDescriptorColorInitialClearConsumed,
    kCount,
  };
  static constexpr size_t kRenderPassDescriptorDirtyReasonCount =
      static_cast<size_t>(RenderPassDescriptorDirtyReason::kCount);
  enum class RenderPassCompatibilityReason : uint32_t {
    kCompatible,
    kNullDescriptor,
    kDepthAttachmentMismatch,
    kStencilAttachmentMismatch,
    kUnexpectedStencilAttachment,
    kFallbackDepthAttachmentMismatch,
    kUnexpectedDepthStencilAttachment,
    kColorAttachmentMismatch,
    kUnexpectedColorAttachment,
    kDummyColorAttachmentMismatch,
    kCount,
  };
  static constexpr size_t kRenderPassCompatibilityReasonCount =
      static_cast<size_t>(RenderPassCompatibilityReason::kCount);
  struct TelemetryStats {
    struct ResolveDirectHostTelemetry {
      uint64_t direct_host_attempt = 0;
      uint64_t direct_host_success = 0;
      uint64_t direct_host_reject_gamma = 0;
      uint64_t direct_host_reject_exp_bias = 0;
      uint64_t direct_host_reject_format_mismatch = 0;
      uint64_t direct_host_reject_sample_select = 0;
      uint64_t direct_host_reject_depth_no_fast = 0;
      uint64_t tile_candidate_total = 0;
      uint64_t tile_reject_source_not_current_rt = 0;
      uint64_t tile_reject_multi_source_rect = 0;
      uint64_t tile_reject_depth = 0;
      uint64_t tile_reject_copy_clear = 0;
      uint64_t tile_reject_uint_transfer_view = 0;
      uint64_t tile_accept = 0;
      uint64_t tile_accept_fast = 0;
      uint64_t tile_accept_full_color = 0;
      uint64_t tile_execute_attempt = 0;
      uint64_t tile_execute_success = 0;
      uint64_t tile_execute_reject = 0;
      uint64_t tile_execute_reject_invalid = 0;
      uint64_t tile_execute_reject_no_active = 0;
      static constexpr size_t kTileNoActiveLastEndReasonCount = 11;
      std::array<uint64_t, kTileNoActiveLastEndReasonCount>
          tile_execute_reject_no_active_last_end_reasons = {};
      uint64_t tile_execute_reject_scaled = 0;
      uint64_t tile_execute_reject_copy_clear = 0;
      uint64_t tile_execute_reject_depth = 0;
      uint64_t tile_execute_reject_shader = 0;
      uint64_t tile_execute_reject_full_color = 0;
      uint64_t tile_execute_reject_gamma = 0;
      uint64_t tile_execute_reject_sample_select = 0;
      uint64_t tile_execute_reject_exp_bias = 0;
      uint64_t tile_execute_reject_format_mismatch = 0;
      uint64_t tile_execute_reject_multi_source_rect = 0;
      uint64_t tile_execute_reject_source_not_current_rt = 0;
      uint64_t tile_execute_reject_uint_transfer_view = 0;
      uint64_t tile_execute_reject_texture = 0;
      uint64_t tile_execute_reject_attachment = 0;
      uint64_t tile_execute_reject_pipeline = 0;
      uint64_t tile_execute_reject_dest_buffer = 0;
      uint64_t store_dontcare_eligible = 0;
      uint64_t store_dontcare_skipped_disabled = 0;
      uint64_t store_dontcare_attempt = 0;
      uint64_t store_dontcare_applied = 0;
    };

    uint64_t render_pass_descriptor_requests = 0;
    uint64_t render_pass_descriptor_cache_hits = 0;
    uint64_t render_pass_descriptor_rebuilds = 0;
    uint64_t render_pass_descriptor_dirty_marks = 0;
    std::array<uint64_t, kRenderPassDescriptorDirtyReasonCount>
        render_pass_descriptor_dirty_reasons = {};
    uint64_t render_pass_compatibility_checks = 0;
    std::array<uint64_t, kRenderPassCompatibilityReasonCount>
        render_pass_compatibility_reasons = {};

    uint64_t update_transfer_lists = 0;
    uint64_t pending_draw_pass_transfer_lists = 0;
    uint64_t pending_draw_pass_transfer_count = 0;
    uint64_t pending_draw_pass_accepted_lists = 0;
    uint64_t pending_draw_pass_fallback_lists = 0;
    uint64_t pending_draw_pass_full_overwrite_lists = 0;
    uint64_t pending_draw_pass_preflight_attempts = 0;
    uint64_t pending_draw_pass_preflight_successes = 0;
    uint64_t pending_draw_pass_preflight_failures = 0;
    uint64_t pending_draw_pass_encode_attempts = 0;
    uint64_t pending_draw_pass_encode_successes = 0;
    uint64_t pending_draw_pass_encode_failures = 0;
    uint64_t pending_draw_pass_flush_attempts = 0;
    uint64_t pending_draw_pass_flush_successes = 0;
    uint64_t pending_draw_pass_flush_failures = 0;
    std::array<uint64_t, kDrawPassTransferRejectionReasonCount>
        pending_draw_pass_rejections = {};

    uint64_t resolve_plan_calls = 0;
    uint64_t resolve_plan_noops = 0;
    uint64_t resolve_plan_copy_export = 0;
    uint64_t resolve_plan_clear = 0;
    uint64_t resolve_plan_copy_only = 0;
    uint64_t resolve_plan_clear_only = 0;
    uint64_t resolve_plan_clear_color = 0;
    uint64_t resolve_plan_clear_depth = 0;
    uint64_t resolve_plan_copy_and_clear = 0;
    uint64_t resolve_plan_needs_encoder_end = 0;
    uint64_t resolve_plan_no_encoder_end = 0;
    uint64_t resolve_execute_calls = 0;
    ResolveDirectHostTelemetry resolve_direct_host = {};

    uint64_t perform_transfer_calls = 0;
    uint64_t perform_transfer_active_encoder_calls = 0;
    uint64_t perform_transfer_standalone_calls = 0;
    uint64_t perform_transfer_resolve_clear_calls = 0;
    uint64_t perform_transfer_no_work_calls = 0;
    uint64_t perform_transfer_work_calls = 0;
  };
  TelemetryStats GetAndResetTelemetryStats();

  bool IsRenderPassDescriptorDirty() const {
    return render_pass_descriptor_dirty_;
  }

  // Get current render targets for capture
  MTL::Texture* GetColorTarget(uint32_t index) const;
  MTL::Texture* GetDepthTarget() const;
  MTL::Texture* GetDummyColorTarget() const;
  MetalRenderTarget* GetColorRenderTarget(uint32_t index) const;
  // Get current render targets for pipeline attachment formats.
  MTL::Texture* GetColorTargetForDraw(uint32_t index) const;
  MTL::Texture* GetDepthTargetForDraw() const;
  MTL::Texture* GetDummyColorTargetForDraw() const;
  double GetDepthTargetClearDepth() const;

  // Get the last REAL (non-dummy) render targets for capture
  MTL::Texture* GetLastRealColorTarget(uint32_t index) const;
  MTL::Texture* GetLastRealDepthTarget() const;

  // Look up a render target texture by key for debug/trace viewer use.
  MTL::Texture* GetRenderTargetTexture(RenderTargetKey key) const;
  // Look up a color render target texture by key components for the trace
  // viewer without exposing RenderTargetKey.
  MTL::Texture* GetColorRenderTargetTexture(
      uint32_t pitch, xenos::MsaaSamples samples, uint32_t base,
      xenos::ColorRenderTargetFormat format) const;

  // Restore EDRAM contents from snapshot (for trace playback), matching
  // D3D12RenderTargetCache::RestoreEdramSnapshot.
  void RestoreEdramSnapshot(const void* snapshot);

  MTL::Buffer* GetEdramBuffer() const { return edram_buffer_; }
  bool WriteEdramUintPow2BindlessDescriptor(
      IRDescriptorTableEntry* entry, uint32_t element_size_bytes_pow2) const;
  void UseBindlessResources(MetalCommandProcessor& command_processor,
                            MTL::ResourceUsage usage) const;

  struct ResolvePlan {
    draw_util::ResolveInfo resolve_info = {};
    bool valid = false;
    bool noop = false;
    bool needs_copy_export = false;
    bool needs_resolve_clear = false;
    bool needs_render_encoder_end = false;
    uint32_t written_address = 0;
    uint32_t written_length = 0;
  };

  // Resolve (copy) render targets to shared memory
  bool PrepareResolvePlan(Memory& memory, ResolvePlan& plan_out);
  bool Resolve(Memory& memory, uint32_t& written_address,
               uint32_t& written_length,
               MTL::CommandBuffer* command_buffer = nullptr,
               const ResolvePlan* prepared_resolve_plan = nullptr);
  bool TryTileDirectHostResolveCopy(
      const ResolvePlan& resolve_plan,
      MTL::RenderCommandEncoder* active_render_encoder,
      MTL::RenderPassDescriptor* active_render_pass_descriptor,
      uint32_t inactive_last_end_reason, uint32_t& written_address,
      uint32_t& written_length);

 protected:
  // Virtual methods from RenderTargetCache
  uint32_t GetMaxRenderTargetWidth() const override;
  uint32_t GetMaxRenderTargetHeight() const override;

  RenderTarget* CreateRenderTarget(RenderTargetKey key) override;

  bool IsHostDepthEncodingDifferent(
      xenos::DepthRenderTargetFormat format) const override;
  void RequestPixelShaderInterlockBarrier() override;

 private:
  static uint32_t GetMetalEdramDumpFormat(RenderTargetKey key);
  MTL::Library* GetOrCreateEdramLoadLibrary(bool msaa);
  MTL::RenderPipelineState* GetOrCreateEdramLoadPipeline(
      MTL::PixelFormat dest_format, uint32_t sample_count);
  bool InitializeEdramBufferViews();
  void ReleaseEdramBufferViews();
  MTL::Texture* GetEdramUintPow2BufferView(
      uint32_t element_size_bytes_pow2) const;

  MetalCommandProcessor& command_processor_;
  TraceWriter* trace_writer_;

  // Metal device reference
  MTL::Device* device_ = nullptr;
  bool gamma_render_target_as_unorm16_ = false;

  std::unique_ptr<MetalHeapPool> render_target_heap_pool_;

  // EDRAM buffer (10MB embedded DRAM)
  MTL::Buffer* edram_buffer_ = nullptr;
  MTL::Texture* edram_r32_uint_buffer_view_ = nullptr;
  MTL::Texture* edram_r32g32_uint_buffer_view_ = nullptr;
  MTL::Texture* edram_r32g32b32a32_uint_buffer_view_ = nullptr;

  // EDRAM compute shaders for tile operations
  MTL::ComputePipelineState* edram_store_pipeline_ = nullptr;  // Linear → Tiled
  std::unordered_map<uint64_t, MTL::RenderPipelineState*> edram_load_pipelines_;
  MTL::Library* edram_load_library_ = nullptr;
  MTL::Library* edram_load_library_msaa_ = nullptr;

  // EDRAM dump compute shaders for host render target → EDRAM copies.
  static constexpr size_t kEdramDumpBppCount = 2;     // 32, 64
  static constexpr size_t kEdramDumpSourceCount = 2;  // float, uint
  static constexpr size_t kEdramDumpMsaaCount = 3;    // 1x, 2x, 4x
  MTL::ComputePipelineState*
      edram_dump_color_pipelines_[kEdramDumpBppCount][kEdramDumpSourceCount]
                                 [kEdramDumpMsaaCount] = {};
  MTL::ComputePipelineState* edram_dump_depth_pipelines_[kEdramDumpMsaaCount] =
      {};

  // Resolve compute shaders (Metal XeSL -> MSL metallib).
  static constexpr size_t kResolveScaledCount = 2;    // false, true
  static constexpr size_t kResolveFullDestCount = 5;  // 8, 16, 32, 64, 128
  static constexpr size_t kResolveFastBppCount = 2;   // 32, 64
  static constexpr size_t kResolveFastMsaaCount = 2;  // 1/2x, 4x
  MTL::ComputePipelineState*
      resolve_full_pipelines_[kResolveScaledCount][kResolveFullDestCount] = {};
  MTL::ComputePipelineState*
      resolve_fast_pipelines_[kResolveScaledCount][kResolveFastBppCount]
                             [kResolveFastMsaaCount] = {};

  // Direct host resolve compute shaders (host RT -> shared/scaled resolve
  // memory) for fast and full color copies plus depth copies.
  static constexpr size_t kDirectHostResolveBppCount = 2;     // 32, 64
  static constexpr size_t kDirectHostResolveMsaaCount = 3;    // 1x, 2x, 4x
  static constexpr size_t kDirectHostResolveScaledCount = 2;  // false, true
  static constexpr size_t kDirectHostResolveSourceCount = 2;  // float, uint
  static constexpr size_t kDirectHostResolveFullDestCount =
      5;  // 8, 16, 32, 64, 128
  MTL::ComputePipelineState* direct_host_resolve_pipelines_
      [kDirectHostResolveBppCount][kDirectHostResolveMsaaCount]
      [kDirectHostResolveScaledCount][kDirectHostResolveSourceCount] = {};
  MTL::ComputePipelineState* direct_host_color_full_resolve_pipelines_
      [kDirectHostResolveMsaaCount][kDirectHostResolveScaledCount]
      [kDirectHostResolveSourceCount][kDirectHostResolveFullDestCount] = {};
  MTL::ComputePipelineState*
      direct_host_depth_resolve_pipelines_[kDirectHostResolveMsaaCount]
                                          [kDirectHostResolveScaledCount] = {};

  struct TileDirectHostResolvePipelineKey {
    uint32_t color_attachment_index = 0;
    uint32_t sample_count = 1;
    std::array<MTL::PixelFormat, xenos::kMaxColorRenderTargets>
        color_attachment_formats = {};

    bool operator==(const TileDirectHostResolvePipelineKey& other) const {
      return color_attachment_index == other.color_attachment_index &&
             sample_count == other.sample_count &&
             color_attachment_formats == other.color_attachment_formats;
    }

    struct Hasher {
      size_t operator()(const TileDirectHostResolvePipelineKey& key) const {
        auto combine = [](size_t seed, size_t value) {
          return seed ^ (value + 0x9E3779B9 + (seed << 6) + (seed >> 2));
        };
        size_t h = key.color_attachment_index;
        h = combine(h, key.sample_count);
        for (MTL::PixelFormat color_format : key.color_attachment_formats) {
          h = combine(h, size_t(color_format));
        }
        return h;
      }
    };
  };
  MTL::Library* tile_direct_host_resolve_library_ = nullptr;
  std::unordered_map<TileDirectHostResolvePipelineKey,
                     MTL::RenderPipelineState*,
                     TileDirectHostResolvePipelineKey::Hasher>
      tile_direct_host_resolve_pipelines_;

  // Host depth store compute shaders (1x/2x/4x MSAA).
  MTL::ComputePipelineState* host_depth_store_pipelines_[3] = {};

  // TransferMode list mirrors D3D12RenderTargetCache::TransferMode so logs and
  // structure stay in sync, even if many modes are not implemented yet.
  enum class TransferMode {
    kColorToColor,
    kColorToDepth,
    kDepthToColor,
    kDepthToDepth,
    kColorToStencilBit,
    kDepthToStencilBit,
    kColorAndHostDepthToDepth,
    kDepthAndHostDepthToDepth,
  };

  struct TransferShaderKey {
    TransferMode mode;
    xenos::MsaaSamples source_msaa_samples;
    xenos::MsaaSamples dest_msaa_samples;
    xenos::MsaaSamples host_depth_source_msaa_samples;
    uint32_t source_resource_format;
    uint32_t dest_resource_format;
    uint32_t dest_sample_id_from_sample;
    uint32_t host_depth_source_is_copy;

    bool operator==(const TransferShaderKey& other) const {
      return mode == other.mode &&
             source_msaa_samples == other.source_msaa_samples &&
             dest_msaa_samples == other.dest_msaa_samples &&
             host_depth_source_msaa_samples ==
                 other.host_depth_source_msaa_samples &&
             source_resource_format == other.source_resource_format &&
             dest_resource_format == other.dest_resource_format &&
             dest_sample_id_from_sample == other.dest_sample_id_from_sample &&
             host_depth_source_is_copy == other.host_depth_source_is_copy;
    }
    bool operator!=(const TransferShaderKey& other) const {
      return !(*this == other);
    }
    bool operator<(const TransferShaderKey& other) const {
      if (mode != other.mode) {
        return mode < other.mode;
      }
      if (source_msaa_samples != other.source_msaa_samples) {
        return source_msaa_samples < other.source_msaa_samples;
      }
      if (dest_msaa_samples != other.dest_msaa_samples) {
        return dest_msaa_samples < other.dest_msaa_samples;
      }
      if (host_depth_source_msaa_samples !=
          other.host_depth_source_msaa_samples) {
        return host_depth_source_msaa_samples <
               other.host_depth_source_msaa_samples;
      }
      if (source_resource_format != other.source_resource_format) {
        return source_resource_format < other.source_resource_format;
      }
      if (dest_resource_format != other.dest_resource_format) {
        return dest_resource_format < other.dest_resource_format;
      }
      if (dest_sample_id_from_sample != other.dest_sample_id_from_sample) {
        return dest_sample_id_from_sample < other.dest_sample_id_from_sample;
      }
      return host_depth_source_is_copy < other.host_depth_source_is_copy;
    }

    struct Hasher {
      size_t operator()(const TransferShaderKey& key) const {
        size_t h = size_t(key.mode);
        h ^= (size_t(key.source_msaa_samples) << 4);
        h ^= (size_t(key.dest_msaa_samples) << 8);
        h ^= (size_t(key.host_depth_source_msaa_samples) << 12);
        h ^= (size_t(key.source_resource_format) << 16);
        h ^= (size_t(key.dest_resource_format) << 24);
        h ^= (size_t(key.dest_sample_id_from_sample) << 28);
        h ^= (size_t(key.host_depth_source_is_copy) << 29);
        return h ^ (h >> 16);
      }
    };
  };

  using TransferColorAttachmentFormats =
      std::array<MTL::PixelFormat, xenos::kMaxColorRenderTargets>;

  struct TransferAttachmentFormats {
    TransferColorAttachmentFormats color_attachment_formats = {};
    MTL::PixelFormat depth_attachment_format = MTL::PixelFormatInvalid;
    MTL::PixelFormat stencil_attachment_format = MTL::PixelFormatInvalid;
  };

  struct TransferPipelineKey {
    TransferShaderKey shader_key;
    uint32_t color_attachment_index = 0;
    uint32_t native_stencil_output = 0;
    TransferColorAttachmentFormats color_attachment_formats = {};
    MTL::PixelFormat depth_attachment_format = MTL::PixelFormatInvalid;
    MTL::PixelFormat stencil_attachment_format = MTL::PixelFormatInvalid;

    bool operator==(const TransferPipelineKey& other) const {
      return shader_key == other.shader_key &&
             color_attachment_index == other.color_attachment_index &&
             native_stencil_output == other.native_stencil_output &&
             color_attachment_formats == other.color_attachment_formats &&
             depth_attachment_format == other.depth_attachment_format &&
             stencil_attachment_format == other.stencil_attachment_format;
    }

    struct Hasher {
      size_t operator()(const TransferPipelineKey& key) const {
        auto combine = [](size_t seed, size_t value) {
          return seed ^ (value + 0x9E3779B9 + (seed << 6) + (seed >> 2));
        };
        size_t h = TransferShaderKey::Hasher()(key.shader_key);
        h = combine(h, key.color_attachment_index);
        h = combine(h, key.native_stencil_output);
        h = combine(h, size_t(key.depth_attachment_format));
        h = combine(h, size_t(key.stencil_attachment_format));
        for (MTL::PixelFormat color_format : key.color_attachment_formats) {
          h = combine(h, size_t(color_format));
        }
        return h;
      }
    };
  };

  struct TransferClearPipelineKey {
    uint32_t color_attachment_index = 0;
    uint32_t sample_count = 1;
    uint32_t dest_is_uint = 0;
    uint32_t is_depth = 0;
    TransferColorAttachmentFormats color_attachment_formats = {};
    MTL::PixelFormat depth_attachment_format = MTL::PixelFormatInvalid;
    MTL::PixelFormat stencil_attachment_format = MTL::PixelFormatInvalid;

    bool operator==(const TransferClearPipelineKey& other) const {
      return color_attachment_index == other.color_attachment_index &&
             sample_count == other.sample_count &&
             dest_is_uint == other.dest_is_uint &&
             is_depth == other.is_depth &&
             color_attachment_formats == other.color_attachment_formats &&
             depth_attachment_format == other.depth_attachment_format &&
             stencil_attachment_format == other.stencil_attachment_format;
    }

    struct Hasher {
      size_t operator()(const TransferClearPipelineKey& key) const {
        auto combine = [](size_t seed, size_t value) {
          return seed ^ (value + 0x9E3779B9 + (seed << 6) + (seed >> 2));
        };
        size_t h = key.color_attachment_index;
        h = combine(h, key.sample_count);
        h = combine(h, key.dest_is_uint);
        h = combine(h, key.is_depth);
        h = combine(h, size_t(key.depth_attachment_format));
        h = combine(h, size_t(key.stencil_attachment_format));
        for (MTL::PixelFormat color_format : key.color_attachment_formats) {
          h = combine(h, size_t(color_format));
        }
        return h;
      }
    };
  };

  struct TransferRectanglePlan {
    uint32_t transfer_index = 0;
    std::array<Transfer::Rectangle, Transfer::kMaxRectanglesWithCutout>
        rectangles = {};
    uint32_t rectangle_count = 0;
  };

  struct TransferInvocation {
    Transfer transfer;
    TransferShaderKey shader_key;
    const TransferRectanglePlan* rectangle_plan = nullptr;
    TransferInvocation(const Transfer& transfer,
                       const TransferShaderKey& shader_key,
                       const TransferRectanglePlan* rectangle_plan = nullptr)
        : transfer(transfer),
          shader_key(shader_key),
          rectangle_plan(rectangle_plan) {}
    bool operator<(const TransferInvocation& other) const {
      if (shader_key != other.shader_key) {
        return shader_key < other.shader_key;
      }
      assert_not_null(transfer.source);
      assert_not_null(other.transfer.source);
      uint32_t source_index =
          static_cast<const MetalRenderTarget*>(transfer.source)
              ->temporary_sort_index();
      uint32_t other_source_index =
          static_cast<const MetalRenderTarget*>(other.transfer.source)
              ->temporary_sort_index();
      if (source_index != other_source_index) {
        return source_index < other_source_index;
      }
      return transfer.start_tiles < other.transfer.start_tiles;
    }
    bool CanBeMergedIntoOneDraw(const TransferInvocation& other) const {
      return shader_key == other.shader_key &&
             transfer.AreSourcesSame(other.transfer);
    }
  };

  enum class DrawPassTransferRejectionReason : uint32_t {
    kNone,
    kInvalidTargetSlot,
    kNoDestination,
    kDepthDestination,
    kDestinationFormatOrTexture,
    kUnsupportedSource,
    kHostDepthSelfSource,
    kHostDepthSourceActiveInDrawPass,
    kSelfSource,
    kDepthToColorSourceActiveInDrawPass,
    kSourceTextureConflict,
    kSourceFormatMismatch,
    kSourceActiveInDrawPass,
    kInvalidRectangles,
  };

  struct PendingDrawPassTransferPlan {
    RenderTarget* render_target = nullptr;
    DrawPassTransferRejectionReason rejection_reason =
        DrawPassTransferRejectionReason::kNoDestination;
    TransferAttachmentFormats attachment_formats = {};
    std::vector<TransferRectanglePlan> transfer_rectangles;
    bool full_overwrite = false;
    bool preflighted = false;
    bool load_action_safe = false;
  };

  std::unordered_map<TransferPipelineKey, MTL::RenderPipelineState*,
                     TransferPipelineKey::Hasher>
      transfer_pipelines_;
  std::vector<TransferInvocation> transfer_invocations_;
  MTL::Library* transfer_library_ = nullptr;
  std::unordered_map<TransferClearPipelineKey, MTL::RenderPipelineState*,
                     TransferClearPipelineKey::Hasher>
      transfer_clear_pipelines_;
  std::array<RenderTarget*, 1 + xenos::kMaxColorRenderTargets>
      pending_draw_pass_render_targets_ = {};
  std::array<std::vector<Transfer>, 1 + xenos::kMaxColorRenderTargets>
      pending_draw_pass_transfers_;
  std::array<PendingDrawPassTransferPlan, 1 + xenos::kMaxColorRenderTargets>
      pending_draw_pass_transfer_plans_ = {};
  uint32_t pending_draw_pass_transfer_mask_ = 0;
  uint32_t pending_draw_pass_full_overwrite_mask_ = 0;
  uint32_t pending_draw_pass_preflighted_transfer_mask_ = 0;
  uint32_t pending_draw_pass_load_dontcare_mask_ = 0;
  mutable TelemetryStats telemetry_;
  MTL::DepthStencilState* transfer_depth_state_ = nullptr;
  MTL::DepthStencilState* transfer_depth_state_none_ = nullptr;
  MTL::DepthStencilState* transfer_depth_clear_state_ = nullptr;
  MTL::DepthStencilState* transfer_stencil_clear_state_ = nullptr;
  MTL::DepthStencilState* transfer_stencil_output_state_ = nullptr;
  MTL::DepthStencilState* transfer_stencil_bit_states_[8] = {};
  MTL::Buffer* transfer_dummy_buffer_ = nullptr;
  MTL::Texture* transfer_dummy_color_float_[3] = {};
  MTL::Texture* transfer_dummy_color_uint_[3] = {};
  MTL::Texture* transfer_dummy_depth_[3] = {};
  MTL::Texture* transfer_dummy_stencil_[3] = {};
  bool msaa_2x_supported_ = true;

  // Current render targets - updated by base class Update() call

  MetalRenderTarget* current_color_targets_[4] = {};
  MetalRenderTarget* current_depth_target_ = nullptr;

  // Track the last REAL (non-dummy) render targets for capture
  MetalRenderTarget* last_real_color_targets_[4] = {};
  MetalRenderTarget* last_real_depth_target_ = nullptr;

  // Track all created render targets so we can find them
  std::unordered_map<uint32_t, MetalRenderTarget*> render_target_map_;

  // Render pass descriptor cache
  MTL::RenderPassDescriptor* cached_render_pass_descriptor_ = nullptr;
  bool render_pass_descriptor_dirty_ = true;
  uint32_t cached_render_pass_descriptor_sample_count_ = 0;
  bool cached_render_pass_descriptor_fallback_depth_required_ = false;

  // Dummy render target for when no render targets are bound
  struct DummyColorTargetEntry {
    std::unique_ptr<MetalRenderTarget> target;
    uint64_t last_used_frame = 0;
    uint64_t last_cleared_frame = 0;
  };
  mutable std::unordered_map<uint64_t, DummyColorTargetEntry>
      dummy_color_targets_;
  mutable MetalRenderTarget* dummy_color_target_ = nullptr;
  uint64_t frame_id_ = 0;

  // Debug helper to log a small region of the current color RT0.
  // Helper methods
  MTL::Texture* CreateColorTexture(uint32_t width, uint32_t height,
                                   xenos::ColorRenderTargetFormat format,
                                   uint32_t samples,
                                   bool transient_render_target_only = false,
                                   bool allow_unpooled_fallback = true);
  MTL::Texture* CreateDepthTexture(uint32_t width, uint32_t height,
                                   xenos::DepthRenderTargetFormat format,
                                   uint32_t samples);
  MTL::Texture* CreateTransientDepthTexture(uint32_t width, uint32_t height,
                                            uint32_t samples);
  MTL::Texture* GetStencilTextureView(MetalRenderTarget* render_target);

  MTL::PixelFormat GetColorResourcePixelFormat(
      xenos::ColorRenderTargetFormat format) const;
  MTL::PixelFormat GetColorDrawPixelFormat(
      xenos::ColorRenderTargetFormat format) const;
  MTL::PixelFormat GetColorOwnershipTransferPixelFormat(
      xenos::ColorRenderTargetFormat format, bool* is_integer_out) const;
  MTL::PixelFormat GetDepthPixelFormat(
      xenos::DepthRenderTargetFormat format) const;
  TransferShaderKey GetTransferShaderKey(
      RenderTargetKey source_key, RenderTargetKey dest_key,
      const RenderTargetKey* host_depth_source_key,
      bool host_depth_source_is_copy, bool stencil_bit,
      bool dest_sample_id_from_sample_default) const;
  bool GetActiveTransferAttachmentFormats(
      MTL::RenderPassDescriptor* pass_descriptor,
      TransferAttachmentFormats& attachment_formats_out) const;
  bool GetCurrentTransferAttachmentFormats(
      TransferAttachmentFormats& attachment_formats_out) const;
  DrawPassTransferRejectionReason GetDrawPassTransferRejectionReason(
      uint32_t render_target_index, RenderTarget* const* render_targets,
      const std::vector<Transfer>& transfers,
      std::vector<TransferRectanglePlan>* transfer_rectangles_out = nullptr)
      const;
  bool PendingDrawPassTransfersFullyOverwriteTarget(
      uint32_t render_target_index, RenderTarget* render_target,
      const std::vector<Transfer>& transfers,
      const std::vector<TransferRectanglePlan>* transfer_rectangles = nullptr)
      const;
  bool BuildTransferRectanglePlans(
      RenderTargetKey dest_key, const std::vector<Transfer>& transfers,
      const Transfer::Rectangle* cutout, bool require_all_rectangles,
      std::vector<TransferRectanglePlan>& transfer_rectangles_out) const;
  bool EnsurePendingDrawPassTransfersPreflighted();
  bool PreflightPendingDrawPassTransfers(
      const TransferAttachmentFormats& attachment_formats);
  bool PreflightPendingDrawPassTransfers(
      MTL::RenderPassDescriptor* pass_descriptor);
  void MarkRenderPassDescriptorDirty(RenderPassDescriptorDirtyReason reason);
  RenderPassCompatibilityReason GetRenderPassDescriptorCompatibilityReason(
      MTL::RenderPassDescriptor* pass_descriptor,
      uint32_t expected_sample_count,
      bool fallback_depth_attachment_required) const;
  void ClearPendingDrawPassTransfers();

  // EDRAM compute shader setup
  bool InitializeEdramComputeShaders();
  void ShutdownEdramComputeShaders();

  // Transfer pipeline setup for host RT ownership transfers. The shader keys
  // intentionally mirror D3D12RenderTargetCache transfer modes.
  MTL::RenderPipelineState* GetOrCreateTransferPipelines(
      const TransferShaderKey& key, MTL::PixelFormat dest_format,
      bool dest_is_uint, bool native_stencil_output,
      uint32_t color_attachment_index = 0,
      const TransferColorAttachmentFormats* color_attachment_formats = nullptr,
      MTL::PixelFormat depth_attachment_format = MTL::PixelFormatInvalid,
      MTL::PixelFormat stencil_attachment_format = MTL::PixelFormatInvalid);
  MTL::RenderPipelineState* GetOrCreateTransferClearPipeline(
      MTL::PixelFormat dest_format, bool dest_is_uint, bool is_depth,
      uint32_t sample_count, uint32_t color_attachment_index = 0,
      const TransferColorAttachmentFormats* color_attachment_formats = nullptr,
      MTL::PixelFormat depth_attachment_format = MTL::PixelFormatInvalid,
      MTL::PixelFormat stencil_attachment_format = MTL::PixelFormatInvalid);
  MTL::Library* GetOrCreateTransferLibrary();
  MTL::Texture* GetTransferDummyTexture(MTL::PixelFormat format,
                                        uint32_t sample_count);
  MTL::Texture* GetTransferDummyColorFloatTexture(uint32_t sample_count);
  MTL::Texture* GetTransferDummyColorUintTexture(uint32_t sample_count);
  MTL::Texture* GetTransferDummyDepthTexture(uint32_t sample_count);
  MTL::Texture* GetTransferDummyStencilTexture(uint32_t sample_count);
  MTL::Buffer* GetTransferDummyBuffer();
  MTL::DepthStencilState* GetTransferDepthStencilState(bool depth_write);
  MTL::DepthStencilState* GetTransferNoDepthStencilState();
  MTL::DepthStencilState* GetTransferDepthClearState();
  MTL::DepthStencilState* GetTransferStencilClearState();
  MTL::DepthStencilState* GetTransferStencilOutputState();
  MTL::DepthStencilState* GetTransferStencilBitState(uint32_t bit);

  // EDRAM tile operations

  void StoreTiledData(MTL::CommandBuffer* command_buffer, MTL::Texture* texture,
                      uint32_t edram_base, uint32_t pitch_tiles,
                      uint32_t height_tiles, bool is_depth);

  // Host render backend transfer/resolve boundary.
  //
  // Ownership transfer support -- copies data between render targets when
  // EDRAM regions are aliased between different RT configurations.  This
  // mirrors D3D12/Vulkan's PerformTransfersAndResolveClears and is the sole
  // transfer execution entry point used by both the draw path (via Update)
  // and the copy path (via Resolve).  All host-side transfer and resolve-
  // clear work flows through this method; no transfer operations bypass the
  // render target cache.
  bool PerformTransfersAndResolveClears(
      uint32_t render_target_count, RenderTarget* const* render_targets,
      const std::vector<Transfer>* render_target_transfers,
      const uint64_t* render_target_resolve_clear_values = nullptr,
      const Transfer::Rectangle* resolve_clear_rectangle = nullptr,
      MTL::CommandBuffer* command_buffer = nullptr,
      MTL::RenderCommandEncoder* active_render_encoder = nullptr,
      MTL::RenderPassDescriptor* active_render_pass_descriptor = nullptr,
      DrawPassTransferEncoderMutationMask* mutations_out = nullptr,
      const PendingDrawPassTransferPlan* prepared_draw_pass_transfer_plans =
          nullptr);

  // Writes contents of host render targets within rectangles from
  // ResolveInfo::GetCopyEdramTileSpan to edram_buffer_.
  void DumpRenderTargets(uint32_t dump_base, uint32_t dump_row_length_used,
                         uint32_t dump_rows, uint32_t dump_pitch,
                         MTL::CommandBuffer* command_buffer = nullptr,
                         const char* encoder_label = nullptr);

  bool TryDirectHostResolveCopy(
      const draw_util::ResolveInfo& resolve_info,
      const draw_util::ResolveCopyShaderConstants& copy_constants,
      draw_util::ResolveCopyShaderIndex copy_shader, uint32_t dump_base,
      uint32_t dump_row_length_used, uint32_t dump_rows, uint32_t dump_pitch,
      bool resolve_has_clear, MTL::CommandBuffer* command_buffer,
      uint32_t& written_address, uint32_t& written_length);
  MTL::Library* GetOrCreateTileDirectHostResolveLibrary();
  MTL::RenderPipelineState* GetOrCreateTileDirectHostResolvePipeline(
      uint32_t color_attachment_index, uint32_t sample_count,
      const TransferColorAttachmentFormats& color_attachment_formats);
  struct ResolveDestinationBuffer {
    MTL::Buffer* buffer = nullptr;
    size_t offset = 0;
    size_t length = 0;
  };
  bool PrepareResolveDestinationBuffer(
      const draw_util::ResolveInfo& resolve_info, bool draw_resolution_scaled,
      ResolveDestinationBuffer& destination);
  MTL::ComputePipelineState* GetResolvePipeline(
      draw_util::ResolveCopyShaderIndex copy_shader, bool scaled) const;
  MTL::ComputePipelineState* GetDirectHostResolvePipeline(
      bool is_64bpp, xenos::MsaaSamples msaa_samples, bool scaled,
      bool source_is_uint) const;
  MTL::ComputePipelineState* GetDirectHostColorFullResolvePipeline(
      xenos::MsaaSamples msaa_samples, bool scaled, bool source_is_uint,
      draw_util::ResolveCopyShaderIndex copy_shader) const;
  MTL::ComputePipelineState* GetDirectHostDepthResolvePipeline(
      xenos::MsaaSamples msaa_samples, bool scaled) const;
};

}  // namespace metal
}  // namespace gpu
}  // namespace xe

#endif  // XENIA_GPU_METAL_METAL_RENDER_TARGET_CACHE_H_
