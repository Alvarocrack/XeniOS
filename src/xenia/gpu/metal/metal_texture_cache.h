/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_METAL_METAL_TEXTURE_CACHE_H_
#define XENIA_GPU_METAL_METAL_TEXTURE_CACHE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "xenia/gpu/dxbc_shader.h"
#include "xenia/gpu/register_file.h"
#include "xenia/gpu/texture_cache.h"
#include "xenia/gpu/texture_info.h"
#include "xenia/gpu/xenos.h"
#include "xenia/memory.h"

#include "third_party/metal-cpp/Metal/Metal.hpp"

namespace xe {
namespace gpu {
namespace metal {

class MetalCommandProcessor;
class MetalSharedMemory;
class MetalHeapPool;

class MetalTextureCache : public TextureCache {
 public:
  MetalTextureCache(MetalCommandProcessor* command_processor,
                    const RegisterFile& register_file,
                    MetalSharedMemory& shared_memory,
                    uint32_t draw_resolution_scale_x,
                    uint32_t draw_resolution_scale_y);
  ~MetalTextureCache();

  bool Initialize();
  void Shutdown();
  void ClearCache() override;
  void CompletedSubmissionUpdated(uint64_t completed_submission_index) override;
  bool TrimViewBindlessPressure(uint32_t needed_slot_count = 1);

  // Null texture accessors for invalid bindings (following D3D12/Vulkan
  // pattern)
  MTL::Texture* GetNullTexture2D() const { return null_texture_2d_; }
  MTL::Texture* GetNullTexture3D() const { return null_texture_3d_; }
  MTL::Texture* GetNullTextureCube() const { return null_texture_cube_; }

  MTL::Texture* GetTextureForBinding(uint32_t fetch_constant,
                                     xenos::FetchOpDimension dimension,
                                     bool is_signed);

  // Bindless persistent heap index lookups.
  // Returns the persistent view_bindless_heap_ slot for the texture that
  // would be resolved for the given binding parameters.  Falls back to the
  // null texture's persistent slot when no valid texture is found.  If
  // texture_for_encoder_out is provided, also returns the Metal texture/view
  // that must be marked as used by the active render encoder.
  uint32_t GetBindlessSRVIndexForBinding(
      uint32_t fetch_constant, xenos::FetchOpDimension dimension,
      bool is_signed, MTL::Texture** texture_for_encoder_out = nullptr);
  // Returns the persistent sampler_bindless_heap_ slot for the sampler that
  // would be created for the given binding.  Falls back to
  // null_sampler_bindless_index_ when no sampler can be resolved.
  uint32_t GetBindlessSamplerIndexForBinding(
      const DxbcShader::SamplerBinding& binding);

  // Persistent bindless indices for null textures.
  uint32_t null_texture_2d_bindless_index() const {
    return null_texture_2d_bindless_index_;
  }
  uint32_t null_texture_3d_bindless_index() const {
    return null_texture_3d_bindless_index_;
  }
  uint32_t null_texture_cube_bindless_index() const {
    return null_texture_cube_bindless_index_;
  }
  uint32_t null_sampler_bindless_index() const {
    return null_sampler_bindless_index_;
  }

  struct TextureSRVKey {
    TextureKey key;
    uint32_t host_swizzle;
    uint8_t swizzled_signs;
  };

  bool AreActiveTextureSRVKeysUpToDate(
      const TextureSRVKey* keys,
      const DxbcShader::TextureBinding* host_shader_bindings,
      size_t host_shader_binding_count) const;
  void WriteActiveTextureSRVKeys(
      TextureSRVKey* keys,
      const DxbcShader::TextureBinding* host_shader_bindings,
      size_t host_shader_binding_count) const;

  MTL::Texture* RequestSwapTexture(uint32_t& width_scaled_out,
                                   uint32_t& height_scaled_out,
                                   xenos::TextureFormat& format_out);

  union SamplerParameters {
    uint32_t value;
    struct {
      xenos::ClampMode clamp_x : 3;
      xenos::ClampMode clamp_y : 3;
      xenos::ClampMode clamp_z : 3;
      xenos::BorderColor border_color : 2;
      uint32_t mag_linear : 1;
      uint32_t min_linear : 1;
      uint32_t mip_linear : 1;
      xenos::AnisoFilter aniso_filter : 3;
      uint32_t mip_min_level : 4;
      uint32_t mip_base_map : 1;
    };

    SamplerParameters() : value(0) { static_assert_size(*this, sizeof(value)); }
    bool operator==(const SamplerParameters& other) const {
      return value == other.value;
    }
    bool operator!=(const SamplerParameters& other) const {
      return value != other.value;
    }
  };

  SamplerParameters GetSamplerParameters(
      const DxbcShader::SamplerBinding& binding) const;
  MTL::SamplerState* GetOrCreateSampler(SamplerParameters parameters);

  // TextureCache virtual method overrides
  void RequestTextures(uint32_t used_texture_mask) override;
  // Preloads texture data described by speculative registers without changing
  // active texture bindings. Used by Metal render-run preflight before opening
  // a render encoder.
  uint32_t PreloadTexturesFromRegisterFile(const RegisterFile& regs,
                                           uint32_t used_texture_mask);
  bool PrepareTextureDataLoadRanges(Texture** textures,
                                    uint32_t texture_count,
                                    uint64_t base_outdated_mask,
                                    uint64_t mips_outdated_mask) override;
  void BeginDirectTextureDataRangeSharedMemoryRequest(
      TextureDataRangeSource source, uint32_t start, uint32_t length) override;
  void EndDirectTextureDataRangeSharedMemoryRequest() override;
  bool RequestTextureDataRange(Texture& texture, TextureDataRangeSource source,
                               uint32_t start, uint32_t length) override;

  bool IsSignedVersionSeparateForFormat(TextureKey key) const override;
  bool IsScaledResolveSupportedForFormat(TextureKey key) const override;
  bool EnsureScaledResolveMemoryCommitted(
      uint32_t start_unscaled, uint32_t length_unscaled,
      uint32_t length_scaled_alignment_log2 = 0) override;
  bool MakeScaledResolveRangeCurrent(uint32_t start_unscaled,
                                     uint32_t length_unscaled,
                                     uint32_t length_scaled_alignment_log2 = 0);
  bool GetCurrentScaledResolveBuffer(MTL::Buffer*& buffer_out,
                                     size_t& buffer_offset_out,
                                     size_t& buffer_length_out) const;
  uint64_t GetCurrentScaledResolveRangeStartScaled() const {
    return scaled_resolve_current_range_start_scaled_;
  }
  uint64_t GetCurrentScaledResolveRangeLengthScaled() const {
    return scaled_resolve_current_range_length_scaled_;
  }
  uint32_t GetHostFormatSwizzle(TextureKey key) const override;
  uint32_t GetMaxHostTextureWidthHeight(
      xenos::DataDimension dimension) const override;
  uint32_t GetMaxHostTextureDepthOrArraySize(
      xenos::DataDimension dimension) const override;
  std::unique_ptr<Texture> CreateTexture(TextureKey key) override;
  bool LoadTextureDataFromResidentMemoryImpl(Texture& texture, bool load_base,
                                             bool load_mips) override;
  // Whether GPU texture uploads can be encoded into the active command buffer
  // without creating a separate upload command buffer or ending a render
  // encoder.
  bool CanUseCurrentCommandBufferForTextureUploads() const;
  bool FlushPendingUploadEncodersForCommandEncoderBoundary();

  struct TelemetryStats {
    uint64_t request_textures_calls = 0;
    uint64_t request_textures_nonzero_mask = 0;
    uint64_t request_textures_work_mask_bits = 0;
    uint64_t request_textures_with_loads = 0;
    uint64_t request_textures_without_loads = 0;
    uint64_t request_textures_loaded_textures = 0;
    uint64_t load_texture_calls = 0;
    uint64_t load_texture_base = 0;
    uint64_t load_texture_mips = 0;
    uint64_t gpu_load_attempts = 0;
    uint64_t gpu_load_successes = 0;
    uint64_t gpu_load_failures = 0;
    uint64_t gpu_load_scaled = 0;
    uint64_t gpu_load_decompressed = 0;
    uint64_t gpu_load_blit_path = 0;
    uint64_t gpu_load_compute_copy_path = 0;
    uint64_t gpu_load_standalone_command_buffers = 0;
    uint64_t gpu_load_upload_batch_command_buffers = 0;
    uint64_t gpu_load_current_submission_command_buffers = 0;
    uint64_t gpu_load_direct_uploads = 0;
    uint64_t gpu_load_repack_uploads = 0;
    uint64_t gpu_load_dispatches = 0;
    uint64_t gpu_load_repack_dispatches = 0;
    uint64_t gpu_load_deferred_encoder_uses = 0;
    uint64_t gpu_load_local_encoder_uses = 0;
    uint64_t deferred_upload_compute_encoder_creates = 0;
    uint64_t deferred_upload_compute_encoder_reuses = 0;
    uint64_t deferred_upload_empty_flushes = 0;
    uint64_t deferred_upload_flushes = 0;
    uint64_t deferred_upload_flushes_with_compute = 0;
    uint64_t deferred_upload_flushes_with_blits = 0;
    uint64_t deferred_upload_copy_count = 0;
    uint64_t immediate_upload_blit_encoder_creates = 0;
    uint64_t ensure_bindless_headroom_calls = 0;
    uint64_t ensure_bindless_headroom_trims = 0;
    uint64_t ensure_bindless_headroom_failures = 0;
  };
  TelemetryStats GetAndResetTelemetryStats();

 private:
  bool EnsureViewBindlessHeadroom(uint32_t target_free_slots) const;
  // GPU-based texture loading entry point. Returns true on success.
  bool TryGpuLoadTexture(Texture& texture, bool load_base, bool load_mips);
  MTL::StorageMode GetCacheTextureStorageMode() const;
  bool ShouldUploadViaBlit() const;
  void BeginUploadCommandBufferBatch();
  void EndUploadCommandBufferBatch();
  void AbortUploadCommandBufferBatch(bool commit_if_has_work = true);
  void BeginDeferredUploadEncoderBatch();
  bool EndDeferredUploadEncoderBatch();
  bool FlushDeferredUploadEncoderBatch();
  class UploadBatchScope;
  MTL::ComputeCommandEncoder* GetDeferredUploadComputeEncoder(
      MTL::CommandBuffer* command_buffer);
  void QueueDeferredUploadCopy(MTL::Buffer* source_buffer,
                               size_t source_offset_bytes,
                               size_t source_row_pitch,
                               size_t source_bytes_per_image,
                               MTL::Texture* destination_texture,
                               uint32_t destination_slice,
                               uint32_t destination_level, uint32_t width,
                               uint32_t height, uint32_t depth);

  // Format / load shader mapping for Metal texture loading.
  bool IsDecompressionNeededForKey(TextureKey key) const;
  LoadShaderIndex GetLoadShaderIndexForKey(TextureKey key) const;
  MTL::PixelFormat GetPixelFormatForKey(TextureKey key) const;

  // Initialize GPU texture_load_* pipelines for Metal.
  bool InitializeLoadPipelines();

  struct Norm16Selection {
    bool unsigned_uses_float = false;
    bool signed_uses_float = false;
  };

  void InitializeNorm16Selection(MTL::Device* device);

  // Metal compute pipelines for texture_load_* shaders (unscaled and
  // resolution-scaled variants), indexed by TextureCache::LoadShaderIndex.
  MTL::ComputePipelineState* load_pipelines_[kLoadShaderCount] = {};
  MTL::ComputePipelineState* load_pipelines_scaled_[kLoadShaderCount] = {};
  MTL::ComputePipelineState* texture_upload_repack_pipeline_ = nullptr;
  TelemetryStats telemetry_;

  // Metal-specific Texture implementation

  class MetalTexture : public Texture {
   public:
    MetalTexture(MetalTextureCache& texture_cache, const TextureKey& key,
                 MTL::Texture* metal_texture, bool track_usage = true,
                 bool is_3d_as_2d_wrapper = false);
    ~MetalTexture() override;

    MTL::Texture* metal_texture() const { return metal_texture_; }
    MTL::Texture* GetOrCreateView(uint32_t host_swizzle,
                                  xenos::FetchOpDimension dimension,
                                  bool is_signed);
    MTL::Texture* GetOrCreate3DAs2DView(uint32_t host_swizzle,
                                        xenos::FetchOpDimension dimension,
                                        bool is_signed);
    uint32_t GetOrCreateBindlessSRVIndex(uint32_t host_swizzle,
                                         xenos::FetchOpDimension dimension,
                                         bool is_signed);
    uint32_t GetOrCreateBindlessSRVIndexAndView(
        uint32_t host_swizzle, xenos::FetchOpDimension dimension,
        bool is_signed, MTL::Texture** view_out);

    // Persistent bindless SRV index for the default resolved view in the
    // view_bindless_heap_. Additional resolved view variants use persistent
    // entries in the same heap keyed by logical view parameters.
    uint32_t bindless_srv_index() const { return bindless_srv_index_; }
    void set_bindless_srv_index(uint32_t index) { bindless_srv_index_ = index; }

   private:
    uint64_t GetViewKey(uint32_t host_swizzle,
                        xenos::FetchOpDimension dimension, bool is_signed,
                        MTL::PixelFormat view_format) const;
    MTL::PixelFormat GetViewPixelFormat(bool is_signed) const;
    MTL::TextureType GetViewType(xenos::FetchOpDimension dimension) const;
    uint32_t GetOrCreateBindlessSRVIndexForResolvedView(uint64_t view_key,
                                                        MTL::Texture* view);

    MetalTextureCache& texture_cache_;
    MTL::Texture* metal_texture_;
    uint32_t bindless_srv_index_ = UINT32_MAX;
    std::unique_ptr<MetalTexture> texture_3d_as_2d_;
    bool is_3d_as_2d_wrapper_ = false;
    std::unordered_map<uint64_t, MTL::Texture*> swizzled_view_cache_;
    std::unordered_map<uint64_t, uint32_t> swizzled_view_bindless_srv_indices_;
  };

 private:
  // Metal texture creation helpers
  MTL::Texture* CreateTexture(MTL::TextureDescriptor* descriptor);
  MTL::Texture* CreateTexture2D(uint32_t width, uint32_t height,
                                uint32_t array_length, MTL::PixelFormat format,
                                MTL::TextureSwizzleChannels swizzle,
                                uint32_t mip_levels = 1);
  MTL::Texture* CreateTexture3D(uint32_t width, uint32_t height, uint32_t depth,
                                MTL::PixelFormat format,
                                MTL::TextureSwizzleChannels swizzle,
                                uint32_t mip_levels = 1);
  MTL::Texture* CreateTextureCube(uint32_t width, MTL::PixelFormat format,
                                  MTL::TextureSwizzleChannels swizzle,
                                  uint32_t mip_levels = 1,
                                  uint32_t cube_count = 1);
  struct ScaledResolveBuffer {
    MTL::Buffer* buffer = nullptr;
    uint64_t base_scaled = 0;
    uint64_t length_scaled = 0;
  };
  struct RetiredScaledResolveBuffer {
    MTL::Buffer* buffer = nullptr;
    uint64_t submission_id = 0;
    uint64_t length_scaled = 0;
  };
  struct RetiredSamplerState {
    MTL::SamplerState* sampler = nullptr;
    uint64_t submission_id = 0;
  };

  bool GetScaledResolveRange(uint32_t start_unscaled, uint32_t length_unscaled,
                             uint32_t length_scaled_alignment_log2,
                             uint64_t& start_scaled_out,
                             uint64_t& length_scaled_out) const;
  bool IsScaledResolveRangeResident(
      uint32_t start_unscaled, uint32_t length_unscaled,
      uint32_t length_scaled_alignment_log2) const;
  bool EnsureScaledResolveBufferRange(uint64_t start_scaled,
                                      uint64_t length_scaled);
  void ClearScaledResolveBuffers();
  void ReleaseOrRetireSamplerState(MTL::SamplerState* sampler);

  // Null texture factory methods (following existing CreateTexture pattern)
  MTL::Texture* CreateNullTexture2D();
  MTL::Texture* CreateNullTexture3D();
  MTL::Texture* CreateNullTextureCube();

  xenos::ClampMode NormalizeClampMode(xenos::ClampMode clamp_mode) const;

  MetalCommandProcessor* command_processor_;

  // Pre-created null textures for invalid bindings (following existing
  // patterns)
  MTL::Texture* null_texture_2d_ = nullptr;
  MTL::Texture* null_texture_3d_ = nullptr;
  MTL::Texture* null_texture_cube_ = nullptr;

  // Persistent bindless heap indices for null textures/samplers.
  // Allocated during Initialize, released during Shutdown.
  uint32_t null_texture_2d_bindless_index_ = UINT32_MAX;
  uint32_t null_texture_3d_bindless_index_ = UINT32_MAX;
  uint32_t null_texture_cube_bindless_index_ = UINT32_MAX;
  uint32_t null_sampler_bindless_index_ = UINT32_MAX;
  MTL::SamplerState* null_sampler_bindless_ = nullptr;

  Norm16Selection r16_selection_;
  Norm16Selection rg16_selection_;
  Norm16Selection rgba16_selection_;

  std::unordered_map<uint32_t, MTL::SamplerState*> sampler_cache_;
  // Maps sampler parameter key -> persistent bindless heap index.
  std::unordered_map<uint32_t, uint32_t> sampler_bindless_indices_;

  class UploadBufferPool;
  struct DeferredUploadCopy {
    MTL::Buffer* source_buffer = nullptr;
    size_t source_offset_bytes = 0;
    size_t source_row_pitch = 0;
    size_t source_bytes_per_image = 0;
    MTL::Texture* destination_texture = nullptr;
    uint32_t destination_slice = 0;
    uint32_t destination_level = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 0;
  };

  mutable std::mutex upload_buffer_pool_mutex_;
  std::shared_ptr<UploadBufferPool> upload_buffer_pool_;
  MTL::CommandBuffer* upload_batch_command_buffer_ = nullptr;
  bool upload_batch_command_buffer_has_work_ = false;
  uint32_t upload_batch_depth_ = 0;
  MTL::CommandBuffer* deferred_upload_command_buffer_ = nullptr;
  MTL::ComputeCommandEncoder* deferred_upload_compute_encoder_ = nullptr;
  std::vector<DeferredUploadCopy> deferred_upload_copies_;
  uint32_t deferred_upload_batch_depth_ = 0;
  std::unique_ptr<MetalHeapPool> texture_heap_pool_;
  bool supports_bc_texture_compression_ = false;

  std::vector<ScaledResolveBuffer> scaled_resolve_buffers_;
  std::vector<RetiredScaledResolveBuffer> scaled_resolve_retired_buffers_;
  std::vector<RetiredSamplerState> retired_sampler_states_;
  uint64_t scaled_resolve_retired_bytes_ = 0;
  size_t scaled_resolve_current_buffer_index_ = size_t(-1);
  uint64_t scaled_resolve_current_range_start_scaled_ = 0;
  uint64_t scaled_resolve_current_range_length_scaled_ = 0;
};

}  // namespace metal
}  // namespace gpu
}  // namespace xe

#endif  // XENIA_GPU_METAL_METAL_TEXTURE_CACHE_H_
