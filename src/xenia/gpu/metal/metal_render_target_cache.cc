/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/gpu/metal/metal_render_target_cache.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "third_party/fmt/include/fmt/format.h"
#include "xenia/base/assert.h"
#include "xenia/base/byte_order.h"
#include "xenia/base/logging.h"
#include "xenia/base/math.h"
#include "xenia/gpu/draw_util.h"
#include "xenia/gpu/gpu_flags.h"
#include "xenia/gpu/metal/metal_heap_pool.h"
#include "xenia/gpu/metal/metal_texture_cache.h"
#include "xenia/gpu/shaders/bytecode/metal/host_depth_store_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/host_depth_store_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/host_depth_store_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_fast_32bpp_1x2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_fast_32bpp_1x2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_fast_32bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_fast_32bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_fast_64bpp_1x2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_fast_64bpp_1x2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_fast_64bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_fast_64bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_full_128bpp_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_full_128bpp_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_full_16bpp_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_full_16bpp_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_full_32bpp_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_full_32bpp_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_full_64bpp_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_full_64bpp_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_full_8bpp_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_full_8bpp_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_32bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_32bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_32bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_32bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_32bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_32bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_64bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_64bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_64bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_64bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_64bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_64bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_128bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_128bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_128bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_128bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_128bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_128bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_16bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_16bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_16bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_16bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_16bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_16bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_32bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_32bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_32bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_32bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_32bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_32bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_64bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_64bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_64bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_64bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_64bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_64bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_8bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_8bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_8bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_8bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_8bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_8bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_128bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_128bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_128bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_128bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_128bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_128bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_16bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_16bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_16bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_16bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_16bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_16bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_32bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_32bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_32bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_32bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_32bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_32bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_64bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_64bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_64bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_64bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_64bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_64bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_8bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_8bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_8bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_8bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_8bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_full_uint_8bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_uint_32bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_uint_32bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_uint_32bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_uint_32bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_uint_32bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_uint_32bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_uint_64bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_uint_64bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_uint_64bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_uint_64bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_uint_64bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_color_uint_64bpp_4xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_depth_32bpp_1xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_depth_32bpp_1xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_depth_32bpp_2xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_depth_32bpp_2xmsaa_scaled_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_depth_32bpp_4xmsaa_cs.h"
#include "xenia/gpu/shaders/bytecode/metal/resolve_host_depth_32bpp_4xmsaa_scaled_cs.h"

#include "third_party/metal-shader-converter/include/metal_irconverter_runtime.h"
#include "xenia/gpu/metal/metal_command_processor.h"
#include "xenia/gpu/texture_info.h"
#include "xenia/gpu/texture_util.h"
#include "xenia/gpu/xenos.h"

DEFINE_bool(
    metal_allow_gamma_unorm16, false,
    "Allow gamma_render_target_as_unorm16 on Metal despite known issues",
    "Metal");
DEFINE_bool(metal_transfer_fast_divmod, true,
            "Use fast exact div/mod in Metal transfer shaders", "Metal");
DEFINE_bool(metal_transfer_native_stencil_output, true,
            "Use Metal fragment stencil output for transfer stencil writes",
            "Metal");
DEFINE_bool(
    metal_transfer_msaa_sample_id, true,
    "Use sample_id in Metal transfer shaders for MSAA (sample-rate shading)",
    "Metal");
DEFINE_bool(metal_transfer_in_draw_pass, true,
            "Encode eligible color ownership transfers in the guest draw pass",
            "Metal");
DEFINE_bool(metal_direct_host_resolve, true,
            "Resolve eligible fast color/depth copies directly from Metal host "
            "render targets to shared/scaled resolve memory",
            "Metal");
DEFINE_bool(
    metal_tile_direct_host_resolve, true,
    "Prototype: resolve eligible fast/full color copies from the active Metal "
    "render pass with a tile shader before ending the render encoder",
    "Metal");
DEFINE_bool(
    metal_tile_direct_host_resolve_dontcare_store, false,
    "Experimental: after a tile direct-host resolve, mark the source color "
    "attachment store action DontCare. Disabled by default until AttachmentPlan "
    "proves no later host render-target observer needs the attachment.",
    "Metal");
DEFINE_bool(metal_use_heaps, true,
            "Use MTLHeap-backed texture allocations in Metal to reduce "
            "allocation overhead and fragmentation.",
            "Metal");
DEFINE_int32(metal_heap_min_bytes, 33554432,
             "Minimum heap size (bytes) for Metal heap allocations.", "Metal");

namespace xe {
namespace gpu {
namespace metal {

namespace {

class ScopedAutoreleasePool {
 public:
  ScopedAutoreleasePool() : pool_(NS::AutoreleasePool::alloc()->init()) {}
  ~ScopedAutoreleasePool() {
    if (pool_) {
      pool_->release();
    }
  }

  ScopedAutoreleasePool(const ScopedAutoreleasePool&) = delete;
  ScopedAutoreleasePool& operator=(const ScopedAutoreleasePool&) = delete;

 private:
  NS::AutoreleasePool* pool_;
};

uint32_t EstimateRenderTargetBytesPerPixel(bool is_64bpp) {
  return is_64bpp ? 8u : 4u;
}

constexpr NS::UInteger kTileDirectHostResolveTileWidth = 16;
constexpr NS::UInteger kTileDirectHostResolveTileHeight = 16;

MTL::ComputePipelineState* CreateComputePipelineFromEmbeddedLibrary(
    MTL::Device* device, const void* metallib_data, size_t metallib_size,
    const char* debug_name) {
  if (!device || !metallib_data || !metallib_size) {
    return nullptr;
  }

  NS::Error* error = nullptr;

  dispatch_data_t data = dispatch_data_create(
      metallib_data, metallib_size, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
  MTL::Library* lib = device->newLibrary(data, &error);
  dispatch_release(data);
  if (!lib) {
    XELOGE("Metal: failed to create {} library: {}", debug_name,
           error ? error->localizedDescription()->utf8String() : "unknown");
    return nullptr;
  }

  // XeSL compute entrypoint name used in the embedded metallibs.
  NS::String* fn_name = NS::String::string("entry_xe", NS::UTF8StringEncoding);
  MTL::Function* fn = lib->newFunction(fn_name);
  if (!fn) {
    XELOGE("Metal: {} missing entry_xe", debug_name);
    lib->release();
    return nullptr;
  }

  // Label the pipeline so each resolve variant is distinguishable in the
  // Xcode GPU trace; the entrypoint is always "entry_xe", so without a label
  // every variant looks identical there.
  MTL::ComputePipelineDescriptor* pipeline_desc =
      MTL::ComputePipelineDescriptor::alloc()->init();
  pipeline_desc->setComputeFunction(fn);
  if (debug_name) {
    pipeline_desc->setLabel(
        NS::String::string(debug_name, NS::UTF8StringEncoding));
  }
  MTL::ComputePipelineState* pipeline = device->newComputePipelineState(
      pipeline_desc, MTL::PipelineOptionNone,
      static_cast<MTL::AutoreleasedComputePipelineReflection*>(nullptr),
      &error);
  pipeline_desc->release();
  fn->release();
  lib->release();

  if (!pipeline) {
    XELOGE("Metal: failed to create {} pipeline: {}", debug_name,
           error ? error->localizedDescription()->utf8String() : "unknown");
    return nullptr;
  }

  return pipeline;
}

bool IsResolveDirectHostRTFastCandidate(
    draw_util::ResolveCopyShaderIndex shader) {
  switch (shader) {
    case draw_util::ResolveCopyShaderIndex::kFast32bpp1x2xMSAA:
    case draw_util::ResolveCopyShaderIndex::kFast32bpp4xMSAA:
    case draw_util::ResolveCopyShaderIndex::kFast64bpp1x2xMSAA:
    case draw_util::ResolveCopyShaderIndex::kFast64bpp4xMSAA:
      return true;
    default:
      return false;
  }
}

size_t DirectHostResolveFullDestIndex(
    draw_util::ResolveCopyShaderIndex shader) {
  switch (shader) {
    case draw_util::ResolveCopyShaderIndex::kFull8bpp:
      return 0;
    case draw_util::ResolveCopyShaderIndex::kFull16bpp:
      return 1;
    case draw_util::ResolveCopyShaderIndex::kFull32bpp:
      return 2;
    case draw_util::ResolveCopyShaderIndex::kFull64bpp:
      return 3;
    case draw_util::ResolveCopyShaderIndex::kFull128bpp:
      return 4;
    default:
      return 5;
  }
}

size_t ResolveFastBppIndex(draw_util::ResolveCopyShaderIndex shader) {
  switch (shader) {
    case draw_util::ResolveCopyShaderIndex::kFast32bpp1x2xMSAA:
    case draw_util::ResolveCopyShaderIndex::kFast32bpp4xMSAA:
      return 0;
    case draw_util::ResolveCopyShaderIndex::kFast64bpp1x2xMSAA:
    case draw_util::ResolveCopyShaderIndex::kFast64bpp4xMSAA:
      return 1;
    default:
      break;
  }
  assert_unhandled_case(shader);
  return 0;
}

size_t ResolveFastMsaaIndex(draw_util::ResolveCopyShaderIndex shader) {
  switch (shader) {
    case draw_util::ResolveCopyShaderIndex::kFast32bpp1x2xMSAA:
    case draw_util::ResolveCopyShaderIndex::kFast64bpp1x2xMSAA:
      return 0;
    case draw_util::ResolveCopyShaderIndex::kFast32bpp4xMSAA:
    case draw_util::ResolveCopyShaderIndex::kFast64bpp4xMSAA:
      return 1;
    default:
      break;
  }
  assert_unhandled_case(shader);
  return 0;
}

bool IsResolveDirectHostRTFullColorCandidate(
    draw_util::ResolveCopyShaderIndex shader) {
  return DirectHostResolveFullDestIndex(shader) < 5;
}

bool IsResolveDirectHostRTFullColorSourcePackable(
    xenos::ColorRenderTargetFormat format) {
  switch (format) {
    case xenos::ColorRenderTargetFormat::k_8_8_8_8:
    case xenos::ColorRenderTargetFormat::k_2_10_10_10:
    case xenos::ColorRenderTargetFormat::k_2_10_10_10_FLOAT:
    case xenos::ColorRenderTargetFormat::k_16_16:
    case xenos::ColorRenderTargetFormat::k_16_16_16_16:
    case xenos::ColorRenderTargetFormat::k_16_16_FLOAT:
    case xenos::ColorRenderTargetFormat::k_16_16_16_16_FLOAT:
    case xenos::ColorRenderTargetFormat::k_2_10_10_10_AS_10_10_10_10:
    case xenos::ColorRenderTargetFormat::k_2_10_10_10_FLOAT_AS_16_16_16_16:
    case xenos::ColorRenderTargetFormat::k_32_FLOAT:
    case xenos::ColorRenderTargetFormat::k_32_32_FLOAT:
      return true;
    default:
      return false;
  }
}

bool IsResolveDirectHostRTCandidate(draw_util::ResolveCopyShaderIndex shader) {
  return IsResolveDirectHostRTFastCandidate(shader) ||
         IsResolveDirectHostRTFullColorCandidate(shader);
}

uint32_t DirectHostResolveFullDestBppLog2(
    draw_util::ResolveCopyShaderIndex shader) {
  switch (shader) {
    case draw_util::ResolveCopyShaderIndex::kFull8bpp:
      return 0;
    case draw_util::ResolveCopyShaderIndex::kFull16bpp:
      return 1;
    case draw_util::ResolveCopyShaderIndex::kFull32bpp:
      return 2;
    case draw_util::ResolveCopyShaderIndex::kFull64bpp:
      return 3;
    case draw_util::ResolveCopyShaderIndex::kFull128bpp:
      return 4;
    default:
      break;
  }
  assert_unhandled_case(shader);
  return 2;
}

uint32_t DirectHostResolvePixelsPerThread(
    draw_util::ResolveCopyShaderIndex shader, bool source_is_64bpp,
    xenos::MsaaSamples msaa_samples) {
  switch (shader) {
    case draw_util::ResolveCopyShaderIndex::kFull8bpp:
      return msaa_samples >= xenos::MsaaSamples::k4X ? 4 : 8;
    case draw_util::ResolveCopyShaderIndex::kFull128bpp:
      return 2;
    case draw_util::ResolveCopyShaderIndex::kFull16bpp:
    case draw_util::ResolveCopyShaderIndex::kFull32bpp:
    case draw_util::ResolveCopyShaderIndex::kFull64bpp:
      return 4;
    default:
      return source_is_64bpp ? 4 : 8;
  }
}

uint32_t TileDirectHostResolveFullPixelsPerThread(
    draw_util::ResolveCopyShaderIndex shader) {
  // Sub-32bpp destinations still need one lane to pack multiple pixels into a
  // dword. Full dword-or-larger destinations use one pixel per lane to avoid
  // the register-heavy four-pixel tile shader unroll.
  switch (shader) {
    case draw_util::ResolveCopyShaderIndex::kFull8bpp:
    case draw_util::ResolveCopyShaderIndex::kFull16bpp:
      return 4;
    case draw_util::ResolveCopyShaderIndex::kFull32bpp:
    case draw_util::ResolveCopyShaderIndex::kFull64bpp:
    case draw_util::ResolveCopyShaderIndex::kFull128bpp:
      return 1;
    default:
      break;
  }
  assert_unhandled_case(shader);
  return 1;
}

const char* ResolveCopyEncoderLabel(bool direct_host_rt_candidate) {
  return direct_host_rt_candidate ? "XeniaResolveCopyDirectCandidate"
                                  : "XeniaResolveCopyFallback";
}

const char* ResolveDumpEncoderLabel(bool direct_host_rt_candidate) {
  return direct_host_rt_candidate ? "XeniaEDRAMDumpResolveDirectCandidate"
                                  : "XeniaEDRAMDumpResolveFallback";
}

constexpr char kDirectHostResolveEncoderLabel[] =
    "XeniaDirectHostResolveEncoder";
constexpr uint32_t kDirectHostResolveDepthFlagHasStencil = 1u << 0;
constexpr uint32_t kDirectHostResolveDepthFlagRoundDepth = 1u << 1;

struct AttachmentLoadStoreActions {
  MTL::LoadAction load = MTL::LoadActionLoad;
  MTL::StoreAction store = MTL::StoreActionStore;
};

AttachmentLoadStoreActions GetRealAttachmentLoadStoreActions(
    bool needs_initial_clear, bool previous_contents_needed = true) {
  if (needs_initial_clear) {
    return {MTL::LoadActionClear, MTL::StoreActionStore};
  }
  return {previous_contents_needed ? MTL::LoadActionLoad
                                   : MTL::LoadActionDontCare,
          MTL::StoreActionStore};
}

AttachmentLoadStoreActions GetTransientAttachmentLoadStoreActions() {
  return {MTL::LoadActionDontCare, MTL::StoreActionDontCare};
}

void SetAttachmentLoadStoreActions(
    MTL::RenderPassAttachmentDescriptor* attachment,
    AttachmentLoadStoreActions actions) {
  attachment->setLoadAction(actions.load);
  attachment->setStoreAction(actions.store);
}

size_t DirectHostResolveBppIndex(bool is_64bpp) { return is_64bpp ? 1u : 0u; }

size_t DirectHostResolveMsaaIndex(xenos::MsaaSamples msaa_samples) {
  switch (msaa_samples) {
    case xenos::MsaaSamples::k1X:
      return 0;
    case xenos::MsaaSamples::k2X:
      return 1;
    case xenos::MsaaSamples::k4X:
      return 2;
    default:
      return 3;
  }
}

void SetEncoderLabel(MTL::CommandEncoder* encoder, const char* label) {
  if (!encoder || !label) {
    return;
  }
  encoder->setLabel(NS::String::string(label, NS::UTF8StringEncoding));
}

void EndSharedMemoryUploadBlitEncoderForCommandBuffer(
    MetalCommandProcessor& command_processor,
    MTL::CommandBuffer* command_buffer) {
  if (command_buffer &&
      command_buffer == command_processor.GetCurrentCommandBuffer()) {
    command_processor.EndSharedMemoryUploadBlitEncoder();
  }
}

void PushEncoderDebugGroup(MTL::CommandEncoder* encoder,
                           const std::string& label) {
  if (!encoder || label.empty()) {
    return;
  }
  encoder->pushDebugGroup(
      NS::String::string(label.c_str(), NS::UTF8StringEncoding));
}

// Packing formats for transferring host RT contents to the EDRAM buffer.
// Keep numeric values in sync with Metal dump shaders in
// InitializeEdramComputeShaders.
enum class MetalEdramDumpFormat : uint32_t {
  kColorRGBA8 = 0,
  kColorRGB10A2Unorm = 1,
  kColorRGB10A2Float = 2,
  kColorRG16Snorm = 3,
  kColorRG16Float = 4,
  kColorR32Float = 5,
  kColorRGBA16Snorm = 6,
  kColorRGBA16Float = 7,
  kColorRGBA16Unorm = 8,
  kColorRG32Float = 9,
  kDepthD24S8 = 16,
  kDepthD24FS8 = 17,
};

constexpr uint32_t kMetalEdramDumpFlagHasStencil = 1u << 0;
constexpr uint32_t kMetalEdramDumpFlagDepthRound = 1u << 1;
constexpr uint32_t kMetalEdramDumpFlagGammaAsLinear = 1u << 2;

size_t MsaaSamplesToIndex(xenos::MsaaSamples samples) {
  switch (samples) {
    case xenos::MsaaSamples::k1X:
      return 0;
    case xenos::MsaaSamples::k2X:
      return 1;
    case xenos::MsaaSamples::k4X:
      return 2;
    default:
      return 0;
  }
}

uint32_t MsaaSamplesToCount(xenos::MsaaSamples samples) {
  switch (samples) {
    case xenos::MsaaSamples::k1X:
      return 1;
    case xenos::MsaaSamples::k2X:
      return 2;
    case xenos::MsaaSamples::k4X:
      return 4;
    default:
      return 1;
  }
}

struct TransferAddressConstants {
  uint32_t dest_pitch;
  uint32_t source_pitch;
  int32_t source_to_dest;
};

struct TransferShaderConstants {
  TransferAddressConstants address;
  TransferAddressConstants host_depth_address;
  uint32_t source_format;
  uint32_t dest_format;
  uint32_t source_is_depth;
  uint32_t dest_is_depth;
  uint32_t source_is_uint;
  uint32_t dest_is_uint;
  uint32_t source_is_64bpp;
  uint32_t dest_is_64bpp;
  uint32_t source_msaa_samples;
  uint32_t dest_msaa_samples;
  uint32_t host_depth_source_msaa_samples;
  uint32_t host_depth_source_is_copy;
  uint32_t depth_round;
  uint32_t msaa_2x_supported;
  uint32_t tile_width_samples;
  uint32_t tile_height_samples;
  uint32_t dest_tile_width_pixels;
  uint32_t dest_tile_height_pixels;
  float dest_tile_width_pixels_inv;
  float dest_tile_height_pixels_inv;
  float source_pitch_tiles_inv;
  float host_depth_source_pitch_tiles_inv;
  float dest_pixel_to_ndc_x;
  float dest_pixel_to_ndc_y;
  uint32_t dest_sample_id;
  uint32_t stencil_mask;
  uint32_t stencil_clear;
};

struct TransferRectInstance {
  float origin_x;
  float origin_y;
  float size_x;
  float size_y;
};

struct TransferClearColorFloatConstants {
  float color[4];
};

struct TransferClearColorUintConstants {
  uint32_t color[4];
};

struct TransferClearDepthConstants {
  float depth;
  float padding[3];
};

enum class TransferOutput {
  kColor,
  kDepth,
  kStencilBit,
};

struct TransferModeInfo {
  TransferOutput output;
  bool source_is_color;
  bool uses_host_depth;
};

constexpr TransferModeInfo kTransferModeInfos[] = {
    {TransferOutput::kColor, true, false},        // kColorToColor
    {TransferOutput::kDepth, true, false},        // kColorToDepth
    {TransferOutput::kColor, false, false},       // kDepthToColor
    {TransferOutput::kDepth, false, false},       // kDepthToDepth
    {TransferOutput::kStencilBit, true, false},   // kColorToStencilBit
    {TransferOutput::kStencilBit, false, false},  // kDepthToStencilBit
    {TransferOutput::kDepth, true, true},         // kColorAndHostDepthToDepth
    {TransferOutput::kDepth, false, true},        // kDepthAndHostDepthToDepth
};

}  // namespace

bool MetalRenderTargetCache::IsKey64bpp(RenderTargetKey key) const {
  // For host texture storage and byte estimates, gamma-as-unorm16 uses
  // RGBA16Unorm. Guest/EDRAM addressing is still 32bpp, so transfer rectangle
  // and shader tile math must use RenderTargetKey::Is64bpp instead.
  return key.Is64bpp() ||
         (!key.is_depth &&
          key.GetColorFormat() ==
              xenos::ColorRenderTargetFormat::k_8_8_8_8_GAMMA &&
          gamma_render_target_as_unorm16_);
}

uint32_t MetalRenderTargetCache::GetMetalEdramDumpFormat(RenderTargetKey key) {
  if (key.is_depth) {
    switch (key.GetDepthFormat()) {
      case xenos::DepthRenderTargetFormat::kD24FS8:
        return static_cast<uint32_t>(MetalEdramDumpFormat::kDepthD24FS8);
      case xenos::DepthRenderTargetFormat::kD24S8:
      default:
        return static_cast<uint32_t>(MetalEdramDumpFormat::kDepthD24S8);
    }
  }
  switch (key.GetColorFormat()) {
    case xenos::ColorRenderTargetFormat::k_8_8_8_8:
    case xenos::ColorRenderTargetFormat::k_8_8_8_8_GAMMA:
      return static_cast<uint32_t>(MetalEdramDumpFormat::kColorRGBA8);
    case xenos::ColorRenderTargetFormat::k_2_10_10_10:
    case xenos::ColorRenderTargetFormat::k_2_10_10_10_AS_10_10_10_10:
      return static_cast<uint32_t>(MetalEdramDumpFormat::kColorRGB10A2Unorm);
    case xenos::ColorRenderTargetFormat::k_2_10_10_10_FLOAT:
    case xenos::ColorRenderTargetFormat::k_2_10_10_10_FLOAT_AS_16_16_16_16:
      return static_cast<uint32_t>(MetalEdramDumpFormat::kColorRGB10A2Float);
    case xenos::ColorRenderTargetFormat::k_16_16:
      return static_cast<uint32_t>(MetalEdramDumpFormat::kColorRG16Snorm);
    case xenos::ColorRenderTargetFormat::k_16_16_FLOAT:
      return static_cast<uint32_t>(MetalEdramDumpFormat::kColorRG16Float);
    case xenos::ColorRenderTargetFormat::k_32_FLOAT:
      return static_cast<uint32_t>(MetalEdramDumpFormat::kColorR32Float);
    case xenos::ColorRenderTargetFormat::k_16_16_16_16:
      return static_cast<uint32_t>(MetalEdramDumpFormat::kColorRGBA16Snorm);
    case xenos::ColorRenderTargetFormat::k_16_16_16_16_FLOAT:
      return static_cast<uint32_t>(MetalEdramDumpFormat::kColorRGBA16Float);
    case xenos::ColorRenderTargetFormat::k_32_32_FLOAT:
      return static_cast<uint32_t>(MetalEdramDumpFormat::kColorRG32Float);
    default:
      return static_cast<uint32_t>(MetalEdramDumpFormat::kColorRGBA8);
  }
}


// MetalRenderTarget implementation
MetalRenderTargetCache::MetalRenderTarget::~MetalRenderTarget() {
  if (stencil_view_) {
    stencil_view_->release();
    stencil_view_ = nullptr;
  }
  if (draw_texture_ && draw_texture_ != texture_) {
    draw_texture_->release();
    draw_texture_ = nullptr;
  }
  if (transfer_texture_ && transfer_texture_ != texture_) {
    transfer_texture_->release();
    transfer_texture_ = nullptr;
  }
  if (msaa_draw_texture_ && msaa_draw_texture_ != msaa_texture_) {
    msaa_draw_texture_->release();
    msaa_draw_texture_ = nullptr;
  }
  if (msaa_transfer_texture_ && msaa_transfer_texture_ != msaa_texture_) {
    msaa_transfer_texture_->release();
    msaa_transfer_texture_ = nullptr;
  }
  if (texture_) {
    texture_->release();
    texture_ = nullptr;
  }
  if (msaa_texture_) {
    msaa_texture_->release();
    msaa_texture_ = nullptr;
  }
}

// MetalRenderTargetCache implementation
MetalRenderTargetCache::MetalRenderTargetCache(
    const RegisterFile& register_file, const Memory& memory,
    TraceWriter* trace_writer, uint32_t draw_resolution_scale_x,
    uint32_t draw_resolution_scale_y, MetalCommandProcessor& command_processor)
    : RenderTargetCache(register_file, memory, trace_writer,
                        draw_resolution_scale_x, draw_resolution_scale_y),
      command_processor_(command_processor),
      trace_writer_(trace_writer) {}

MetalRenderTargetCache::~MetalRenderTargetCache() { Shutdown(true); }

MetalRenderTargetCache::TelemetryStats
MetalRenderTargetCache::GetAndResetTelemetryStats() {
  TelemetryStats stats = telemetry_;
  telemetry_ = TelemetryStats();
  return stats;
}

RenderTargetCache::Path MetalRenderTargetCache::GetPath() const {
  return Path::kHostRenderTargets;
}

bool MetalRenderTargetCache::InitializeEdramBufferViews() {
  ReleaseEdramBufferViews();
  if (!edram_buffer_) {
    return false;
  }

  struct ViewInit {
    uint32_t element_size_bytes_pow2;
    MTL::PixelFormat format;
    MTL::Texture** texture_out;
    const char* label;
  };
  const ViewInit kViews[] = {
      {2, MTL::PixelFormatR32Uint, &edram_r32_uint_buffer_view_,
       "XeniaEDRAMR32UintView"},
      {3, MTL::PixelFormatRG32Uint, &edram_r32g32_uint_buffer_view_,
       "XeniaEDRAMR32G32UintView"},
      {4, MTL::PixelFormatRGBA32Uint, &edram_r32g32b32a32_uint_buffer_view_,
       "XeniaEDRAMR32G32B32A32UintView"},
  };

  for (const ViewInit& view_init : kViews) {
    const NS::UInteger bytes_per_element = NS::UInteger(1u)
                                           << view_init.element_size_bytes_pow2;
    const NS::UInteger width = edram_buffer_->length() / bytes_per_element;
    if (!width) {
      XELOGE("MetalRenderTargetCache: invalid EDRAM bindless width for {}",
             view_init.label);
      ReleaseEdramBufferViews();
      return false;
    }

    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
    desc->setTextureType(MTL::TextureTypeTextureBuffer);
    desc->setPixelFormat(view_init.format);
    desc->setWidth(width);
    desc->setHeight(1);
    desc->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite |
                   MTL::TextureUsagePixelFormatView);
    desc->setResourceOptions(edram_buffer_->resourceOptions());
    desc->setStorageMode(edram_buffer_->storageMode());

    MTL::Texture* texture =
        edram_buffer_->newTexture(desc, 0, edram_buffer_->length());
    desc->release();
    if (!texture) {
      XELOGE("MetalRenderTargetCache: failed to create EDRAM bindless view {}",
             view_init.label);
      ReleaseEdramBufferViews();
      return false;
    }
    texture->setLabel(
        NS::String::string(view_init.label, NS::UTF8StringEncoding));
    *view_init.texture_out = texture;
  }

  return true;
}

void MetalRenderTargetCache::ReleaseEdramBufferViews() {
  if (edram_r32_uint_buffer_view_) {
    edram_r32_uint_buffer_view_->release();
    edram_r32_uint_buffer_view_ = nullptr;
  }
  if (edram_r32g32_uint_buffer_view_) {
    edram_r32g32_uint_buffer_view_->release();
    edram_r32g32_uint_buffer_view_ = nullptr;
  }
  if (edram_r32g32b32a32_uint_buffer_view_) {
    edram_r32g32b32a32_uint_buffer_view_->release();
    edram_r32g32b32a32_uint_buffer_view_ = nullptr;
  }
}

MTL::Texture* MetalRenderTargetCache::GetEdramUintPow2BufferView(
    uint32_t element_size_bytes_pow2) const {
  switch (element_size_bytes_pow2) {
    case 2:
      return edram_r32_uint_buffer_view_;
    case 3:
      return edram_r32g32_uint_buffer_view_;
    case 4:
      return edram_r32g32b32a32_uint_buffer_view_;
    default:
      assert_unhandled_case(element_size_bytes_pow2);
      return nullptr;
  }
}

bool MetalRenderTargetCache::WriteEdramUintPow2BindlessDescriptor(
    IRDescriptorTableEntry* entry, uint32_t element_size_bytes_pow2) const {
  if (!entry || !edram_buffer_) {
    return false;
  }
  MTL::Texture* texture_view =
      GetEdramUintPow2BufferView(element_size_bytes_pow2);
  if (!texture_view) {
    return false;
  }
  IRBufferView buffer_view = {};
  const uint64_t bytes_per_element = uint64_t(1u) << element_size_bytes_pow2;
  buffer_view.buffer = edram_buffer_;
  buffer_view.bufferOffset = 0;
  buffer_view.bufferSize = edram_buffer_->length();
  buffer_view.textureBufferView = texture_view;
  buffer_view.textureViewOffsetInElements = uint32_t(
      (uint64_t(edram_buffer_->gpuAddress()) % 16u) / bytes_per_element);
  buffer_view.typedBuffer = true;
  IRDescriptorTableSetBufferView(entry, &buffer_view);
  return true;
}

void MetalRenderTargetCache::UseBindlessResources(
    MetalCommandProcessor& command_processor, MTL::ResourceUsage usage) const {
  if (edram_buffer_) {
    command_processor.UseRenderEncoderResource(edram_buffer_, usage);
  }
  if (edram_r32_uint_buffer_view_) {
    command_processor.UseRenderEncoderResource(edram_r32_uint_buffer_view_,
                                               usage);
  }
  if (edram_r32g32_uint_buffer_view_) {
    command_processor.UseRenderEncoderResource(edram_r32g32_uint_buffer_view_,
                                               usage);
  }
  if (edram_r32g32b32a32_uint_buffer_view_) {
    command_processor.UseRenderEncoderResource(
        edram_r32g32b32a32_uint_buffer_view_, usage);
  }
}

bool MetalRenderTargetCache::Initialize() {
  device_ = command_processor_.GetMetalDevice();
  if (!device_) {
    XELOGE("MetalRenderTargetCache: No Metal device available");
    return false;
  }

  // 2x msaa and unorm16 support virtually guarunteed as minimum OS target /
  // Metal version currently is MacOS 15 / Metal 3
  msaa_2x_supported_ = device_->supportsTextureSampleCount(2);

  gamma_render_target_as_unorm16_ = ::cvars::gamma_render_target_as_unorm16 &&
                                    ::cvars::metal_allow_gamma_unorm16;
  if (::cvars::gamma_render_target_as_unorm16 &&
      !::cvars::metal_allow_gamma_unorm16) {
    XELOGW(
        "Metal: gamma_render_target_as_unorm16 disabled due to known issues; "
        "set --metal_allow_gamma_unorm16=true to force");
  }

  if (::cvars::metal_use_heaps) {
    size_t min_heap_bytes = std::max<int32_t>(0, ::cvars::metal_heap_min_bytes);
    render_target_heap_pool_ = std::make_unique<MetalHeapPool>(
        device_, MTL::StorageModePrivate, min_heap_bytes, "XeniaRT");
  }

  // Create the EDRAM buffer.
  //
  // The guest has 10 MiB of EDRAM for samples, but with host resolution
  // scaling enabled the compute path addresses a scaled EDRAM layout (the
  // shaders multiply the tile dimensions by resolution_scale_x/y). The buffer
  // therefore must be scaled by the same factor to avoid out-of-bounds writes.
  const uint32_t scale_x = std::max<uint32_t>(1u, draw_resolution_scale_x());
  const uint32_t scale_y = std::max<uint32_t>(1u, draw_resolution_scale_y());
  const size_t edram_dwords = size_t(xenos::kEdramTileCount) *
                              size_t(xenos::kEdramTileWidthSamples) *
                              size_t(xenos::kEdramTileHeightSamples) *
                              size_t(scale_x) * size_t(scale_y);
  const size_t edram_size_bytes = edram_dwords * sizeof(uint32_t);
  const bool edram_cpu_visible = false;
  const MTL::ResourceOptions edram_storage_mode =
      edram_cpu_visible ? MTL::ResourceStorageModeShared
                        : MTL::ResourceStorageModePrivate;
  edram_buffer_ = device_->newBuffer(edram_size_bytes, edram_storage_mode);
  if (!edram_buffer_) {
    XELOGE("MetalRenderTargetCache: Failed to create EDRAM buffer");
    return false;
  }
  edram_buffer_->setLabel(
      NS::String::string("EDRAM Buffer", NS::UTF8StringEncoding));
  if (edram_cpu_visible) {
    void* edram_contents = edram_buffer_->contents();
    if (edram_contents) {
      std::memset(edram_contents, 0, edram_size_bytes);
    }
  } else {
    ScopedAutoreleasePool autorelease_pool;
    MTL::CommandBuffer* cmd =
        command_processor_.CreateStandaloneTransferCommandBuffer(
            "XeniaCB reason=edram-init");
    if (cmd) {
      MTL::BlitCommandEncoder* blit = cmd->blitCommandEncoder();
      if (blit) {
        blit->fillBuffer(
            edram_buffer_,
            NS::Range::Make(0, static_cast<NS::UInteger>(edram_size_bytes)), 0);
        blit->endEncoding();
        command_processor_.CommitStandaloneAsync(cmd);
      } else {
        cmd->release();
      }
    }
  }
  if (!InitializeEdramBufferViews()) {
    XELOGE("MetalRenderTargetCache: Failed to create EDRAM bindless views");
    return false;
  }
  // Initialize EDRAM compute shaders
  if (!InitializeEdramComputeShaders()) {
    XELOGE(
        "MetalRenderTargetCache: Failed to initialize EDRAM compute shaders");
    return false;
  }

  // Initialize base class
  InitializeCommon();

  return true;
}

void MetalRenderTargetCache::Shutdown(bool from_destructor) {

  if (!from_destructor) {
    ClearCache();
  }

  // Clean up dummy target
  dummy_color_targets_.clear();
  dummy_color_target_ = nullptr;
  if (cached_render_pass_descriptor_) {
    cached_render_pass_descriptor_->release();
    cached_render_pass_descriptor_ = nullptr;
  }

  for (auto& it : transfer_pipelines_) {
    if (it.second) {
      it.second->release();
    }
  }
  transfer_pipelines_.clear();
  for (auto& it : edram_load_pipelines_) {
    if (it.second) {
      it.second->release();
    }
  }
  edram_load_pipelines_.clear();
  for (auto& it : transfer_clear_pipelines_) {
    if (it.second) {
      it.second->release();
    }
  }
  transfer_clear_pipelines_.clear();
  for (auto& it : tile_direct_host_resolve_pipelines_) {
    if (it.second) {
      it.second->release();
    }
  }
  tile_direct_host_resolve_pipelines_.clear();
  if (transfer_library_) {
    transfer_library_->release();
    transfer_library_ = nullptr;
  }
  if (tile_direct_host_resolve_library_) {
    tile_direct_host_resolve_library_->release();
    tile_direct_host_resolve_library_ = nullptr;
  }
  if (edram_load_library_) {
    edram_load_library_->release();
    edram_load_library_ = nullptr;
  }
  if (edram_load_library_msaa_) {
    edram_load_library_msaa_->release();
    edram_load_library_msaa_ = nullptr;
  }
  if (transfer_depth_state_) {
    transfer_depth_state_->release();
    transfer_depth_state_ = nullptr;
  }
  if (transfer_depth_state_none_) {
    transfer_depth_state_none_->release();
    transfer_depth_state_none_ = nullptr;
  }
  if (transfer_depth_clear_state_) {
    transfer_depth_clear_state_->release();
    transfer_depth_clear_state_ = nullptr;
  }
  if (transfer_stencil_clear_state_) {
    transfer_stencil_clear_state_->release();
    transfer_stencil_clear_state_ = nullptr;
  }
  if (transfer_stencil_output_state_) {
    transfer_stencil_output_state_->release();
    transfer_stencil_output_state_ = nullptr;
  }
  for (auto& state : transfer_stencil_bit_states_) {
    if (state) {
      state->release();
      state = nullptr;
    }
  }
  if (transfer_dummy_buffer_) {
    transfer_dummy_buffer_->release();
    transfer_dummy_buffer_ = nullptr;
  }
  for (size_t i = 0; i < xe::countof(transfer_dummy_color_float_); ++i) {
    if (transfer_dummy_color_float_[i]) {
      transfer_dummy_color_float_[i]->release();
      transfer_dummy_color_float_[i] = nullptr;
    }
    if (transfer_dummy_color_uint_[i]) {
      transfer_dummy_color_uint_[i]->release();
      transfer_dummy_color_uint_[i] = nullptr;
    }
    if (transfer_dummy_depth_[i]) {
      transfer_dummy_depth_[i]->release();
      transfer_dummy_depth_[i] = nullptr;
    }
    if (transfer_dummy_stencil_[i]) {
      transfer_dummy_stencil_[i]->release();
      transfer_dummy_stencil_[i] = nullptr;
    }
  }

  // Clean up EDRAM compute shaders
  ShutdownEdramComputeShaders();
  ReleaseEdramBufferViews();

  if (edram_buffer_) {
    edram_buffer_->release();
    edram_buffer_ = nullptr;
  }

  // Destroy all render targets
  DestroyAllRenderTargets(!from_destructor);
  render_target_map_.clear();

  if (render_target_heap_pool_) {
    render_target_heap_pool_->Shutdown();
    render_target_heap_pool_.reset();
  }

  // Shutdown base class
  if (!from_destructor) {
    ShutdownCommon();
  }
}

bool MetalRenderTargetCache::InitializeEdramComputeShaders() {
  // Initialize the resolve / EDRAM compute pipelines used by the Metal backend.
  const bool draw_resolution_scaled = IsDrawResolutionScaled();
  edram_store_pipeline_ = nullptr;
  for (auto& by_bpp : edram_dump_color_pipelines_) {
    for (auto& by_source : by_bpp) {
      for (auto*& pipeline : by_source) {
        pipeline = nullptr;
      }
    }
  }
  for (auto*& pipeline : edram_dump_depth_pipelines_) {
    pipeline = nullptr;
  }
  for (auto& by_scaled : resolve_full_pipelines_) {
    for (auto*& pipeline : by_scaled) {
      pipeline = nullptr;
    }
  }
  for (auto& by_scaled : resolve_fast_pipelines_) {
    for (auto& by_bpp : by_scaled) {
      for (auto*& pipeline : by_bpp) {
        pipeline = nullptr;
      }
    }
  }
  for (auto& by_bpp : direct_host_resolve_pipelines_) {
    for (auto& by_msaa : by_bpp) {
      for (auto& by_scaled : by_msaa) {
        for (auto*& pipeline : by_scaled) {
          pipeline = nullptr;
        }
      }
    }
  }
  for (auto& by_msaa : direct_host_color_full_resolve_pipelines_) {
    for (auto& by_scaled : by_msaa) {
      for (auto& by_source : by_scaled) {
        for (auto*& pipeline : by_source) {
          pipeline = nullptr;
        }
      }
    }
  }
  for (auto& by_msaa : direct_host_depth_resolve_pipelines_) {
    for (auto*& pipeline : by_msaa) {
      pipeline = nullptr;
    }
  }
  for (size_t i = 0; i < xe::countof(host_depth_store_pipelines_); ++i) {
    host_depth_store_pipelines_[i] = nullptr;
  }

  NS::Error* error = nullptr;

  struct ResolveFullPipelineConfig {
    const void* metallib_data;
    size_t metallib_size;
    bool scaled;
    draw_util::ResolveCopyShaderIndex copy_shader;
    const char* debug_name;
  };
  struct ResolveFastPipelineConfig {
    const void* metallib_data;
    size_t metallib_size;
    bool scaled;
    draw_util::ResolveCopyShaderIndex copy_shader;
    const char* debug_name;
  };
#define XE_RESOLVE_FULL_CONFIG(id, scaled, copy_shader) \
  {id##_metallib, sizeof(id##_metallib), scaled,        \
   draw_util::ResolveCopyShaderIndex::copy_shader, #id}
#define XE_RESOLVE_FAST_CONFIG(id, scaled, copy_shader)     \
  {                                                         \
    id##_metallib, sizeof(id##_metallib), scaled,           \
        draw_util::ResolveCopyShaderIndex::copy_shader, #id \
  }
  static constexpr ResolveFullPipelineConfig kResolveFullPipelineConfigs[] = {
      XE_RESOLVE_FULL_CONFIG(resolve_full_8bpp_cs, false, kFull8bpp),
      XE_RESOLVE_FULL_CONFIG(resolve_full_16bpp_cs, false, kFull16bpp),
      XE_RESOLVE_FULL_CONFIG(resolve_full_32bpp_cs, false, kFull32bpp),
      XE_RESOLVE_FULL_CONFIG(resolve_full_64bpp_cs, false, kFull64bpp),
      XE_RESOLVE_FULL_CONFIG(resolve_full_128bpp_cs, false, kFull128bpp),
      XE_RESOLVE_FULL_CONFIG(resolve_full_8bpp_scaled_cs, true, kFull8bpp),
      XE_RESOLVE_FULL_CONFIG(resolve_full_16bpp_scaled_cs, true, kFull16bpp),
      XE_RESOLVE_FULL_CONFIG(resolve_full_32bpp_scaled_cs, true, kFull32bpp),
      XE_RESOLVE_FULL_CONFIG(resolve_full_64bpp_scaled_cs, true, kFull64bpp),
      XE_RESOLVE_FULL_CONFIG(resolve_full_128bpp_scaled_cs, true, kFull128bpp),
  };
  static constexpr ResolveFastPipelineConfig kResolveFastPipelineConfigs[] = {
      XE_RESOLVE_FAST_CONFIG(resolve_fast_32bpp_1x2xmsaa_cs, false,
                             kFast32bpp1x2xMSAA),
      XE_RESOLVE_FAST_CONFIG(resolve_fast_32bpp_4xmsaa_cs, false,
                             kFast32bpp4xMSAA),
      XE_RESOLVE_FAST_CONFIG(resolve_fast_64bpp_1x2xmsaa_cs, false,
                             kFast64bpp1x2xMSAA),
      XE_RESOLVE_FAST_CONFIG(resolve_fast_64bpp_4xmsaa_cs, false,
                             kFast64bpp4xMSAA),
      XE_RESOLVE_FAST_CONFIG(resolve_fast_32bpp_1x2xmsaa_scaled_cs, true,
                             kFast32bpp1x2xMSAA),
      XE_RESOLVE_FAST_CONFIG(resolve_fast_32bpp_4xmsaa_scaled_cs, true,
                             kFast32bpp4xMSAA),
      XE_RESOLVE_FAST_CONFIG(resolve_fast_64bpp_1x2xmsaa_scaled_cs, true,
                             kFast64bpp1x2xMSAA),
      XE_RESOLVE_FAST_CONFIG(resolve_fast_64bpp_4xmsaa_scaled_cs, true,
                             kFast64bpp4xMSAA),
  };
#undef XE_RESOLVE_FAST_CONFIG
#undef XE_RESOLVE_FULL_CONFIG

  for (const ResolveFullPipelineConfig& cfg : kResolveFullPipelineConfigs) {
    if (cfg.scaled && !draw_resolution_scaled) {
      continue;
    }
    size_t scaled_index = cfg.scaled ? 1u : 0u;
    MTL::ComputePipelineState*& pipeline =
        resolve_full_pipelines_[scaled_index][DirectHostResolveFullDestIndex(
            cfg.copy_shader)];
    pipeline = CreateComputePipelineFromEmbeddedLibrary(
        device_, cfg.metallib_data, cfg.metallib_size, cfg.debug_name);
    if (!pipeline) {
      XELOGE("Metal: failed to initialize resolve compute pipelines");
      return false;
    }
  }
  for (const ResolveFastPipelineConfig& cfg : kResolveFastPipelineConfigs) {
    if (cfg.scaled && !draw_resolution_scaled) {
      continue;
    }
    size_t scaled_index = cfg.scaled ? 1u : 0u;
    MTL::ComputePipelineState*& pipeline =
        resolve_fast_pipelines_[scaled_index][ResolveFastBppIndex(
            cfg.copy_shader)][ResolveFastMsaaIndex(cfg.copy_shader)];
    pipeline = CreateComputePipelineFromEmbeddedLibrary(
        device_, cfg.metallib_data, cfg.metallib_size, cfg.debug_name);
    if (!pipeline) {
      XELOGE("Metal: failed to initialize resolve compute pipelines");
      return false;
    }
  }

  struct DirectHostResolvePipelineConfig {
    const void* metallib_data;
    size_t metallib_size;
    bool is_64bpp;
    xenos::MsaaSamples msaa_samples;
    bool scaled;
    bool source_is_uint;
    const char* debug_name;
  };
#define XE_DIRECT_HOST_RESOLVE_CONFIG(id, is_64bpp, msaa, scaled, source_uint) \
  {                                                                            \
    id##_metallib, sizeof(id##_metallib), is_64bpp, msaa, scaled, source_uint, \
        #id                                                                    \
  }
  static constexpr DirectHostResolvePipelineConfig
      kDirectHostResolvePipelineConfigs[] = {
          XE_DIRECT_HOST_RESOLVE_CONFIG(resolve_host_color_32bpp_1xmsaa_cs,
                                        false, xenos::MsaaSamples::k1X, false,
                                        false),
          XE_DIRECT_HOST_RESOLVE_CONFIG(resolve_host_color_32bpp_2xmsaa_cs,
                                        false, xenos::MsaaSamples::k2X, false,
                                        false),
          XE_DIRECT_HOST_RESOLVE_CONFIG(resolve_host_color_32bpp_4xmsaa_cs,
                                        false, xenos::MsaaSamples::k4X, false,
                                        false),
          XE_DIRECT_HOST_RESOLVE_CONFIG(resolve_host_color_64bpp_1xmsaa_cs,
                                        true, xenos::MsaaSamples::k1X, false,
                                        false),
          XE_DIRECT_HOST_RESOLVE_CONFIG(resolve_host_color_64bpp_2xmsaa_cs,
                                        true, xenos::MsaaSamples::k2X, false,
                                        false),
          XE_DIRECT_HOST_RESOLVE_CONFIG(resolve_host_color_64bpp_4xmsaa_cs,
                                        true, xenos::MsaaSamples::k4X, false,
                                        false),
          XE_DIRECT_HOST_RESOLVE_CONFIG(
              resolve_host_color_32bpp_1xmsaa_scaled_cs, false,
              xenos::MsaaSamples::k1X, true, false),
          XE_DIRECT_HOST_RESOLVE_CONFIG(
              resolve_host_color_32bpp_2xmsaa_scaled_cs, false,
              xenos::MsaaSamples::k2X, true, false),
          XE_DIRECT_HOST_RESOLVE_CONFIG(
              resolve_host_color_32bpp_4xmsaa_scaled_cs, false,
              xenos::MsaaSamples::k4X, true, false),
          XE_DIRECT_HOST_RESOLVE_CONFIG(
              resolve_host_color_64bpp_1xmsaa_scaled_cs, true,
              xenos::MsaaSamples::k1X, true, false),
          XE_DIRECT_HOST_RESOLVE_CONFIG(
              resolve_host_color_64bpp_2xmsaa_scaled_cs, true,
              xenos::MsaaSamples::k2X, true, false),
          XE_DIRECT_HOST_RESOLVE_CONFIG(
              resolve_host_color_64bpp_4xmsaa_scaled_cs, true,
              xenos::MsaaSamples::k4X, true, false),
          XE_DIRECT_HOST_RESOLVE_CONFIG(resolve_host_color_uint_32bpp_1xmsaa_cs,
                                        false, xenos::MsaaSamples::k1X, false,
                                        true),
          XE_DIRECT_HOST_RESOLVE_CONFIG(resolve_host_color_uint_32bpp_2xmsaa_cs,
                                        false, xenos::MsaaSamples::k2X, false,
                                        true),
          XE_DIRECT_HOST_RESOLVE_CONFIG(resolve_host_color_uint_32bpp_4xmsaa_cs,
                                        false, xenos::MsaaSamples::k4X, false,
                                        true),
          XE_DIRECT_HOST_RESOLVE_CONFIG(resolve_host_color_uint_64bpp_1xmsaa_cs,
                                        true, xenos::MsaaSamples::k1X, false,
                                        true),
          XE_DIRECT_HOST_RESOLVE_CONFIG(resolve_host_color_uint_64bpp_2xmsaa_cs,
                                        true, xenos::MsaaSamples::k2X, false,
                                        true),
          XE_DIRECT_HOST_RESOLVE_CONFIG(resolve_host_color_uint_64bpp_4xmsaa_cs,
                                        true, xenos::MsaaSamples::k4X, false,
                                        true),
          XE_DIRECT_HOST_RESOLVE_CONFIG(
              resolve_host_color_uint_32bpp_1xmsaa_scaled_cs, false,
              xenos::MsaaSamples::k1X, true, true),
          XE_DIRECT_HOST_RESOLVE_CONFIG(
              resolve_host_color_uint_32bpp_2xmsaa_scaled_cs, false,
              xenos::MsaaSamples::k2X, true, true),
          XE_DIRECT_HOST_RESOLVE_CONFIG(
              resolve_host_color_uint_32bpp_4xmsaa_scaled_cs, false,
              xenos::MsaaSamples::k4X, true, true),
          XE_DIRECT_HOST_RESOLVE_CONFIG(
              resolve_host_color_uint_64bpp_1xmsaa_scaled_cs, true,
              xenos::MsaaSamples::k1X, true, true),
          XE_DIRECT_HOST_RESOLVE_CONFIG(
              resolve_host_color_uint_64bpp_2xmsaa_scaled_cs, true,
              xenos::MsaaSamples::k2X, true, true),
          XE_DIRECT_HOST_RESOLVE_CONFIG(
              resolve_host_color_uint_64bpp_4xmsaa_scaled_cs, true,
              xenos::MsaaSamples::k4X, true, true),
      };
#undef XE_DIRECT_HOST_RESOLVE_CONFIG

  for (const DirectHostResolvePipelineConfig& cfg :
       kDirectHostResolvePipelineConfigs) {
    if (cfg.scaled && !draw_resolution_scaled) {
      continue;
    }
    MTL::ComputePipelineState* pipeline =
        CreateComputePipelineFromEmbeddedLibrary(
            device_, cfg.metallib_data, cfg.metallib_size, cfg.debug_name);
    if (!pipeline) {
      XELOGW(
          "Metal: failed to initialize optional direct host resolve "
          "pipeline {}",
          cfg.debug_name);
      continue;
    }
    size_t msaa_index = DirectHostResolveMsaaIndex(cfg.msaa_samples);
    if (msaa_index >= kDirectHostResolveMsaaCount) {
      pipeline->release();
      continue;
    }
    direct_host_resolve_pipelines_[DirectHostResolveBppIndex(cfg.is_64bpp)]
                                  [msaa_index][cfg.scaled ? 1u : 0u]
                                  [cfg.source_is_uint ? 1u : 0u] = pipeline;
  }

  struct DirectHostColorFullResolvePipelineConfig {
    const void* metallib_data;
    size_t metallib_size;
    draw_util::ResolveCopyShaderIndex copy_shader;
    xenos::MsaaSamples msaa_samples;
    bool scaled;
    bool source_is_uint;
    const char* debug_name;
  };
#define XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(id, shader, msaa, scaled,   \
                                                 source_uint)                \
  {                                                                          \
    id##_metallib, sizeof(id##_metallib), shader, msaa, scaled, source_uint, \
        #id                                                                  \
  }
  static constexpr DirectHostColorFullResolvePipelineConfig
      kDirectHostColorFullResolvePipelineConfigs[] = {
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_8bpp_1xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull8bpp,
              xenos::MsaaSamples::k1X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_8bpp_1xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull8bpp,
              xenos::MsaaSamples::k1X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_8bpp_2xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull8bpp,
              xenos::MsaaSamples::k2X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_8bpp_2xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull8bpp,
              xenos::MsaaSamples::k2X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_8bpp_4xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull8bpp,
              xenos::MsaaSamples::k4X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_8bpp_4xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull8bpp,
              xenos::MsaaSamples::k4X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_16bpp_1xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull16bpp,
              xenos::MsaaSamples::k1X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_16bpp_1xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull16bpp,
              xenos::MsaaSamples::k1X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_16bpp_2xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull16bpp,
              xenos::MsaaSamples::k2X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_16bpp_2xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull16bpp,
              xenos::MsaaSamples::k2X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_16bpp_4xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull16bpp,
              xenos::MsaaSamples::k4X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_16bpp_4xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull16bpp,
              xenos::MsaaSamples::k4X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_32bpp_1xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull32bpp,
              xenos::MsaaSamples::k1X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_32bpp_1xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull32bpp,
              xenos::MsaaSamples::k1X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_32bpp_2xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull32bpp,
              xenos::MsaaSamples::k2X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_32bpp_2xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull32bpp,
              xenos::MsaaSamples::k2X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_32bpp_4xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull32bpp,
              xenos::MsaaSamples::k4X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_32bpp_4xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull32bpp,
              xenos::MsaaSamples::k4X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_64bpp_1xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull64bpp,
              xenos::MsaaSamples::k1X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_64bpp_1xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull64bpp,
              xenos::MsaaSamples::k1X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_64bpp_2xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull64bpp,
              xenos::MsaaSamples::k2X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_64bpp_2xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull64bpp,
              xenos::MsaaSamples::k2X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_64bpp_4xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull64bpp,
              xenos::MsaaSamples::k4X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_64bpp_4xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull64bpp,
              xenos::MsaaSamples::k4X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_128bpp_1xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull128bpp,
              xenos::MsaaSamples::k1X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_128bpp_1xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull128bpp,
              xenos::MsaaSamples::k1X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_128bpp_2xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull128bpp,
              xenos::MsaaSamples::k2X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_128bpp_2xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull128bpp,
              xenos::MsaaSamples::k2X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_128bpp_4xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull128bpp,
              xenos::MsaaSamples::k4X, false, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_128bpp_4xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull128bpp,
              xenos::MsaaSamples::k4X, true, false),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_8bpp_1xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull8bpp,
              xenos::MsaaSamples::k1X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_8bpp_1xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull8bpp,
              xenos::MsaaSamples::k1X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_8bpp_2xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull8bpp,
              xenos::MsaaSamples::k2X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_8bpp_2xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull8bpp,
              xenos::MsaaSamples::k2X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_8bpp_4xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull8bpp,
              xenos::MsaaSamples::k4X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_8bpp_4xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull8bpp,
              xenos::MsaaSamples::k4X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_16bpp_1xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull16bpp,
              xenos::MsaaSamples::k1X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_16bpp_1xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull16bpp,
              xenos::MsaaSamples::k1X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_16bpp_2xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull16bpp,
              xenos::MsaaSamples::k2X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_16bpp_2xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull16bpp,
              xenos::MsaaSamples::k2X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_16bpp_4xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull16bpp,
              xenos::MsaaSamples::k4X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_16bpp_4xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull16bpp,
              xenos::MsaaSamples::k4X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_32bpp_1xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull32bpp,
              xenos::MsaaSamples::k1X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_32bpp_1xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull32bpp,
              xenos::MsaaSamples::k1X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_32bpp_2xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull32bpp,
              xenos::MsaaSamples::k2X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_32bpp_2xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull32bpp,
              xenos::MsaaSamples::k2X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_32bpp_4xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull32bpp,
              xenos::MsaaSamples::k4X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_32bpp_4xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull32bpp,
              xenos::MsaaSamples::k4X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_64bpp_1xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull64bpp,
              xenos::MsaaSamples::k1X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_64bpp_1xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull64bpp,
              xenos::MsaaSamples::k1X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_64bpp_2xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull64bpp,
              xenos::MsaaSamples::k2X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_64bpp_2xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull64bpp,
              xenos::MsaaSamples::k2X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_64bpp_4xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull64bpp,
              xenos::MsaaSamples::k4X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_64bpp_4xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull64bpp,
              xenos::MsaaSamples::k4X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_128bpp_1xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull128bpp,
              xenos::MsaaSamples::k1X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_128bpp_1xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull128bpp,
              xenos::MsaaSamples::k1X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_128bpp_2xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull128bpp,
              xenos::MsaaSamples::k2X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_128bpp_2xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull128bpp,
              xenos::MsaaSamples::k2X, true, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_128bpp_4xmsaa_cs,
              draw_util::ResolveCopyShaderIndex::kFull128bpp,
              xenos::MsaaSamples::k4X, false, true),
          XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG(
              resolve_host_color_full_uint_128bpp_4xmsaa_scaled_cs,
              draw_util::ResolveCopyShaderIndex::kFull128bpp,
              xenos::MsaaSamples::k4X, true, true),
      };
#undef XE_DIRECT_HOST_COLOR_FULL_RESOLVE_CONFIG

  for (const DirectHostColorFullResolvePipelineConfig& cfg :
       kDirectHostColorFullResolvePipelineConfigs) {
    if (cfg.scaled && !draw_resolution_scaled) {
      continue;
    }
    MTL::ComputePipelineState* pipeline =
        CreateComputePipelineFromEmbeddedLibrary(
            device_, cfg.metallib_data, cfg.metallib_size, cfg.debug_name);
    if (!pipeline) {
      XELOGW(
          "Metal: failed to initialize optional direct host full color "
          "resolve pipeline {}",
          cfg.debug_name);
      continue;
    }
    size_t msaa_index = DirectHostResolveMsaaIndex(cfg.msaa_samples);
    size_t dest_index = DirectHostResolveFullDestIndex(cfg.copy_shader);
    if (msaa_index >= kDirectHostResolveMsaaCount ||
        dest_index >= kDirectHostResolveFullDestCount) {
      pipeline->release();
      continue;
    }
    direct_host_color_full_resolve_pipelines_[msaa_index][cfg.scaled ? 1u : 0u]
                                             [cfg.source_is_uint ? 1u : 0u]
                                             [dest_index] = pipeline;
  }

  struct DirectHostDepthResolvePipelineConfig {
    const void* metallib_data;
    size_t metallib_size;
    xenos::MsaaSamples msaa_samples;
    bool scaled;
    const char* debug_name;
  };
#define XE_DIRECT_HOST_DEPTH_RESOLVE_CONFIG(id, msaa, scaled) \
  {                                                           \
    id##_metallib, sizeof(id##_metallib), msaa, scaled, #id   \
  }
  static constexpr DirectHostDepthResolvePipelineConfig
      kDirectHostDepthResolvePipelineConfigs[] = {
          XE_DIRECT_HOST_DEPTH_RESOLVE_CONFIG(
              resolve_host_depth_32bpp_1xmsaa_cs, xenos::MsaaSamples::k1X,
              false),
          XE_DIRECT_HOST_DEPTH_RESOLVE_CONFIG(
              resolve_host_depth_32bpp_2xmsaa_cs, xenos::MsaaSamples::k2X,
              false),
          XE_DIRECT_HOST_DEPTH_RESOLVE_CONFIG(
              resolve_host_depth_32bpp_4xmsaa_cs, xenos::MsaaSamples::k4X,
              false),
          XE_DIRECT_HOST_DEPTH_RESOLVE_CONFIG(
              resolve_host_depth_32bpp_1xmsaa_scaled_cs,
              xenos::MsaaSamples::k1X, true),
          XE_DIRECT_HOST_DEPTH_RESOLVE_CONFIG(
              resolve_host_depth_32bpp_2xmsaa_scaled_cs,
              xenos::MsaaSamples::k2X, true),
          XE_DIRECT_HOST_DEPTH_RESOLVE_CONFIG(
              resolve_host_depth_32bpp_4xmsaa_scaled_cs,
              xenos::MsaaSamples::k4X, true),
      };
#undef XE_DIRECT_HOST_DEPTH_RESOLVE_CONFIG

  for (const DirectHostDepthResolvePipelineConfig& cfg :
       kDirectHostDepthResolvePipelineConfigs) {
    if (cfg.scaled && !draw_resolution_scaled) {
      continue;
    }
    MTL::ComputePipelineState* pipeline =
        CreateComputePipelineFromEmbeddedLibrary(
            device_, cfg.metallib_data, cfg.metallib_size, cfg.debug_name);
    if (!pipeline) {
      XELOGW(
          "Metal: failed to initialize optional direct host depth resolve "
          "pipeline {}",
          cfg.debug_name);
      continue;
    }
    size_t msaa_index = DirectHostResolveMsaaIndex(cfg.msaa_samples);
    if (msaa_index >= kDirectHostResolveMsaaCount) {
      pipeline->release();
      continue;
    }
    direct_host_depth_resolve_pipelines_[msaa_index][cfg.scaled ? 1u : 0u] =
        pipeline;
  }

  host_depth_store_pipelines_[size_t(xenos::MsaaSamples::k1X)] =
      CreateComputePipelineFromEmbeddedLibrary(
          device_, host_depth_store_1xmsaa_cs_metallib,
          sizeof(host_depth_store_1xmsaa_cs_metallib),
          "host_depth_store_1xmsaa");
  host_depth_store_pipelines_[size_t(xenos::MsaaSamples::k2X)] =
      CreateComputePipelineFromEmbeddedLibrary(
          device_, host_depth_store_2xmsaa_cs_metallib,
          sizeof(host_depth_store_2xmsaa_cs_metallib),
          "host_depth_store_2xmsaa");
  host_depth_store_pipelines_[size_t(xenos::MsaaSamples::k4X)] =
      CreateComputePipelineFromEmbeddedLibrary(
          device_, host_depth_store_4xmsaa_cs_metallib,
          sizeof(host_depth_store_4xmsaa_cs_metallib),
          "host_depth_store_4xmsaa");

  for (size_t i = 0; i < xe::countof(host_depth_store_pipelines_); ++i) {
    if (!host_depth_store_pipelines_[i]) {
      XELOGE("Metal: failed to initialize host depth store pipelines");
      return false;
    }
  }

  // EDRAM dump compute shaders -- parameterized MSL template compiled for
  // float, uint ownership-transfer, and depth sources.
  // with different #defines. Each variant differs in MSAA sample count,
  // bits-per-pixel, and whether it dumps color or depth data.
  //
  // Shared preamble: constants, utilities, and ALL pack functions so every
  // variant can reference them through #if guards in the kernel template.
  static const char kEdramDumpPreamble[] = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct EdramDumpConstants {
  uint dispatch_first_tile;
  uint source_base_tiles;
  uint dest_pitch_tiles;
  uint source_pitch_tiles;
  uint2 resolution_scale;
  uint tile_size_x;
  uint tile_size_y;
  float tile_size_inv_x;
  float tile_size_inv_y;
  float source_pitch_tiles_inv;
  uint format;
  uint flags;
  uint padding;
};

inline void XeFastDivMod(uint x, uint w, float inv_w, thread uint& q,
                         thread uint& r) {
  if (w == 0u) {
    q = 0u;
    r = 0u;
    return;
  }
  q = uint(float(x) * inv_w);
  r = x - q * w;
  if (r >= w) {
    r -= w;
    q += 1u;
  } else if (r > x) {
    r += w;
    q -= 1u;
  }
}

constant uint kDumpFormatColorRGBA8 = 0;
constant uint kDumpFormatColorRGB10A2Unorm = 1;
constant uint kDumpFormatColorRGB10A2Float = 2;
constant uint kDumpFormatColorRG16Snorm = 3;
constant uint kDumpFormatColorRG16Float = 4;
constant uint kDumpFormatColorR32Float = 5;
constant uint kDumpFormatColorRGBA16Snorm = 6;
constant uint kDumpFormatColorRGBA16Float = 7;
constant uint kDumpFormatColorRGBA16Unorm = 8;
constant uint kDumpFormatColorRG32Float = 9;
constant uint kDumpFormatDepthD24S8 = 16;
constant uint kDumpFormatDepthD24FS8 = 17;
constant uint kDumpFlagHasStencil = 1;   // bit 0
constant uint kDumpFlagDepthRound = 2;   // bit 1
constant uint kDumpFlagGammaAsLinear = 4; // bit 2

// --- Color 32bpp helpers (guarded so depth-only compiles skip them) ---
#if DUMP_IS_DEPTH == 0 && DUMP_BPP == 32

// PWL gamma encode: linear -> gamma (for gamma RTs stored as linear RGBA16Unorm)
inline float XeLinearToPWLGamma(float value) {
  float clamped = clamp(value, 0.0f, 1.0f);
  float scale, offset;
  if (clamped >= (128.0f / 1023.0f)) {
    if (clamped >= (512.0f / 1023.0f)) { scale = 1023.0f / 8.0f; offset = 128.0f / 255.0f; }
    else { scale = 1023.0f / 4.0f; offset = 64.0f / 255.0f; }
  } else {
    if (clamped >= (64.0f / 1023.0f)) { scale = 1023.0f / 2.0f; offset = 32.0f / 255.0f; }
    else { scale = 1023.0f; offset = 0.0f; }
  }
  return trunc(clamped * scale) * (1.0f / 255.0f) + offset;
}
inline float3 XeLinearToPWLGamma3(float3 v) {
  return float3(XeLinearToPWLGamma(v.r), XeLinearToPWLGamma(v.g), XeLinearToPWLGamma(v.b));
}

inline uint XePackUnorm(float value, float scale) {
  return uint(clamp(value, 0.0f, 1.0f) * scale + 0.5f);
}

inline uint XePackSnorm16(float value) {
  float clamped = clamp(value, -1.0f, 1.0f);
  float bias = clamped >= 0.0f ? 0.5f : -0.5f;
  int packed = int(clamped * 32767.0f + bias);
  return uint(packed) & 0xFFFFu;
}

uint XePreClampedFloat32To7e3(float value) {
  uint f32 = as_type<uint>(value);
  uint biased_f32;
  if (f32 < 0x3E800000u) {
    uint f32_exp = f32 >> 23u;
    uint shift = 125u - f32_exp;
    shift = min(shift, 24u);
    uint mantissa = (f32 & 0x7FFFFFu) | 0x800000u;
    biased_f32 = mantissa >> shift;
  } else {
    biased_f32 = f32 + 0xC2000000u;
  }
  uint round_bit = (biased_f32 >> 16u) & 1u;
  uint f10 = biased_f32 + 0x7FFFu + round_bit;
  return (f10 >> 16u) & 0x3FFu;
}

uint XeUnclampedFloat32To7e3(float value) {
  float clamped = min(max(value, 0.0f), 31.875f);
  return XePreClampedFloat32To7e3(clamped);
}

uint XePackColor32bpp(uint format, float4 color) {
  switch (format) {
    case kDumpFormatColorRGBA8: {
      uint r = XePackUnorm(color.r, 255.0f);
      uint g = XePackUnorm(color.g, 255.0f);
      uint b = XePackUnorm(color.b, 255.0f);
      uint a = XePackUnorm(color.a, 255.0f);
      return r | (g << 8u) | (b << 16u) | (a << 24u);
    }
    case kDumpFormatColorRGB10A2Unorm: {
      uint r = XePackUnorm(color.r, 1023.0f);
      uint g = XePackUnorm(color.g, 1023.0f);
      uint b = XePackUnorm(color.b, 1023.0f);
      uint a = XePackUnorm(color.a, 3.0f);
      return r | (g << 10u) | (b << 20u) | (a << 30u);
    }
    case kDumpFormatColorRGB10A2Float: {
      uint r = XeUnclampedFloat32To7e3(color.r);
      uint g = XeUnclampedFloat32To7e3(color.g);
      uint b = XeUnclampedFloat32To7e3(color.b);
      uint a = XePackUnorm(color.a, 3.0f);
      return (r & 0x3FFu) | ((g & 0x3FFu) << 10u) |
             ((b & 0x3FFu) << 20u) | ((a & 0x3u) << 30u);
    }
    case kDumpFormatColorRG16Snorm: {
      uint r = XePackSnorm16(color.r);
      uint g = XePackSnorm16(color.g);
      return r | (g << 16u);
    }
    case kDumpFormatColorRG16Float:
      return as_type<uint>(half2(color.rg));
    case kDumpFormatColorR32Float:
      return as_type<uint>(color.r);
    default: {
      uint r = XePackUnorm(color.r, 255.0f);
      uint g = XePackUnorm(color.g, 255.0f);
      uint b = XePackUnorm(color.b, 255.0f);
      uint a = XePackUnorm(color.a, 255.0f);
      return r | (g << 8u) | (b << 16u) | (a << 24u);
    }
  }
}

uint XePackColor32bppUint(uint format, uint4 color) {
  switch (format) {
    case kDumpFormatColorRG16Snorm:
    case kDumpFormatColorRG16Float:
      return (color.r & 0xFFFFu) | ((color.g & 0xFFFFu) << 16u);
    case kDumpFormatColorR32Float:
      return color.r;
    default:
      return color.r;
  }
}

#endif  // DUMP_IS_DEPTH == 0 && DUMP_BPP == 32

// --- Color 64bpp helpers ---
#if DUMP_IS_DEPTH == 0 && DUMP_BPP == 64

inline uint XePackSnorm16(float value) {
  float clamped = clamp(value, -1.0f, 1.0f);
  float bias = clamped >= 0.0f ? 0.5f : -0.5f;
  int packed = int(clamped * 32767.0f + bias);
  return uint(packed) & 0xFFFFu;
}

inline uint XePackUnorm(float value, float scale) {
  return uint(clamp(value, 0.0f, 1.0f) * scale + 0.5f);
}

uint2 XePackColor64bpp(uint format, float4 color) {
  switch (format) {
    case kDumpFormatColorRGBA16Snorm: {
      uint r = XePackSnorm16(color.r);
      uint g = XePackSnorm16(color.g);
      uint b = XePackSnorm16(color.b);
      uint a = XePackSnorm16(color.a);
      uint rg = r | (g << 16u);
      uint ba = b | (a << 16u);
      return uint2(rg, ba);
    }
    case kDumpFormatColorRGBA16Float: {
      uint rg = as_type<uint>(half2(color.rg));
      uint ba = as_type<uint>(half2(color.ba));
      return uint2(rg, ba);
    }
    case kDumpFormatColorRGBA16Unorm: {
      uint r = XePackUnorm(color.r, 65535.0f);
      uint g = XePackUnorm(color.g, 65535.0f);
      uint b = XePackUnorm(color.b, 65535.0f);
      uint a = XePackUnorm(color.a, 65535.0f);
      uint rg = r | (g << 16u);
      uint ba = b | (a << 16u);
      return uint2(rg, ba);
    }
    case kDumpFormatColorRG32Float: {
      uint r = as_type<uint>(color.r);
      uint g = as_type<uint>(color.g);
      return uint2(r, g);
    }
    default: {
      uint rg = as_type<uint>(half2(color.rg));
      uint ba = as_type<uint>(half2(color.ba));
      return uint2(rg, ba);
    }
  }
}

uint2 XePackColor64bppUint(uint format, uint4 color) {
  switch (format) {
    case kDumpFormatColorRGBA16Snorm:
    case kDumpFormatColorRGBA16Float: {
      uint rg = (color.r & 0xFFFFu) | ((color.g & 0xFFFFu) << 16u);
      uint ba = (color.b & 0xFFFFu) | ((color.a & 0xFFFFu) << 16u);
      return uint2(rg, ba);
    }
    case kDumpFormatColorRG32Float:
      return uint2(color.r, color.g);
    default:
      return uint2(color.r, color.g);
  }
}

#endif  // DUMP_IS_DEPTH == 0 && DUMP_BPP == 64

// --- Depth 32bpp helpers ---
#if DUMP_IS_DEPTH == 1

inline uint XeRoundToNearestEven(float value) {
  float floor_value = floor(value);
  float frac = value - floor_value;
  uint result = uint(floor_value);
  if (frac > 0.5f || (frac == 0.5f && (result & 1u))) {
    result += 1u;
  }
  return result;
}

uint XeFloat32To20e4(float value, bool round_to_nearest_even) {
  uint f32 = as_type<uint>(value);
  f32 = min((f32 <= 0x7FFFFFFFu) ? f32 : 0u, 0x3FFFFFF8u);
  uint denormalized =
      ((f32 & 0x7FFFFFu) | 0x800000u) >> min(113u - (f32 >> 23u), 24u);
  uint f24 = (f32 < 0x38800000u) ? denormalized : (f32 + 0xC8000000u);
  if (round_to_nearest_even) {
    f24 += 3u + ((f24 >> 3u) & 1u);
  }
  return (f24 >> 3u) & 0xFFFFFFu;
}

#endif  // DUMP_IS_DEPTH == 1
)METAL";

  // Parameterized kernel template. Uses #if on DUMP_MSAA_SAMPLES, DUMP_BPP,
  // and DUMP_IS_DEPTH to select texture type, sample access, EDRAM element
  // type, pack function, and the depth-specific tile-half swizzle.
  static const char kEdramDumpKernelTemplate[] = R"METAL(

kernel void DUMP_KERNEL_NAME(
#if DUMP_IS_DEPTH == 1
  #if DUMP_MSAA_SAMPLES == 1
    texture2d<float, access::read> source [[texture(0)]],
    texture2d<uint, access::read> stencil [[texture(1)]],
  #else
    texture2d_ms<float, access::read> source [[texture(0)]],
    texture2d_ms<uint, access::read> stencil [[texture(1)]],
  #endif
    device uint* edram [[buffer(0)]],
#else  // color
  #if DUMP_SOURCE_IS_UINT == 1
    #if DUMP_MSAA_SAMPLES == 1
    texture2d<uint, access::read> source [[texture(0)]],
    #else
    texture2d_ms<uint, access::read> source [[texture(0)]],
    #endif
  #else
    #if DUMP_MSAA_SAMPLES == 1
    texture2d<float, access::read> source [[texture(0)]],
    #else
    texture2d_ms<float, access::read> source [[texture(0)]],
    #endif
  #endif
  #if DUMP_BPP == 64
    device uint2* edram [[buffer(0)]],
  #else
    device uint* edram [[buffer(0)]],
  #endif
#endif
    constant EdramDumpConstants& constants [[buffer(1)]],
    uint3 tid [[thread_position_in_grid]]) {
  const uint kEdramTileCount = 2048u;

  uint2 tile_size = uint2(constants.tile_size_x, constants.tile_size_y);

  uint tile_coord_x = 0u;
  uint tile_coord_y = 0u;
  uint sample_in_tile_x = 0u;
  uint sample_in_tile_y = 0u;
  XeFastDivMod(tid.x, tile_size.x, constants.tile_size_inv_x, tile_coord_x,
               sample_in_tile_x);
  XeFastDivMod(tid.y, tile_size.y, constants.tile_size_inv_y, tile_coord_y,
               sample_in_tile_y);
  uint2 tile_coord = uint2(tile_coord_x, tile_coord_y);
  uint2 sample_in_tile = uint2(sample_in_tile_x, sample_in_tile_y);

#if DUMP_IS_DEPTH == 1
  // Depth tiles use a half-width swizzle for EDRAM sample indexing.
  uint2 edram_sample_in_tile = sample_in_tile;
  uint tile_width_half = tile_size.x >> 1u;
  edram_sample_in_tile.x =
      (edram_sample_in_tile.x < tile_width_half)
          ? (edram_sample_in_tile.x + tile_width_half)
          : (edram_sample_in_tile.x - tile_width_half);
#endif

  uint rect_tile_index = tile_coord.y * constants.dest_pitch_tiles + tile_coord.x;

  uint nonwrapped_tile = constants.dispatch_first_tile + rect_tile_index;
  uint wrapped_tile = nonwrapped_tile & (kEdramTileCount - 1u);

  uint tile_samples = tile_size.x * tile_size.y;
#if DUMP_IS_DEPTH == 1
  uint sample_index =
      edram_sample_in_tile.y * tile_size.x + edram_sample_in_tile.x;
#else
  uint sample_index = sample_in_tile.y * tile_size.x + sample_in_tile.x;
#endif
  uint edram_index = wrapped_tile * tile_samples + sample_index;

  uint source_linear_tile = nonwrapped_tile - constants.source_base_tiles;
  uint source_tile_y = 0u;
  uint source_tile_x = 0u;
  XeFastDivMod(source_linear_tile, constants.source_pitch_tiles,
               constants.source_pitch_tiles_inv, source_tile_y, source_tile_x);

#if DUMP_MSAA_SAMPLES == 1
  uint2 source_coord = uint2(source_tile_x * tile_size.x + sample_in_tile.x,
                             source_tile_y * tile_size.y + sample_in_tile.y);
#else
  uint2 source_sample = uint2(source_tile_x * tile_size.x + sample_in_tile.x,
                              source_tile_y * tile_size.y + sample_in_tile.y);
  #if DUMP_MSAA_SAMPLES == 2
  uint sample_id = (source_sample.y & 1u) ? DUMP_2X_SAMPLE_MAP_1
                                           : DUMP_2X_SAMPLE_MAP_0;
  uint2 pixel_coord = uint2(source_sample.x, source_sample.y >> 1);
  #else  // 4x
  uint sample_x = source_sample.x & 1u;
  uint sample_y = source_sample.y & 1u;
  uint sample_id = sample_x | (sample_y << 1u);
  uint2 pixel_coord = uint2(source_sample.x >> 1, source_sample.y >> 1);
  #endif
#endif

  // --- Read and pack ---
#if DUMP_IS_DEPTH == 1
  #if DUMP_MSAA_SAMPLES == 1
  float depth = source.read(source_coord).r;
  #else
  float depth = source.read(pixel_coord, sample_id).r;
  #endif

  uint depth24;
  if (constants.format == kDumpFormatDepthD24FS8) {
    bool round_depth = (constants.flags & kDumpFlagDepthRound) != 0u;
    depth24 = XeFloat32To20e4(depth * 2.0f, round_depth);
  } else {
    float depth_f = clamp(depth, 0.0f, 1.0f) * 16777215.0f;
    depth24 = XeRoundToNearestEven(depth_f);
  }

  uint stencil_value = 0u;
  if ((constants.flags & kDumpFlagHasStencil) != 0u) {
  #if DUMP_MSAA_SAMPLES == 1
    stencil_value = stencil.read(source_coord).x & 0xFFu;
  #else
    stencil_value = stencil.read(pixel_coord, sample_id).x & 0xFFu;
  #endif
  }

  edram[edram_index] = (depth24 << 8u) | stencil_value;

#elif DUMP_BPP == 32
  #if DUMP_SOURCE_IS_UINT == 1
    #if DUMP_MSAA_SAMPLES == 1
    uint4 color = source.read(source_coord);
    #else
    uint4 color = source.read(pixel_coord, sample_id);
    #endif

  edram[edram_index] = XePackColor32bppUint(constants.format, color);
  #else
    #if DUMP_MSAA_SAMPLES == 1
    float4 color = source.read(source_coord);
    #else
    float4 color = source.read(pixel_coord, sample_id);
    #endif

    // If source is a linear RGBA16Unorm gamma RT, convert to PWL gamma encoding
    if (constants.flags & kDumpFlagGammaAsLinear) {
      color.rgb = XeLinearToPWLGamma3(color.rgb);
    }

    edram[edram_index] = XePackColor32bpp(constants.format, color);
  #endif

#else  // color 64bpp
  #if DUMP_SOURCE_IS_UINT == 1
    #if DUMP_MSAA_SAMPLES == 1
    uint4 color = source.read(source_coord);
    #else
    uint4 color = source.read(pixel_coord, sample_id);
    #endif

  edram[edram_index] = XePackColor64bppUint(constants.format, color);
  #else
    #if DUMP_MSAA_SAMPLES == 1
    float4 color = source.read(source_coord);
    #else
    float4 color = source.read(pixel_coord, sample_id);
    #endif

    edram[edram_index] = XePackColor64bpp(constants.format, color);
  #endif
#endif
}
)METAL";

  // Configuration table for the EDRAM dump shader variants.
  struct EdramDumpConfig {
    const char* kernel_name;
    uint32_t msaa_samples;
    uint32_t bpp;
    uint32_t is_depth;
    uint32_t source_is_uint;
    MTL::ComputePipelineState** pipeline;
  };
  const EdramDumpConfig kEdramDumpConfigs[] = {
      {"edram_dump_color_32bpp_1xmsaa", 1, 32, 0, 0,
       &edram_dump_color_pipelines_[0][0][0]},
      {"edram_dump_color_32bpp_2xmsaa", 2, 32, 0, 0,
       &edram_dump_color_pipelines_[0][0][1]},
      {"edram_dump_color_32bpp_4xmsaa", 4, 32, 0, 0,
       &edram_dump_color_pipelines_[0][0][2]},
      {"edram_dump_color_uint_32bpp_1xmsaa", 1, 32, 0, 1,
       &edram_dump_color_pipelines_[0][1][0]},
      {"edram_dump_color_uint_32bpp_2xmsaa", 2, 32, 0, 1,
       &edram_dump_color_pipelines_[0][1][1]},
      {"edram_dump_color_uint_32bpp_4xmsaa", 4, 32, 0, 1,
       &edram_dump_color_pipelines_[0][1][2]},
      {"edram_dump_depth_32bpp_1xmsaa", 1, 32, 1, 0,
       &edram_dump_depth_pipelines_[0]},
      {"edram_dump_depth_32bpp_2xmsaa", 2, 32, 1, 0,
       &edram_dump_depth_pipelines_[1]},
      {"edram_dump_depth_32bpp_4xmsaa", 4, 32, 1, 0,
       &edram_dump_depth_pipelines_[2]},
      {"edram_dump_color_64bpp_1xmsaa", 1, 64, 0, 0,
       &edram_dump_color_pipelines_[1][0][0]},
      {"edram_dump_color_64bpp_2xmsaa", 2, 64, 0, 0,
       &edram_dump_color_pipelines_[1][0][1]},
      {"edram_dump_color_64bpp_4xmsaa", 4, 64, 0, 0,
       &edram_dump_color_pipelines_[1][0][2]},
      {"edram_dump_color_uint_64bpp_1xmsaa", 1, 64, 0, 1,
       &edram_dump_color_pipelines_[1][1][0]},
      {"edram_dump_color_uint_64bpp_2xmsaa", 2, 64, 0, 1,
       &edram_dump_color_pipelines_[1][1][1]},
      {"edram_dump_color_uint_64bpp_4xmsaa", 4, 64, 0, 1,
       &edram_dump_color_pipelines_[1][1][2]},
  };

  auto append_dump_define = [](std::string& src, const char* name,
                               uint32_t value) {
    src.append("#define ");
    src.append(name);
    src.push_back(' ');
    src.append(std::to_string(value));
    src.push_back('\n');
  };
  auto append_dump_define_str = [](std::string& src, const char* name,
                                   const char* value) {
    src.append("#define ");
    src.append(name);
    src.push_back(' ');
    src.append(value);
    src.push_back('\n');
  };

  for (const auto& cfg : kEdramDumpConfigs) {
    std::string dump_source;
    dump_source.reserve(8192);

    // Prepend per-variant #defines before the shared preamble.
    append_dump_define(dump_source, "DUMP_MSAA_SAMPLES", cfg.msaa_samples);
    append_dump_define(dump_source, "DUMP_BPP", cfg.bpp);
    append_dump_define(dump_source, "DUMP_IS_DEPTH", cfg.is_depth);
    append_dump_define(dump_source, "DUMP_SOURCE_IS_UINT", cfg.source_is_uint);
    append_dump_define_str(dump_source, "DUMP_KERNEL_NAME", cfg.kernel_name);
    if (cfg.msaa_samples == 2) {
      // 2x MSAA sample remapping: guest sample 0/1 -> host sample indices.
      uint32_t map_0 =
          draw_util::GetD3D10SampleIndexForGuest2xMSAA(0, msaa_2x_supported_);
      uint32_t map_1 =
          draw_util::GetD3D10SampleIndexForGuest2xMSAA(1, msaa_2x_supported_);
      append_dump_define(dump_source, "DUMP_2X_SAMPLE_MAP_0", map_0);
      append_dump_define(dump_source, "DUMP_2X_SAMPLE_MAP_1", map_1);
    }

    dump_source.append(kEdramDumpPreamble);
    dump_source.append(kEdramDumpKernelTemplate);

    NS::String* ns_source =
        NS::String::string(dump_source.c_str(), NS::UTF8StringEncoding);
    MTL::Library* lib = device_->newLibrary(ns_source, nullptr, &error);
    if (!lib) {
      XELOGW("Metal: failed to compile {} shader: {}", cfg.kernel_name,
             error ? error->localizedDescription()->utf8String() : "unknown");
      continue;
    }
    NS::String* fn_name =
        NS::String::string(cfg.kernel_name, NS::UTF8StringEncoding);
    MTL::Function* fn = lib->newFunction(fn_name);
    if (!fn) {
      XELOGW("Metal: {} missing entrypoint", cfg.kernel_name);
      lib->release();
      continue;
    }
    *cfg.pipeline = device_->newComputePipelineState(fn, &error);
    fn->release();
    lib->release();
    if (!*cfg.pipeline) {
      XELOGW("Metal: failed to create {} pipeline: {}", cfg.kernel_name,
             error ? error->localizedDescription()->utf8String() : "unknown");
    }
  }

  return true;
}

void MetalRenderTargetCache::ShutdownEdramComputeShaders() {
  if (edram_store_pipeline_) {
    edram_store_pipeline_->release();
    edram_store_pipeline_ = nullptr;
  }
  for (auto& by_bpp : edram_dump_color_pipelines_) {
    for (auto& by_source : by_bpp) {
      for (auto*& pipeline : by_source) {
        if (pipeline) {
          pipeline->release();
          pipeline = nullptr;
        }
      }
    }
  }
  for (auto*& pipeline : edram_dump_depth_pipelines_) {
    if (pipeline) {
      pipeline->release();
      pipeline = nullptr;
    }
  }
  for (auto& by_scaled : resolve_full_pipelines_) {
    for (auto*& pipeline : by_scaled) {
      if (pipeline) {
        pipeline->release();
        pipeline = nullptr;
      }
    }
  }
  for (auto& by_scaled : resolve_fast_pipelines_) {
    for (auto& by_bpp : by_scaled) {
      for (auto*& pipeline : by_bpp) {
        if (pipeline) {
          pipeline->release();
          pipeline = nullptr;
        }
      }
    }
  }
  for (auto& by_bpp : direct_host_resolve_pipelines_) {
    for (auto& by_msaa : by_bpp) {
      for (auto& by_scaled : by_msaa) {
        for (auto*& pipeline : by_scaled) {
          if (pipeline) {
            pipeline->release();
            pipeline = nullptr;
          }
        }
      }
    }
  }
  for (auto& by_msaa : direct_host_color_full_resolve_pipelines_) {
    for (auto& by_scaled : by_msaa) {
      for (auto& by_source : by_scaled) {
        for (auto*& pipeline : by_source) {
          if (pipeline) {
            pipeline->release();
            pipeline = nullptr;
          }
        }
      }
    }
  }
  for (auto& by_msaa : direct_host_depth_resolve_pipelines_) {
    for (auto*& pipeline : by_msaa) {
      if (pipeline) {
        pipeline->release();
        pipeline = nullptr;
      }
    }
  }
  for (size_t i = 0; i < xe::countof(host_depth_store_pipelines_); ++i) {
    if (host_depth_store_pipelines_[i]) {
      host_depth_store_pipelines_[i]->release();
      host_depth_store_pipelines_[i] = nullptr;
    }
  }
}

void MetalRenderTargetCache::MarkRenderPassDescriptorDirty(
    RenderPassDescriptorDirtyReason reason) {
  render_pass_descriptor_dirty_ = true;
  ++telemetry_.render_pass_descriptor_dirty_marks;
  size_t reason_index = static_cast<size_t>(reason);
  if (reason_index < telemetry_.render_pass_descriptor_dirty_reasons.size()) {
    ++telemetry_.render_pass_descriptor_dirty_reasons[reason_index];
  }
}

void MetalRenderTargetCache::ClearCache() {
  ClearPendingDrawPassTransfers();

  // Clear current bindings
  for (uint32_t i = 0; i < 4; ++i) {
    current_color_targets_[i] = nullptr;
  }
  current_depth_target_ = nullptr;
  MarkRenderPassDescriptorDirty(RenderPassDescriptorDirtyReason::kClearCache);

  dummy_color_targets_.clear();
  dummy_color_target_ = nullptr;
  render_target_map_.clear();

  // Call base implementation
  RenderTargetCache::ClearCache();
}

void MetalRenderTargetCache::BeginFrame() {
  (void)FlushPendingDrawPassTransfers();

  ++frame_id_;

  // Call base implementation
  RenderTargetCache::BeginFrame();

}

bool MetalRenderTargetCache::PrepareResolveDestinationBuffer(
    const draw_util::ResolveInfo& resolve_info, bool draw_resolution_scaled,
    ResolveDestinationBuffer& destination) {
  destination = {};
  if (draw_resolution_scaled) {
    auto* texture_cache = command_processor_.texture_cache();
    auto* metal_texture_cache =
        texture_cache ? static_cast<MetalTextureCache*>(texture_cache)
                      : nullptr;
    if (!metal_texture_cache) {
      return false;
    }
    const uint32_t range_length = resolve_info.copy_dest_extent_start -
                                  resolve_info.copy_dest_base +
                                  resolve_info.copy_dest_extent_length;
    return metal_texture_cache->EnsureScaledResolveMemoryCommitted(
               resolve_info.copy_dest_extent_start,
               resolve_info.copy_dest_extent_length) &&
           metal_texture_cache->MakeScaledResolveRangeCurrent(
               resolve_info.copy_dest_base, range_length) &&
           metal_texture_cache->GetCurrentScaledResolveBuffer(
               destination.buffer, destination.offset, destination.length);
  }

  auto* shared = command_processor_.shared_memory();
  destination.buffer = shared ? shared->GetBuffer() : nullptr;
  if (!destination.buffer) {
    return false;
  }
  // TODO(xenios-jp): Move resolve/export destination residency into a
  // command-processor preflight before the render encoder is opened. Keeping it
  // classified here makes the remaining active shared-memory upload breaks
  // visible as resolve_copy_dest telemetry.
  return command_processor_.RequestSharedMemoryRange(
      MetalCommandProcessor::SharedMemoryRequestReason::kResolveCopyDest,
      resolve_info.copy_dest_extent_start, resolve_info.copy_dest_extent_length);
}

MTL::ComputePipelineState* MetalRenderTargetCache::GetResolvePipeline(
    draw_util::ResolveCopyShaderIndex copy_shader, bool scaled) const {
  size_t scaled_index = scaled ? 1u : 0u;
  if (IsResolveDirectHostRTFastCandidate(copy_shader)) {
    return resolve_fast_pipelines_[scaled_index][ResolveFastBppIndex(
        copy_shader)][ResolveFastMsaaIndex(copy_shader)];
  }
  size_t full_dest_index = DirectHostResolveFullDestIndex(copy_shader);
  if (full_dest_index < kResolveFullDestCount) {
    return resolve_full_pipelines_[scaled_index][full_dest_index];
  }
  return nullptr;
}

MTL::ComputePipelineState* MetalRenderTargetCache::GetDirectHostResolvePipeline(
    bool is_64bpp, xenos::MsaaSamples msaa_samples, bool scaled,
    bool source_is_uint) const {
  size_t msaa_index = DirectHostResolveMsaaIndex(msaa_samples);
  if (msaa_index >= kDirectHostResolveMsaaCount) {
    return nullptr;
  }
  return direct_host_resolve_pipelines_[DirectHostResolveBppIndex(
      is_64bpp)][msaa_index][scaled ? 1u : 0u][source_is_uint ? 1u : 0u];
}

MTL::ComputePipelineState*
MetalRenderTargetCache::GetDirectHostColorFullResolvePipeline(
    xenos::MsaaSamples msaa_samples, bool scaled, bool source_is_uint,
    draw_util::ResolveCopyShaderIndex copy_shader) const {
  size_t msaa_index = DirectHostResolveMsaaIndex(msaa_samples);
  size_t dest_index = DirectHostResolveFullDestIndex(copy_shader);
  if (msaa_index >= kDirectHostResolveMsaaCount ||
      dest_index >= kDirectHostResolveFullDestCount) {
    return nullptr;
  }
  return direct_host_color_full_resolve_pipelines_[msaa_index][scaled ? 1u : 0u]
                                                  [source_is_uint ? 1u : 0u]
                                                  [dest_index];
}

MTL::ComputePipelineState*
MetalRenderTargetCache::GetDirectHostDepthResolvePipeline(
    xenos::MsaaSamples msaa_samples, bool scaled) const {
  size_t msaa_index = DirectHostResolveMsaaIndex(msaa_samples);
  if (msaa_index >= kDirectHostResolveMsaaCount) {
    return nullptr;
  }
  return direct_host_depth_resolve_pipelines_[msaa_index][scaled ? 1u : 0u];
}

bool MetalRenderTargetCache::Update(
    bool is_rasterization_done, reg::RB_DEPTHCONTROL normalized_depth_control,
    uint32_t normalized_color_mask, const Shader& vertex_shader) {
  // Pending draw-pass transfers are ownership-visible already. If control
  // reaches another RT update before the command processor encoded them,
  // preserve correctness by falling back to the standalone transfer path first.
  if (!FlushPendingDrawPassTransfers()) {
    return false;
  }

  // Use the base class logic to update the current render target setup.
  if (!RenderTargetCache::Update(is_rasterization_done,
                                 normalized_depth_control,
                                 normalized_color_mask, vertex_shader)) {
    XELOGE("MetalRenderTargetCache::Update - Base class Update failed");
    return false;
  }

  // After base class update, retrieve the actual render targets that were
  // selected This is the KEY to connecting base class management with
  // Metal-specific rendering
  RenderTarget* const* accumulated_targets =
      last_update_accumulated_render_targets();

  // Check if render targets actually changed
  bool targets_changed = false;

  // Check depth target
  MetalRenderTarget* new_depth_target =
      accumulated_targets[0]
          ? static_cast<MetalRenderTarget*>(accumulated_targets[0])
          : nullptr;
  if (new_depth_target != current_depth_target_) {
    targets_changed = true;
    current_depth_target_ = new_depth_target;
    if (current_depth_target_) {
      XELOGD(
          "MetalRenderTargetCache::Update - Depth target changed: key={:08X}",
          current_depth_target_->key().key);
    }
  }

  // Check color targets
  for (uint32_t i = 0; i < xenos::kMaxColorRenderTargets; ++i) {
    MetalRenderTarget* new_color_target =
        accumulated_targets[i + 1]
            ? static_cast<MetalRenderTarget*>(accumulated_targets[i + 1])
            : nullptr;
    if (new_color_target != current_color_targets_[i]) {
      targets_changed = true;
      current_color_targets_[i] = new_color_target;
      if (current_color_targets_[i]) {
        XELOGD(
            "MetalRenderTargetCache::Update - Color target {} changed: "
            "key={:08X}",
            i, current_color_targets_[i]->key().key);
      }
    }
  }

  // Perform ownership transfers - this is critical for correct rendering when
  // EDRAM regions are aliased between different RT configurations.
  // The base class Update() populates last_update_transfers() with the needed
  // transfers based on EDRAM tile overlaps.
  const std::vector<Transfer>* update_transfers = last_update_transfers();
  if (::cvars::metal_transfer_in_draw_pass) {
    std::array<std::vector<Transfer>, 1 + xenos::kMaxColorRenderTargets>
        fallback_transfers;
    bool fallback_transfer_work = false;
    for (uint32_t i = 0; i < 1 + xenos::kMaxColorRenderTargets; ++i) {
      const std::vector<Transfer>& transfers = update_transfers[i];
      if (transfers.empty()) {
        continue;
      }
      ++telemetry_.update_transfer_lists;
      ++telemetry_.pending_draw_pass_transfer_lists;
      telemetry_.pending_draw_pass_transfer_count += transfers.size();
      PendingDrawPassTransferPlan pending_plan_storage;
      DrawPassTransferRejectionReason draw_pass_rejection =
          GetDrawPassTransferRejectionReason(
              i, accumulated_targets, transfers,
              &pending_plan_storage.transfer_rectangles);
      size_t rejection_index = static_cast<size_t>(draw_pass_rejection);
      if (rejection_index < telemetry_.pending_draw_pass_rejections.size()) {
        ++telemetry_.pending_draw_pass_rejections[rejection_index];
      }
      PendingDrawPassTransferPlan& pending_plan =
          pending_draw_pass_transfer_plans_[i];
      pending_plan = std::move(pending_plan_storage);
      pending_plan.render_target = accumulated_targets[i];
      pending_plan.rejection_reason = draw_pass_rejection;
      if (draw_pass_rejection == DrawPassTransferRejectionReason::kNone) {
        ++telemetry_.pending_draw_pass_accepted_lists;
        pending_draw_pass_render_targets_[i] = accumulated_targets[i];
        pending_draw_pass_transfers_[i] = transfers;
        pending_draw_pass_transfer_mask_ |= uint32_t(1) << i;
        pending_draw_pass_preflighted_transfer_mask_ = 0;
        pending_draw_pass_load_dontcare_mask_ = 0;
        pending_plan.full_overwrite =
            PendingDrawPassTransfersFullyOverwriteTarget(
                i, accumulated_targets[i], transfers,
                &pending_plan.transfer_rectangles);
        if (pending_plan.full_overwrite) {
          ++telemetry_.pending_draw_pass_full_overwrite_lists;
          pending_draw_pass_full_overwrite_mask_ |= uint32_t(1) << i;
          MarkRenderPassDescriptorDirty(
              RenderPassDescriptorDirtyReason::kPendingFullOverwriteTransfer);
        }
        auto* dest_metal_rt =
            static_cast<MetalRenderTarget*>(accumulated_targets[i]);
        if (dest_metal_rt->needs_initial_clear()) {
          dest_metal_rt->SetNeedsInitialClear(false);
          MarkRenderPassDescriptorDirty(
              RenderPassDescriptorDirtyReason::kPendingInitialClearConsumed);
        }
      } else {
        ++telemetry_.pending_draw_pass_fallback_lists;
        fallback_transfers[i] = transfers;
        fallback_transfer_work = true;
      }
    }
    if (fallback_transfer_work) {
      PerformTransfersAndResolveClears(
          1 + xenos::kMaxColorRenderTargets, accumulated_targets,
          fallback_transfers.data(), nullptr, nullptr, nullptr);
    }
    if (HasPendingDrawPassTransfers() &&
        !EnsurePendingDrawPassTransfersPreflighted()) {
      if (!FlushPendingDrawPassTransfers()) {
        return false;
      }
    }
  } else {
    for (uint32_t i = 0; i < 1 + xenos::kMaxColorRenderTargets; ++i) {
      if (!update_transfers[i].empty()) {
        ++telemetry_.update_transfer_lists;
      }
    }
    PerformTransfersAndResolveClears(1 + xenos::kMaxColorRenderTargets,
                                     accumulated_targets, update_transfers,
                                     nullptr, nullptr, nullptr);
  }

  // Only mark render pass descriptor as dirty if targets actually changed
  if (targets_changed) {
    MarkRenderPassDescriptorDirty(
        RenderPassDescriptorDirtyReason::kTargetsChanged);
  }

  return true;
}

void MetalRenderTargetCache::ClearPendingDrawPassTransfers() {
  bool load_dontcare_descriptor_used =
      pending_draw_pass_load_dontcare_mask_ != 0;
  for (auto& transfers : pending_draw_pass_transfers_) {
    transfers.clear();
  }
  pending_draw_pass_render_targets_.fill(nullptr);
  pending_draw_pass_transfer_plans_.fill(PendingDrawPassTransferPlan());
  pending_draw_pass_transfer_mask_ = 0;
  pending_draw_pass_full_overwrite_mask_ = 0;
  pending_draw_pass_preflighted_transfer_mask_ = 0;
  pending_draw_pass_load_dontcare_mask_ = 0;
  if (load_dontcare_descriptor_used) {
    MarkRenderPassDescriptorDirty(
        RenderPassDescriptorDirtyReason::kLoadDontCareDescriptorConsumed);
  }
}

MetalRenderTargetCache::DrawPassTransferRejectionReason
MetalRenderTargetCache::GetDrawPassTransferRejectionReason(
    uint32_t render_target_index, RenderTarget* const* render_targets,
    const std::vector<Transfer>& transfers,
    std::vector<TransferRectanglePlan>* transfer_rectangles_out) const {
  if (!render_targets || transfers.empty() ||
      render_target_index > xenos::kMaxColorRenderTargets) {
    return DrawPassTransferRejectionReason::kInvalidTargetSlot;
  }
  auto* dest_metal_rt =
      static_cast<MetalRenderTarget*>(render_targets[render_target_index]);
  if (!dest_metal_rt) {
    return DrawPassTransferRejectionReason::kNoDestination;
  }
  RenderTargetKey dest_key = dest_metal_rt->key();
  if (dest_key.is_depth != (render_target_index == 0)) {
    return DrawPassTransferRejectionReason::kDepthDestination;
  }

  bool dest_is_uint = false;
  MTL::Texture* dest_draw_texture = dest_metal_rt->draw_texture();
  if (dest_key.is_depth) {
    MTL::PixelFormat depth_format =
        GetDepthPixelFormat(dest_key.GetDepthFormat());
    if (!dest_draw_texture ||
        dest_draw_texture->pixelFormat() != depth_format) {
      return DrawPassTransferRejectionReason::kDestinationFormatOrTexture;
    }
  } else {
    MTL::PixelFormat transfer_format = GetColorOwnershipTransferPixelFormat(
        dest_key.GetColorFormat(), &dest_is_uint);
    MTL::PixelFormat draw_format =
        GetColorDrawPixelFormat(dest_key.GetColorFormat());
    if (dest_is_uint || draw_format != transfer_format || !dest_draw_texture ||
        dest_draw_texture != dest_metal_rt->transfer_texture() ||
        dest_draw_texture->pixelFormat() != transfer_format) {
      return DrawPassTransferRejectionReason::kDestinationFormatOrTexture;
    }
  }

  auto is_active_draw_pass_texture = [&](const MetalRenderTarget* rt,
                                         MTL::Texture* texture) -> bool {
    for (uint32_t i = 0; i < 1 + xenos::kMaxColorRenderTargets; ++i) {
      auto* active_rt = static_cast<MetalRenderTarget*>(render_targets[i]);
      if (!active_rt) {
        continue;
      }
      MTL::Texture* active_draw_texture = active_rt->draw_texture();
      if (rt == active_rt || texture == active_draw_texture ||
          (rt && rt->draw_texture() == active_draw_texture)) {
        return true;
      }
    }
    return false;
  };

  for (const Transfer& transfer : transfers) {
    if (!transfer.source) {
      return DrawPassTransferRejectionReason::kUnsupportedSource;
    }
    auto* source_rt = static_cast<MetalRenderTarget*>(transfer.source);
    RenderTargetKey source_key = source_rt->key();
    if (transfer.host_depth_source) {
      if (!dest_key.is_depth) {
        return DrawPassTransferRejectionReason::kUnsupportedSource;
      }
      auto* host_depth_rt =
          static_cast<MetalRenderTarget*>(transfer.host_depth_source);
      if (!host_depth_rt || host_depth_rt == dest_metal_rt) {
        return DrawPassTransferRejectionReason::kHostDepthSelfSource;
      }
      RenderTargetKey host_depth_key = host_depth_rt->key();
      if (!host_depth_key.is_depth) {
        return DrawPassTransferRejectionReason::kUnsupportedSource;
      }
      MTL::Texture* host_depth_texture = host_depth_rt->texture();
      if (!host_depth_texture || host_depth_texture == dest_draw_texture ||
          host_depth_rt->draw_texture() == dest_draw_texture) {
        return DrawPassTransferRejectionReason::kSourceTextureConflict;
      }
      MTL::PixelFormat host_depth_format =
          GetDepthPixelFormat(host_depth_key.GetDepthFormat());
      if (host_depth_texture->pixelFormat() != host_depth_format) {
        return DrawPassTransferRejectionReason::kSourceFormatMismatch;
      }
      if (is_active_draw_pass_texture(host_depth_rt, host_depth_texture)) {
        return DrawPassTransferRejectionReason::
            kHostDepthSourceActiveInDrawPass;
      }
    }
    if (source_rt == dest_metal_rt) {
      return DrawPassTransferRejectionReason::kSelfSource;
    }

    MTL::Texture* source_texture = source_key.is_depth
                                       ? source_rt->texture()
                                       : source_rt->transfer_texture();
    if (!source_texture || source_texture == dest_draw_texture ||
        source_rt->draw_texture() == dest_draw_texture) {
      return DrawPassTransferRejectionReason::kSourceTextureConflict;
    }
    if (source_key.is_depth) {
      MTL::PixelFormat source_depth_format =
          GetDepthPixelFormat(source_key.GetDepthFormat());
      if (source_texture->pixelFormat() != source_depth_format) {
        return DrawPassTransferRejectionReason::kSourceFormatMismatch;
      }
    } else {
      MTL::PixelFormat source_transfer_format =
          GetColorOwnershipTransferPixelFormat(source_key.GetColorFormat(),
                                               nullptr);
      if (source_texture->pixelFormat() != source_transfer_format) {
        return DrawPassTransferRejectionReason::kSourceFormatMismatch;
      }
    }

    if (is_active_draw_pass_texture(source_rt, source_texture)) {
      if (source_key.is_depth && !dest_key.is_depth) {
        return DrawPassTransferRejectionReason::
            kDepthToColorSourceActiveInDrawPass;
      }
      return DrawPassTransferRejectionReason::kSourceActiveInDrawPass;
    }
  }

  std::vector<TransferRectanglePlan> local_transfer_rectangles;
  std::vector<TransferRectanglePlan>& transfer_rectangles =
      transfer_rectangles_out ? *transfer_rectangles_out
                              : local_transfer_rectangles;
  if (!BuildTransferRectanglePlans(dest_key, transfers, nullptr, true,
                                   transfer_rectangles)) {
    return DrawPassTransferRejectionReason::kInvalidRectangles;
  }

  return DrawPassTransferRejectionReason::kNone;
}

bool MetalRenderTargetCache::BuildTransferRectanglePlans(
    RenderTargetKey dest_key, const std::vector<Transfer>& transfers,
    const Transfer::Rectangle* cutout, bool require_all_rectangles,
    std::vector<TransferRectanglePlan>& transfer_rectangles_out) const {
  transfer_rectangles_out.clear();
  transfer_rectangles_out.reserve(transfers.size());
  for (uint32_t transfer_index = 0; transfer_index < transfers.size();
       ++transfer_index) {
    const Transfer& transfer = transfers[transfer_index];
    TransferRectanglePlan plan;
    plan.transfer_index = transfer_index;
    plan.rectangle_count = transfer.GetRectangles(
        dest_key.base_tiles, dest_key.GetPitchTiles(), dest_key.msaa_samples,
        dest_key.Is64bpp(), plan.rectangles.data(), cutout);
    if (!plan.rectangle_count) {
      if (require_all_rectangles) {
        transfer_rectangles_out.clear();
        return false;
      }
      continue;
    }
    transfer_rectangles_out.push_back(plan);
  }
  return true;
}

bool MetalRenderTargetCache::PendingDrawPassTransfersFullyOverwriteTarget(
    uint32_t render_target_index, RenderTarget* render_target,
    const std::vector<Transfer>& transfers,
    const std::vector<TransferRectanglePlan>* transfer_rectangles) const {
  if (!render_target || transfers.empty() ||
      render_target_index > xenos::kMaxColorRenderTargets) {
    return false;
  }

  auto* dest_metal_rt = static_cast<MetalRenderTarget*>(render_target);
  RenderTargetKey dest_key = dest_metal_rt->key();
  if (dest_key.is_depth != (render_target_index == 0)) {
    return false;
  }

  MTL::Texture* dest_texture = dest_metal_rt->draw_texture();
  if (!dest_texture) {
    return false;
  }
  uint32_t dest_width = uint32_t(dest_texture->width());
  uint32_t dest_height = uint32_t(dest_texture->height());
  if (!dest_width || !dest_height) {
    return false;
  }

  auto is_full_target_rectangle =
      [&](const Transfer::Rectangle& rect) -> bool {
    uint32_t scaled_x = rect.x_pixels * draw_resolution_scale_x();
    uint32_t scaled_y = rect.y_pixels * draw_resolution_scale_y();
    uint32_t scaled_width = rect.width_pixels * draw_resolution_scale_x();
    uint32_t scaled_height = rect.height_pixels * draw_resolution_scale_y();
    return !scaled_x && !scaled_y && scaled_width == dest_width &&
           scaled_height == dest_height;
  };

  std::vector<TransferRectanglePlan> local_transfer_rectangles;
  if (!transfer_rectangles) {
    if (!BuildTransferRectanglePlans(dest_key, transfers, nullptr, true,
                                     local_transfer_rectangles)) {
      return false;
    }
    transfer_rectangles = &local_transfer_rectangles;
  }
  if (transfer_rectangles->size() != transfers.size()) {
    return false;
  }
  for (const TransferRectanglePlan& transfer_plan : *transfer_rectangles) {
    if (transfer_plan.rectangle_count != 1 ||
        !is_full_target_rectangle(transfer_plan.rectangles[0])) {
      return false;
    }
  }
  return true;
}

bool MetalRenderTargetCache::GetActiveTransferAttachmentFormats(
    MTL::RenderPassDescriptor* pass_descriptor,
    TransferAttachmentFormats& attachment_formats_out) const {
  attachment_formats_out.color_attachment_formats.fill(
      MTL::PixelFormatInvalid);
  attachment_formats_out.depth_attachment_format = MTL::PixelFormatInvalid;
  attachment_formats_out.stencil_attachment_format = MTL::PixelFormatInvalid;
  if (!pass_descriptor) {
    return false;
  }

  auto* color_attachments = pass_descriptor->colorAttachments();
  if (color_attachments) {
    for (uint32_t i = 0; i < xenos::kMaxColorRenderTargets; ++i) {
      auto* color_attachment = color_attachments->object(i);
      MTL::Texture* texture =
          color_attachment ? color_attachment->texture() : nullptr;
      if (texture) {
        attachment_formats_out.color_attachment_formats[i] =
            texture->pixelFormat();
      }
    }
  }

  if (auto* depth_attachment = pass_descriptor->depthAttachment()) {
    if (MTL::Texture* texture = depth_attachment->texture()) {
      attachment_formats_out.depth_attachment_format = texture->pixelFormat();
    }
  }
  if (auto* stencil_attachment = pass_descriptor->stencilAttachment()) {
    if (MTL::Texture* texture = stencil_attachment->texture()) {
      attachment_formats_out.stencil_attachment_format = texture->pixelFormat();
    }
  }
  return true;
}

bool MetalRenderTargetCache::GetCurrentTransferAttachmentFormats(
    TransferAttachmentFormats& attachment_formats_out) const {
  attachment_formats_out.color_attachment_formats.fill(
      MTL::PixelFormatInvalid);
  attachment_formats_out.depth_attachment_format = MTL::PixelFormatInvalid;
  attachment_formats_out.stencil_attachment_format = MTL::PixelFormatInvalid;

  bool has_color_attachment = false;
  for (uint32_t i = 0; i < xenos::kMaxColorRenderTargets; ++i) {
    MTL::Texture* texture =
        current_color_targets_[i] ? current_color_targets_[i]->draw_texture()
                                  : nullptr;
    if (!texture) {
      continue;
    }
    attachment_formats_out.color_attachment_formats[i] =
        texture->pixelFormat();
    has_color_attachment = true;
  }
  if (!has_color_attachment) {
    attachment_formats_out.color_attachment_formats[0] =
        GetColorDrawPixelFormat(xenos::ColorRenderTargetFormat::k_8_8_8_8);
  }

  MTL::Texture* depth_texture =
      current_depth_target_ ? current_depth_target_->draw_texture() : nullptr;
  if (depth_texture) {
    MTL::PixelFormat depth_pixel_format = depth_texture->pixelFormat();
    attachment_formats_out.depth_attachment_format = depth_pixel_format;
    if (depth_pixel_format == MTL::PixelFormatDepth32Float_Stencil8 ||
        depth_pixel_format == MTL::PixelFormatDepth24Unorm_Stencil8 ||
        depth_pixel_format == MTL::PixelFormatX32_Stencil8) {
      attachment_formats_out.stencil_attachment_format = depth_pixel_format;
    }
  }
  return true;
}

bool MetalRenderTargetCache::PreflightPendingDrawPassTransfers(
    const TransferAttachmentFormats& attachment_formats) {
  if (!HasPendingDrawPassTransfers()) {
    return true;
  }

  // Shared RenderTargetCache state uses slot 0 for depth and slots 1..4 for
  // color, so preflight depth transfers against the active depth attachment.
  for (uint32_t i = 0; i <= xenos::kMaxColorRenderTargets; ++i) {
    if (!(pending_draw_pass_transfer_mask_ & (uint32_t(1) << i))) {
      continue;
    }

    auto* dest_metal_rt =
        static_cast<MetalRenderTarget*>(pending_draw_pass_render_targets_[i]);
    if (!dest_metal_rt || pending_draw_pass_transfers_[i].empty()) {
      return false;
    }

    RenderTargetKey dest_key = dest_metal_rt->key();
    PendingDrawPassTransferPlan& pending_plan =
        pending_draw_pass_transfer_plans_[i];
    if (pending_plan.transfer_rectangles.size() !=
        pending_draw_pass_transfers_[i].size()) {
      if (!BuildTransferRectanglePlans(dest_key, pending_draw_pass_transfers_[i],
                                       nullptr, true,
                                       pending_plan.transfer_rectangles)) {
        return false;
      }
    }
    bool dest_is_uint = false;
    uint32_t color_attachment_index = 0;
    MTL::PixelFormat dest_format = MTL::PixelFormatInvalid;
    if (dest_key.is_depth) {
      if (i != 0) {
        return false;
      }
      dest_format = GetDepthPixelFormat(dest_key.GetDepthFormat());
      MTL::Texture* depth_texture = dest_metal_rt->draw_texture();
      if (!depth_texture || depth_texture->pixelFormat() != dest_format ||
          attachment_formats.depth_attachment_format != dest_format ||
          !GetTransferDepthStencilState(true)) {
        return false;
      }
      if (dest_format == MTL::PixelFormatDepth32Float_Stencil8 ||
          dest_format == MTL::PixelFormatDepth24Unorm_Stencil8) {
        if (attachment_formats.stencil_attachment_format != dest_format ||
            !GetTransferStencilClearState()) {
          return false;
        }
        bool native_stencil_output_ready =
            ::cvars::metal_transfer_native_stencil_output &&
            GetTransferStencilOutputState();
        if (!native_stencil_output_ready) {
          for (uint32_t bit = 0; bit < 8; ++bit) {
            if (!GetTransferStencilBitState(bit)) {
              return false;
            }
          }
        }
      }
      uint32_t sample_count = MsaaSamplesToCount(dest_key.msaa_samples);
      if (!GetOrCreateTransferClearPipeline(
              dest_format, false, true, sample_count, 0,
              &attachment_formats.color_attachment_formats,
              attachment_formats.depth_attachment_format,
              attachment_formats.stencil_attachment_format)) {
        return false;
      }
    } else {
      if (i == 0) {
        return false;
      }
      color_attachment_index = i - 1;
      MTL::Texture* attachment_texture = dest_metal_rt->draw_texture();
      if (!attachment_texture ||
          attachment_texture != dest_metal_rt->draw_texture()) {
        return false;
      }
      dest_format = GetColorOwnershipTransferPixelFormat(
          dest_key.GetColorFormat(), &dest_is_uint);
      if (dest_is_uint ||
          attachment_formats.color_attachment_formats[color_attachment_index] !=
              dest_format ||
          dest_metal_rt->transfer_texture() != attachment_texture ||
          !GetTransferNoDepthStencilState()) {
        return false;
      }
    }

    auto is_active_draw_pass_texture = [&](const MetalRenderTarget* rt,
                                           MTL::Texture* texture) -> bool {
      for (uint32_t active_index = 0;
           active_index <= xenos::kMaxColorRenderTargets; ++active_index) {
        auto* active_rt = static_cast<MetalRenderTarget*>(
            pending_draw_pass_render_targets_[active_index]);
        if (!active_rt) {
          continue;
        }
        MTL::Texture* active_draw_texture = active_rt->draw_texture();
        if (rt == active_rt || texture == active_draw_texture ||
            (rt && rt->draw_texture() == active_draw_texture)) {
          return true;
        }
      }
      return false;
    };

    for (const Transfer& transfer : pending_draw_pass_transfers_[i]) {
      auto* source_rt = static_cast<MetalRenderTarget*>(transfer.source);
      if (!source_rt) {
        return false;
      }
      RenderTargetKey source_key = source_rt->key();
      MetalRenderTarget* host_depth_rt = nullptr;
      RenderTargetKey host_depth_key;
      if (transfer.host_depth_source) {
        host_depth_rt =
            static_cast<MetalRenderTarget*>(transfer.host_depth_source);
        if (!dest_key.is_depth || !host_depth_rt ||
            host_depth_rt == dest_metal_rt) {
          return false;
        }
        host_depth_key = host_depth_rt->key();
        if (!host_depth_key.is_depth) {
          return false;
        }
        MTL::Texture* host_depth_texture = host_depth_rt->texture();
        if (!host_depth_texture ||
            host_depth_texture != host_depth_rt->draw_texture() ||
            host_depth_texture == dest_metal_rt->draw_texture() ||
            host_depth_texture->pixelFormat() !=
                GetDepthPixelFormat(host_depth_key.GetDepthFormat()) ||
            is_active_draw_pass_texture(host_depth_rt, host_depth_texture)) {
          return false;
        }
      }
      if (source_key.is_depth && !GetStencilTextureView(source_rt)) {
        return false;
      }
      bool dest_sample_id_from_sample_default =
          dest_key.msaa_samples != xenos::MsaaSamples::k1X &&
          ::cvars::metal_transfer_msaa_sample_id;
      TransferShaderKey shader_key = GetTransferShaderKey(
          source_key, dest_key, host_depth_rt ? &host_depth_key : nullptr,
          false, false, dest_sample_id_from_sample_default);
      if (!GetOrCreateTransferPipelines(
              shader_key, dest_format, false, false, color_attachment_index,
              &attachment_formats.color_attachment_formats,
              attachment_formats.depth_attachment_format,
              attachment_formats.stencil_attachment_format)) {
        return false;
      }
      if (dest_key.is_depth) {
        TransferShaderKey stencil_shader_key = GetTransferShaderKey(
            source_key, dest_key, nullptr, false, true,
            dest_sample_id_from_sample_default);
        bool native_stencil_output_ready =
            ::cvars::metal_transfer_native_stencil_output &&
            GetTransferStencilOutputState() &&
            GetOrCreateTransferPipelines(
                stencil_shader_key, dest_format, false, true,
                color_attachment_index,
                &attachment_formats.color_attachment_formats,
                attachment_formats.depth_attachment_format,
                attachment_formats.stencil_attachment_format);
        if (!native_stencil_output_ready &&
            !GetOrCreateTransferPipelines(
                stencil_shader_key, dest_format, false, false,
                color_attachment_index,
                &attachment_formats.color_attachment_formats,
                attachment_formats.depth_attachment_format,
                attachment_formats.stencil_attachment_format)) {
          return false;
        }
      }
    }
  }

  return true;
}

bool MetalRenderTargetCache::EnsurePendingDrawPassTransfersPreflighted() {
  if (!HasPendingDrawPassTransfers()) {
    return true;
  }
  if (pending_draw_pass_preflighted_transfer_mask_ ==
      pending_draw_pass_transfer_mask_) {
    return true;
  }

  ++telemetry_.pending_draw_pass_preflight_attempts;
  TransferAttachmentFormats attachment_formats;
  if (!GetCurrentTransferAttachmentFormats(attachment_formats) ||
      !PreflightPendingDrawPassTransfers(attachment_formats)) {
    ++telemetry_.pending_draw_pass_preflight_failures;
    pending_draw_pass_preflighted_transfer_mask_ = 0;
    for (PendingDrawPassTransferPlan& plan : pending_draw_pass_transfer_plans_) {
      plan.preflighted = false;
      plan.load_action_safe = false;
    }
    return false;
  }

  ++telemetry_.pending_draw_pass_preflight_successes;
  pending_draw_pass_preflighted_transfer_mask_ =
      pending_draw_pass_transfer_mask_;
  for (uint32_t i = 0; i <= xenos::kMaxColorRenderTargets; ++i) {
    if (!(pending_draw_pass_transfer_mask_ & (uint32_t(1) << i))) {
      continue;
    }
    PendingDrawPassTransferPlan& plan = pending_draw_pass_transfer_plans_[i];
    plan.attachment_formats = attachment_formats;
    plan.preflighted = true;
    plan.load_action_safe = plan.full_overwrite;
  }
  return true;
}

bool MetalRenderTargetCache::PreflightPendingDrawPassTransfers(
    MTL::RenderPassDescriptor* pass_descriptor) {
  TransferAttachmentFormats attachment_formats;
  if (!GetActiveTransferAttachmentFormats(pass_descriptor,
                                          attachment_formats)) {
    return false;
  }
  return PreflightPendingDrawPassTransfers(attachment_formats);
}

bool MetalRenderTargetCache::BuildCurrentAttachmentPlan(
    uint32_t expected_sample_count, bool fallback_depth_attachment_required,
    AttachmentPlan& plan_out) {
  plan_out = AttachmentPlan();
  plan_out.expected_sample_count = expected_sample_count;
  plan_out.fallback_depth_attachment_required =
      fallback_depth_attachment_required;
  plan_out.coverage_samples = std::max(1u, expected_sample_count);
  plan_out.pending_transfer_mask = pending_draw_pass_transfer_mask_;
  plan_out.full_overwrite_mask = pending_draw_pass_full_overwrite_mask_;

  pending_draw_pass_load_dontcare_mask_ = 0;
  if (HasPendingDrawPassTransfers() &&
      EnsurePendingDrawPassTransfersPreflighted()) {
    for (uint32_t i = 0; i <= xenos::kMaxColorRenderTargets; ++i) {
      if ((pending_draw_pass_transfer_mask_ & (uint32_t(1) << i)) &&
          pending_draw_pass_transfer_plans_[i].load_action_safe) {
        pending_draw_pass_load_dontcare_mask_ |= uint32_t(1) << i;
      }
    }
  }
  plan_out.load_dontcare_mask = pending_draw_pass_load_dontcare_mask_;

  auto update_coverage = [&](MTL::Texture* texture) {
    if (!texture || plan_out.coverage_width) {
      return;
    }
    plan_out.coverage_width = static_cast<uint32_t>(texture->width());
    plan_out.coverage_height = static_cast<uint32_t>(texture->height());
    if (texture->sampleCount() > 0) {
      plan_out.coverage_samples =
          std::max<uint32_t>(plan_out.coverage_samples,
                             static_cast<uint32_t>(texture->sampleCount()));
    }
  };
  auto fill_attachment = [&](AttachmentPlanAttachment& attachment,
                             MetalRenderTarget* render_target,
                             uint32_t pending_index) {
    if (!render_target || !render_target->texture()) {
      return;
    }
    attachment.render_target = render_target;
    attachment.key = render_target->key();
    attachment.texture = render_target->draw_texture();
    attachment.format =
        attachment.texture ? attachment.texture->pixelFormat()
                           : MTL::PixelFormatInvalid;
    attachment.sample_count =
        attachment.texture && attachment.texture->sampleCount()
            ? static_cast<uint32_t>(attachment.texture->sampleCount())
            : 1u;
    attachment.bound = attachment.texture != nullptr;
    attachment.needs_initial_clear = render_target->needs_initial_clear();
    const uint32_t pending_bit = uint32_t(1) << pending_index;
    attachment.pending_transfer =
        (pending_draw_pass_transfer_mask_ & pending_bit) != 0;
    attachment.full_overwrite =
        (pending_draw_pass_full_overwrite_mask_ & pending_bit) != 0;
    attachment.load_action_safe =
        (pending_draw_pass_load_dontcare_mask_ & pending_bit) != 0;
    attachment.previous_contents_needed =
        attachment.bound && !attachment.needs_initial_clear &&
        !attachment.load_action_safe;
    if (attachment.bound) {
      plan_out.has_any_render_target = true;
      update_coverage(attachment.texture);
    }
  };

  fill_attachment(plan_out.depth, current_depth_target_, 0);
  for (uint32_t i = 0; i < xenos::kMaxColorRenderTargets; ++i) {
    fill_attachment(plan_out.colors[i], current_color_targets_[i], i + 1);
    if (plan_out.colors[i].bound) {
      plan_out.has_any_color_target = true;
    }
  }
  return true;
}

bool MetalRenderTargetCache::EncodePendingDrawPassTransfers(
    MTL::RenderCommandEncoder* encoder,
    MTL::RenderPassDescriptor* pass_descriptor,
    DrawPassTransferEncoderMutationMask* mutations_out) {
  if (mutations_out) {
    *mutations_out = kDrawPassTransferEncoderMutationNone;
  }
  if (!HasPendingDrawPassTransfers()) {
    return true;
  }
  ++telemetry_.pending_draw_pass_encode_attempts;
  if (!encoder) {
    ++telemetry_.pending_draw_pass_encode_failures;
    return false;
  }
  if (!PreflightPendingDrawPassTransfers(pass_descriptor)) {
    ++telemetry_.pending_draw_pass_encode_failures;
    return false;
  }

  bool success = PerformTransfersAndResolveClears(
      1 + xenos::kMaxColorRenderTargets,
      pending_draw_pass_render_targets_.data(),
      pending_draw_pass_transfers_.data(), nullptr, nullptr, nullptr, encoder,
      pass_descriptor, mutations_out, pending_draw_pass_transfer_plans_.data());
  if (success) {
    ++telemetry_.pending_draw_pass_encode_successes;
    ClearPendingDrawPassTransfers();
  } else {
    ++telemetry_.pending_draw_pass_encode_failures;
  }
  return success;
}

bool MetalRenderTargetCache::FlushPendingDrawPassTransfers() {
  if (!HasPendingDrawPassTransfers()) {
    return true;
  }
  ++telemetry_.pending_draw_pass_flush_attempts;
  bool success = PerformTransfersAndResolveClears(
      1 + xenos::kMaxColorRenderTargets,
      pending_draw_pass_render_targets_.data(),
      pending_draw_pass_transfers_.data(), nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, pending_draw_pass_transfer_plans_.data());
  if (!success) {
    ++telemetry_.pending_draw_pass_flush_failures;
    return false;
  }
  ++telemetry_.pending_draw_pass_flush_successes;
  ClearPendingDrawPassTransfers();
  return true;
}

MetalRenderTargetCache::TransferShaderKey
MetalRenderTargetCache::GetTransferShaderKey(
    RenderTargetKey source_key, RenderTargetKey dest_key,
    const RenderTargetKey* host_depth_source_key,
    bool host_depth_source_is_copy, bool stencil_bit,
    bool dest_sample_id_from_sample_default) const {
  TransferShaderKey shader_key = {};
  shader_key.source_msaa_samples = source_key.msaa_samples;
  shader_key.dest_msaa_samples = dest_key.msaa_samples;
  shader_key.source_resource_format = source_key.resource_format;
  shader_key.dest_resource_format = dest_key.resource_format;
  shader_key.host_depth_source_msaa_samples = xenos::MsaaSamples::k1X;
  shader_key.host_depth_source_is_copy = 0;

  if (stencil_bit) {
    shader_key.mode = source_key.is_depth ? TransferMode::kDepthToStencilBit
                                          : TransferMode::kColorToStencilBit;
  } else if (dest_key.is_depth) {
    if (host_depth_source_key) {
      shader_key.mode = source_key.is_depth
                            ? TransferMode::kDepthAndHostDepthToDepth
                            : TransferMode::kColorAndHostDepthToDepth;
      shader_key.host_depth_source_is_copy =
          host_depth_source_is_copy ? 1 : 0;
      shader_key.host_depth_source_msaa_samples =
          host_depth_source_is_copy ? xenos::MsaaSamples::k1X
                                    : host_depth_source_key->msaa_samples;
    } else {
      shader_key.mode = source_key.is_depth ? TransferMode::kDepthToDepth
                                            : TransferMode::kColorToDepth;
    }
  } else {
    shader_key.mode = source_key.is_depth ? TransferMode::kDepthToColor
                                          : TransferMode::kColorToColor;
  }

  const TransferModeInfo& mode_info = kTransferModeInfos[size_t(shader_key.mode)];
  bool transfer_use_sample_id = dest_sample_id_from_sample_default;
  if (transfer_use_sample_id) {
    bool source_is_multisample =
        source_key.msaa_samples != xenos::MsaaSamples::k1X;
    bool host_depth_is_multisample =
        mode_info.uses_host_depth &&
        shader_key.host_depth_source_msaa_samples != xenos::MsaaSamples::k1X &&
        !shader_key.host_depth_source_is_copy;
    if (!source_is_multisample && !host_depth_is_multisample) {
      transfer_use_sample_id = false;
    }
  }
  shader_key.dest_sample_id_from_sample = transfer_use_sample_id ? 1u : 0u;
  return shader_key;
}

uint32_t MetalRenderTargetCache::GetMaxRenderTargetWidth() const {
  // Metal maximum texture dimension
  return 16384;
}

uint32_t MetalRenderTargetCache::GetMaxRenderTargetHeight() const {
  // Metal maximum texture dimension
  return 16384;
}

bool MetalRenderTargetCache::IsGammaFormatHostStorageSeparate() const {
  return gamma_render_target_as_unorm16_;
}

void MetalRenderTargetCache::RequestPixelShaderInterlockBarrier() {
  command_processor_.EndRenderEncoder();
  PixelShaderInterlockFullEdramBarrierPlaced();
}

RenderTargetCache::RenderTarget* MetalRenderTargetCache::CreateRenderTarget(
    RenderTargetKey key) {
  // Calculate dimensions
  uint32_t width = key.GetWidth();
  uint32_t height =
      GetRenderTargetHeight(key.pitch_tiles_at_32bpp, key.msaa_samples);

  // Apply resolution scaling
  width *= draw_resolution_scale_x();
  height *= draw_resolution_scale_y();

  // Create Metal render target
  auto* render_target = new MetalRenderTarget(key);

  // Create the texture based on format
  MTL::Texture* texture = nullptr;
  uint32_t samples = 1 << uint32_t(key.msaa_samples);

  if (key.is_depth) {
    texture = CreateDepthTexture(width, height, key.GetDepthFormat(), samples);
  } else {
    texture = CreateColorTexture(width, height, key.GetColorFormat(), samples);
  }

  if (!texture) {
    delete render_target;
    return nullptr;
  }

  render_target->SetTexture(texture);
  if (!key.is_depth) {
    MTL::PixelFormat resource_format =
        GetColorResourcePixelFormat(key.GetColorFormat());
    MTL::PixelFormat draw_format =
        GetColorDrawPixelFormat(key.GetColorFormat());
    MTL::PixelFormat transfer_format =
        GetColorOwnershipTransferPixelFormat(key.GetColorFormat(), nullptr);
    if (draw_format != resource_format) {
      MTL::Texture* draw_view = texture->newTextureView(draw_format);
      if (!draw_view) {
        XELOGE("Failed to create texture view for render target");
      }
      render_target->SetDrawTexture(draw_view);
    }
    if (transfer_format != resource_format) {
      MTL::Texture* transfer_view = texture->newTextureView(transfer_format);
      if (!transfer_view) {
        XELOGE("Failed to create texture view for render target");
      }
      render_target->SetTransferTexture(transfer_view);
    }
    if (render_target->msaa_texture()) {
      if (draw_format != render_target->msaa_texture()->pixelFormat()) {
        MTL::Texture* msaa_draw_view =
            render_target->msaa_texture()->newTextureView(draw_format);
        if (!msaa_draw_view) {
          XELOGE("Failed to create texture view for render target");
        }
        render_target->SetMsaaDrawTexture(msaa_draw_view);
      }
      if (transfer_format != render_target->msaa_texture()->pixelFormat()) {
        MTL::Texture* msaa_transfer_view =
            render_target->msaa_texture()->newTextureView(transfer_format);
        if (!msaa_transfer_view) {
          XELOGE("Failed to create texture view for render target");
        }
        render_target->SetMsaaTransferTexture(msaa_transfer_view);
      }
    }
  }

  // NOTE: Unlike the previous implementation, we do NOT load EDRAM data here.
  // This matches D3D12's approach where:
  // 1. CreateRenderTarget creates an empty texture
  // 2. Data transfer happens via ownership transfers in
  // PerformTransfersAndResolveClears
  // 3. The EDRAM buffer is only used as scratch space for resolves
  //
  // The ownership transfer system (called from Update()) handles copying data
  // between render target textures when EDRAM regions are aliased between
  // different RT configurations.

  // Store in our map for later retrieval
  render_target_map_[key.key] = render_target;

  return render_target;
}

bool MetalRenderTargetCache::IsHostDepthEncodingDifferent(
    xenos::DepthRenderTargetFormat format) const {
  // Metal uses different depth encoding than Xbox 360
  // D24S8 on Xbox 360 vs D32Float_S8 on Metal
  return format == xenos::DepthRenderTargetFormat::kD24S8 ||
         format == xenos::DepthRenderTargetFormat::kD24FS8;
}

void MetalRenderTargetCache::RestoreEdramSnapshot(const void* snapshot) {
  if (!snapshot) {
    return;
  }

  if (IsDrawResolutionScaled()) {
    return;
  }

  if (GetPath() == Path::kPixelShaderInterlock) {
    if (!edram_buffer_) {
      return;
    }

    constexpr size_t kSnapshotSize = xenos::kEdramSizeBytes;
    if (void* edram_contents = edram_buffer_->contents()) {
      std::memcpy(edram_contents, snapshot, kSnapshotSize);
      return;
    }

    MTL::ResourceOptions staging_options =
        MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeWriteCombined;
    MTL::Buffer* staging = device_->newBuffer(kSnapshotSize, staging_options);
    if (!staging) {
      return;
    }
    void* staging_contents = staging->contents();
    if (!staging_contents) {
      staging->release();
      return;
    }
    std::memcpy(staging_contents, snapshot, kSnapshotSize);

    ScopedAutoreleasePool autorelease_pool;
    MTL::CommandBuffer* cmd =
        command_processor_.CreateStandaloneTransferCommandBuffer(
            "XeniaCB reason=edram-snapshot-restore");
    if (!cmd) {
      staging->release();
      return;
    }

    MTL::BlitCommandEncoder* blit = cmd->blitCommandEncoder();
    if (!blit) {
      cmd->release();
      staging->release();
      return;
    }

    blit->copyFromBuffer(staging, 0, edram_buffer_, 0, kSnapshotSize);
    blit->endEncoding();
    command_processor_.CommitStandaloneAndWait(cmd);
    staging->release();
    return;
  }

  RenderTarget* full_edram_rt =
      PrepareFullEdram1280xRenderTargetForSnapshotRestoration(
          xenos::ColorRenderTargetFormat::k_32_FLOAT);
  if (!full_edram_rt) {
    return;
  }

  MetalRenderTarget* metal_rt = static_cast<MetalRenderTarget*>(full_edram_rt);
  MTL::Texture* texture = metal_rt->texture();
  if (!texture) {
    return;
  }

  constexpr uint32_t kPitchTilesAt32bpp = 16;
  constexpr uint32_t kWidth =
      kPitchTilesAt32bpp * xenos::kEdramTileWidthSamples;
  constexpr uint32_t kTileRows = xenos::kEdramTileCount / kPitchTilesAt32bpp;
  constexpr uint32_t kHeight = kTileRows * xenos::kEdramTileHeightSamples;

  size_t staging_size = size_t(kWidth) * size_t(kHeight) * sizeof(uint32_t);
  MTL::Buffer* staging =
      device_->newBuffer(staging_size, MTL::ResourceStorageModeShared);
  if (!staging) {
    return;
  }

  auto* dst_base = static_cast<uint8_t*>(staging->contents());
  const uint8_t* src = static_cast<const uint8_t*>(snapshot);
  uint32_t bytes_per_row = kWidth * sizeof(uint32_t);

  for (uint32_t y_tile = 0; y_tile < kTileRows; ++y_tile) {
    for (uint32_t x_tile = 0; x_tile < kPitchTilesAt32bpp; ++x_tile) {
      uint32_t tile_index = y_tile * kPitchTilesAt32bpp + x_tile;
      const uint8_t* tile_src =
          src + tile_index * xenos::kEdramTileWidthSamples *
                    xenos::kEdramTileHeightSamples * sizeof(uint32_t);

      for (uint32_t sample_row = 0; sample_row < xenos::kEdramTileHeightSamples;
           ++sample_row) {
        uint32_t dst_y = y_tile * xenos::kEdramTileHeightSamples + sample_row;
        uint32_t dst_x = x_tile * xenos::kEdramTileWidthSamples;

        uint8_t* dst_row =
            dst_base + dst_y * bytes_per_row + dst_x * sizeof(uint32_t);
        const uint8_t* src_row = tile_src + sample_row *
                                                xenos::kEdramTileWidthSamples *
                                                sizeof(uint32_t);

        std::memcpy(dst_row, src_row,
                    xenos::kEdramTileWidthSamples * sizeof(uint32_t));
      }
    }
  }

  ScopedAutoreleasePool autorelease_pool;
  MTL::CommandBuffer* cmd =
      command_processor_.CreateStandaloneTransferCommandBuffer(
          "XeniaCB reason=rt-texture-upload");
  if (!cmd) {
    staging->release();
    return;
  }

  MTL::BlitCommandEncoder* blit = cmd->blitCommandEncoder();
  if (!blit) {
    cmd->release();
    staging->release();
    return;
  }

  blit->copyFromBuffer(staging, 0, bytes_per_row, 0,
                       MTL::Size::Make(kWidth, kHeight, 1), texture, 0, 0,
                       MTL::Origin::Make(0, 0, 0));
  blit->endEncoding();
  command_processor_.CommitStandaloneAndWait(cmd);
  staging->release();
  if (metal_rt->needs_initial_clear()) {
    metal_rt->SetNeedsInitialClear(false);
    MarkRenderPassDescriptorDirty(
        RenderPassDescriptorDirtyReason::kRestoreInitialClearConsumed);
  }

  // Seed edram_buffer_ with the restored full-EDRAM render target contents
  // so subsequent DumpRenderTargets and resolve passes see the same initial
  // EDRAM state as D3D12/Vulkan.
  DumpRenderTargets(0, kPitchTilesAt32bpp, kTileRows, kPitchTilesAt32bpp,
                    nullptr, "XeniaEDRAMDumpRestoreUpload");
}

MTL::Texture* MetalRenderTargetCache::CreateColorTexture(
    uint32_t width, uint32_t height, xenos::ColorRenderTargetFormat format,
    uint32_t samples, bool transient_render_target_only,
    bool allow_unpooled_fallback) {
  MTL::PixelFormat resource_format = GetColorResourcePixelFormat(format);
  MTL::PixelFormat draw_format = GetColorDrawPixelFormat(format);
  MTL::PixelFormat transfer_format =
      GetColorOwnershipTransferPixelFormat(format, nullptr);
  bool needs_pixel_format_view =
      draw_format != resource_format || transfer_format != resource_format;

  MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
  desc->setWidth(width);
  desc->setHeight(height ? height : 720);  // Default height if not specified
  desc->setPixelFormat(resource_format);
  desc->setTextureType(samples > 1 ? MTL::TextureType2DMultisample
                                   : MTL::TextureType2D);
  desc->setSampleCount(samples);
  MTL::TextureUsage usage = MTL::TextureUsageRenderTarget;
  if (!transient_render_target_only) {
    usage |= MTL::TextureUsageShaderRead;
  }
  if (needs_pixel_format_view) {
    usage |= MTL::TextureUsagePixelFormatView;
  }
  desc->setUsage(usage);

  MTL::Texture* texture = nullptr;
  bool can_use_memoryless = transient_render_target_only &&
                            !needs_pixel_format_view &&
                            device_->supportsFamily(MTL::GPUFamilyApple1);
  if (can_use_memoryless) {
    // Dummy fallback color targets are transient (load/store don't care) and
    // never sampled - memoryless is optimal on Apple TBDR GPUs.
    desc->setStorageMode(MTL::StorageModeMemoryless);
    texture = device_->newTexture(desc);
  }
  if (!texture) {
    desc->setStorageMode(MTL::StorageModePrivate);
    if (render_target_heap_pool_ && !can_use_memoryless) {
      texture = render_target_heap_pool_->CreateTexture(desc);
    }
    if (!texture && (!render_target_heap_pool_ || allow_unpooled_fallback)) {
      texture = device_->newTexture(desc);
    }
  }
  desc->release();
  // Initial clear is handled on first bind via load actions; avoid
  // synchronous clears here to keep the host RT path fast.
  return texture;
}

MTL::Texture* MetalRenderTargetCache::CreateDepthTexture(
    uint32_t width, uint32_t height, xenos::DepthRenderTargetFormat format,
    uint32_t samples) {
  MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
  desc->setWidth(width);
  desc->setHeight(height ? height : 720);  // Default height if not specified
  MTL::PixelFormat pixel_format = GetDepthPixelFormat(format);
  desc->setPixelFormat(pixel_format);
  desc->setTextureType(samples > 1 ? MTL::TextureType2DMultisample
                                   : MTL::TextureType2D);
  desc->setSampleCount(samples);
  MTL::TextureUsage usage =
      MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead;
  if (pixel_format == MTL::PixelFormatDepth32Float_Stencil8 ||
      pixel_format == MTL::PixelFormatDepth24Unorm_Stencil8) {
    usage |= MTL::TextureUsagePixelFormatView;
  }
  desc->setUsage(usage);
  desc->setStorageMode(MTL::StorageModePrivate);

  MTL::Texture* texture = nullptr;
  if (render_target_heap_pool_) {
    texture = render_target_heap_pool_->CreateTexture(desc);
  }
  if (!texture) {
    texture = device_->newTexture(desc);
  }
  desc->release();
  // Initial clear is handled on first bind via load actions; avoid
  // synchronous clears here to keep the host RT path fast.
  return texture;
}

MTL::Texture* MetalRenderTargetCache::CreateTransientDepthTexture(
    uint32_t width, uint32_t height, uint32_t samples) {
  if (!device_) {
    return nullptr;
  }

  MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
  desc->setWidth(std::max(1u, width));
  desc->setHeight(std::max(1u, height));
  desc->setPixelFormat(MTL::PixelFormatDepth32Float);
  desc->setTextureType(samples > 1 ? MTL::TextureType2DMultisample
                                   : MTL::TextureType2D);
  desc->setSampleCount(std::max(1u, samples));
  desc->setUsage(MTL::TextureUsageRenderTarget);

  MTL::Texture* texture = nullptr;
  bool can_use_memoryless = device_->supportsFamily(MTL::GPUFamilyApple1);
  if (can_use_memoryless) {
    desc->setStorageMode(MTL::StorageModeMemoryless);
    texture = device_->newTexture(desc);
  }
  if (!texture) {
    desc->setStorageMode(MTL::StorageModePrivate);
    if (render_target_heap_pool_ && !can_use_memoryless) {
      texture = render_target_heap_pool_->CreateTexture(desc);
    }
    if (!texture) {
      texture = device_->newTexture(desc);
    }
  }

  desc->release();
  return texture;
}

MTL::PixelFormat MetalRenderTargetCache::GetColorResourcePixelFormat(
    xenos::ColorRenderTargetFormat format) const {
  switch (format) {
    case xenos::ColorRenderTargetFormat::k_8_8_8_8:
      return MTL::PixelFormatRGBA8Unorm;
    case xenos::ColorRenderTargetFormat::k_8_8_8_8_GAMMA:
      // Updated to suport unorm16 for gamma render targets.
      return gamma_render_target_as_unorm16_ ? MTL::PixelFormatRGBA16Unorm
                                             : MTL::PixelFormatRGBA8Unorm;
    case xenos::ColorRenderTargetFormat::k_2_10_10_10:
    case xenos::ColorRenderTargetFormat::k_2_10_10_10_AS_10_10_10_10:
      return MTL::PixelFormatRGB10A2Unorm;
    case xenos::ColorRenderTargetFormat::k_2_10_10_10_FLOAT:
    case xenos::ColorRenderTargetFormat::k_2_10_10_10_FLOAT_AS_16_16_16_16:
      // Match D3D12 behavior: store as RGBA16F and pack to float10 on dump.
      return MTL::PixelFormatRGBA16Float;
    case xenos::ColorRenderTargetFormat::k_16_16:
      return MTL::PixelFormatRG16Snorm;
    case xenos::ColorRenderTargetFormat::k_16_16_16_16:
      return MTL::PixelFormatRGBA16Snorm;
    case xenos::ColorRenderTargetFormat::k_16_16_FLOAT:
      return MTL::PixelFormatRG16Float;
    case xenos::ColorRenderTargetFormat::k_16_16_16_16_FLOAT:
      return MTL::PixelFormatRGBA16Float;
    case xenos::ColorRenderTargetFormat::k_32_FLOAT:
      return MTL::PixelFormatR32Float;
    case xenos::ColorRenderTargetFormat::k_32_32_FLOAT:
      return MTL::PixelFormatRG32Float;
    default:
      XELOGE("MetalRenderTargetCache: Unsupported color format {}",
             static_cast<uint32_t>(format));
      return MTL::PixelFormatRGBA8Unorm;
  }
}

MTL::PixelFormat MetalRenderTargetCache::GetColorDrawPixelFormat(
    xenos::ColorRenderTargetFormat format) const {
  switch (format) {
    case xenos::ColorRenderTargetFormat::k_16_16:
      return MTL::PixelFormatRG16Snorm;
    case xenos::ColorRenderTargetFormat::k_16_16_16_16:
      return MTL::PixelFormatRGBA16Snorm;
    case xenos::ColorRenderTargetFormat::k_16_16_FLOAT:
      return MTL::PixelFormatRG16Float;
    case xenos::ColorRenderTargetFormat::k_16_16_16_16_FLOAT:
      return MTL::PixelFormatRGBA16Float;
    case xenos::ColorRenderTargetFormat::k_32_FLOAT:
      return MTL::PixelFormatR32Float;
    case xenos::ColorRenderTargetFormat::k_32_32_FLOAT:
      return MTL::PixelFormatRG32Float;
    default:
      return GetColorResourcePixelFormat(format);
  }
}

MTL::PixelFormat MetalRenderTargetCache::GetColorOwnershipTransferPixelFormat(
    xenos::ColorRenderTargetFormat format, bool* is_integer_out) const {
  if (is_integer_out) {
    *is_integer_out = true;
  }
  switch (format) {
    case xenos::ColorRenderTargetFormat::k_16_16:
    case xenos::ColorRenderTargetFormat::k_16_16_FLOAT:
      return MTL::PixelFormatRG16Uint;
    case xenos::ColorRenderTargetFormat::k_16_16_16_16:
    case xenos::ColorRenderTargetFormat::k_16_16_16_16_FLOAT:
      return MTL::PixelFormatRGBA16Uint;
    case xenos::ColorRenderTargetFormat::k_32_FLOAT:
      return MTL::PixelFormatR32Uint;
    case xenos::ColorRenderTargetFormat::k_32_32_FLOAT:
      return MTL::PixelFormatRG32Uint;
    default:
      if (is_integer_out) {
        *is_integer_out = false;
      }
      // Ownership transfers must use a linear resource view to avoid
      // implicit sRGB conversion for gamma render targets.
      return GetColorResourcePixelFormat(format);
  }
}

MTL::PixelFormat MetalRenderTargetCache::GetDepthPixelFormat(
    xenos::DepthRenderTargetFormat format) const {
  switch (format) {
    case xenos::DepthRenderTargetFormat::kD24S8:
    case xenos::DepthRenderTargetFormat::kD24FS8:
      // Metal doesn't have D24S8, use D32Float_S8
      return MTL::PixelFormatDepth32Float_Stencil8;
    default:
      XELOGE("MetalRenderTargetCache: Unsupported depth format {}",
             static_cast<uint32_t>(format));
      return MTL::PixelFormatDepth32Float_Stencil8;
  }
}

MTL::Texture* MetalRenderTargetCache::GetStencilTextureView(
    MetalRenderTarget* render_target) {
  if (!render_target) {
    return nullptr;
  }
  if (render_target->stencil_view()) {
    return render_target->stencil_view();
  }
  RenderTargetKey key = render_target->key();
  if (!key.is_depth) {
    return nullptr;
  }
  MTL::Texture* depth_texture = render_target->texture();
  if (!depth_texture) {
    return nullptr;
  }
  MTL::Texture* view =
      depth_texture->newTextureView(MTL::PixelFormatX32_Stencil8);
  if (view) {
    render_target->SetStencilView(view);
  }
  return view;
}

MTL::RenderPassDescriptor* MetalRenderTargetCache::GetRenderPassDescriptor(
    uint32_t expected_sample_count,
    bool fallback_depth_attachment_required) {
  ++telemetry_.render_pass_descriptor_requests;
  if (!render_pass_descriptor_dirty_ && cached_render_pass_descriptor_ &&
      cached_render_pass_descriptor_sample_count_ == expected_sample_count &&
      cached_render_pass_descriptor_fallback_depth_required_ ==
          fallback_depth_attachment_required) {
    ++telemetry_.render_pass_descriptor_cache_hits;
    return cached_render_pass_descriptor_;
  }
  if (cached_render_pass_descriptor_sample_count_ != expected_sample_count ||
      cached_render_pass_descriptor_fallback_depth_required_ !=
          fallback_depth_attachment_required) {
    MarkRenderPassDescriptorDirty(
        RenderPassDescriptorDirtyReason::kSampleCountOrFallbackChanged);
  }
  ++telemetry_.render_pass_descriptor_rebuilds;

  // Release old descriptor
  if (cached_render_pass_descriptor_) {
    cached_render_pass_descriptor_->release();
    cached_render_pass_descriptor_ = nullptr;
  }

  // Create new descriptor
  cached_render_pass_descriptor_ =
      MTL::RenderPassDescriptor::renderPassDescriptor();
  if (!cached_render_pass_descriptor_) {
    XELOGE("MetalRenderTargetCache: Failed to create render pass descriptor");
    return nullptr;
  }
  cached_render_pass_descriptor_->retain();
  cached_render_pass_descriptor_sample_count_ = expected_sample_count;
  cached_render_pass_descriptor_fallback_depth_required_ =
      fallback_depth_attachment_required;
  render_pass_descriptor_dirty_ = false;
  if (::cvars::metal_tile_direct_host_resolve) {
    cached_render_pass_descriptor_->setTileWidth(
        kTileDirectHostResolveTileWidth);
    cached_render_pass_descriptor_->setTileHeight(
        kTileDirectHostResolveTileHeight);
  }

  AttachmentPlan attachment_plan;
  if (!BuildCurrentAttachmentPlan(expected_sample_count,
                                  fallback_depth_attachment_required,
                                  attachment_plan)) {
    return nullptr;
  }
  bool has_any_render_target = attachment_plan.has_any_render_target;
  bool has_any_color_target = attachment_plan.has_any_color_target;
  uint32_t coverage_width = attachment_plan.coverage_width;
  uint32_t coverage_height = attachment_plan.coverage_height;
  uint32_t coverage_samples = attachment_plan.coverage_samples;

  // Bind the actual render targets retrieved from base class in Update()

  // Bind depth target if present
  const AttachmentPlanAttachment& depth_plan = attachment_plan.depth;
  if (depth_plan.bound && depth_plan.render_target && depth_plan.texture) {
    auto* depth_attachment = cached_render_pass_descriptor_->depthAttachment();
    depth_attachment->setTexture(depth_plan.texture);

    // Clear on first bind to avoid synchronous clears at creation.
    bool depth_needs_clear = depth_plan.needs_initial_clear;
    bool depth_load_dontcare = depth_plan.load_action_safe;
    AttachmentLoadStoreActions depth_load_store =
        GetRealAttachmentLoadStoreActions(depth_needs_clear,
                                          !depth_load_dontcare);
    SetAttachmentLoadStoreActions(depth_attachment, depth_load_store);
    if (depth_needs_clear) {
      depth_attachment->setClearDepth(GetDepthTargetClearDepth());
      depth_plan.render_target->SetNeedsInitialClear(false);
      MarkRenderPassDescriptorDirty(
          RenderPassDescriptorDirtyReason::
              kDescriptorDepthInitialClearConsumed);
    }

    // If the depth texture includes stencil, bind the same texture to the
    // stencil attachment too (Metal requires explicit stencil attachment
    // binding to match pipeline state).
    MTL::PixelFormat depth_pixel_format = depth_plan.texture->pixelFormat();
    if (depth_pixel_format == MTL::PixelFormatDepth32Float_Stencil8 ||
        depth_pixel_format == MTL::PixelFormatDepth24Unorm_Stencil8 ||
        depth_pixel_format == MTL::PixelFormatX32_Stencil8) {
      auto* stencil_attachment =
          cached_render_pass_descriptor_->stencilAttachment();
      stencil_attachment->setTexture(depth_plan.texture);
      AttachmentLoadStoreActions stencil_load_store = depth_load_store;
      if (!depth_needs_clear && depth_load_dontcare) {
        stencil_load_store = {MTL::LoadActionClear, MTL::StoreActionStore};
      }
      SetAttachmentLoadStoreActions(stencil_attachment, stencil_load_store);
      if (depth_needs_clear || (!depth_needs_clear && depth_load_dontcare)) {
        stencil_attachment->setClearStencil(0);
      }
    }

    has_any_render_target = true;

    // Track this as a real render target for capture
    last_real_depth_target_ = depth_plan.render_target;
  }

  // Bind color targets
  for (uint32_t i = 0; i < 4; ++i) {
    const AttachmentPlanAttachment& color_plan = attachment_plan.colors[i];
    if (color_plan.bound && color_plan.render_target && color_plan.texture) {
      auto* color_attachment =
          cached_render_pass_descriptor_->colorAttachments()->object(i);
      color_attachment->setTexture(color_plan.texture);

      // Clear on first bind to avoid synchronous clears at creation.
      bool color_needs_clear = color_plan.needs_initial_clear;
      bool color_load_dontcare = color_plan.load_action_safe;
      AttachmentLoadStoreActions color_load_store =
          GetRealAttachmentLoadStoreActions(color_needs_clear,
                                            !color_load_dontcare);
      SetAttachmentLoadStoreActions(color_attachment, color_load_store);
      if (color_needs_clear) {
        color_attachment->setClearColor(
            MTL::ClearColor::Make(0.0, 0.0, 0.0, 0.0));
        color_plan.render_target->SetNeedsInitialClear(false);
        MarkRenderPassDescriptorDirty(
            RenderPassDescriptorDirtyReason::
                kDescriptorColorInitialClearConsumed);
      }

      has_any_render_target = true;
      has_any_color_target = true;

      // Track this as a real render target for capture
      last_real_color_targets_[i] = color_plan.render_target;
    }
  }

  // If no color render targets are bound, attach a dummy color target so Metal
  // has at least one color attachment. This mirrors the D3D12/Vulkan behavior
  // where an RTV is always bound when drawing, and also keeps pipeline state
  // validation happy for depth-only passes.
  if (!has_any_color_target) {
    xenos::ColorRenderTargetFormat fmt =
        xenos::ColorRenderTargetFormat::k_8_8_8_8;
    uint32_t samples = std::max(1u, expected_sample_count);

    uint32_t width = 1280;
    uint32_t height = 720;
    if (current_depth_target_ && current_depth_target_->texture()) {
      width = static_cast<uint32_t>(current_depth_target_->texture()->width());
      height =
          static_cast<uint32_t>(current_depth_target_->texture()->height());
      if (current_depth_target_->texture()->sampleCount() > 0) {
        samples = std::max<uint32_t>(
            samples, static_cast<uint32_t>(
                         current_depth_target_->texture()->sampleCount()));
      }
    } else if (last_real_color_targets_[0] &&
               last_real_color_targets_[0]->texture()) {
      width = static_cast<uint32_t>(
          last_real_color_targets_[0]->texture()->width());
      height = static_cast<uint32_t>(
          last_real_color_targets_[0]->texture()->height());
    } else if (last_real_depth_target_ && last_real_depth_target_->texture()) {
      width =
          static_cast<uint32_t>(last_real_depth_target_->texture()->width());
      height =
          static_cast<uint32_t>(last_real_depth_target_->texture()->height());
      if (last_real_depth_target_->texture()->sampleCount() > 0) {
        samples = std::max<uint32_t>(
            samples, static_cast<uint32_t>(
                         last_real_depth_target_->texture()->sampleCount()));
      }
    }

    uint32_t dummy_sample_count =
        samples >= 4u ? 4u : (samples == 2u ? 2u : 1u);
    // Cache dummy color targets by shape/format only so depth-only passes with
    // changing EDRAM bases can reuse the same transient attachment.
    uint64_t dummy_key = uint64_t(width & 0xFFFFu) |
                         (uint64_t(height & 0xFFFFu) << 16) |
                         (uint64_t(dummy_sample_count & 0xFFu) << 32) |
                         (uint64_t(uint32_t(fmt) & 0xFFFFu) << 40);
    auto evict_oldest_dummy_target = [&](uint64_t keep_key) -> bool {
      uint64_t oldest_key = 0;
      uint64_t oldest_frame = frame_id_;
      bool found = false;
      for (const auto& it : dummy_color_targets_) {
        if (it.first == keep_key) {
          continue;
        }
        if (!found || it.second.last_used_frame < oldest_frame) {
          oldest_frame = it.second.last_used_frame;
          oldest_key = it.first;
          found = true;
        }
      }
      if (found) {
        dummy_color_targets_.erase(oldest_key);
      }
      return found;
    };

    auto& entry = dummy_color_targets_[dummy_key];
    if (!entry.target || !entry.target->texture()) {
      RenderTargetKey dummy_rt_key;
      dummy_rt_key.key = 0;
      dummy_rt_key.is_depth = 0;
      dummy_rt_key.resource_format = uint32_t(fmt);
      dummy_rt_key.msaa_samples =
          dummy_sample_count >= 4u   ? xenos::MsaaSamples::k4X
          : dummy_sample_count == 2u ? xenos::MsaaSamples::k2X
                                     : xenos::MsaaSamples::k1X;
      entry.target = std::make_unique<MetalRenderTarget>(dummy_rt_key);
      entry.last_cleared_frame = frame_id_ - 1;
      MTL::Texture* tex =
          CreateColorTexture(width, height, fmt, dummy_sample_count,
                             /*transient_render_target_only=*/true,
                             /*allow_unpooled_fallback=*/false);
      while (!tex && render_target_heap_pool_ &&
             dummy_color_targets_.size() > 1 &&
             evict_oldest_dummy_target(dummy_key)) {
        tex = CreateColorTexture(width, height, fmt, dummy_sample_count,
                                 /*transient_render_target_only=*/true,
                                 /*allow_unpooled_fallback=*/false);
      }
      if (!tex) {
        static uint64_t last_unpooled_fallback_log_frame = 0;
        if (render_target_heap_pool_ &&
            last_unpooled_fallback_log_frame != frame_id_) {
          XELOGW(
              "Metal RT dummy target: heap allocation failed for {}x{} {}x; "
              "falling back to unpooled texture",
              width, height, dummy_sample_count);
          last_unpooled_fallback_log_frame = frame_id_;
        }
        tex = CreateColorTexture(width, height, fmt, dummy_sample_count,
                                 /*transient_render_target_only=*/true,
                                 /*allow_unpooled_fallback=*/true);
      }
      entry.target->SetTexture(tex);
      if (tex) {
        MTL::PixelFormat resource_format = GetColorResourcePixelFormat(fmt);
        MTL::PixelFormat draw_format = GetColorDrawPixelFormat(fmt);
        MTL::PixelFormat transfer_format =
            GetColorOwnershipTransferPixelFormat(fmt, nullptr);
        if (draw_format != resource_format) {
          MTL::Texture* draw_view = tex->newTextureView(draw_format);
          if (!draw_view) {
            XELOGE("Failed to create texture view for render target");
          }
          entry.target->SetDrawTexture(draw_view);
        }
        if (transfer_format != resource_format) {
          MTL::Texture* transfer_view = tex->newTextureView(transfer_format);
          if (!transfer_view) {
            XELOGE("Failed to create texture view for render target");
          }
          entry.target->SetTransferTexture(transfer_view);
        }
      }
    }

    entry.last_used_frame = frame_id_;
    dummy_color_target_ = entry.target.get();

    // Keep this cache small - dummy targets are transient fallback attachments.
    constexpr size_t kMaxDummyColorTargets = 8;
    while (dummy_color_targets_.size() > kMaxDummyColorTargets) {
      if (!evict_oldest_dummy_target(dummy_key)) {
        break;
      }
    }

    auto* color_attachment =
        cached_render_pass_descriptor_->colorAttachments()->object(0);
    color_attachment->setTexture(dummy_color_target_->draw_texture());
    SetAttachmentLoadStoreActions(color_attachment,
                                  GetTransientAttachmentLoadStoreActions());

    has_any_render_target = true;
    if (!coverage_width && dummy_color_target_->draw_texture()) {
      coverage_width =
          static_cast<uint32_t>(dummy_color_target_->draw_texture()->width());
      coverage_height =
          static_cast<uint32_t>(dummy_color_target_->draw_texture()->height());
      if (dummy_color_target_->draw_texture()->sampleCount() > 0) {
        coverage_samples = std::max<uint32_t>(
            coverage_samples,
            static_cast<uint32_t>(
                dummy_color_target_->draw_texture()->sampleCount()));
      }
    }
  }

  if (fallback_depth_attachment_required && !current_depth_target_) {
    uint32_t width = coverage_width ? coverage_width : 1280;
    uint32_t height = coverage_height ? coverage_height : 720;
    uint32_t samples = std::max(1u, coverage_samples);
    MTL::Texture* fallback_depth_texture =
        CreateTransientDepthTexture(width, height, samples);
    if (!fallback_depth_texture) {
      XELOGE(
          "MetalRenderTargetCache: Failed to create transient depth "
          "attachment");
      cached_render_pass_descriptor_->release();
      cached_render_pass_descriptor_ = nullptr;
      MarkRenderPassDescriptorDirty(
          RenderPassDescriptorDirtyReason::kFallbackDepthCreateFailed);
      return nullptr;
    }

    auto* depth_attachment = cached_render_pass_descriptor_->depthAttachment();
    depth_attachment->setTexture(fallback_depth_texture);
    SetAttachmentLoadStoreActions(depth_attachment,
                                  GetTransientAttachmentLoadStoreActions());
    fallback_depth_texture->release();

    has_any_render_target = true;
  }

  return cached_render_pass_descriptor_;
}

bool MetalRenderTargetCache::IsRenderPassDescriptorCompatible(
    MTL::RenderPassDescriptor* pass_descriptor,
    uint32_t expected_sample_count,
    bool fallback_depth_attachment_required) const {
  RenderPassCompatibilityReason reason;
  if (pass_descriptor && pass_descriptor == cached_render_pass_descriptor_ &&
      !render_pass_descriptor_dirty_ &&
      cached_render_pass_descriptor_sample_count_ == expected_sample_count &&
      cached_render_pass_descriptor_fallback_depth_required_ ==
          fallback_depth_attachment_required) {
    reason = RenderPassCompatibilityReason::kCompatible;
  } else {
    reason = GetRenderPassDescriptorCompatibilityReason(
        pass_descriptor, expected_sample_count,
        fallback_depth_attachment_required);
  }
  ++telemetry_.render_pass_compatibility_checks;
  size_t reason_index = static_cast<size_t>(reason);
  if (reason_index < telemetry_.render_pass_compatibility_reasons.size()) {
    ++telemetry_.render_pass_compatibility_reasons[reason_index];
  }
  return reason == RenderPassCompatibilityReason::kCompatible;
}

MetalRenderTargetCache::RenderPassCompatibilityReason
MetalRenderTargetCache::GetRenderPassDescriptorCompatibilityReason(
    MTL::RenderPassDescriptor* pass_descriptor,
    uint32_t expected_sample_count,
    bool fallback_depth_attachment_required) const {
  (void)expected_sample_count;
  if (!pass_descriptor) {
    return RenderPassCompatibilityReason::kNullDescriptor;
  }

  MTL::Texture* expected_depth =
      current_depth_target_ ? current_depth_target_->draw_texture() : nullptr;
  auto* depth_attachment = pass_descriptor->depthAttachment();
  auto* stencil_attachment = pass_descriptor->stencilAttachment();
  if (expected_depth) {
    if (!depth_attachment || depth_attachment->texture() != expected_depth) {
      return RenderPassCompatibilityReason::kDepthAttachmentMismatch;
    }
    MTL::PixelFormat depth_pixel_format = expected_depth->pixelFormat();
    bool expects_stencil =
        depth_pixel_format == MTL::PixelFormatDepth32Float_Stencil8 ||
        depth_pixel_format == MTL::PixelFormatDepth24Unorm_Stencil8 ||
        depth_pixel_format == MTL::PixelFormatX32_Stencil8;
    if (expects_stencil) {
      if (!stencil_attachment ||
          stencil_attachment->texture() != expected_depth) {
        return RenderPassCompatibilityReason::kStencilAttachmentMismatch;
      }
    } else if (stencil_attachment && stencil_attachment->texture()) {
      return RenderPassCompatibilityReason::kUnexpectedStencilAttachment;
    }
  } else {
    MTL::Texture* depth_texture =
        depth_attachment ? depth_attachment->texture() : nullptr;
    MTL::Texture* stencil_texture =
        stencil_attachment ? stencil_attachment->texture() : nullptr;
    if (fallback_depth_attachment_required) {
      if (!depth_texture ||
          depth_texture->pixelFormat() != MTL::PixelFormatDepth32Float ||
          stencil_texture) {
        return RenderPassCompatibilityReason::
            kFallbackDepthAttachmentMismatch;
      }
    } else if (depth_texture || stencil_texture) {
      return RenderPassCompatibilityReason::
          kUnexpectedDepthStencilAttachment;
    }
  }

  auto* color_attachments = pass_descriptor->colorAttachments();
  bool has_current_color_target = false;
  for (uint32_t i = 0; i < xenos::kMaxColorRenderTargets; ++i) {
    if (current_color_targets_[i] &&
        current_color_targets_[i]->draw_texture()) {
      has_current_color_target = true;
      break;
    }
  }
  for (uint32_t i = 0; i < xenos::kMaxColorRenderTargets; ++i) {
    auto* color_attachment = color_attachments->object(i);
    MTL::Texture* expected_color =
        current_color_targets_[i] ? current_color_targets_[i]->draw_texture()
                                  : nullptr;
    if (expected_color) {
      if (!color_attachment || color_attachment->texture() != expected_color) {
        return RenderPassCompatibilityReason::kColorAttachmentMismatch;
      }
      continue;
    }
    if (color_attachment && color_attachment->texture()) {
      if (!has_current_color_target && i == 0) {
        continue;
      }
      return RenderPassCompatibilityReason::kUnexpectedColorAttachment;
    }
  }

  if (has_current_color_target) {
    return RenderPassCompatibilityReason::kCompatible;
  }

  MTL::Texture* expected_dummy =
      dummy_color_target_ ? dummy_color_target_->draw_texture() : nullptr;
  auto* color_attachment_0 = color_attachments->object(0);
  if (expected_dummy && color_attachment_0 &&
      color_attachment_0->texture() == expected_dummy) {
    return RenderPassCompatibilityReason::kCompatible;
  }
  return RenderPassCompatibilityReason::kDummyColorAttachmentMismatch;
}

MTL::Texture* MetalRenderTargetCache::GetColorTarget(uint32_t index) const {
  if (index >= 4 || !current_color_targets_[index]) {
    return nullptr;
  }
  return current_color_targets_[index]->texture();
}

MTL::Texture* MetalRenderTargetCache::GetDepthTarget() const {
  if (!current_depth_target_) {
    return nullptr;
  }
  return current_depth_target_->texture();
}

MTL::Texture* MetalRenderTargetCache::GetDummyColorTarget() const {
  if (dummy_color_target_ && dummy_color_target_->texture()) {
    return dummy_color_target_->texture();
  }
  return nullptr;
}

MetalRenderTargetCache::MetalRenderTarget*
MetalRenderTargetCache::GetColorRenderTarget(uint32_t index) const {
  if (index >= 4) {
    return nullptr;
  }
  return current_color_targets_[index];
}

MTL::Texture* MetalRenderTargetCache::GetColorTargetForDraw(
    uint32_t index) const {
  if (index >= 4 || !current_color_targets_[index]) {
    return nullptr;
  }
  return current_color_targets_[index]->draw_texture();
}

MTL::Texture* MetalRenderTargetCache::GetDepthTargetForDraw() const {
  if (!current_depth_target_) {
    return nullptr;
  }
  return current_depth_target_->draw_texture();
}

MTL::Texture* MetalRenderTargetCache::GetDummyColorTargetForDraw() const {
  if (dummy_color_target_ && dummy_color_target_->draw_texture()) {
    return dummy_color_target_->draw_texture();
  }
  return nullptr;
}

double MetalRenderTargetCache::GetDepthTargetClearDepth() const {
  if (!current_depth_target_) {
    return 1.0;
  }
  return current_depth_target_->key().GetDepthFormat() ==
                 xenos::DepthRenderTargetFormat::kD24FS8
             ? 0.0
             : 1.0;
}

MTL::Texture* MetalRenderTargetCache::GetLastRealColorTarget(
    uint32_t index) const {
  if (index >= 4 || !last_real_color_targets_[index]) {
    return nullptr;
  }
  return last_real_color_targets_[index]->texture();
}

MTL::Texture* MetalRenderTargetCache::GetLastRealDepthTarget() const {
  if (!last_real_depth_target_) {
    return nullptr;
  }
  return last_real_depth_target_->texture();
}

MTL::Texture* MetalRenderTargetCache::GetRenderTargetTexture(
    RenderTargetKey key) const {
  auto it = render_target_map_.find(key.key);
  if (it == render_target_map_.end()) {
    return nullptr;
  }
  MetalRenderTarget* target = it->second;
  return target ? target->texture() : nullptr;
}

MTL::Texture* MetalRenderTargetCache::GetColorRenderTargetTexture(
    uint32_t pitch, xenos::MsaaSamples samples, uint32_t base,
    xenos::ColorRenderTargetFormat format) const {
  if (!pitch) {
    return nullptr;
  }
  RenderTargetKey key;
  key.base_tiles = base;
  uint32_t msaa_samples_x_log2 = uint32_t(samples >= xenos::MsaaSamples::k4X);
  key.pitch_tiles_at_32bpp =
      ((pitch << msaa_samples_x_log2) + (xenos::kEdramTileWidthSamples - 1)) /
      xenos::kEdramTileWidthSamples;
  key.msaa_samples = samples;
  key.is_depth = 0;
  xenos::ColorRenderTargetFormat resource_format =
      (format == xenos::ColorRenderTargetFormat::k_8_8_8_8_GAMMA &&
       !gamma_render_target_as_unorm16_)
          ? xenos::ColorRenderTargetFormat::k_8_8_8_8
          : xenos::GetStorageColorFormat(format);
  key.resource_format = uint32_t(resource_format);
  return GetRenderTargetTexture(key);
}

void MetalRenderTargetCache::StoreTiledData(MTL::CommandBuffer* command_buffer,
                                            MTL::Texture* texture,
                                            uint32_t edram_base,
                                            uint32_t pitch_tiles,
                                            uint32_t height_tiles,
                                            bool is_depth) {
  MTL::Texture* source_texture = texture;
  MTL::Texture* temp_texture = nullptr;

  // Check if this is a depth/stencil texture
  bool is_depth_stencil_format =
      texture->pixelFormat() == MTL::PixelFormatDepth32Float_Stencil8 ||
      texture->pixelFormat() == MTL::PixelFormatDepth32Float ||
      texture->pixelFormat() == MTL::PixelFormatDepth16Unorm ||
      texture->pixelFormat() == MTL::PixelFormatDepth24Unorm_Stencil8 ||
      texture->pixelFormat() == MTL::PixelFormatX32_Stencil8;

  if (is_depth_stencil_format) {
    // For storing depth/stencil data back to EDRAM, we would need to read
    // from the depth texture This is complex because depth textures can't be
    // directly read in compute shaders. For now, we'll skip storing depth
    // data back to EDRAM since depth buffers are typically write-only during
    // rendering and don't need to be preserved across frames.
    return;
  }

  // If texture is multisample, create a temporary non-multisample texture and
  // resolve to it first
  if (texture->textureType() == MTL::TextureType2DMultisample) {
    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
    desc->setWidth(texture->width());
    desc->setHeight(texture->height());
    desc->setPixelFormat(texture->pixelFormat());
    desc->setTextureType(MTL::TextureType2D);  // Regular 2D texture
    desc->setSampleCount(1);                   // Non-multisample
    desc->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    desc->setStorageMode(MTL::StorageModePrivate);

    if (render_target_heap_pool_) {
      temp_texture = render_target_heap_pool_->CreateTexture(desc);
    }
    if (!temp_texture) {
      temp_texture = device_->newTexture(desc);
    }
    desc->release();
    if (!temp_texture) {
      XELOGE(
          "MetalRenderTargetCache::StoreTiledData - Failed to create "
          "temporary "
          "texture");
      return;
    }

    // Resolve multisample texture to temporary texture
    MTL::RenderPassDescriptor* resolve_desc =
        MTL::RenderPassDescriptor::renderPassDescriptor();
    if (resolve_desc) {
      auto* color_attachment = resolve_desc->colorAttachments()->object(0);
      color_attachment->setTexture(texture);              // Multisample source
      color_attachment->setResolveTexture(temp_texture);  // Resolved output
      color_attachment->setLoadAction(MTL::LoadActionLoad);
      color_attachment->setStoreAction(MTL::StoreActionMultisampleResolve);

      EndSharedMemoryUploadBlitEncoderForCommandBuffer(command_processor_,
                                                       command_buffer);
      MTL::RenderCommandEncoder* render_encoder =
          command_buffer->renderCommandEncoder(resolve_desc);
      if (render_encoder) {
        render_encoder->endEncoding();
        // render_encoder is autoreleased - do not release
      }
    }

    source_texture = temp_texture;
  }

  // Create compute encoder
  EndSharedMemoryUploadBlitEncoderForCommandBuffer(command_processor_,
                                                   command_buffer);
  MTL::ComputeCommandEncoder* encoder = command_buffer->computeCommandEncoder();
  if (!encoder) {
    if (temp_texture) {
      temp_texture->release();
    }
    return;
  }
  SetEncoderLabel(encoder, "XeniaEDRAMStoreEncoder");

  // Set compute pipeline
  if (!edram_store_pipeline_) {
    XELOGE("StoreTiledData: edram_store_pipeline_ is null");
    encoder->endEncoding();
    if (temp_texture) {
      temp_texture->release();
    }
    return;
  }
  encoder->setComputePipelineState(edram_store_pipeline_);

  // Bind input texture (either original or resolved)
  encoder->setTexture(source_texture, 0);

  // Bind EDRAM buffer
  encoder->setBuffer(edram_buffer_, 0, 0);
  encoder->useResource(source_texture, MTL::ResourceUsageRead);
  encoder->useResource(edram_buffer_, MTL::ResourceUsageWrite);

  // Pass parameters inline via setBytes (avoids transient buffer allocation)
  uint32_t params[2] = {edram_base, pitch_tiles};
  encoder->setBytes(&params[0], sizeof(uint32_t), 1);
  encoder->setBytes(&params[1], sizeof(uint32_t), 2);

  // Calculate thread group sizes
  MTL::Size threads_per_threadgroup = MTL::Size::Make(8, 8, 1);
  MTL::Size threadgroups = MTL::Size::Make(
      (source_texture->width() + 7) / 8, (source_texture->height() + 7) / 8, 1);

  // Dispatch compute
  encoder->dispatchThreadgroups(threadgroups, threads_per_threadgroup);
  encoder->endEncoding();
  // encoder is autoreleased - do not release

  if (temp_texture) {
    temp_texture->release();
  }
}

void MetalRenderTargetCache::DumpRenderTargets(
    uint32_t dump_base, uint32_t dump_row_length_used, uint32_t dump_rows,
    uint32_t dump_pitch, MTL::CommandBuffer* command_buffer,
    const char* encoder_label) {
  assert_true(GetPath() == Path::kHostRenderTargets);

  XELOGGPU(
      "MetalRenderTargetCache::DumpRenderTargets: base={} row_length_used={} "
      "rows={} pitch={}",
      dump_base, dump_row_length_used, dump_rows, dump_pitch);

  std::vector<ResolveCopyDumpRectangle> rectangles;
  GetResolveCopyRectanglesToDump(dump_base, dump_row_length_used, dump_rows,
                                 dump_pitch, rectangles);

  XELOGGPU("MetalRenderTargetCache::DumpRenderTargets: {} rectangles to dump",
           rectangles.size());
  if (rectangles.empty()) {
    XELOGW(
        "MetalRenderTargetCache::DumpRenderTargets: no rectangles for base={} "
        "row_length_used={} rows={} pitch={}",
        dump_base, dump_row_length_used, dump_rows, dump_pitch);
    return;
  }

  if (!edram_buffer_) {
    XELOGW(
        "MetalRenderTargetCache::DumpRenderTargets: EDRAM buffer not "
        "initialized, skipping GPU dump");
    return;
  }

  struct EdramDumpConstants {
    uint32_t dispatch_first_tile;
    uint32_t source_base_tiles;
    uint32_t dest_pitch_tiles;
    uint32_t source_pitch_tiles;
    uint32_t resolution_scale_x;
    uint32_t resolution_scale_y;
    uint32_t tile_size_x;
    uint32_t tile_size_y;
    float tile_size_inv_x;
    float tile_size_inv_y;
    float source_pitch_tiles_inv;
    uint32_t format;
    uint32_t flags;
    uint32_t padding;
  };

  ScopedAutoreleasePool autorelease_pool;
  bool standalone = false;
  MTL::CommandBuffer* cmd = command_buffer;
  if (!cmd) {
    cmd = command_processor_.CreateStandaloneTransferCommandBuffer(
        "XeniaCB reason=rt-dump");
    if (!cmd) {
      XELOGE("MetalRenderTargetCache::DumpRenderTargets: no command buffer");
      return;
    }
    standalone = true;
  }

  EndSharedMemoryUploadBlitEncoderForCommandBuffer(command_processor_, cmd);
  MTL::ComputeCommandEncoder* encoder = cmd->computeCommandEncoder();
  if (!encoder) {
    XELOGE("MetalRenderTargetCache::DumpRenderTargets: no compute encoder");
    if (standalone) {
      cmd->release();
    }
    return;
  }
  SetEncoderLabel(encoder,
                  encoder_label ? encoder_label : "XeniaEDRAMDumpEncoder");
  PushEncoderDebugGroup(
      encoder,
      fmt::format("{} base={} rows={} pitch={}",
                  encoder_label ? encoder_label : "XeniaEDRAMDumpEncoder",
                  dump_base, dump_rows, dump_pitch));

  encoder->setBuffer(edram_buffer_, 0, 0);
  encoder->useResource(edram_buffer_, MTL::ResourceUsageWrite);

  uint32_t scale_x = draw_resolution_scale_x();
  uint32_t scale_y = draw_resolution_scale_y();

  for (const ResolveCopyDumpRectangle& rect : rectangles) {
    auto* rt = static_cast<MetalRenderTarget*>(rect.render_target);
    if (!rt) {
      continue;
    }

    RenderTargetKey key = rt->key();
    MTL::Texture* tex = nullptr;
    bool dump_source_is_uint = false;
    if (key.is_depth) {
      tex = rt->texture();
      if (!tex) {
        continue;
      }
      MTL::PixelFormat expected_format =
          GetDepthPixelFormat(key.GetDepthFormat());
      assert_true(tex->pixelFormat() == expected_format,
                  "Dump depth must bind resource pixel format");
    } else {
      bool ownership_transfer_is_uint = false;
      MTL::PixelFormat ownership_transfer_format =
          GetColorOwnershipTransferPixelFormat(key.GetColorFormat(),
                                               &ownership_transfer_is_uint);
      dump_source_is_uint = ownership_transfer_is_uint;
      if (dump_source_is_uint) {
        tex =
            (key.msaa_samples != xenos::MsaaSamples::k1X && rt->msaa_texture())
                ? rt->msaa_transfer_texture()
                : rt->transfer_texture();
      } else {
        tex = rt->texture();
      }
      if (!tex) {
        continue;
      }
      MTL::PixelFormat expected_format =
          dump_source_is_uint
              ? ownership_transfer_format
              : GetColorResourcePixelFormat(key.GetColorFormat());
      assert_true(tex->pixelFormat() == expected_format,
                  dump_source_is_uint
                      ? "Dump color uint must bind ownership-transfer format"
                      : "Dump color must bind resource pixel format");
    }

    uint32_t dump_format = GetMetalEdramDumpFormat(key);
    uint32_t dump_flags = 0;
    MTL::Texture* stencil_tex = nullptr;
    if (key.is_depth) {
      if (!::cvars::depth_float24_convert_in_pixel_shader &&
          ::cvars::depth_float24_round) {
        dump_flags |= kMetalEdramDumpFlagDepthRound;
      }
      stencil_tex = GetStencilTextureView(rt);
      if (stencil_tex) {
        dump_flags |= kMetalEdramDumpFlagHasStencil;
      }
    }

    // Choose the appropriate dump pipeline based on:
    // - 32bpp vs 64bpp (key.Is64bpp())
    // - color vs depth (key.is_depth)
    // - MSAA sample count (key.msaa_samples)
    // This mirrors D3D12's dump pipeline selection: use key.Is64bpp() directly,
    // NOT IsKey64bpp() which includes gamma-as-unorm16. The EDRAM buffer is
    // always 32bpp for gamma formats; only the host texture storage is 64bpp.
    MTL::ComputePipelineState* dump_pipeline = nullptr;
    bool is_64bpp = key.Is64bpp();

    // If this is a gamma RT stored as linear RGBA16Unorm, we need to encode
    // to PWL gamma when dumping. Set the flag for the dump shader.
    if (!key.is_depth &&
        key.GetColorFormat() ==
            xenos::ColorRenderTargetFormat::k_8_8_8_8_GAMMA &&
        gamma_render_target_as_unorm16_) {
      dump_flags |= kMetalEdramDumpFlagGammaAsLinear;
    }

    size_t msaa_index = MsaaSamplesToIndex(key.msaa_samples);
    if (msaa_index != SIZE_MAX) {
      if (!key.is_depth) {
        dump_pipeline =
            edram_dump_color_pipelines_[is_64bpp ? 1u : 0u]
                                       [dump_source_is_uint ? 1u : 0u]
                                       [msaa_index];
      } else {
        dump_pipeline = edram_dump_depth_pipelines_[msaa_index];
      }
    }

    if (!dump_pipeline) {
      XELOGGPU(
          "MetalRenderTargetCache::DumpRenderTargets: no dump pipeline for "
          "key=0x{:08X} (is_depth={}, is_64bpp={}, msaa={})",
          key.key, key.is_depth ? 1 : 0, is_64bpp ? 1 : 0,
          static_cast<uint32_t>(key.msaa_samples));
      continue;
    }

    XELOGGPU(
        "MetalRenderTargetCache::DumpRenderTargets: dump RT key=0x{:08X} "
        "(is_depth={}, is_64bpp={}, source_uint={}, msaa={}) tex={}x{} "
        "pipeline={:p}",
        key.key, key.is_depth ? 1 : 0, is_64bpp ? 1 : 0,
        dump_source_is_uint ? 1 : 0, static_cast<uint32_t>(key.msaa_samples),
        tex->width(), tex->height(), static_cast<void*>(dump_pipeline));

    ResolveCopyDumpRectangle::Dispatch
        dispatches[ResolveCopyDumpRectangle::kMaxDispatches];
    uint32_t dispatch_count =
        rect.GetDispatches(dump_pitch, dump_row_length_used, dispatches);
    if (!dispatch_count) {
      continue;
    }

    for (uint32_t i = 0; i < dispatch_count; ++i) {
      const ResolveCopyDumpRectangle::Dispatch& dispatch = dispatches[i];

      EdramDumpConstants constants;
      constants.dispatch_first_tile = dump_base + dispatch.offset;
      constants.source_base_tiles = key.base_tiles;
      constants.dest_pitch_tiles = dump_pitch;
      constants.source_pitch_tiles = key.GetPitchTiles();
      constants.resolution_scale_x = scale_x;
      constants.resolution_scale_y = scale_y;
      uint32_t tile_size_x = (is_64bpp ? 40u : 80u) * scale_x;
      uint32_t tile_size_y = 16u * scale_y;
      constants.tile_size_x = tile_size_x;
      constants.tile_size_y = tile_size_y;
      constants.tile_size_inv_x =
          tile_size_x ? (1.0f / float(tile_size_x)) : 0.0f;
      constants.tile_size_inv_y =
          tile_size_y ? (1.0f / float(tile_size_y)) : 0.0f;
      constants.source_pitch_tiles_inv =
          constants.source_pitch_tiles
              ? (1.0f / float(constants.source_pitch_tiles))
              : 0.0f;
      constants.format = dump_format;
      constants.flags = dump_flags;
      constants.padding = 0;

      encoder->setComputePipelineState(dump_pipeline);
      encoder->setTexture(tex, 0);
      if (stencil_tex) {
        encoder->setTexture(stencil_tex, 1);
      }
      encoder->useResource(tex, MTL::ResourceUsageRead);
      if (stencil_tex) {
        encoder->useResource(stencil_tex, MTL::ResourceUsageRead);
      }
      encoder->setBytes(&constants, sizeof(constants), 1);

      // Thread group dispatch:
      // - 40x16 threads per group (same as D3D12/Vulkan)
      // - For 32bpp: two groups per tile along X (80 samples / 40 threads)
      // - For 64bpp: one group per tile along X (40 samples / 40 threads)
      uint32_t groups_x = dispatch.width_tiles * scale_x;
      if (!is_64bpp) {
        groups_x <<= 1;  // Double for 32bpp
      }
      uint32_t groups_y = dispatch.height_tiles * scale_y;

      MTL::Size threads_per_group = MTL::Size::Make(40, 16, 1);
      MTL::Size threadgroups = MTL::Size::Make(groups_x, groups_y, 1);
      encoder->dispatchThreadgroups(threadgroups, threads_per_group);
    }
  }

  encoder->popDebugGroup();
  encoder->endEncoding();
  if (standalone) {
    command_processor_.CommitStandaloneAndWait(cmd);
  }
}

bool MetalRenderTargetCache::TryDirectHostResolveCopy(
    const draw_util::ResolveInfo& resolve_info,
    const draw_util::ResolveCopyShaderConstants& copy_constants,
    draw_util::ResolveCopyShaderIndex copy_shader, uint32_t dump_base,
    uint32_t dump_row_length_used, uint32_t dump_rows, uint32_t dump_pitch,
    bool resolve_has_clear, MTL::CommandBuffer* command_buffer,
    uint32_t& written_address, uint32_t& written_length) {

  TelemetryStats::ResolveDirectHostTelemetry& direct_telemetry =
      telemetry_.resolve_direct_host;
  ++direct_telemetry.direct_host_attempt;

  auto reject = []() { return false; };
  auto reject_gamma = [&]() {
    ++direct_telemetry.direct_host_reject_gamma;
    return false;
  };
  auto reject_exp_bias = [&]() {
    ++direct_telemetry.direct_host_reject_exp_bias;
    return false;
  };
  auto reject_format_mismatch = [&]() {
    ++direct_telemetry.direct_host_reject_format_mismatch;
    return false;
  };
  auto reject_sample_select = [&]() {
    ++direct_telemetry.direct_host_reject_sample_select;
    return false;
  };
  auto reject_depth_no_fast = [&]() {
    ++direct_telemetry.direct_host_reject_depth_no_fast;
    return false;
  };
  bool tile_candidate_pending = false;
  bool tile_candidate_recorded = false;
  auto record_tile_reject = [&](uint64_t& counter) {
    if (tile_candidate_recorded) {
      return;
    }
    ++direct_telemetry.tile_candidate_total;
    ++counter;
    tile_candidate_recorded = true;
    tile_candidate_pending = false;
  };
  auto record_tile_accept = [&](bool full_color) {
    if (tile_candidate_recorded) {
      return;
    }
    ++direct_telemetry.tile_candidate_total;
    ++direct_telemetry.tile_accept;
    if (full_color) {
      ++direct_telemetry.tile_accept_full_color;
    } else {
      ++direct_telemetry.tile_accept_fast;
    }
    // This is source-side eligibility for a later liveness check. No store action
    // is changed by this telemetry-only path.
    ++direct_telemetry.store_dontcare_eligible;
    tile_candidate_recorded = true;
    tile_candidate_pending = false;
  };
  auto is_current_color_render_target = [&](MetalRenderTarget* rt) {
    if (!rt) {
      return false;
    }
    for (MetalRenderTarget* current_color_target : current_color_targets_) {
      if (current_color_target == rt) {
        return true;
      }
    }
    return false;
  };

  if (GetPath() != Path::kHostRenderTargets) {
    return reject();
  }
  const bool resolve_is_depth = resolve_info.IsCopyingDepth();
  const bool copy_shader_is_fast =
      IsResolveDirectHostRTFastCandidate(copy_shader);
  const bool copy_shader_is_full_color =
      !resolve_is_depth && IsResolveDirectHostRTFullColorCandidate(copy_shader);
  if (resolve_has_clear) {
    record_tile_reject(direct_telemetry.tile_reject_copy_clear);
  } else if (resolve_is_depth) {
    record_tile_reject(direct_telemetry.tile_reject_depth);
  } else {
    tile_candidate_pending = true;
  }

  xenos::ColorRenderTargetFormat resolve_color_format =
      xenos::ColorRenderTargetFormat::k_8_8_8_8;
  xenos::DepthRenderTargetFormat resolve_depth_format =
      xenos::DepthRenderTargetFormat::kD24S8;
  if (resolve_is_depth) {
    if (!xenos::IsSingleCopySampleSelected(
            resolve_info.copy_dest_coordinate_info.copy_sample_select)) {
      return reject_sample_select();
    }
    if (!copy_shader_is_fast) {
      return reject_depth_no_fast();
    }
    resolve_depth_format =
        xenos::DepthRenderTargetFormat(resolve_info.depth_edram_info.format);
  } else {
    resolve_color_format =
        xenos::ColorRenderTargetFormat(resolve_info.color_edram_info.format);
    if (resolve_color_format ==
        xenos::ColorRenderTargetFormat::k_8_8_8_8_GAMMA) {
      // TODO(xenios-jp): Support direct host gamma resolves only once the
      // shader can mirror the dump path's linear RGBA16Unorm host storage to
      // guest PWL-gamma RGBA8 conversion.
      return reject_gamma();
    }
    if (copy_shader_is_fast) {
      if (!xenos::IsSingleCopySampleSelected(
              resolve_info.copy_dest_coordinate_info.copy_sample_select)) {
        return reject_sample_select();
      }
      if (resolve_info.copy_dest_info.copy_dest_exp_bias) {
        return reject_exp_bias();
      }
      if (!xenos::IsColorResolveFormatBitwiseEquivalent(
              resolve_color_format,
              xenos::ColorFormat(
                  resolve_info.copy_dest_info.copy_dest_format))) {
        return reject_format_mismatch();
      }
    } else if (!copy_shader_is_full_color) {
      if (!xenos::IsSingleCopySampleSelected(
              resolve_info.copy_dest_coordinate_info.copy_sample_select)) {
        return reject_sample_select();
      }
      if (resolve_info.copy_dest_info.copy_dest_exp_bias) {
        return reject_exp_bias();
      }
      if (!xenos::IsColorResolveFormatBitwiseEquivalent(
              resolve_color_format,
              xenos::ColorFormat(
                  resolve_info.copy_dest_info.copy_dest_format))) {
        return reject_format_mismatch();
      }
      return reject();
    }
  }

  struct DirectHostResolveConstants {
    uint32_t dispatch_offset;
    uint32_t dump_base;
    uint32_t dump_pitch_tiles;
    uint32_t source_base_tiles;
    uint32_t source_pitch_tiles;
    uint32_t thread_count_x;
    uint32_t thread_count_y;
    uint32_t height_scaled;
    uint32_t msaa_2x_sample_0;
    uint32_t msaa_2x_sample_1;
    uint32_t flags;
  };

  struct DirectHostResolveSource {
    RenderTargetKey key;
    MTL::Texture* texture;
    MTL::Texture* stencil_texture;
    MTL::ComputePipelineState* pipeline;
    ResolveCopyDumpRectangle::Dispatch
        dispatches[ResolveCopyDumpRectangle::kMaxDispatches];
    uint32_t dispatch_count;
    uint32_t flags;
    uint32_t pixels_per_thread;
    bool is_64bpp;
    bool is_depth;
  };

  std::vector<ResolveCopyDumpRectangle> rectangles;
  GetResolveCopyRectanglesToDump(dump_base, dump_row_length_used, dump_rows,
                                 dump_pitch, rectangles);
  if (rectangles.empty()) {
    return reject();
  }
  if (tile_candidate_pending && rectangles.size() != 1) {
    record_tile_reject(direct_telemetry.tile_reject_multi_source_rect);
  }

  uint64_t covered_tiles = 0;
  std::vector<DirectHostResolveSource> sources;
  sources.reserve(rectangles.size());
  for (const ResolveCopyDumpRectangle& rect : rectangles) {
    if (!rect.rows || rect.row_last_end <= rect.row_first_start) {
      return reject();
    }
    if (rect.rows == 1) {
      covered_tiles += rect.row_last_end - rect.row_first_start;
    } else {
      covered_tiles += dump_row_length_used - rect.row_first_start;
      covered_tiles += uint64_t(rect.rows - 2) * dump_row_length_used;
      covered_tiles += rect.row_last_end;
    }

    auto* rt = static_cast<MetalRenderTarget*>(rect.render_target);
    if (!rt) {
      return reject();
    }
    RenderTargetKey key = rt->key();
    if (key.is_depth != resolve_is_depth) {
      return reject();
    }

    bool source_is_uint = false;
    MTL::PixelFormat expected_format = MTL::PixelFormatInvalid;
    MTL::Texture* texture = nullptr;
    MTL::Texture* stencil_texture = nullptr;
    MTL::ComputePipelineState* pipeline = nullptr;
    uint32_t source_flags = 0;
    bool is_64bpp = false;
    if (resolve_is_depth) {
      if (key.GetDepthFormat() != resolve_depth_format ||
          key.msaa_samples != resolve_info.depth_edram_info.msaa_samples) {
        return reject_format_mismatch();
      }
      texture = rt->texture();
      expected_format = GetDepthPixelFormat(key.GetDepthFormat());
      if (!::cvars::depth_float24_convert_in_pixel_shader &&
          ::cvars::depth_float24_round) {
        source_flags |= kDirectHostResolveDepthFlagRoundDepth;
      }
      stencil_texture = GetStencilTextureView(rt);
      if (stencil_texture) {
        source_flags |= kDirectHostResolveDepthFlagHasStencil;
      }
      pipeline = GetDirectHostDepthResolvePipeline(key.msaa_samples,
                                                   IsDrawResolutionScaled());
    } else {
      if (key.GetColorFormat() != resolve_color_format ||
          key.msaa_samples != resolve_info.color_edram_info.msaa_samples) {
        return reject_format_mismatch();
      }
      if (copy_shader_is_full_color &&
          !IsResolveDirectHostRTFullColorSourcePackable(resolve_color_format)) {
        return reject_format_mismatch();
      }

      MTL::PixelFormat ownership_transfer_format =
          GetColorOwnershipTransferPixelFormat(key.GetColorFormat(),
                                               &source_is_uint);
      if (tile_candidate_pending) {
        if (!is_current_color_render_target(rt)) {
          record_tile_reject(
              direct_telemetry.tile_reject_source_not_current_rt);
        } else if (source_is_uint) {
          record_tile_reject(direct_telemetry.tile_reject_uint_transfer_view);
        }
      }
      texture = source_is_uint
                    ? ((key.msaa_samples != xenos::MsaaSamples::k1X &&
                        rt->msaa_texture())
                           ? rt->msaa_transfer_texture()
                           : rt->transfer_texture())
                    : rt->texture();
      expected_format = source_is_uint
                            ? ownership_transfer_format
                            : GetColorResourcePixelFormat(key.GetColorFormat());
      is_64bpp = key.Is64bpp();
      pipeline = copy_shader_is_full_color
                     ? GetDirectHostColorFullResolvePipeline(
                           key.msaa_samples, IsDrawResolutionScaled(),
                           source_is_uint, copy_shader)
                     : GetDirectHostResolvePipeline(is_64bpp, key.msaa_samples,
                                                    IsDrawResolutionScaled(),
                                                    source_is_uint);
    }
    if (!texture) {
      return reject();
    }
    if (texture->pixelFormat() != expected_format) {
      return reject_format_mismatch();
    }
    if (!pipeline) {
      return reject();
    }

    DirectHostResolveSource source = {};
    source.key = key;
    source.texture = texture;
    source.stencil_texture = stencil_texture;
    source.pipeline = pipeline;
    source.dispatch_count =
        rect.GetDispatches(dump_pitch, dump_row_length_used, source.dispatches);
    source.flags = source_flags;
    source.pixels_per_thread =
        DirectHostResolvePixelsPerThread(copy_shader, is_64bpp,
                                         key.msaa_samples);
    source.is_64bpp = is_64bpp;
    source.is_depth = resolve_is_depth;
    if (!source.dispatch_count) {
      return reject();
    }
    const uint32_t tile_size_x =
        (source.is_64bpp ? 40u : 80u) * draw_resolution_scale_x();
    const uint32_t tile_pixel_size_x =
        tile_size_x >>
        uint32_t(source.key.msaa_samples >= xenos::MsaaSamples::k4X);
    for (uint32_t i = 0; i < source.dispatch_count; ++i) {
      uint32_t dispatch_pixel_width =
          source.dispatches[i].width_tiles * tile_pixel_size_x;
      if (dispatch_pixel_width % source.pixels_per_thread) {
        return reject();
      }
    }
    sources.push_back(source);
  }

  const uint64_t required_tiles =
      uint64_t(dump_row_length_used) * uint64_t(dump_rows);
  if (covered_tiles != required_tiles) {
    if (tile_candidate_pending) {
      record_tile_reject(direct_telemetry.tile_reject_multi_source_rect);
    }
    return reject();
  }
  if (tile_candidate_pending) {
    record_tile_accept(copy_shader_is_full_color);
  }

  auto* texture_cache = command_processor_.texture_cache();
  const bool draw_resolution_scaled = IsDrawResolutionScaled();
  ResolveDestinationBuffer destination = {};
  if (!PrepareResolveDestinationBuffer(resolve_info, draw_resolution_scaled,
                                       destination)) {
    return reject();
  }

  command_processor_.SetSwapDestSwap(
      resolve_info.copy_dest_base, resolve_info.copy_dest_info.copy_dest_swap);

  ScopedAutoreleasePool autorelease_pool;
  bool standalone = false;
  MTL::CommandBuffer* cmd = command_buffer;
  if (!cmd) {
    cmd = command_processor_.CreateStandaloneTransferCommandBuffer(
        "XeniaCB reason=direct-host-resolve");
    if (!cmd) {
      return reject();
    }
    standalone = true;
  }

  EndSharedMemoryUploadBlitEncoderForCommandBuffer(command_processor_, cmd);
  MTL::ComputeCommandEncoder* encoder = cmd->computeCommandEncoder();
  if (!encoder) {
    if (standalone) {
      cmd->release();
    }
    return reject();
  }

  SetEncoderLabel(encoder, kDirectHostResolveEncoderLabel);
  PushEncoderDebugGroup(
      encoder,
      fmt::format(
          "{} rects={} dest={}", kDirectHostResolveEncoderLabel, sources.size(),
          draw_resolution_scaled ? "scaled_resolve_memory" : "shared_memory"));

  if (draw_resolution_scaled) {
    encoder->setBytes(&copy_constants.dest_relative,
                      sizeof(copy_constants.dest_relative), 0);
  } else {
    encoder->setBytes(&copy_constants, sizeof(copy_constants), 0);
  }
  encoder->setBuffer(destination.buffer, destination.offset, 1);
  encoder->useResource(destination.buffer, MTL::ResourceUsageWrite);

  const uint32_t scale_x = draw_resolution_scale_x();
  const uint32_t scale_y = draw_resolution_scale_y();
  const uint32_t height_scaled =
      (resolve_info.height_div_8 << xenos::kResolveAlignmentPixelsLog2) *
      scale_y;
  const uint32_t msaa_2x_sample_0 =
      draw_util::GetD3D10SampleIndexForGuest2xMSAA(0, msaa_2x_supported_);
  const uint32_t msaa_2x_sample_1 =
      draw_util::GetD3D10SampleIndexForGuest2xMSAA(1, msaa_2x_supported_);

  uint64_t direct_rectangles = 0;
  uint64_t direct_dispatches = 0;
  uint64_t direct_threadgroups = 0;
  uint64_t direct_pixels = 0;
  uint64_t direct_bytes_avoided = 0;

  for (const DirectHostResolveSource& source : sources) {
    ++direct_rectangles;
    encoder->setTexture(source.texture, 0);
    if (source.is_depth) {
      encoder->setTexture(source.stencil_texture, 1);
    }
    encoder->useResource(source.texture, MTL::ResourceUsageRead);
    if (source.stencil_texture) {
      encoder->useResource(source.stencil_texture, MTL::ResourceUsageRead);
    }
    encoder->setComputePipelineState(source.pipeline);

    const uint32_t tile_size_x = (source.is_64bpp ? 40u : 80u) * scale_x;
    const uint32_t tile_size_y = 16u * scale_y;
    const uint32_t tile_pixel_size_x =
        tile_size_x >>
        uint32_t(source.key.msaa_samples >= xenos::MsaaSamples::k4X);
    const uint32_t tile_pixel_size_y =
        tile_size_y >>
        uint32_t(source.key.msaa_samples >= xenos::MsaaSamples::k2X);
    const uint32_t pixels_per_thread = source.pixels_per_thread;
    for (uint32_t i = 0; i < source.dispatch_count; ++i) {
      const ResolveCopyDumpRectangle::Dispatch& dispatch = source.dispatches[i];
      DirectHostResolveConstants constants = {};
      constants.dispatch_offset = dispatch.offset;
      constants.dump_base = dump_base;
      constants.dump_pitch_tiles = dump_pitch;
      constants.source_base_tiles = source.key.base_tiles;
      constants.source_pitch_tiles = source.key.GetPitchTiles();
      constants.thread_count_x =
          dispatch.width_tiles * tile_pixel_size_x / pixels_per_thread;
      constants.thread_count_y = dispatch.height_tiles * tile_pixel_size_y;
      constants.height_scaled = height_scaled;
      constants.msaa_2x_sample_0 = msaa_2x_sample_0;
      constants.msaa_2x_sample_1 = msaa_2x_sample_1;
      constants.flags = source.flags;
      encoder->setBytes(&constants, sizeof(constants), 2);

      uint32_t threadgroups_x = (constants.thread_count_x + 7u) >> 3;
      uint32_t threadgroups_y = (constants.thread_count_y + 7u) >> 3;
      encoder->dispatchThreadgroups(
          MTL::Size::Make(threadgroups_x, threadgroups_y, 1),
          MTL::Size::Make(8, 8, 1));

      ++direct_dispatches;
      direct_threadgroups +=
          uint64_t(threadgroups_x) * uint64_t(threadgroups_y);
      uint64_t dispatch_pixels = uint64_t(dispatch.width_tiles) *
                                 uint64_t(dispatch.height_tiles) *
                                 uint64_t(tile_size_x) * uint64_t(tile_size_y);
      direct_pixels += dispatch_pixels;
      direct_bytes_avoided +=
          dispatch_pixels *
          uint64_t(EstimateRenderTargetBytesPerPixel(source.is_64bpp));
    }
  }

  if (!draw_resolution_scaled) {
    command_processor_.MarkSharedMemoryComputeWritePending(
        resolve_info.copy_dest_extent_start,
        resolve_info.copy_dest_extent_length, encoder);
  }

  encoder->popDebugGroup();
  encoder->endEncoding();
  if (standalone) {
    command_processor_.CommitStandaloneAndWait(cmd);
  }

  written_address = resolve_info.copy_dest_extent_start;
  written_length = resolve_info.copy_dest_extent_length;
  if (texture_cache) {
    texture_cache->MarkRangeAsResolved(written_address, written_length);
  }

  ++direct_telemetry.direct_host_success;
  return true;
}

MTL::Library* MetalRenderTargetCache::GetOrCreateTileDirectHostResolveLibrary() {
  if (tile_direct_host_resolve_library_) {
    return tile_direct_host_resolve_library_;
  }

  static const char kTileDirectHostResolveSource[] = R"METAL(
#include <metal_stdlib>
using namespace metal;

constant uint kXenosMsaaSamples1X = 0u;
constant uint kXenosMsaaSamples2X = 1u;
constant uint kEdramTileCount = 2048u;
constant uint kXenosColorRTFormatRGBA8 = 0u;
constant uint kXenosColorRTFormatRGBA8Gamma = 1u;
constant uint kXenosColorRTFormatRGB10A2 = 2u;
constant uint kXenosColorRTFormatRGB10A2Float = 3u;
constant uint kXenosColorRTFormatRG16 = 4u;
constant uint kXenosColorRTFormatRGBA16 = 5u;
constant uint kXenosColorRTFormatRG16Float = 6u;
constant uint kXenosColorRTFormatRGBA16Float = 7u;
constant uint kXenosColorRTFormatRGB10A2AsRGB10A2 = 10u;
constant uint kXenosColorRTFormatRGB10A2FloatAsRGBA16 = 12u;
constant uint kXenosColorRTFormatRG32Float = 15u;
constant uint kXenosFormat_1_5_5_5 = 3u;
constant uint kXenosFormat_5_6_5 = 4u;
constant uint kXenosFormat_6_5_5 = 5u;
constant uint kXenosFormat_8_8_8_8 = 6u;
constant uint kXenosFormat_2_10_10_10 = 7u;
constant uint kXenosFormat_8_8 = 10u;
constant uint kXenosFormat_8_8_8_8_A = 14u;
constant uint kXenosFormat_4_4_4_4 = 15u;
constant uint kXenosFormat_10_11_11 = 16u;
constant uint kXenosFormat_11_11_10 = 17u;
constant uint kXenosFormat_16_16_EDRAM = 13u;
constant uint kXenosFormat_16_16_16_16_EDRAM = 21u;
constant uint kXenosFormat_16 = 24u;
constant uint kXenosFormat_16_16 = 25u;
constant uint kXenosFormat_16_16_16_16 = 26u;
constant uint kXenosFormat_16_16_FLOAT = 31u;
constant uint kXenosFormat_16_16_16_16_FLOAT = 32u;
constant uint kXenosFormat_32_32_32_32_FLOAT = 38u;
constant uint kXenosFormat_8_8_8_8_AS_16_16_16_16 = 50u;
constant uint kXenosFormat_2_10_10_10_AS_16_16_16_16 = 54u;
constant uint kXenosFormat_10_11_11_AS_16_16_16_16 = 55u;
constant uint kXenosFormat_11_11_10_AS_16_16_16_16 = 56u;
constant uint kXenosCopySampleSelect01 = 4u;
constant uint kXenosCopySampleSelect0123 = 6u;
constant bool kTileDirectHostResolveIsFullColor [[function_constant(0)]];
constant uint kTileDirectHostResolveFullDestBppLog2 [[function_constant(1)]];
constant uint kTileDirectHostResolveFullPixelsPerThread [[function_constant(2)]];
constant uint kTileDirectHostResolveSourceFormat [[function_constant(3)]];
constant uint kTileDirectHostResolveDestFormat [[function_constant(4)]];
constant uint kTileDirectHostResolveTileWidth = 16u;
constant uint kTileDirectHostResolveTileHeight = 16u;

struct TileDirectHostResolveConstants {
  uint edram_info;
  uint coordinate_info;
  uint dest_info;
  uint dest_coordinate_info;
  uint dest_base;
  uint dump_base;
  uint dump_pitch_tiles;
  uint dump_row_length_used;
  uint dump_rows;
  uint rect_row_first;
  uint rect_rows;
  uint rect_row_first_start;
  uint rect_row_last_end;
  uint source_base_tiles;
  uint source_pitch_tiles;
  uint source_width;
  uint source_height;
  uint height_scaled;
  uint msaa_2x_sample_0;
  uint msaa_2x_sample_1;
  uint msaa_samples;
  uint is_64bpp;
  uint source_format;
  uint is_full_color;
  uint dest_format;
  float dest_exp_bias_factor;
  uint pixels_per_thread;
  uint dest_bpp_log2;
  uint source_to_dump_tiles;
  uint rect_row_last;
  uint padding2;
  uint padding3;
};

struct TileResolveInfo {
  uint edram_pitch_tiles;
  uint edram_msaa_samples;
  uint edram_base_tiles;
  uint edram_format;
  uint edram_format_ints_log2;
  uint2 edram_offset_scaled;
  uint width_scaled;
  uint dest_endian_128;
  bool dest_is_array;
  uint dest_slice;
  uint dest_format;
  float dest_exp_bias_factor;
  bool dest_swap;
  uint dest_row_pitch_macro_tiles;
  uint dest_slice_pitch_3d_macro_tiles;
  uint2 dest_xy_offset_scaled;
  uint sample_select;
  uint dest_base;
};

inline TileResolveInfo TileGetResolveInfo(
    constant TileDirectHostResolveConstants& c) {
  TileResolveInfo info;
  uint edram_info = c.edram_info;
  uint coordinate_info = c.coordinate_info;
  uint dest_info = c.dest_info;
  uint dest_coordinate_info = c.dest_coordinate_info;
  info.edram_pitch_tiles = edram_info & ((1u << 10u) - 1u);
  info.edram_msaa_samples = (edram_info >> 10u) & ((1u << 2u) - 1u);
  info.edram_base_tiles = (edram_info >> 13u) & ((1u << 11u) - 1u);
  info.edram_format = (edram_info >> 24u) & ((1u << 4u) - 1u);
  info.edram_format_ints_log2 = (edram_info >> 28u) & 1u;
  info.edram_offset_scaled =
      (((uint2(coordinate_info) >> uint2(0u, 4u)) &
        ((uint2(1u) << uint2(4u, 1u)) - 1u)) << 3u);
  info.width_scaled = ((coordinate_info >> 5u) & ((1u << 11u) - 1u)) << 3u;
  info.dest_endian_128 = dest_info & ((1u << 3u) - 1u);
  info.dest_is_array = (dest_info & (1u << 3u)) != 0u;
  info.dest_slice = (dest_info >> 4u) & ((1u << 3u) - 1u);
  info.dest_format = (dest_info >> 7u) & ((1u << 6u) - 1u);
  info.dest_exp_bias_factor = c.dest_exp_bias_factor;
  info.dest_swap = (dest_info & (1u << 24u)) != 0u;
  info.dest_row_pitch_macro_tiles =
      dest_coordinate_info & ((1u << 10u) - 1u);
  info.dest_slice_pitch_3d_macro_tiles =
      ((dest_coordinate_info >> 10u) & ((1u << 10u) - 1u)) << 1u;
  info.dest_xy_offset_scaled =
      (((uint2(dest_coordinate_info) >> uint2(20u, 24u)) &
        ((1u << 4u) - 1u)) << 3u);
  info.sample_select = (dest_coordinate_info >> 28u) & ((1u << 3u) - 1u);
  info.dest_base = c.dest_base;
  return info;
}

inline uint XeEndianSwap32(uint value, uint endian) {
  if (endian == 1u || endian == 2u) {
    value = ((value & 0x00FF00FFu) << 8u) |
            ((value & 0xFF00FF00u) >> 8u);
  }
  if (endian == 2u || endian == 3u) {
    value = (value << 16u) | (value >> 16u);
  }
  return value;
}

inline uint2 XeEndianSwap16(uint2 value, uint endian) {
  if (endian == 1u) {
    value = ((value & 0x00FF00FFu) << 8u) |
            ((value & 0xFF00FF00u) >> 8u);
  }
  return value;
}

inline uint4 XeEndianSwap32(uint4 value, uint endian) {
  return uint4(XeEndianSwap32(value.x, endian),
               XeEndianSwap32(value.y, endian),
               XeEndianSwap32(value.z, endian),
               XeEndianSwap32(value.w, endian));
}

inline uint2 XeEndianSwap64(uint2 value, uint endian) {
  if (endian == 4u) {
    value = value.yx;
    endian = 2u;
  }
  return uint2(XeEndianSwap32(value.x, endian),
               XeEndianSwap32(value.y, endian));
}

inline uint4 XeEndianSwap64(uint4 value, uint endian) {
  if (endian == 4u) {
    value = value.yxwz;
    endian = 2u;
  }
  return XeEndianSwap32(value, endian);
}

inline uint4 XeEndianSwap128(uint4 value, uint endian) {
  if (endian == 5u) {
    value = value.wzyx;
    endian = 2u;
  }
  return XeEndianSwap64(value, endian);
}

inline uint XenosTextureTiledAddressCombine(uint outer_inner_bytes, uint bank,
                                            uint pipe, uint y_lsb) {
  return (y_lsb << 4u) | (pipe << 6u) | (bank << 11u) |
         (outer_inner_bytes & 0xFu) |
         (((outer_inner_bytes >> 4u) & 0x1u) << 5u) |
         (((outer_inner_bytes >> 5u) & 0x7u) << 8u) |
         ((outer_inner_bytes >> 8u) << 12u);
}

inline uint XenosTextureTiledAddress2D(uint2 p, uint pitch_macro_tiles,
                                       uint bytes_per_block_log2) {
  uint outer_blocks =
      (((p.y >> 5u) * pitch_macro_tiles + (p.x >> 5u)) << 6u);
  uint inner_blocks = (((p.y >> 1u) & 0x7u) << 3u) | (p.x & 0x7u);
  uint outer_inner_bytes = (outer_blocks | inner_blocks)
                           << bytes_per_block_log2;
  uint bank = (p.y >> 4u) & 0x1u;
  uint pipe = ((p.x >> 3u) & 0x3u) ^ (((p.y >> 3u) & 0x1u) << 1u);
  return XenosTextureTiledAddressCombine(outer_inner_bytes, bank, pipe,
                                         p.y & 1u);
}

inline uint XenosTextureTiledAddress3D(uint3 p, uint pitch_macro_tiles,
                                       uint height_macro_tiles,
                                       uint bytes_per_block_log2) {
  uint outer_blocks =
      (((((p.z >> 2u) * height_macro_tiles + (p.y >> 4u)) *
         pitch_macro_tiles) +
        (p.x >> 5u)) << 7u);
  uint inner_blocks =
      ((p.z & 0x3u) << 5u) | (((p.y >> 1u) & 0x3u) << 3u) |
      (p.x & 0x7u);
  uint outer_inner_bytes = (outer_blocks | inner_blocks)
                           << bytes_per_block_log2;
  uint bank = ((p.y >> 3u) ^ (p.z >> 2u)) & 0x1u;
  uint pipe = ((p.x >> 3u) & 0x3u) ^ (bank << 1u);
  return XenosTextureTiledAddressCombine(outer_inner_bytes, bank, pipe,
                                         p.y & 1u);
}

inline uint TileDestPixelAddress(TileResolveInfo info, uint2 pixel_index,
                                 uint bytes_per_block_log2) {
  uint2 host_position = pixel_index + info.dest_xy_offset_scaled;
  uint address;
  if (info.dest_is_array) {
    address = XenosTextureTiledAddress3D(
        uint3(host_position, info.dest_slice), info.dest_row_pitch_macro_tiles,
        info.dest_slice_pitch_3d_macro_tiles, bytes_per_block_log2);
  } else {
    address = XenosTextureTiledAddress2D(host_position,
                                         info.dest_row_pitch_macro_tiles,
                                         bytes_per_block_log2);
  }
  return address + info.dest_base;
}

inline uint TileFirstSampleIndex(uint sample_select) {
  if (sample_select <= 3u) {
    return sample_select;
  }
  if (sample_select == 5u) {
    return 2u;
  }
  return 0u;
}

inline uint2 TileSampleOffsetForIndex(uint sample_index) {
  return (uint2(sample_index) >> uint2(1u, 0u)) & 1u;
}

inline bool TilePositionInResolveRect(
    constant TileDirectHostResolveConstants& c, uint2 source_sample) {
  if (c.source_pitch_tiles == 0u || c.dump_pitch_tiles == 0u ||
      c.dump_row_length_used == 0u || c.dump_rows == 0u || c.rect_rows == 0u) {
    return false;
  }
  uint tile_size_x = c.is_64bpp != 0u ? 40u : 80u;
  uint tile_size_y = 16u;
  uint source_tile_x = source_sample.x / tile_size_x;
  uint source_tile_y = source_sample.y / tile_size_y;
  uint local_tile =
      (c.source_base_tiles + source_tile_y * c.source_pitch_tiles +
       source_tile_x + c.source_to_dump_tiles) &
      (kEdramTileCount - 1u);
  uint dump_tile_y = local_tile / c.dump_pitch_tiles;
  uint dump_tile_x = local_tile - dump_tile_y * c.dump_pitch_tiles;
  if (dump_tile_y < c.rect_row_first || dump_tile_y >= c.rect_row_last) {
    return false;
  }
  uint row_start = 0u;
  uint row_end = c.dump_row_length_used;
  if (c.rect_rows == 1u) {
    row_start = c.rect_row_first_start;
    row_end = c.rect_row_last_end;
  } else if (dump_tile_y == c.rect_row_first) {
    row_start = c.rect_row_first_start;
  } else if (dump_tile_y == c.rect_row_last - 1u) {
    row_end = c.rect_row_last_end;
  }
  return dump_tile_x >= row_start && dump_tile_x < row_end;
}

inline uint TilePackUnorm(float value, float scale) {
  return uint(clamp(value, 0.0f, 1.0f) * scale + 0.5f);
}

inline uint TilePackR5G5B5A1UNorm(float4 f) {
  uint4 n = uint4(clamp(f, 0.0f, 1.0f) *
                  float4(31.0f, 31.0f, 31.0f, 1.0f) + 0.5f);
  return n.r | (n.g << 5u) | (n.b << 10u) | (n.a << 15u);
}

inline uint TilePackR5G6B5UNorm(float3 f) {
  uint3 n = uint3(clamp(f, 0.0f, 1.0f) *
                  float3(31.0f, 63.0f, 31.0f) + 0.5f);
  return n.r | (n.g << 5u) | (n.b << 11u);
}

inline uint TilePackR5G5B6UNorm(float3 f) {
  uint3 n = uint3(clamp(f, 0.0f, 1.0f) *
                  float3(31.0f, 31.0f, 63.0f) + 0.5f);
  return n.r | (n.g << 5u) | (n.b << 10u);
}

inline uint TilePackR8G8B8A8UNorm(float4 f) {
  uint4 n = uint4(clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f);
  return n.r | (n.g << 8u) | (n.b << 16u) | (n.a << 24u);
}

inline uint TilePackR10G10B10A2UNorm(float4 f) {
  uint4 n = uint4(clamp(f, 0.0f, 1.0f) *
                  float4(1023.0f, 1023.0f, 1023.0f, 3.0f) + 0.5f);
  return n.r | (n.g << 10u) | (n.b << 20u) | (n.a << 30u);
}

inline uint TilePackR4G4B4A4UNorm(float4 f) {
  uint4 n = uint4(clamp(f, 0.0f, 1.0f) * 15.0f + 0.5f);
  return n.r | (n.g << 4u) | (n.b << 8u) | (n.a << 12u);
}

inline uint TilePackR11G11B10UNorm(float3 f) {
  uint3 n = uint3(clamp(f, 0.0f, 1.0f) *
                  float3(2047.0f, 2047.0f, 1023.0f) + 0.5f);
  return n.r | (n.g << 11u) | (n.b << 22u);
}

inline uint TilePackR10G11B11UNorm(float3 f) {
  uint3 n = uint3(clamp(f, 0.0f, 1.0f) *
                  float3(1023.0f, 2047.0f, 2047.0f) + 0.5f);
  return n.r | (n.g << 10u) | (n.b << 21u);
}

inline uint TilePackR16G16UNorm(float2 f) {
  uint2 n = uint2(clamp(f, 0.0f, 1.0f) * 65535.0f + 0.5f);
  return n.r | (n.g << 16u);
}

inline uint2 TilePackR16G16B16A16UNorm(float4 f) {
  uint4 n = uint4(clamp(f, 0.0f, 1.0f) * 65535.0f + 0.5f);
  return uint2(n.r | (n.g << 16u), n.b | (n.a << 16u));
}

inline uint TilePackSnorm16(float value) {
  float clamped = clamp(value, -1.0f, 1.0f);
  float bias = clamped >= 0.0f ? 0.5f : -0.5f;
  return uint(int(clamped * 32767.0f + bias)) & 0xFFFFu;
}

inline uint TilePreClampedFloat32To7e3(float value) {
  uint f32 = as_type<uint>(value);
  uint biased_f32;
  if (f32 < 0x3E800000u) {
    uint f32_exp = f32 >> 23u;
    uint shift = min(125u - f32_exp, 24u);
    uint mantissa = (f32 & 0x7FFFFFu) | 0x800000u;
    biased_f32 = mantissa >> shift;
  } else {
    biased_f32 = f32 + 0xC2000000u;
  }
  uint round_bit = (biased_f32 >> 16u) & 1u;
  uint f10 = biased_f32 + 0x7FFFu + round_bit;
  return (f10 >> 16u) & 0x3FFu;
}

inline uint TileFloat32To7e3(float value) {
  return TilePreClampedFloat32To7e3(clamp(value, 0.0f, 31.875f));
}

inline uint TilePackR10G10B10A2Float(float4 color) {
  return TileFloat32To7e3(color.r) |
         (TileFloat32To7e3(color.g) << 10u) |
         (TileFloat32To7e3(color.b) << 20u) |
         (TilePackUnorm(color.a, 3.0f) << 30u);
}

inline uint TilePack32(float4 color, uint format) {
  switch (format) {
    case kXenosColorRTFormatRGBA8:
    case kXenosColorRTFormatRGBA8Gamma:
      return TilePackUnorm(color.r, 255.0f) |
             (TilePackUnorm(color.g, 255.0f) << 8u) |
             (TilePackUnorm(color.b, 255.0f) << 16u) |
             (TilePackUnorm(color.a, 255.0f) << 24u);
    case kXenosColorRTFormatRGB10A2:
    case kXenosColorRTFormatRGB10A2AsRGB10A2:
      return TilePackUnorm(color.r, 1023.0f) |
             (TilePackUnorm(color.g, 1023.0f) << 10u) |
             (TilePackUnorm(color.b, 1023.0f) << 20u) |
             (TilePackUnorm(color.a, 3.0f) << 30u);
    case kXenosColorRTFormatRGB10A2Float:
    case kXenosColorRTFormatRGB10A2FloatAsRGBA16:
      return TilePackR10G10B10A2Float(color);
    case kXenosColorRTFormatRG16:
      return TilePackSnorm16(color.r) | (TilePackSnorm16(color.g) << 16u);
    case kXenosColorRTFormatRG16Float:
      return as_type<uint>(half2(color.rg));
    default:
      return as_type<uint>(color.r);
  }
}

inline uint2 TilePack64(float4 color, uint format) {
  switch (format) {
    case kXenosColorRTFormatRGBA16:
      return uint2(TilePackSnorm16(color.r) | (TilePackSnorm16(color.g) << 16u),
                   TilePackSnorm16(color.b) | (TilePackSnorm16(color.a) << 16u));
    case kXenosColorRTFormatRGBA16Float:
      return uint2(as_type<uint>(half2(color.rg)),
                   as_type<uint>(half2(color.ba)));
    default:
      return as_type<uint2>(color.rg);
  }
}

inline float4 TileUnpackR8G8B8A8UNorm(uint packed) {
  return float4((uint4(packed) >> uint4(0u, 8u, 16u, 24u)) & 255u) *
         (1.0f / 255.0f);
}

inline float4 TileUnpackR10G10B10A2UNorm(uint packed) {
  return float4((uint4(packed) >> uint4(0u, 10u, 20u, 30u)) &
                uint4(1023u, 1023u, 1023u, 3u)) *
         float4(1.0f / 1023.0f, 1.0f / 1023.0f, 1.0f / 1023.0f,
                1.0f / 3.0f);
}

inline float TileUnpackR10Float(uint packed) {
  uint f10 = packed & 0x3FFu;
  if (f10 == 0u) {
    return 0.0f;
  }
  uint mantissa = f10 & 0x7Fu;
  uint exponent = f10 >> 7u;
  if (exponent == 0u) {
    uint mantissa_lzcnt = clz(mantissa) - 24u;
    exponent = 1u - mantissa_lzcnt;
    mantissa = (mantissa << mantissa_lzcnt) & 0x7Fu;
  }
  return as_type<float>(((exponent + 124u) << 23u) | (mantissa << 16u));
}

inline float4 TileUnpackR10G10B10A2Float(uint packed) {
  return float4(TileUnpackR10Float(packed),
                TileUnpackR10Float(packed >> 10u),
                TileUnpackR10Float(packed >> 20u),
                float((packed >> 30u) & 3u) * (1.0f / 3.0f));
}

inline float4 TileRoundTripR10G10B10A2Float(float4 color) {
  return TileUnpackR10G10B10A2Float(TilePackR10G10B10A2Float(color));
}

inline float2 TileUnpackR16G16Edram(uint packed) {
  int r = int(packed << 16u) >> 16;
  int g = int(packed) >> 16;
  return max(float2(-32.0f),
             float2(float(r), float(g)) * (32.0f / 32767.0f));
}

inline float4 TileUnpackR16G16B16A16Edram(uint2 packed) {
  int4 values = int2(packed).xxyy << int4(16, 0, 16, 0) >> 16;
  return max(float4(-32.0f), float4(values) * (32.0f / 32767.0f));
}

inline float4 TileUnpack32(uint packed, uint format) {
  switch (format) {
    case kXenosColorRTFormatRGBA8:
    case kXenosColorRTFormatRGBA8Gamma:
      return TileUnpackR8G8B8A8UNorm(packed);
    case kXenosColorRTFormatRGB10A2:
    case kXenosColorRTFormatRGB10A2AsRGB10A2:
      return TileUnpackR10G10B10A2UNorm(packed);
    case kXenosColorRTFormatRGB10A2Float:
    case kXenosColorRTFormatRGB10A2FloatAsRGBA16:
      return TileUnpackR10G10B10A2Float(packed);
    case kXenosColorRTFormatRG16:
      return float4(TileUnpackR16G16Edram(packed), 0.0f, 0.0f);
    case kXenosColorRTFormatRG16Float:
      return float4(float2(as_type<half2>(packed)), 0.0f, 0.0f);
    default:
      return float4(as_type<float>(packed), 0.0f, 0.0f, 0.0f);
  }
}

inline float4 TileUnpack64(uint2 packed, uint format) {
  switch (format) {
    case kXenosColorRTFormatRGBA16:
      return TileUnpackR16G16B16A16Edram(packed);
    case kXenosColorRTFormatRGBA16Float:
      return float4(float2(as_type<half2>(packed.x)),
                    float2(as_type<half2>(packed.y)));
    default:
      return float4(as_type<float2>(packed), 0.0f, 0.0f);
  }
}

inline float4 TileRoundTripSourceColor(
    constant TileDirectHostResolveConstants& c, float4 color) {
  return c.is_64bpp != 0u
             ? TileUnpack64(TilePack64(color,
                                       kTileDirectHostResolveSourceFormat),
                            kTileDirectHostResolveSourceFormat)
             : TileUnpack32(TilePack32(color,
                                       kTileDirectHostResolveSourceFormat),
                            kTileDirectHostResolveSourceFormat);
}

inline uint2 TilePackFull16bpp4Pixels(float4 pixel_0, float4 pixel_1,
                                      float4 pixel_2, float4 pixel_3,
                                      uint format) {
  uint2 packed;
  switch (format) {
    case kXenosFormat_1_5_5_5:
      packed.x = TilePackR5G5B5A1UNorm(pixel_0) |
                 (TilePackR5G5B5A1UNorm(pixel_1) << 16u);
      packed.y = TilePackR5G5B5A1UNorm(pixel_2) |
                 (TilePackR5G5B5A1UNorm(pixel_3) << 16u);
      break;
    case kXenosFormat_5_6_5:
      packed.x = TilePackR5G6B5UNorm(pixel_0.rgb) |
                 (TilePackR5G6B5UNorm(pixel_1.rgb) << 16u);
      packed.y = TilePackR5G6B5UNorm(pixel_2.rgb) |
                 (TilePackR5G6B5UNorm(pixel_3.rgb) << 16u);
      break;
    case kXenosFormat_6_5_5:
      packed.x = TilePackR5G5B6UNorm(pixel_0.rgb) |
                 (TilePackR5G5B6UNorm(pixel_1.rgb) << 16u);
      packed.y = TilePackR5G5B6UNorm(pixel_2.rgb) |
                 (TilePackR5G5B6UNorm(pixel_3.rgb) << 16u);
      break;
    case kXenosFormat_8_8:
      packed.x = TilePackR8G8B8A8UNorm(
          float4(pixel_0.rg, pixel_1.rg));
      packed.y = TilePackR8G8B8A8UNorm(
          float4(pixel_2.rg, pixel_3.rg));
      break;
    case kXenosFormat_4_4_4_4:
      packed.x = TilePackR4G4B4A4UNorm(pixel_0) |
                 (TilePackR4G4B4A4UNorm(pixel_1) << 16u);
      packed.y = TilePackR4G4B4A4UNorm(pixel_2) |
                 (TilePackR4G4B4A4UNorm(pixel_3) << 16u);
      break;
    case kXenosFormat_16:
      packed = TilePackR16G16B16A16UNorm(
          float4(pixel_0.r, pixel_1.r, pixel_2.r, pixel_3.r));
      break;
    default:
      packed.x = as_type<uint>(half2(pixel_0.r, pixel_1.r));
      packed.y = as_type<uint>(half2(pixel_2.r, pixel_3.r));
      break;
  }
  return packed;
}

inline uint4 TilePackFull32bpp4Pixels(float4 pixel_0, float4 pixel_1,
                                      float4 pixel_2, float4 pixel_3,
                                      uint format) {
  uint4 packed;
  switch (format) {
    case kXenosFormat_8_8_8_8:
    case kXenosFormat_8_8_8_8_A:
    case kXenosFormat_8_8_8_8_AS_16_16_16_16:
      packed = uint4(TilePackR8G8B8A8UNorm(pixel_0),
                     TilePackR8G8B8A8UNorm(pixel_1),
                     TilePackR8G8B8A8UNorm(pixel_2),
                     TilePackR8G8B8A8UNorm(pixel_3));
      break;
    case kXenosFormat_2_10_10_10:
    case kXenosFormat_2_10_10_10_AS_16_16_16_16:
      packed = uint4(TilePackR10G10B10A2UNorm(pixel_0),
                     TilePackR10G10B10A2UNorm(pixel_1),
                     TilePackR10G10B10A2UNorm(pixel_2),
                     TilePackR10G10B10A2UNorm(pixel_3));
      break;
    case kXenosFormat_10_11_11:
    case kXenosFormat_10_11_11_AS_16_16_16_16:
      packed = uint4(TilePackR11G11B10UNorm(pixel_0.rgb),
                     TilePackR11G11B10UNorm(pixel_1.rgb),
                     TilePackR11G11B10UNorm(pixel_2.rgb),
                     TilePackR11G11B10UNorm(pixel_3.rgb));
      break;
    case kXenosFormat_11_11_10:
    case kXenosFormat_11_11_10_AS_16_16_16_16:
      packed = uint4(TilePackR10G11B11UNorm(pixel_0.rgb),
                     TilePackR10G11B11UNorm(pixel_1.rgb),
                     TilePackR10G11B11UNorm(pixel_2.rgb),
                     TilePackR10G11B11UNorm(pixel_3.rgb));
      break;
    case kXenosFormat_16_16_EDRAM:
    case kXenosFormat_16_16:
      packed = uint4(TilePackR16G16UNorm(pixel_0.rg),
                     TilePackR16G16UNorm(pixel_1.rg),
                     TilePackR16G16UNorm(pixel_2.rg),
                     TilePackR16G16UNorm(pixel_3.rg));
      break;
    case kXenosFormat_16_16_FLOAT:
      packed = uint4(as_type<uint>(half2(pixel_0.r, pixel_0.g)),
                     as_type<uint>(half2(pixel_1.r, pixel_1.g)),
                     as_type<uint>(half2(pixel_2.r, pixel_2.g)),
                     as_type<uint>(half2(pixel_3.r, pixel_3.g)));
      break;
    default:
      packed = as_type<uint4>(float4(pixel_0.r, pixel_1.r,
                                     pixel_2.r, pixel_3.r));
      break;
  }
  return packed;
}

inline uint TilePackFull32bppPixel(float4 pixel, uint format) {
  switch (format) {
    case kXenosFormat_8_8_8_8:
    case kXenosFormat_8_8_8_8_A:
    case kXenosFormat_8_8_8_8_AS_16_16_16_16:
      return TilePackR8G8B8A8UNorm(pixel);
    case kXenosFormat_2_10_10_10:
    case kXenosFormat_2_10_10_10_AS_16_16_16_16:
      return TilePackR10G10B10A2UNorm(pixel);
    case kXenosFormat_10_11_11:
    case kXenosFormat_10_11_11_AS_16_16_16_16:
      return TilePackR11G11B10UNorm(pixel.rgb);
    case kXenosFormat_11_11_10:
    case kXenosFormat_11_11_10_AS_16_16_16_16:
      return TilePackR10G11B11UNorm(pixel.rgb);
    case kXenosFormat_16_16_EDRAM:
    case kXenosFormat_16_16:
      return TilePackR16G16UNorm(pixel.rg);
    case kXenosFormat_16_16_FLOAT:
      return as_type<uint>(half2(pixel.r, pixel.g));
    default:
      return as_type<uint>(pixel.r);
  }
}

struct TilePackFull64bppResult {
  uint4 packed_01;
  uint4 packed_23;
};

inline TilePackFull64bppResult TilePackFull64bpp4Pixels(
    float4 pixel_0, float4 pixel_1, float4 pixel_2, float4 pixel_3,
    uint format) {
  TilePackFull64bppResult result;
  switch (format) {
    case kXenosFormat_16_16_16_16_EDRAM:
    case kXenosFormat_16_16_16_16: {
      uint2 packed_0 = TilePackR16G16B16A16UNorm(pixel_0);
      uint2 packed_1 = TilePackR16G16B16A16UNorm(pixel_1);
      uint2 packed_2 = TilePackR16G16B16A16UNorm(pixel_2);
      uint2 packed_3 = TilePackR16G16B16A16UNorm(pixel_3);
      result.packed_01 = uint4(packed_0, packed_1);
      result.packed_23 = uint4(packed_2, packed_3);
    } break;
    case kXenosFormat_16_16_16_16_FLOAT:
      result.packed_01 =
          uint4(as_type<uint>(half2(pixel_0.r, pixel_0.g)),
                as_type<uint>(half2(pixel_0.b, pixel_0.a)),
                as_type<uint>(half2(pixel_1.r, pixel_1.g)),
                as_type<uint>(half2(pixel_1.b, pixel_1.a)));
      result.packed_23 =
          uint4(as_type<uint>(half2(pixel_2.r, pixel_2.g)),
                as_type<uint>(half2(pixel_2.b, pixel_2.a)),
                as_type<uint>(half2(pixel_3.r, pixel_3.g)),
                as_type<uint>(half2(pixel_3.b, pixel_3.a)));
      break;
    default:
      result.packed_01 = as_type<uint4>(
          float4(pixel_0.rg, pixel_1.rg));
      result.packed_23 = as_type<uint4>(
          float4(pixel_2.rg, pixel_3.rg));
      break;
  }
  return result;
}

inline uint2 TilePackFull64bppPixel(float4 pixel, uint format) {
  switch (format) {
    case kXenosFormat_16_16_16_16_EDRAM:
    case kXenosFormat_16_16_16_16:
      return TilePackR16G16B16A16UNorm(pixel);
    case kXenosFormat_16_16_16_16_FLOAT:
      return uint2(as_type<uint>(half2(pixel.r, pixel.g)),
                   as_type<uint>(half2(pixel.b, pixel.a)));
    default:
      return as_type<uint2>(pixel.rg);
  }
}

inline bool TileIsRgb10A2FloatToRgba16UNormResolve() {
  return (kTileDirectHostResolveSourceFormat ==
              kXenosColorRTFormatRGB10A2Float ||
          kTileDirectHostResolveSourceFormat ==
              kXenosColorRTFormatRGB10A2FloatAsRGBA16) &&
         (kTileDirectHostResolveDestFormat ==
              kXenosFormat_16_16_16_16_EDRAM ||
          kTileDirectHostResolveDestFormat == kXenosFormat_16_16_16_16);
}

inline uint2 TilePackRoundedRgb10A2FloatToRgba16UNorm(float4 color,
                                                       float exp_bias,
                                                       bool dest_swap) {
  color.xyz *= exp_bias;
  if (dest_swap) {
    color = color.bgra;
  }
  return TilePackR16G16B16A16UNorm(color);
}

inline float4 TileApplyFullColorExpBiasAndSwap(TileResolveInfo info,
                                               float4 color,
                                               float exp_bias) {
  if ((info.edram_format == kXenosColorRTFormatRGB10A2Float ||
       info.edram_format == kXenosColorRTFormatRGB10A2FloatAsRGBA16) &&
      kTileDirectHostResolveDestFormat != kXenosFormat_16_16_16_16_FLOAT &&
      kTileDirectHostResolveDestFormat != kXenosFormat_32_32_32_32_FLOAT) {
    color.xyz *= exp_bias;
  } else {
    color *= exp_bias;
  }
  if (info.dest_swap) {
    color = color.bgra;
  }
  return color;
}

inline float TileSelectFull8Red(TileResolveInfo info, float4 color) {
  if (!info.dest_swap) {
    return color.r;
  }
  switch (kTileDirectHostResolveSourceFormat) {
    case kXenosColorRTFormatRGBA8:
    case kXenosColorRTFormatRGBA8Gamma:
    case kXenosColorRTFormatRGB10A2:
    case kXenosColorRTFormatRGB10A2Float:
    case kXenosColorRTFormatRGB10A2AsRGB10A2:
    case kXenosColorRTFormatRGB10A2FloatAsRGBA16:
    case kXenosColorRTFormatRGBA16:
    case kXenosColorRTFormatRGBA16Float:
      return color.b;
    case kXenosColorRTFormatRG32Float:
      return color.g;
    default:
      return color.r;
  }
}

struct TileFullLoadResult {
  float4 color;
  float exp_bias;
  bool valid;
};

#define DEFINE_TILE_DIRECT_HOST_RESOLVE_COLOR(ID)                             \
struct ColorBlock##ID {                                                       \
  float4 color [[color(ID)]];                                                  \
};                                                                            \
inline TileFullLoadResult TileReadFullColorSample##ID(                        \
    imageblock<ColorBlock##ID, imageblock_layout_implicit> block,             \
    constant TileDirectHostResolveConstants& c, ushort2 local_tid,            \
    uint2 pixel_pos, uint sample_index) {                                      \
  TileFullLoadResult result;                                                   \
  result.color = float4(0.0f);                                                \
  result.exp_bias = 1.0f;                                                     \
  result.valid = false;                                                       \
  uint2 source_sample = pixel_pos;                                             \
  uint sample_id = 0u;                                                        \
  if (c.msaa_samples == kXenosMsaaSamples2X) {                                \
    uint sample_y = sample_index & 1u;                                        \
    source_sample = uint2(pixel_pos.x, (pixel_pos.y << 1u) + sample_y);       \
    sample_id = sample_y != 0u ? c.msaa_2x_sample_1                          \
                               : c.msaa_2x_sample_0;                         \
  } else if (c.msaa_samples != kXenosMsaaSamples1X) {                         \
    uint2 sample_offset = TileSampleOffsetForIndex(sample_index);             \
    source_sample = (pixel_pos << 1u) + sample_offset;                        \
    sample_id = sample_offset.x | (sample_offset.y << 1u);                   \
  }                                                                           \
  result.valid = TilePositionInResolveRect(c, source_sample);                 \
  if (!result.valid) {                                                        \
    return result;                                                            \
  }                                                                           \
  if (c.msaa_samples == kXenosMsaaSamples1X) {                                \
    result.color = block.read(local_tid).color;                               \
  } else {                                                                    \
    result.color = block.read(local_tid, ushort(sample_id),                   \
                              imageblock_data_rate::sample).color;           \
  }                                                                           \
  result.color = TileRoundTripSourceColor(c, result.color);                  \
  return result;                                                              \
}                                                                             \
inline TileFullLoadResult TileReadFullRgb10A2FloatSample##ID(                \
    imageblock<ColorBlock##ID, imageblock_layout_implicit> block,             \
    constant TileDirectHostResolveConstants& c, ushort2 local_tid,            \
    uint2 pixel_pos, uint sample_index) {                                      \
  TileFullLoadResult result;                                                   \
  result.color = float4(0.0f);                                                \
  result.exp_bias = 1.0f;                                                     \
  result.valid = false;                                                       \
  uint2 source_sample = pixel_pos;                                             \
  uint sample_id = 0u;                                                        \
  if (c.msaa_samples == kXenosMsaaSamples2X) {                                \
    uint sample_y = sample_index & 1u;                                        \
    source_sample = uint2(pixel_pos.x, (pixel_pos.y << 1u) + sample_y);       \
    sample_id = sample_y != 0u ? c.msaa_2x_sample_1                          \
                               : c.msaa_2x_sample_0;                         \
  } else if (c.msaa_samples != kXenosMsaaSamples1X) {                         \
    uint2 sample_offset = TileSampleOffsetForIndex(sample_index);             \
    source_sample = (pixel_pos << 1u) + sample_offset;                        \
    sample_id = sample_offset.x | (sample_offset.y << 1u);                   \
  }                                                                           \
  result.valid = TilePositionInResolveRect(c, source_sample);                 \
  if (!result.valid) {                                                        \
    return result;                                                            \
  }                                                                           \
  if (c.msaa_samples == kXenosMsaaSamples1X) {                                \
    result.color = block.read(local_tid).color;                               \
  } else {                                                                    \
    result.color = block.read(local_tid, ushort(sample_id),                   \
                              imageblock_data_rate::sample).color;           \
  }                                                                           \
  result.color = TileRoundTripR10G10B10A2Float(result.color);                 \
  return result;                                                              \
}                                                                             \
inline TileFullLoadResult TileLoadFullRawColor##ID(                           \
    imageblock<ColorBlock##ID, imageblock_layout_implicit> block,             \
    constant TileDirectHostResolveConstants& c, TileResolveInfo info,         \
    ushort2 tid, uint2 pos, uint lane) {                                      \
  ushort2 local_tid = ushort2(tid.x + ushort(lane), tid.y);                  \
  uint2 pixel_pos = pos + uint2(lane, 0u);                                    \
  uint first_sample = TileFirstSampleIndex(info.sample_select);               \
  TileFullLoadResult result = TileReadFullColorSample##ID(                   \
      block, c, local_tid, pixel_pos, first_sample);                         \
  result.exp_bias = info.dest_exp_bias_factor;                               \
  if (!result.valid) {                                                        \
    return result;                                                            \
  }                                                                           \
  if (info.sample_select >= kXenosCopySampleSelect01) {                      \
    result.exp_bias *= 0.5f;                                                  \
    TileFullLoadResult sample = TileReadFullColorSample##ID(                 \
        block, c, local_tid, pixel_pos, first_sample + 1u);                  \
    if (!sample.valid) {                                                       \
      result.valid = false;                                                    \
      return result;                                                           \
    }                                                                          \
    result.color += sample.color;                                             \
    if (info.sample_select >= kXenosCopySampleSelect0123) {                  \
      result.exp_bias *= 0.5f;                                                \
      sample = TileReadFullColorSample##ID(block, c, local_tid, pixel_pos,   \
                                           first_sample + 2u);               \
      if (!sample.valid) {                                                     \
        result.valid = false;                                                  \
        return result;                                                         \
      }                                                                        \
      result.color += sample.color;                                           \
      sample = TileReadFullColorSample##ID(block, c, local_tid, pixel_pos,   \
                                           first_sample + 3u);               \
      if (!sample.valid) {                                                     \
        result.valid = false;                                                  \
        return result;                                                         \
      }                                                                        \
      result.color += sample.color;                                           \
    }                                                                         \
  }                                                                           \
  return result;                                                              \
}                                                                             \
inline TileFullLoadResult TileLoadFullRgb10A2FloatColor##ID(                 \
    imageblock<ColorBlock##ID, imageblock_layout_implicit> block,             \
    constant TileDirectHostResolveConstants& c, TileResolveInfo info,         \
    ushort2 tid, uint2 pos, uint lane) {                                      \
  ushort2 local_tid = ushort2(tid.x + ushort(lane), tid.y);                  \
  uint2 pixel_pos = pos + uint2(lane, 0u);                                    \
  uint first_sample = TileFirstSampleIndex(info.sample_select);               \
  TileFullLoadResult result = TileReadFullRgb10A2FloatSample##ID(            \
      block, c, local_tid, pixel_pos, first_sample);                         \
  result.exp_bias = info.dest_exp_bias_factor;                               \
  if (!result.valid) {                                                        \
    return result;                                                            \
  }                                                                           \
  if (info.sample_select >= kXenosCopySampleSelect01) {                      \
    result.exp_bias *= 0.5f;                                                  \
    TileFullLoadResult sample = TileReadFullRgb10A2FloatSample##ID(          \
        block, c, local_tid, pixel_pos, first_sample + 1u);                  \
    if (!sample.valid) {                                                       \
      result.valid = false;                                                    \
      return result;                                                           \
    }                                                                          \
    result.color += sample.color;                                             \
    if (info.sample_select >= kXenosCopySampleSelect0123) {                  \
      result.exp_bias *= 0.5f;                                                \
      sample = TileReadFullRgb10A2FloatSample##ID(                           \
          block, c, local_tid, pixel_pos, first_sample + 2u);                 \
      if (!sample.valid) {                                                     \
        result.valid = false;                                                  \
        return result;                                                         \
      }                                                                        \
      result.color += sample.color;                                           \
      sample = TileReadFullRgb10A2FloatSample##ID(                           \
          block, c, local_tid, pixel_pos, first_sample + 3u);                 \
      if (!sample.valid) {                                                     \
        result.valid = false;                                                  \
        return result;                                                         \
      }                                                                        \
      result.color += sample.color;                                           \
    }                                                                         \
  }                                                                           \
  return result;                                                              \
}                                                                             \
kernel void xenia_tile_direct_host_resolve_color##ID(                         \
    imageblock<ColorBlock##ID, imageblock_layout_implicit> block,             \
    constant TileDirectHostResolveConstants& c [[buffer(0)]],                 \
    device uint* dest [[buffer(1)]],                                           \
    ushort2 tid [[thread_position_in_threadgroup]],                           \
    uint2 grid_pos [[thread_position_in_grid]],                               \
    uint2 tile_pos [[threadgroup_position_in_grid]]) {                        \
  ushort2 local_tid = tid;                                                     \
  uint2 pos = grid_pos;                                                        \
  if (kTileDirectHostResolveIsFullColor) {                                    \
    /* 8x8 full-color dispatches are remapped to 4x16 four-pixel blocks. */   \
    uint threads_per_tile_width =                                             \
        kTileDirectHostResolveFullPixelsPerThread == 4u ? 8u : 16u;           \
    uint block_index = uint(tid.y) * threads_per_tile_width + uint(tid.x);    \
    uint blocks_per_row =                                                     \
        kTileDirectHostResolveTileWidth /                                     \
        kTileDirectHostResolveFullPixelsPerThread;                            \
    local_tid = ushort2(                                                      \
        ushort((block_index - (block_index / blocks_per_row) *                \
                blocks_per_row) * kTileDirectHostResolveFullPixelsPerThread), \
        ushort(block_index / blocks_per_row));                                \
    pos = tile_pos * uint2(kTileDirectHostResolveTileWidth,                   \
                           kTileDirectHostResolveTileHeight) +                \
          uint2(local_tid);                                                    \
  }                                                                           \
  if (pos.x >= c.source_width || pos.y >= c.source_height) {                  \
    return;                                                                   \
  }                                                                           \
  TileResolveInfo info = TileGetResolveInfo(c);                               \
  if (pos.x < info.edram_offset_scaled.x ||                                  \
      pos.y < info.edram_offset_scaled.y) {                                   \
    return;                                                                   \
  }                                                                           \
  uint2 pixel_index = pos - info.edram_offset_scaled;                         \
  if (pixel_index.x >= info.width_scaled ||                                   \
      pixel_index.y >= c.height_scaled) {                                     \
    return;                                                                   \
  }                                                                           \
  if (kTileDirectHostResolveIsFullColor) {                                    \
    if (kTileDirectHostResolveFullPixelsPerThread == 0u ||                   \
        pixel_index.x + kTileDirectHostResolveFullPixelsPerThread >          \
            info.width_scaled ||                                             \
        pos.x + kTileDirectHostResolveFullPixelsPerThread > c.source_width) { \
      return;                                                                 \
    }                                                                         \
    if (kTileDirectHostResolveFullPixelsPerThread == 1u) {                   \
      if (kTileDirectHostResolveFullDestBppLog2 == 3u &&                     \
          TileIsRgb10A2FloatToRgba16UNormResolve()) {                        \
        TileFullLoadResult r0 = TileLoadFullRgb10A2FloatColor##ID(           \
            block, c, info, local_tid, pos, 0u);                              \
        if (!r0.valid) {                                                       \
          return;                                                             \
        }                                                                     \
        uint address = TileDestPixelAddress(                                  \
            info, pixel_index, kTileDirectHostResolveFullDestBppLog2);       \
        uint dest_index = address >> 2u;                                     \
        uint2 packed = XeEndianSwap64(                                       \
            TilePackRoundedRgb10A2FloatToRgba16UNorm(                        \
                r0.color, r0.exp_bias, info.dest_swap),                      \
            info.dest_endian_128);                                            \
        dest[dest_index] = packed.x;                                          \
        dest[dest_index + 1u] = packed.y;                                    \
        return;                                                               \
      }                                                                       \
      TileFullLoadResult r0 = TileLoadFullRawColor##ID(                      \
          block, c, info, local_tid, pos, 0u);                                \
      if (!r0.valid) {                                                         \
        return;                                                               \
      }                                                                       \
      float4 pixel_0 = TileApplyFullColorExpBiasAndSwap(                    \
          info, r0.color, r0.exp_bias);                                       \
      uint address = TileDestPixelAddress(                                   \
          info, pixel_index, kTileDirectHostResolveFullDestBppLog2);         \
      uint dest_index = address >> 2u;                                       \
      if (kTileDirectHostResolveFullDestBppLog2 == 2u) {                     \
        dest[dest_index] = XeEndianSwap32(                                   \
            TilePackFull32bppPixel(                                           \
                pixel_0, kTileDirectHostResolveDestFormat),                  \
            info.dest_endian_128);                                            \
        return;                                                               \
      }                                                                       \
      if (kTileDirectHostResolveFullDestBppLog2 == 3u) {                     \
        uint2 packed = XeEndianSwap64(                                       \
            TilePackFull64bppPixel(                                           \
                pixel_0, kTileDirectHostResolveDestFormat),                  \
            info.dest_endian_128);                                            \
        dest[dest_index] = packed.x;                                          \
        dest[dest_index + 1u] = packed.y;                                    \
        return;                                                               \
      }                                                                       \
      if (kTileDirectHostResolveFullDestBppLog2 == 4u) {                     \
        uint4 packed = XeEndianSwap128(as_type<uint4>(pixel_0),              \
                                       info.dest_endian_128);                \
        dest[dest_index] = packed.x;                                          \
        dest[dest_index + 1u] = packed.y;                                    \
        dest[dest_index + 2u] = packed.z;                                    \
        dest[dest_index + 3u] = packed.w;                                    \
      }                                                                       \
      return;                                                                 \
    }                                                                         \
    if (kTileDirectHostResolveFullDestBppLog2 == 0u) {                       \
      TileFullLoadResult r0 = TileLoadFullRawColor##ID(                      \
          block, c, info, local_tid, pos, 0u);                                \
      TileFullLoadResult r1 = TileLoadFullRawColor##ID(                      \
          block, c, info, local_tid, pos, 1u);                                      \
      TileFullLoadResult r2 = TileLoadFullRawColor##ID(                      \
          block, c, info, local_tid, pos, 2u);                                      \
      TileFullLoadResult r3 = TileLoadFullRawColor##ID(                      \
          block, c, info, local_tid, pos, 3u);                                      \
      bool valid = r0.valid && r1.valid && r2.valid && r3.valid;             \
      float4 pixels_0123 = float4(                                           \
          TileSelectFull8Red(info, r0.color) * r0.exp_bias,                  \
          TileSelectFull8Red(info, r1.color) * r1.exp_bias,                  \
          TileSelectFull8Red(info, r2.color) * r2.exp_bias,                  \
          TileSelectFull8Red(info, r3.color) * r3.exp_bias);                 \
      uint address = TileDestPixelAddress(info, pixel_index, 0u);            \
      uint dest_index = address >> 2u;                                       \
      if (kTileDirectHostResolveFullPixelsPerThread == 4u) {                 \
        if (!valid) {                                                         \
          return;                                                             \
        }                                                                     \
        dest[dest_index] = TilePackR8G8B8A8UNorm(pixels_0123);               \
        return;                                                               \
      }                                                                       \
      if (kTileDirectHostResolveFullPixelsPerThread != 8u) {                 \
        return;                                                               \
      }                                                                       \
      TileFullLoadResult r4 = TileLoadFullRawColor##ID(                      \
          block, c, info, local_tid, pos, 4u);                                      \
      TileFullLoadResult r5 = TileLoadFullRawColor##ID(                      \
          block, c, info, local_tid, pos, 5u);                                      \
      TileFullLoadResult r6 = TileLoadFullRawColor##ID(                      \
          block, c, info, local_tid, pos, 6u);                                      \
      TileFullLoadResult r7 = TileLoadFullRawColor##ID(                      \
          block, c, info, local_tid, pos, 7u);                                      \
      valid = valid && r4.valid && r5.valid && r6.valid && r7.valid;         \
      if (!valid) {                                                           \
        return;                                                               \
      }                                                                       \
      float4 pixels_4567 = float4(                                           \
          TileSelectFull8Red(info, r4.color) * r4.exp_bias,                  \
          TileSelectFull8Red(info, r5.color) * r5.exp_bias,                  \
          TileSelectFull8Red(info, r6.color) * r6.exp_bias,                  \
          TileSelectFull8Red(info, r7.color) * r7.exp_bias);                 \
      dest[dest_index] = TilePackR8G8B8A8UNorm(pixels_0123);                 \
      dest[dest_index + 1u] = TilePackR8G8B8A8UNorm(pixels_4567);            \
      return;                                                                 \
    }                                                                         \
    if (kTileDirectHostResolveFullDestBppLog2 == 4u) {                       \
      TileFullLoadResult r0 = TileLoadFullRawColor##ID(                      \
          block, c, info, local_tid, pos, 0u);                                      \
      TileFullLoadResult r1 = TileLoadFullRawColor##ID(                      \
          block, c, info, local_tid, pos, 1u);                                      \
      if (!r0.valid || !r1.valid) {                                           \
        return;                                                               \
      }                                                                       \
      float4 pixel_0 = TileApplyFullColorExpBiasAndSwap(                    \
          info, r0.color, r0.exp_bias);                                       \
      float4 pixel_1 = TileApplyFullColorExpBiasAndSwap(                    \
          info, r1.color, r1.exp_bias);                                       \
      uint address = TileDestPixelAddress(info, pixel_index, 4u);            \
      uint dest_index = address >> 2u;                                       \
      uint4 packed = XeEndianSwap128(as_type<uint4>(pixel_0),                \
                                     info.dest_endian_128);                  \
      dest[dest_index] = packed.x;                                            \
      dest[dest_index + 1u] = packed.y;                                      \
      dest[dest_index + 2u] = packed.z;                                      \
      dest[dest_index + 3u] = packed.w;                                      \
      address = TileDestPixelAddress(info, pixel_index + uint2(1u, 0u),      \
                                     4u);                                    \
      dest_index = address >> 2u;                                            \
      packed = XeEndianSwap128(as_type<uint4>(pixel_1),                     \
                               info.dest_endian_128);                       \
      dest[dest_index] = packed.x;                                            \
      dest[dest_index + 1u] = packed.y;                                      \
      dest[dest_index + 2u] = packed.z;                                      \
      dest[dest_index + 3u] = packed.w;                                      \
      return;                                                                 \
    }                                                                         \
    TileFullLoadResult r0 = TileLoadFullRawColor##ID(                        \
        block, c, info, local_tid, pos, 0u);                                        \
    TileFullLoadResult r1 = TileLoadFullRawColor##ID(                        \
        block, c, info, local_tid, pos, 1u);                                        \
    TileFullLoadResult r2 = TileLoadFullRawColor##ID(                        \
        block, c, info, local_tid, pos, 2u);                                        \
    TileFullLoadResult r3 = TileLoadFullRawColor##ID(                        \
        block, c, info, local_tid, pos, 3u);                                        \
    if (!r0.valid || !r1.valid || !r2.valid || !r3.valid) {                  \
      return;                                                                 \
    }                                                                         \
    float4 pixel_0 = TileApplyFullColorExpBiasAndSwap(                      \
        info, r0.color, r0.exp_bias);                                         \
    float4 pixel_1 = TileApplyFullColorExpBiasAndSwap(                      \
        info, r1.color, r1.exp_bias);                                         \
    float4 pixel_2 = TileApplyFullColorExpBiasAndSwap(                      \
        info, r2.color, r2.exp_bias);                                         \
    float4 pixel_3 = TileApplyFullColorExpBiasAndSwap(                      \
        info, r3.color, r3.exp_bias);                                         \
    uint address = TileDestPixelAddress(                                     \
        info, pixel_index, kTileDirectHostResolveFullDestBppLog2);           \
    uint dest_index = address >> 2u;                                         \
    if (kTileDirectHostResolveFullDestBppLog2 == 1u) {                       \
      uint2 packed = XeEndianSwap16(                                         \
          TilePackFull16bpp4Pixels(pixel_0, pixel_1, pixel_2, pixel_3,       \
                                   kTileDirectHostResolveDestFormat),         \
          info.dest_endian_128);                                             \
      dest[dest_index] = packed.x;                                            \
      dest[dest_index + 1u] = packed.y;                                      \
      return;                                                                 \
    }                                                                         \
    if (kTileDirectHostResolveFullDestBppLog2 == 2u) {                       \
      uint4 packed = XeEndianSwap32(                                         \
          TilePackFull32bpp4Pixels(pixel_0, pixel_1, pixel_2, pixel_3,       \
                                   kTileDirectHostResolveDestFormat),         \
          info.dest_endian_128);                                             \
      dest[dest_index] = packed.x;                                            \
      dest[dest_index + 1u] = packed.y;                                      \
      dest[dest_index + 2u] = packed.z;                                      \
      dest[dest_index + 3u] = packed.w;                                      \
      return;                                                                 \
    }                                                                         \
    if (kTileDirectHostResolveFullDestBppLog2 != 3u) {                       \
      return;                                                                 \
    }                                                                         \
    TilePackFull64bppResult packed_64 = TilePackFull64bpp4Pixels(           \
        pixel_0, pixel_1, pixel_2, pixel_3,                                  \
        kTileDirectHostResolveDestFormat);                                    \
    uint4 packed = XeEndianSwap64(packed_64.packed_01,                      \
                                  info.dest_endian_128);                    \
    dest[dest_index] = packed.x;                                              \
    dest[dest_index + 1u] = packed.y;                                        \
    dest[dest_index + 2u] = packed.z;                                        \
    dest[dest_index + 3u] = packed.w;                                        \
    address = TileDestPixelAddress(info, pixel_index + uint2(2u, 0u), 3u);   \
    dest_index = address >> 2u;                                              \
    packed = XeEndianSwap64(packed_64.packed_23,                            \
                            info.dest_endian_128);                          \
    dest[dest_index] = packed.x;                                              \
    dest[dest_index + 1u] = packed.y;                                        \
    dest[dest_index + 2u] = packed.z;                                        \
    dest[dest_index + 3u] = packed.w;                                        \
    return;                                                                   \
  }                                                                           \
  uint selected_sample = TileFirstSampleIndex(info.sample_select);            \
  uint2 source_sample = pos;                                                   \
  uint sample_id = 0u;                                                        \
  if (c.msaa_samples == kXenosMsaaSamples2X) {                                \
    uint sample_y = selected_sample & 1u;                                     \
    source_sample = uint2(pos.x, (pos.y << 1u) + sample_y);                  \
    sample_id = sample_y != 0u ? c.msaa_2x_sample_1                          \
                               : c.msaa_2x_sample_0;                         \
  } else if (c.msaa_samples != kXenosMsaaSamples1X) {                         \
    uint2 sample_offset = TileSampleOffsetForIndex(selected_sample);          \
    source_sample = (pos << 1u) + sample_offset;                              \
    sample_id = sample_offset.x | (sample_offset.y << 1u);                   \
  }                                                                           \
  if (!TilePositionInResolveRect(c, source_sample)) {                         \
    return;                                                                   \
  }                                                                           \
  float4 color;                                                               \
  if (c.msaa_samples == kXenosMsaaSamples1X) {                                \
    color = block.read(local_tid).color;                                      \
  } else {                                                                    \
    color = block.read(local_tid, ushort(sample_id),                          \
                       imageblock_data_rate::sample).color;                  \
  }                                                                           \
  if (info.dest_swap) {                                                       \
    color = color.bgra;                                                       \
  }                                                                           \
  uint address = TileDestPixelAddress(info, pixel_index,                      \
                                      c.is_64bpp != 0u ? 3u : 2u);           \
  uint dest_index = address >> 2u;                                            \
  if (c.is_64bpp != 0u) {                                                     \
    uint2 packed = XeEndianSwap64(TilePack64(color, c.source_format),         \
                                  info.dest_endian_128);                     \
    dest[dest_index] = packed.x;                                               \
    dest[dest_index + 1u] = packed.y;                                         \
  } else {                                                                    \
    dest[dest_index] =                                                        \
        XeEndianSwap32(TilePack32(color, c.source_format),                    \
                       info.dest_endian_128);                                 \
  }                                                                           \
}

DEFINE_TILE_DIRECT_HOST_RESOLVE_COLOR(0)
DEFINE_TILE_DIRECT_HOST_RESOLVE_COLOR(1)
DEFINE_TILE_DIRECT_HOST_RESOLVE_COLOR(2)
DEFINE_TILE_DIRECT_HOST_RESOLVE_COLOR(3)
#undef DEFINE_TILE_DIRECT_HOST_RESOLVE_COLOR
)METAL";

  NS::Error* error = nullptr;
  MTL::CompileOptions* compile_options = MTL::CompileOptions::alloc()->init();
  compile_options->setFastMathEnabled(true);
  compile_options->setLanguageVersion(MTL::LanguageVersion2_4);
  auto source_str = NS::String::string(kTileDirectHostResolveSource,
                                       NS::UTF8StringEncoding);
  tile_direct_host_resolve_library_ =
      device_->newLibrary(source_str, compile_options, &error);
  compile_options->release();
  if (!tile_direct_host_resolve_library_) {
    XELOGE(
        "MetalRenderTargetCache: failed to compile tile direct host resolve "
        "library: {}",
        error && error->localizedDescription()
            ? error->localizedDescription()->utf8String()
            : "unknown error");
  }
  return tile_direct_host_resolve_library_;
}

MTL::RenderPipelineState*
MetalRenderTargetCache::GetOrCreateTileDirectHostResolvePipeline(
    uint32_t color_attachment_index, uint32_t sample_count, bool full_color,
    uint32_t full_dest_bpp_log2, uint32_t full_pixels_per_thread,
    uint32_t full_source_format, uint32_t full_dest_format,
    const TransferColorAttachmentFormats& color_attachment_formats) {
  if (color_attachment_index >= xenos::kMaxColorRenderTargets) {
    return nullptr;
  }
  TileDirectHostResolvePipelineKey key = {};
  key.color_attachment_index = color_attachment_index;
  key.sample_count = sample_count ? sample_count : 1;
  key.full_color = full_color ? 1u : 0u;
  key.full_dest_bpp_log2 = full_color ? full_dest_bpp_log2 : 0u;
  key.full_pixels_per_thread = full_color ? full_pixels_per_thread : 0u;
  key.full_source_format = full_color ? full_source_format : 0u;
  key.full_dest_format = full_color ? full_dest_format : 0u;
  key.color_attachment_formats = color_attachment_formats;
  auto it = tile_direct_host_resolve_pipelines_.find(key);
  if (it != tile_direct_host_resolve_pipelines_.end()) {
    return it->second;
  }

  MTL::Library* library = GetOrCreateTileDirectHostResolveLibrary();
  if (!library) {
    return nullptr;
  }
  std::string function_name =
      fmt::format("xenia_tile_direct_host_resolve_color{}",
                  color_attachment_index);
  NS::String* function_name_ns =
      NS::String::string(function_name.c_str(), NS::UTF8StringEncoding);
  MTL::FunctionConstantValues* function_constants =
      MTL::FunctionConstantValues::alloc()->init();
  bool function_full_color = key.full_color != 0;
  function_constants->setConstantValue(&function_full_color, MTL::DataTypeBool,
                                       NS::UInteger(0));
  function_constants->setConstantValue(&key.full_dest_bpp_log2,
                                       MTL::DataTypeUInt, NS::UInteger(1));
  function_constants->setConstantValue(&key.full_pixels_per_thread,
                                       MTL::DataTypeUInt, NS::UInteger(2));
  function_constants->setConstantValue(&key.full_source_format,
                                       MTL::DataTypeUInt, NS::UInteger(3));
  function_constants->setConstantValue(&key.full_dest_format,
                                       MTL::DataTypeUInt, NS::UInteger(4));
  NS::Error* function_error = nullptr;
  MTL::Function* tile_function =
      library->newFunction(function_name_ns, function_constants,
                           &function_error);
  function_constants->release();
  if (!tile_function) {
    XELOGE(
        "MetalRenderTargetCache: failed to specialize tile direct host resolve "
        "function {}: {}",
        function_name,
        function_error && function_error->localizedDescription()
            ? function_error->localizedDescription()->utf8String()
            : "unknown error");
    return nullptr;
  }

  MTL::TileRenderPipelineDescriptor* desc =
      MTL::TileRenderPipelineDescriptor::alloc()->init();
  desc->setTileFunction(tile_function);
  // Label the specialized pipeline so each format/bpp/MSAA variant is
  // distinguishable in the Xcode GPU trace. The function name is identical
  // across variants (specialization is via function constants), so without
  // this every variant shows as the same "xenia_tile_direct_host_resolve_*".
  std::string pipeline_label =
      key.full_color
          ? fmt::format(
                "{} full dst{}bpp src_fmt={} dst_fmt={} {}xMSAA ppt{}",
                function_name, 8u << key.full_dest_bpp_log2,
                key.full_source_format, key.full_dest_format,
                key.sample_count, key.full_pixels_per_thread)
          : fmt::format("{} raw {}xMSAA", function_name, key.sample_count);
  desc->setLabel(
      NS::String::string(pipeline_label.c_str(), NS::UTF8StringEncoding));
  desc->setRasterSampleCount(key.sample_count);
  desc->setThreadgroupSizeMatchesTileSize(!key.full_color ||
                                          key.full_pixels_per_thread == 1u);
  desc->setMaxTotalThreadsPerThreadgroup(
      kTileDirectHostResolveTileWidth * kTileDirectHostResolveTileHeight);
  for (uint32_t i = 0; i < xenos::kMaxColorRenderTargets; ++i) {
    desc->colorAttachments()->object(i)->setPixelFormat(
        key.color_attachment_formats[i]);
  }

  NS::Error* error = nullptr;
  MTL::RenderPipelineState* pipeline = device_->newRenderPipelineState(
      desc, MTL::PipelineOptionNone,
      static_cast<MTL::AutoreleasedRenderPipelineReflection*>(nullptr), &error);
  desc->release();
  tile_function->release();
  if (!pipeline) {
    XELOGE(
        "MetalRenderTargetCache: failed to create tile direct host resolve "
        "pipeline: {}",
        error && error->localizedDescription()
            ? error->localizedDescription()->utf8String()
            : "unknown error");
    return nullptr;
  }

  tile_direct_host_resolve_pipelines_.emplace(key, pipeline);
  return pipeline;
}

// TODO(xenios-jp): Keep the tile direct-host resolve telemetry tied to the
// semantic buckets, not just to Metal implementation failures.
//
// Existing direct-host counters:
// - direct_host_attempt/success: copy exports covered by the current
//   host-texture-to-guest-memory direct resolve path.
// - direct_host_reject_gamma/exp_bias/format_mismatch/sample_select/
//   depth_no_fast: semantic cases where the existing direct-host path does not
//   promise byte-identical Xenos output.
//
// Tile-path eligibility counters:
// - tile_candidate_total/accept: direct-host-safe copies that are also
//   source-side candidates for doing the export before the active render pass
//   stores the attachment.
// - tile_accept_fast: currently implemented. This is the fast bitwise color
//   subset with one fully covered source rectangle, a single sample select, no
//   gamma or exponent bias, no scaled resolve, a non-uint transfer view, and a
//   source that is the current color attachment.
// - tile_accept_full_color: implemented for the same one-rectangle,
//   non-scaled, non-uint, active-color-attachment window as tile_accept_fast,
//   using tile/imageblock loads plus the full color resolve destination packing
//   path. It intentionally still rejects gamma, depth, copy_clear, uint
//   transfer views, and scaled resolves.
// - tile_reject_source_not_current_rt/multi_source_rect: not tile-local enough
//   for this prototype. Multi-rect/alias cases need either the old compute path
//   or a different per-rectangle strategy.
// - tile_reject_depth: possible only with a separate depth/stencil tile path
//   that preserves Xenos depth rounding/layout rules; not a color-path tweak.
// - tile_reject_copy_clear: should become a deferred EDRAM ownership/clear
//   transaction. It is not just a copy shader variant because the clear is a
//   later visibility rule.
// - tile_reject_uint_transfer_view: technically possible only if the tile
//   shader gets a uint/packed path that exactly matches the transfer view. Keep
//   rejected until that representation is proven.
//
// Tile execution counters:
// - tile_execute_attempt/success/reject measure the live in-pass replacement
//   of "end render encoder -> compute direct-host resolve".
// - tile_execute_reject_no_active means telemetry says the copy was source-side
//   eligible, but by execution time there was no active render encoder left.
//   Fixing this is scheduling/lifetime work, not shader work.
// - tile_execute_reject_full_color/depth/copy_clear/uint/format mirror the
//   feasibility buckets above. full_color should now drain when the source is
//   still active; remaining full_color rejects point at shader/pipeline bugs.
//
// Store elision:
// - store_dontcare_eligible is only source-side eligibility: "if liveness later
//   proves this host RT does not escape, the tile path could discard the color
//   attachment store".
// - store_dontcare_skipped_disabled/attempt/applied explain why source-eligible
//   cases did or did not call setColorStoreAction(DontCare). The call is
//   mechanically easy, but semantically valid only when no later host-RT
//   observer needs the attachment contents. Keep the cvar opt-in until real RT
//   liveness proof exists.
//
// Representative prototype telemetry:
// - Fast subset covered: 360/360 in one sample, 478/480 in another; the missing
//   fast cases were no-active-encoder timing, not shader capability.
// - Not-yet-covered recurring buckets after the full_color expansion: depth,
//   copy_clear, uint transfer view, format mismatch, scaled resolves, and
//   no_active scheduling.
bool MetalRenderTargetCache::TryTileDirectHostResolveCopy(
    const ResolvePlan& resolve_plan,
    MTL::RenderCommandEncoder* active_render_encoder,
    MTL::RenderPassDescriptor* active_render_pass_descriptor,
    uint32_t inactive_last_end_reason, uint32_t& written_address,
    uint32_t& written_length) {
  written_address = 0;
  written_length = 0;
  if (!::cvars::metal_tile_direct_host_resolve ||
      !::cvars::metal_direct_host_resolve) {
    return false;
  }
  TelemetryStats::ResolveDirectHostTelemetry& direct_telemetry =
      telemetry_.resolve_direct_host;
  ++direct_telemetry.tile_execute_attempt;
  auto reject = [&]() {
    ++direct_telemetry.tile_execute_reject;
    return false;
  };
  auto reject_with = [&](uint64_t& counter) {
    ++counter;
    return reject();
  };

  if (GetPath() != Path::kHostRenderTargets || !resolve_plan.valid ||
      resolve_plan.noop || !resolve_plan.needs_copy_export) {
    return reject_with(direct_telemetry.tile_execute_reject_invalid);
  }
  if (!active_render_encoder || !active_render_pass_descriptor) {
    ++direct_telemetry.tile_execute_reject_no_active;
    if (inactive_last_end_reason >=
        direct_telemetry.tile_execute_reject_no_active_last_end_reasons
            .size()) {
      inactive_last_end_reason = 0;
    }
    ++direct_telemetry.tile_execute_reject_no_active_last_end_reasons
          [inactive_last_end_reason];
    return reject();
  }
  if (resolve_plan.needs_resolve_clear) {
    return reject_with(direct_telemetry.tile_execute_reject_copy_clear);
  }
  if (IsDrawResolutionScaled()) {
    return reject_with(direct_telemetry.tile_execute_reject_scaled);
  }

  const draw_util::ResolveInfo& resolve_info = resolve_plan.resolve_info;
  if (resolve_info.IsCopyingDepth()) {
    return reject_with(direct_telemetry.tile_execute_reject_depth);
  }

  draw_util::ResolveCopyShaderConstants copy_constants;
  uint32_t group_count_x = 0, group_count_y = 0;
  draw_util::ResolveCopyShaderIndex copy_shader =
      resolve_info.GetCopyShader(draw_resolution_scale_x(),
                                 draw_resolution_scale_y(), copy_constants,
                                 group_count_x, group_count_y);
  if (!group_count_x || !group_count_y) {
    return reject_with(direct_telemetry.tile_execute_reject_shader);
  }
  const bool copy_shader_is_fast =
      IsResolveDirectHostRTFastCandidate(copy_shader);
  const bool copy_shader_is_full_color =
      IsResolveDirectHostRTFullColorCandidate(copy_shader);
  if (!copy_shader_is_fast && !copy_shader_is_full_color) {
    return reject_with(direct_telemetry.tile_execute_reject_shader);
  }

  xenos::ColorRenderTargetFormat resolve_color_format =
      xenos::ColorRenderTargetFormat(resolve_info.color_edram_info.format);
  if (resolve_color_format ==
      xenos::ColorRenderTargetFormat::k_8_8_8_8_GAMMA) {
    return reject_with(direct_telemetry.tile_execute_reject_gamma);
  }
  xenos::CopySampleSelect sample_select =
      resolve_info.copy_dest_coordinate_info.copy_sample_select;
  if (copy_shader_is_fast) {
    if (!xenos::IsSingleCopySampleSelected(sample_select)) {
      return reject_with(direct_telemetry.tile_execute_reject_sample_select);
    }
    if (resolve_info.copy_dest_info.copy_dest_exp_bias) {
      return reject_with(direct_telemetry.tile_execute_reject_exp_bias);
    }
    if (!xenos::IsColorResolveFormatBitwiseEquivalent(
            resolve_color_format,
            xenos::ColorFormat(resolve_info.copy_dest_info.copy_dest_format))) {
      return reject_with(direct_telemetry.tile_execute_reject_format_mismatch);
    }
  } else if (!IsResolveDirectHostRTFullColorSourcePackable(
                 resolve_color_format)) {
    return reject_with(direct_telemetry.tile_execute_reject_format_mismatch);
  }

  uint32_t dump_base, dump_row_length_used, dump_rows, dump_pitch;
  resolve_info.GetCopyEdramTileSpan(dump_base, dump_row_length_used, dump_rows,
                                    dump_pitch);
  std::vector<ResolveCopyDumpRectangle> rectangles;
  GetResolveCopyRectanglesToDump(dump_base, dump_row_length_used, dump_rows,
                                 dump_pitch, rectangles);
  if (rectangles.size() != 1) {
    return reject_with(direct_telemetry.tile_execute_reject_multi_source_rect);
  }
  const ResolveCopyDumpRectangle& rect = rectangles[0];
  if (!rect.rows || rect.row_last_end <= rect.row_first_start) {
    return reject_with(direct_telemetry.tile_execute_reject_multi_source_rect);
  }
  uint64_t covered_tiles = 0;
  if (rect.rows == 1) {
    covered_tiles = rect.row_last_end - rect.row_first_start;
  } else {
    covered_tiles = dump_row_length_used - rect.row_first_start;
    covered_tiles += uint64_t(rect.rows - 2) * dump_row_length_used;
    covered_tiles += rect.row_last_end;
  }
  uint64_t required_tiles = uint64_t(dump_row_length_used) * dump_rows;
  if (covered_tiles != required_tiles) {
    return reject_with(direct_telemetry.tile_execute_reject_multi_source_rect);
  }

  auto* rt = static_cast<MetalRenderTarget*>(rect.render_target);
  if (!rt) {
    return reject_with(
        direct_telemetry.tile_execute_reject_source_not_current_rt);
  }
  RenderTargetKey key = rt->key();
  if (key.is_depth || key.GetColorFormat() != resolve_color_format ||
      key.msaa_samples != resolve_info.color_edram_info.msaa_samples) {
    return reject_with(direct_telemetry.tile_execute_reject_format_mismatch);
  }

  bool source_is_uint = false;
  GetColorOwnershipTransferPixelFormat(key.GetColorFormat(), &source_is_uint);
  if (source_is_uint) {
    return reject_with(direct_telemetry.tile_execute_reject_uint_transfer_view);
  }

  uint32_t selected_sample = uint32_t(sample_select);
  if (copy_shader_is_fast && key.msaa_samples == xenos::MsaaSamples::k2X &&
      selected_sample > 1) {
    return reject_with(direct_telemetry.tile_execute_reject_sample_select);
  }
  const uint32_t pixels_per_thread =
      copy_shader_is_full_color
          ? TileDirectHostResolveFullPixelsPerThread(copy_shader)
          : DirectHostResolvePixelsPerThread(copy_shader, key.Is64bpp(),
                                             key.msaa_samples);
  if (copy_shader_is_full_color &&
      (!pixels_per_thread ||
       (pixels_per_thread != 1u && pixels_per_thread != 4u) ||
       (kTileDirectHostResolveTileWidth % pixels_per_thread) != 0u)) {
    return reject_with(direct_telemetry.tile_execute_reject_shader);
  }

  uint32_t color_attachment_index = xenos::kMaxColorRenderTargets;
  for (uint32_t i = 0; i < xenos::kMaxColorRenderTargets; ++i) {
    if (current_color_targets_[i] == rt) {
      color_attachment_index = i;
      break;
    }
  }
  if (color_attachment_index >= xenos::kMaxColorRenderTargets) {
    return reject_with(
        direct_telemetry.tile_execute_reject_source_not_current_rt);
  }

  MTL::Texture* source_texture = rt->draw_texture();
  if (!source_texture ||
      source_texture->pixelFormat() !=
          GetColorDrawPixelFormat(key.GetColorFormat())) {
    return reject_with(direct_telemetry.tile_execute_reject_texture);
  }
  MTL::RenderPassColorAttachmentDescriptor* color_attachment =
      active_render_pass_descriptor->colorAttachments()->object(
          color_attachment_index);
  if (!color_attachment || color_attachment->texture() != source_texture) {
    return reject_with(direct_telemetry.tile_execute_reject_attachment);
  }

  TransferAttachmentFormats attachment_formats;
  if (!GetActiveTransferAttachmentFormats(active_render_pass_descriptor,
                                          attachment_formats)) {
    return reject_with(direct_telemetry.tile_execute_reject_attachment);
  }
  const uint32_t full_dest_bpp_log2 =
      copy_shader_is_full_color
          ? DirectHostResolveFullDestBppLog2(copy_shader)
          : 0u;
  const uint32_t full_pixels_per_thread =
      copy_shader_is_full_color ? pixels_per_thread : 0u;
  const uint32_t full_source_format =
      copy_shader_is_full_color ? uint32_t(key.GetColorFormat()) : 0u;
  const uint32_t full_dest_format =
      copy_shader_is_full_color
          ? uint32_t(resolve_info.copy_dest_info.copy_dest_format)
          : 0u;
  MTL::RenderPipelineState* pipeline = GetOrCreateTileDirectHostResolvePipeline(
      color_attachment_index, uint32_t(source_texture->sampleCount()),
      copy_shader_is_full_color, full_dest_bpp_log2, full_pixels_per_thread,
      full_source_format, full_dest_format,
      attachment_formats.color_attachment_formats);
  if (!pipeline) {
    return reject_with(direct_telemetry.tile_execute_reject_pipeline);
  }

  auto* shared = command_processor_.shared_memory();
  MTL::Buffer* destination_buffer = shared ? shared->GetBuffer() : nullptr;
  if (!destination_buffer) {
    return reject_with(direct_telemetry.tile_execute_reject_dest_buffer);
  }

  struct TileDirectHostResolveConstants {
    uint32_t edram_info;
    uint32_t coordinate_info;
    uint32_t dest_info;
    uint32_t dest_coordinate_info;
    uint32_t dest_base;
    uint32_t dump_base;
    uint32_t dump_pitch_tiles;
    uint32_t dump_row_length_used;
    uint32_t dump_rows;
    uint32_t rect_row_first;
    uint32_t rect_rows;
    uint32_t rect_row_first_start;
    uint32_t rect_row_last_end;
    uint32_t source_base_tiles;
    uint32_t source_pitch_tiles;
    uint32_t source_width;
    uint32_t source_height;
    uint32_t height_scaled;
    uint32_t msaa_2x_sample_0;
    uint32_t msaa_2x_sample_1;
    uint32_t msaa_samples;
    uint32_t is_64bpp;
    uint32_t source_format;
    uint32_t is_full_color;
    uint32_t dest_format;
    float dest_exp_bias_factor;
    uint32_t pixels_per_thread;
    uint32_t dest_bpp_log2;
    uint32_t source_to_dump_tiles;
    uint32_t rect_row_last;
    uint32_t padding2;
    uint32_t padding3;
  };
  static_assert_size(TileDirectHostResolveConstants, 32 * sizeof(uint32_t));

  TileDirectHostResolveConstants constants = {};
  constants.edram_info = copy_constants.dest_relative.edram_info.packed;
  constants.coordinate_info =
      copy_constants.dest_relative.coordinate_info.packed;
  constants.dest_info = copy_constants.dest_relative.dest_info.value;
  constants.dest_coordinate_info =
      copy_constants.dest_relative.dest_coordinate_info.packed;
  constants.dest_base = copy_constants.dest_base;
  constants.dump_base = dump_base;
  constants.dump_pitch_tiles = dump_pitch;
  constants.dump_row_length_used = dump_row_length_used;
  constants.dump_rows = dump_rows;
  constants.rect_row_first = rect.row_first;
  constants.rect_rows = rect.rows;
  constants.rect_row_first_start = rect.row_first_start;
  constants.rect_row_last_end = rect.row_last_end;
  constants.rect_row_last = rect.row_first + rect.rows;
  constants.source_base_tiles = key.base_tiles;
  constants.source_pitch_tiles = key.GetPitchTiles();
  constants.source_width = uint32_t(source_texture->width());
  constants.source_height = uint32_t(source_texture->height());
  constants.height_scaled =
      resolve_info.height_div_8 << xenos::kResolveAlignmentPixelsLog2;
  constants.msaa_2x_sample_0 =
      draw_util::GetD3D10SampleIndexForGuest2xMSAA(0, msaa_2x_supported_);
  constants.msaa_2x_sample_1 =
      draw_util::GetD3D10SampleIndexForGuest2xMSAA(1, msaa_2x_supported_);
  constants.msaa_samples = uint32_t(key.msaa_samples);
  constants.is_64bpp = key.Is64bpp() ? 1u : 0u;
  constants.source_format = uint32_t(key.GetColorFormat());
  constants.is_full_color = copy_shader_is_full_color ? 1u : 0u;
  constants.dest_format =
      uint32_t(resolve_info.copy_dest_info.copy_dest_format);
  const int32_t dest_exp_bias = resolve_info.copy_dest_info.copy_dest_exp_bias;
  const uint32_t dest_exp_bias_bits =
      uint32_t(int32_t(0x3F800000) + dest_exp_bias * int32_t(1 << 23));
  std::memcpy(&constants.dest_exp_bias_factor, &dest_exp_bias_bits,
              sizeof(constants.dest_exp_bias_factor));
  constants.pixels_per_thread = full_pixels_per_thread;
  constants.dest_bpp_log2 =
      copy_shader_is_full_color ? full_dest_bpp_log2
                                : (key.Is64bpp() ? 3u : 2u);
  constants.source_to_dump_tiles =
      (xenos::kEdramTileCount - dump_base) & (xenos::kEdramTileCount - 1u);

  ++telemetry_.resolve_execute_calls;
  command_processor_.SetSwapDestSwap(
      resolve_info.copy_dest_base, resolve_info.copy_dest_info.copy_dest_swap);

  PushEncoderDebugGroup(
      active_render_encoder,
      fmt::format("{} color{} dest=shared_memory",
                  kDirectHostResolveEncoderLabel, color_attachment_index));
  active_render_encoder->setRenderPipelineState(pipeline);
  active_render_encoder->setTileBytes(&constants, sizeof(constants), 0);
  active_render_encoder->setTileBuffer(destination_buffer, 0, 1);
  active_render_encoder->useResource(destination_buffer, MTL::ResourceUsageWrite,
                                     MTL::RenderStageTile);
  const NS::UInteger threads_per_tile_width =
      copy_shader_is_full_color && full_pixels_per_thread == 4u ? 8u
                                                                : 16u;
  const NS::UInteger threads_per_tile_height =
      copy_shader_is_full_color && full_pixels_per_thread == 4u ? 8u
                                                                : 16u;
  active_render_encoder->dispatchThreadsPerTile(MTL::Size::Make(
      threads_per_tile_width, threads_per_tile_height, 1));
  active_render_encoder->popDebugGroup();

  written_address = resolve_info.copy_dest_extent_start;
  written_length = resolve_info.copy_dest_extent_length;
  command_processor_.MarkSharedMemoryRenderWritePending(
      written_address, written_length, MTL::RenderStageTile);
  if (auto* texture_cache = command_processor_.texture_cache()) {
    texture_cache->MarkRangeAsResolved(written_address, written_length);
  }

  ++direct_telemetry.direct_host_attempt;
  ++direct_telemetry.direct_host_success;
  ++direct_telemetry.tile_candidate_total;
  ++direct_telemetry.tile_accept;
  if (copy_shader_is_full_color) {
    ++direct_telemetry.tile_accept_full_color;
  } else {
    ++direct_telemetry.tile_accept_fast;
  }
  ++direct_telemetry.store_dontcare_eligible;
  // Source-side eligibility is not attachment liveness proof. Keep storing
  // until AttachmentPlan proves no later host RT observer can read this color.
  const bool store_dontcare_liveness_proven = false;
  if (::cvars::metal_tile_direct_host_resolve_dontcare_store &&
      store_dontcare_liveness_proven) {
    ++direct_telemetry.store_dontcare_attempt;
    active_render_encoder->setColorStoreAction(MTL::StoreActionDontCare,
                                               color_attachment_index);
    ++direct_telemetry.store_dontcare_applied;
  } else {
    ++direct_telemetry.store_dontcare_skipped_disabled;
  }
  ++direct_telemetry.tile_execute_success;
  return true;
}

MTL::Library* MetalRenderTargetCache::GetOrCreateEdramLoadLibrary(bool msaa) {
  MTL::Library*& library =
      msaa ? edram_load_library_msaa_ : edram_load_library_;
  if (library) {
    return library;
  }

  static const char kEdramLoadShaderSource[] = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct EdramLoadConstants {
  uint base_tiles;
  uint pitch_tiles;
  uint format;
  uint format_is_64bpp;
  uint msaa_samples;
  uint sample_id;
  uint resolution_scale_x;
  uint resolution_scale_y;
};

struct VSOut {
  float4 position [[position]];
};

vertex VSOut edram_load_vs(uint vid [[vertex_id]]) {
  float2 pt = float2((vid << 1) & 2, vid & 2);
  VSOut out;
  out.position = float4(pt * 2.0f - 1.0f, 0.0f, 1.0f);
  return out;
}

constant uint kXenosMsaaSamples1X = 0u;
constant uint kXenosMsaaSamples2X = 1u;
constant uint kXenosMsaaSamples4X = 2u;
constant uint kEdramTileCount = 2048u;

uint XeEdramOffsetInts(uint2 pixel_index, uint base_tiles, bool wrap,
                       uint pitch_tiles, uint msaa_samples, bool is_depth,
                       uint format_ints_log2, uint pixel_sample_index,
                       uint2 resolution_scale) {
  uint msaa_samples_x_log2 = (msaa_samples >= kXenosMsaaSamples4X) ? 1u : 0u;
  uint msaa_samples_y_log2 = (msaa_samples >= kXenosMsaaSamples2X) ? 1u : 0u;
  uint2 rt_sample_index =
      pixel_index << uint2(msaa_samples_x_log2, msaa_samples_y_log2);
  rt_sample_index +=
      (uint2(pixel_sample_index) >> uint2(1u, 0u)) & 1u;
  uint2 tile_size_at_32bpp = uint2(80u, 16u) * resolution_scale;
  uint2 tile_size_samples =
      tile_size_at_32bpp >> uint2(format_ints_log2, 0u);
  uint2 tile_offset_xy = rt_sample_index / tile_size_samples;
  base_tiles += tile_offset_xy.y * pitch_tiles + tile_offset_xy.x;
  rt_sample_index -= tile_offset_xy * tile_size_samples;
  if (is_depth) {
    uint tile_width_half = tile_size_samples.x >> 1u;
    rt_sample_index.x =
        uint(int(rt_sample_index.x) +
             ((rt_sample_index.x >= tile_width_half)
                  ? -int(tile_width_half)
                  : int(tile_width_half)));
  }
  uint address =
      base_tiles * (tile_size_at_32bpp.x * tile_size_at_32bpp.y) +
      ((rt_sample_index.y * tile_size_samples.x + rt_sample_index.x) <<
       format_ints_log2);
  if (wrap) {
    address %= tile_size_at_32bpp.x * tile_size_at_32bpp.y * kEdramTileCount;
  }
  return address;
}

float XeFloat7e3To32(uint f10) {
  f10 &= 0x3FFu;
  if (f10 == 0u) {
    return 0.0f;
  }
  uint mantissa = f10 & 0x7Fu;
  uint exponent = f10 >> 7u;
  if (exponent == 0u) {
    uint mantissa_lzcnt = clz(mantissa) - 24u;
    exponent = uint(int(1) - int(mantissa_lzcnt));
    mantissa = (mantissa << mantissa_lzcnt) & 0x7Fu;
  }
  uint f32 = ((exponent + 124u) << 23u) | (mantissa << 16u);
  return as_type<float>(f32);
}

float4 XeUnpackR8G8B8A8UNorm(uint packed) {
  float4 value = float4(packed & 0xFFu, (packed >> 8u) & 0xFFu,
                        (packed >> 16u) & 0xFFu, packed >> 24u);
  return value * (1.0f / 255.0f);
}

float4 XeUnpackR10G10B10A2UNorm(uint packed) {
  float4 value = float4(packed & 0x3FFu, (packed >> 10u) & 0x3FFu,
                        (packed >> 20u) & 0x3FFu, (packed >> 30u) & 0x3u);
  return value * float4(1.0f / 1023.0f, 1.0f / 1023.0f, 1.0f / 1023.0f,
                        1.0f / 3.0f);
}

float4 XeUnpackR10G10B10A2Float(uint packed) {
  float r = XeFloat7e3To32(packed & 0x3FFu);
  float g = XeFloat7e3To32((packed >> 10u) & 0x3FFu);
  float b = XeFloat7e3To32((packed >> 20u) & 0x3FFu);
  float a = float((packed >> 30u) & 0x3u) * (1.0f / 3.0f);
  return float4(r, g, b, a);
}

float2 XeUnpackR16G16Edram(uint packed) {
  int r = int(packed << 16u) >> 16u;
  int g = int(packed) >> 16u;
  float2 value = float2(float(r), float(g)) * (32.0f / 32767.0f);
  return max(value, float2(-1.0f));
}

float4 XeUnpackR16G16B16A16Edram(uint2 packed) {
  int r = int(packed.x << 16u) >> 16u;
  int g = int(packed.x) >> 16u;
  int b = int(packed.y << 16u) >> 16u;
  int a = int(packed.y) >> 16u;
  float4 value = float4(float(r), float(g), float(b), float(a)) *
                 (32.0f / 32767.0f);
  return max(value, float4(-1.0f));
}

float2 XeUnpackHalf2(uint packed) {
  return float2(as_type<half2>(packed));
}

float4 XeUnpackColor32bpp(uint format, uint packed) {
  switch (format) {
    case 0u:  // kXenosColorRenderTargetFormat_8_8_8_8
    case 1u:  // kXenosColorRenderTargetFormat_8_8_8_8_GAMMA
      return XeUnpackR8G8B8A8UNorm(packed);
    case 2u:  // kXenosColorRenderTargetFormat_2_10_10_10
    case 10u: // kXenosColorRenderTargetFormat_2_10_10_10_AS_10_10_10_10
      return XeUnpackR10G10B10A2UNorm(packed);
    case 3u:  // kXenosColorRenderTargetFormat_2_10_10_10_FLOAT
    case 12u: // kXenosColorRenderTargetFormat_2_10_10_10_FLOAT_AS_16_16_16_16
      return XeUnpackR10G10B10A2Float(packed);
    case 4u: {  // kXenosColorRenderTargetFormat_16_16
      float2 rg = XeUnpackR16G16Edram(packed);
      return float4(rg, 0.0f, 1.0f);
    }
    case 6u: {  // kXenosColorRenderTargetFormat_16_16_FLOAT
      float2 rg = XeUnpackHalf2(packed);
      return float4(rg, 0.0f, 1.0f);
    }
    case 14u:  // kXenosColorRenderTargetFormat_32_FLOAT
      return float4(as_type<float>(packed), 0.0f, 0.0f, 1.0f);
    default:
      return float4(0.0f);
  }
}

float4 XeUnpackColor64bpp(uint format, uint2 packed) {
  switch (format) {
    case 5u:  // kXenosColorRenderTargetFormat_16_16_16_16
      return XeUnpackR16G16B16A16Edram(packed);
    case 7u: {  // kXenosColorRenderTargetFormat_16_16_16_16_FLOAT
      float2 rg = XeUnpackHalf2(packed.x);
      float2 ba = XeUnpackHalf2(packed.y);
      return float4(rg, ba);
    }
    case 15u:  // kXenosColorRenderTargetFormat_32_32_FLOAT
      return float4(as_type<float>(packed.x), as_type<float>(packed.y),
                    0.0f, 0.0f);
    default:
      return float4(0.0f);
  }
}

struct EdramLoadOut {
  float4 color [[color(0)]];
#if XE_EDRAM_LOAD_MSAA
  uint sample_mask [[sample_mask]];
#endif
};

fragment EdramLoadOut edram_load_ps(
    VSOut in [[stage_in]],
    constant EdramLoadConstants& constants [[buffer(0)]],
    device const uint* edram [[buffer(1)]]) {
  uint2 pixel = uint2(in.position.xy);
  uint format_ints_log2 = constants.format_is_64bpp;
  uint address = XeEdramOffsetInts(
      pixel, constants.base_tiles, true, constants.pitch_tiles,
      constants.msaa_samples, false, format_ints_log2, constants.sample_id,
      uint2(constants.resolution_scale_x, constants.resolution_scale_y));
  float4 color;
  if (constants.format_is_64bpp != 0u) {
    uint2 packed = uint2(edram[address], edram[address + 1u]);
    color = XeUnpackColor64bpp(constants.format, packed);
  } else {
    color = XeUnpackColor32bpp(constants.format, edram[address]);
  }
  EdramLoadOut out;
  out.color = color;
#if XE_EDRAM_LOAD_MSAA
  out.sample_mask = 1u << (constants.sample_id & 0x1Fu);
#endif
  return out;
}
)METAL";

  std::string source;
  source.reserve(sizeof(kEdramLoadShaderSource) + 32);
  source.append(msaa ? "#define XE_EDRAM_LOAD_MSAA 1\n"
                     : "#define XE_EDRAM_LOAD_MSAA 0\n");
  source.append(kEdramLoadShaderSource);

  NS::Error* error = nullptr;
  auto source_str = NS::String::string(source.c_str(), NS::UTF8StringEncoding);
  library = device_->newLibrary(source_str, nullptr, &error);
  if (!library) {
    XELOGE("Metal: failed to compile edram load shader: {}",
           error && error->localizedDescription()
               ? error->localizedDescription()->utf8String()
               : "unknown error");
  }
  return library;
}

MTL::RenderPipelineState* MetalRenderTargetCache::GetOrCreateEdramLoadPipeline(
    MTL::PixelFormat dest_format, uint32_t sample_count) {
  uint64_t key = uint64_t(dest_format) | (uint64_t(sample_count) << 32);
  auto it = edram_load_pipelines_.find(key);
  if (it != edram_load_pipelines_.end()) {
    return it->second;
  }

  bool msaa = sample_count > 1;
  MTL::Library* lib = GetOrCreateEdramLoadLibrary(msaa);
  if (!lib) {
    return nullptr;
  }

  NS::String* vs_name =
      NS::String::string("edram_load_vs", NS::UTF8StringEncoding);
  NS::String* ps_name =
      NS::String::string("edram_load_ps", NS::UTF8StringEncoding);
  MTL::Function* vs = lib->newFunction(vs_name);
  MTL::Function* ps = lib->newFunction(ps_name);
  if (!vs || !ps) {
    if (vs) {
      vs->release();
    }
    if (ps) {
      ps->release();
    }
    XELOGE("Metal: edram load missing shader entrypoints");
    return nullptr;
  }

  MTL::RenderPipelineDescriptor* desc =
      MTL::RenderPipelineDescriptor::alloc()->init();
  desc->setVertexFunction(vs);
  desc->setFragmentFunction(ps);
  desc->colorAttachments()->object(0)->setPixelFormat(dest_format);
  desc->setDepthAttachmentPixelFormat(MTL::PixelFormatInvalid);
  desc->setSampleCount(sample_count);

  NS::Error* error = nullptr;
  MTL::RenderPipelineState* pipeline =
      device_->newRenderPipelineState(desc, &error);
  desc->release();
  vs->release();
  ps->release();

  if (!pipeline) {
    XELOGE("Metal: failed to create edram load pipeline: {}",
           error ? error->localizedDescription()->utf8String() : "unknown");
    return nullptr;
  }

  edram_load_pipelines_.emplace(key, pipeline);
  return pipeline;
}

bool MetalRenderTargetCache::PrepareResolvePlan(Memory& memory,
                                                ResolvePlan& plan_out) {
  ++telemetry_.resolve_plan_calls;
  plan_out = ResolvePlan();
  const RegisterFile& regs = register_file();

  // Fixed16 formats may be truncated to -1..1 when backed by SNORM.
  bool fixed_rg16_trunc = IsFixedRG16TruncatedToMinus1To1();
  bool fixed_rgba16_trunc = IsFixedRGBA16TruncatedToMinus1To1();

  if (!trace_writer_) {
    XELOGE("MetalRenderTargetCache::Resolve: trace_writer_ is null");
    return false;
  }

  if (!draw_util::GetResolveInfo(regs, memory, *trace_writer_,
                                 draw_resolution_scale_x(),
                                 draw_resolution_scale_y(), fixed_rg16_trunc,
                                 fixed_rgba16_trunc,
                                 plan_out.resolve_info)) {
    XELOGE("MetalRenderTargetCache::Resolve: GetResolveInfo failed");
    return false;
  }
  plan_out.valid = true;

  const draw_util::ResolveInfo& resolve_info = plan_out.resolve_info;
  plan_out.noop = !resolve_info.coordinate_info.width_div_8 ||
                  !resolve_info.height_div_8;
  if (plan_out.noop) {
    ++telemetry_.resolve_plan_noops;
    ++telemetry_.resolve_plan_no_encoder_end;
    return true;
  }
  plan_out.needs_copy_export = resolve_info.copy_dest_extent_length != 0;
  plan_out.needs_resolve_clear =
      resolve_info.IsClearingDepth() || resolve_info.IsClearingColor();
  if (plan_out.needs_copy_export) {
    ++telemetry_.resolve_plan_copy_export;
  }
  if (plan_out.needs_resolve_clear) {
    ++telemetry_.resolve_plan_clear;
    if (resolve_info.IsClearingColor()) {
      ++telemetry_.resolve_plan_clear_color;
    }
    if (resolve_info.IsClearingDepth()) {
      ++telemetry_.resolve_plan_clear_depth;
    }
  }
  if (plan_out.needs_copy_export && plan_out.needs_resolve_clear) {
    ++telemetry_.resolve_plan_copy_and_clear;
  } else if (plan_out.needs_copy_export) {
    ++telemetry_.resolve_plan_copy_only;
  } else if (plan_out.needs_resolve_clear) {
    ++telemetry_.resolve_plan_clear_only;
  }
  // TODO (xenios-jp): Add a queued resolve-clear system for clear-only resolves
  // that can be materialized before the next observer. Full clears could become
  // descriptor-time loadActionClear in the next render pass, avoiding an
  // immediate render encoder end on Apple TBDR GPUs. This needs a real
  // ownership transaction because PrepareHostRenderTargetsResolveClear mutates
  // tile ownership, and every later draw, transfer, export, readback, or
  // ownership query must observe the cleared contents.
  plan_out.needs_render_encoder_end =
      plan_out.needs_copy_export || plan_out.needs_resolve_clear;
  if (plan_out.needs_render_encoder_end) {
    ++telemetry_.resolve_plan_needs_encoder_end;
  } else {
    ++telemetry_.resolve_plan_no_encoder_end;
  }
  if (plan_out.needs_copy_export) {
    plan_out.written_address = resolve_info.copy_dest_extent_start;
    plan_out.written_length = resolve_info.copy_dest_extent_length;
  }
  return true;
}

bool MetalRenderTargetCache::Resolve(
    Memory& memory, uint32_t& written_address, uint32_t& written_length,
    MTL::CommandBuffer* command_buffer,
    const ResolvePlan* prepared_resolve_plan) {
  ++telemetry_.resolve_execute_calls;
  written_address = 0;
  written_length = 0;
  ResolvePlan resolve_plan_storage;
  if (!prepared_resolve_plan) {
    if (!PrepareResolvePlan(memory, resolve_plan_storage)) {
      return false;
    }
    prepared_resolve_plan = &resolve_plan_storage;
  }
  const ResolvePlan& resolve_plan = *prepared_resolve_plan;
  const draw_util::ResolveInfo& resolve_info = resolve_plan.resolve_info;

  // Nothing to do.
  if (resolve_plan.noop) {
    return true;
  }

  bool is_depth = resolve_info.IsCopyingDepth();

  bool draw_resolution_scaled = IsDrawResolutionScaled();

  const auto& coord = resolve_info.coordinate_info;
  uint32_t resolve_width = coord.width_div_8 * 8;
  uint32_t resolve_height = resolve_info.height_div_8 * 8;

  // Try GPU compute resolve first (RT -> EDRAM -> shared memory), matching
  // D3D12/Vulkan behavior for the supported cases.
  if (edram_buffer_) {
    // Copy dispatch -- only when there is an actual copy extent.
    bool copy_succeeded = !resolve_plan.needs_copy_export;
    if (resolve_plan.needs_copy_export) {
      draw_util::ResolveCopyShaderConstants copy_constants;
      uint32_t group_count_x = 0, group_count_y = 0;
      draw_util::ResolveCopyShaderIndex copy_shader =
          resolve_info.GetCopyShader(draw_resolution_scale_x(),
                                     draw_resolution_scale_y(), copy_constants,
                                     group_count_x, group_count_y);
      bool direct_host_resolve_enabled = ::cvars::metal_direct_host_resolve;
      bool direct_host_rt_candidate =
          direct_host_resolve_enabled &&
          IsResolveDirectHostRTCandidate(copy_shader);
      const draw_util::ResolveCopyShaderInfo& copy_shader_info =
          draw_util::resolve_copy_shader_info[size_t(copy_shader)];

      // Match D3D12/Vulkan: dump host RT ownership into EDRAM, then resolve
      // from EDRAM to shared memory. Resolve-time blend fallback is not correct
      // because blending state is per-draw, not per-resolve.
      uint32_t dump_base, dump_row_length_used, dump_rows, dump_pitch;
      resolve_info.GetCopyEdramTileSpan(dump_base, dump_row_length_used,
                                        dump_rows, dump_pitch);
      if (direct_host_resolve_enabled &&
          TryDirectHostResolveCopy(resolve_info, copy_constants, copy_shader,
                                   dump_base, dump_row_length_used, dump_rows,
                                   dump_pitch, resolve_plan.needs_resolve_clear,
                                   command_buffer, written_address,
                                   written_length)) {
        copy_succeeded = true;
      } else {
        DumpRenderTargets(dump_base, dump_row_length_used, dump_rows,
                          dump_pitch, command_buffer,
                          ResolveDumpEncoderLabel(direct_host_rt_candidate));

        uint32_t dest_base = resolve_info.copy_dest_base;
        uint32_t dest_local_start =
            resolve_info.copy_dest_extent_start - dest_base;
        uint32_t dest_local_end =
            dest_local_start + resolve_info.copy_dest_extent_length;

        command_processor_.SetSwapDestSwap(
            dest_base, resolve_info.copy_dest_info.copy_dest_swap);

        // For now, only apply the 8888 restriction to color resolves; depth
        // resolves may use different destination formats.
        uint32_t bytes_per_pixel = 4;

        // Select the appropriate Metal pipeline for this shader.
        MTL::ComputePipelineState* pipeline =
            GetResolvePipeline(copy_shader, draw_resolution_scaled);
        if (draw_resolution_scaled && !pipeline) {
          static uint32_t missing_scaled_pipeline_log_count = 0;
          if (missing_scaled_pipeline_log_count < 8) {
            ++missing_scaled_pipeline_log_count;
            XELOGW(
                "MetalResolve: scaled resolve pipeline missing for shader {}",
                int(copy_shader));
          }
        }

        if (pipeline && group_count_x && group_count_y) {
          ResolveDestinationBuffer destination = {};
          if (!PrepareResolveDestinationBuffer(resolve_info,
                                               draw_resolution_scaled,
                                               destination)) {
            XELOGE(
                "MetalRenderTargetCache::Resolve: failed to prepare resolve "
                "destination for 0x{:08X} len {}",
                resolve_info.copy_dest_extent_start,
                resolve_info.copy_dest_extent_length);
            return false;
          }

          {
            ScopedAutoreleasePool autorelease_pool;
            bool standalone = false;
            MTL::CommandBuffer* cmd = command_buffer;
            if (!cmd) {
              cmd = command_processor_.CreateStandaloneTransferCommandBuffer(
                  "XeniaCB reason=resolve-compute");
              if (!cmd) {
                XELOGE(
                    "MetalRenderTargetCache::Resolve: failed to get command "
                    "buffer for GPU path");
              }
              standalone = (cmd != nullptr);
            }
            if (cmd) {
              EndSharedMemoryUploadBlitEncoderForCommandBuffer(
                  command_processor_, cmd);
              MTL::ComputeCommandEncoder* encoder =
                  cmd->computeCommandEncoder();
              if (!encoder) {
                XELOGE(
                    "MetalRenderTargetCache::Resolve: failed to get compute "
                    "encoder for GPU path");
                if (standalone) {
                  cmd->release();
                }
              } else {
                SetEncoderLabel(
                    encoder, ResolveCopyEncoderLabel(direct_host_rt_candidate));
                PushEncoderDebugGroup(
                    encoder,
                    fmt::format(
                        "{} shader=\"{}\" direct_host_rt_candidate={} "
                        "source=edram_buffer dest={}",
                        ResolveCopyEncoderLabel(direct_host_rt_candidate),
                        copy_shader_info.debug_name,
                        direct_host_rt_candidate ? 1 : 0,
                        draw_resolution_scaled ? "scaled_resolve_memory"
                                               : "shared_memory"));
                encoder->setComputePipelineState(pipeline);

                // Buffer 0: push constants
                if (draw_resolution_scaled) {
                  encoder->setBytes(&copy_constants.dest_relative,
                                    sizeof(copy_constants.dest_relative), 0);
                } else {
                  encoder->setBytes(&copy_constants, sizeof(copy_constants), 0);
                }

                // Buffer 1: destination memory (shared or scaled resolve).
                encoder->setBuffer(destination.buffer, destination.offset, 1);

                // Buffer 2: EDRAM source buffer.
                encoder->setBuffer(edram_buffer_, 0, 2);
                encoder->useResource(destination.buffer,
                                     MTL::ResourceUsageWrite);
                encoder->useResource(edram_buffer_, MTL::ResourceUsageRead);

                encoder->dispatchThreadgroups(
                    MTL::Size::Make(group_count_x, group_count_y, 1),
                    MTL::Size::Make(8, 8, 1));

                if (!draw_resolution_scaled) {
                  command_processor_.MarkSharedMemoryComputeWritePending(
                      resolve_info.copy_dest_extent_start,
                      resolve_info.copy_dest_extent_length, encoder);
                }

                encoder->popDebugGroup();
                encoder->endEncoding();
                if (standalone) {
                  command_processor_.CommitStandaloneAndWait(cmd);
                }

                written_address = resolve_plan.written_address;
                written_length = resolve_plan.written_length;

                // Mark the range as resolved in the texture cache so that any
                // textures overlapping this range will be reloaded from the
                // updated shared memory. This matches D3D12/Vulkan behavior.
                if (auto* tex_cache = command_processor_.texture_cache()) {
                  tex_cache->MarkRangeAsResolved(written_address,
                                                 written_length);
                }

                copy_succeeded = true;
              }
            }
          }
        }
      }

      if (!copy_succeeded) {
        XELOGE(
            "MetalRenderTargetCache::Resolve: no valid GPU resolve shader / "
            "pipeline for this configuration");
      }
    }  // if (needs_copy_export)

    // Clearing -- runs independently of whether the copy succeeded, matching
    // D3D12/Vulkan behavior.
    bool clear_depth =
        resolve_plan.needs_resolve_clear && resolve_info.IsClearingDepth();
    bool clear_color =
        resolve_plan.needs_resolve_clear && resolve_info.IsClearingColor();
    bool clear_succeeded = !(clear_depth || clear_color);
    if (clear_depth || clear_color) {
      clear_succeeded = true;
      Transfer::Rectangle clear_rectangle;
      RenderTarget* clear_targets[2] = {};
      std::vector<Transfer> clear_transfers[2];
      if (PrepareHostRenderTargetsResolveClear(
              resolve_info, clear_rectangle, clear_targets[0],
              clear_transfers[0], clear_targets[1], clear_transfers[1])) {
        uint64_t clear_values[2];
        clear_values[0] = resolve_info.rb_depth_clear;
        clear_values[1] = resolve_info.rb_color_clear |
                          (uint64_t(resolve_info.rb_color_clear_lo) << 32);
        PerformTransfersAndResolveClears(2, clear_targets, clear_transfers,
                                         clear_values, &clear_rectangle,
                                         command_buffer);
      }
    }

    return copy_succeeded && clear_succeeded;
  }

  XELOGE(
      "MetalRenderTargetCache::Resolve: no valid GPU resolve shader / pipeline "
      "for this configuration");
  return false;
}

bool MetalRenderTargetCache::PerformTransfersAndResolveClears(
    uint32_t render_target_count, RenderTarget* const* render_targets,
    const std::vector<Transfer>* render_target_transfers,
    const uint64_t* render_target_resolve_clear_values,
    const Transfer::Rectangle* resolve_clear_rectangle,
    MTL::CommandBuffer* command_buffer,
    MTL::RenderCommandEncoder* active_render_encoder,
    MTL::RenderPassDescriptor* active_render_pass_descriptor,
    DrawPassTransferEncoderMutationMask* mutations_out,
    const PendingDrawPassTransferPlan* prepared_draw_pass_transfer_plans) {
  ++telemetry_.perform_transfer_calls;
  if (mutations_out) {
    *mutations_out = kDrawPassTransferEncoderMutationNone;
  }
  if (!render_targets || !render_target_transfers) {
    return false;
  }

  bool resolve_clear_needed =
      render_target_resolve_clear_values && resolve_clear_rectangle;
  bool use_active_render_encoder = active_render_encoder != nullptr;
  if (use_active_render_encoder) {
    ++telemetry_.perform_transfer_active_encoder_calls;
  } else {
    ++telemetry_.perform_transfer_standalone_calls;
  }
  if (resolve_clear_needed) {
    ++telemetry_.perform_transfer_resolve_clear_calls;
  }
  if (use_active_render_encoder &&
      (resolve_clear_needed || !active_render_pass_descriptor)) {
    return false;
  }
  auto mark_active_encoder_mutation =
      [&](DrawPassTransferEncoderMutationMask mutations) {
        if (use_active_render_encoder && mutations_out) {
          *mutations_out |= mutations;
        }
      };
  TransferAttachmentFormats active_attachment_formats;
  if (use_active_render_encoder &&
      !GetActiveTransferAttachmentFormats(
          active_render_pass_descriptor, active_attachment_formats)) {
    return false;
  }
  bool any_work = false;
  bool host_depth_store_needed = false;
  std::array<std::vector<TransferRectanglePlan>,
             1 + xenos::kMaxColorRenderTargets>
      local_transfer_rectangle_plans;
  std::array<const std::vector<TransferRectanglePlan>*,
             1 + xenos::kMaxColorRenderTargets>
      transfer_rectangle_plans = {};
  if (render_target_count > transfer_rectangle_plans.size()) {
    return false;
  }
  for (uint32_t i = 0; i < render_target_count; ++i) {
    RenderTarget* dest_rt = render_targets[i];
    if (!dest_rt) {
      continue;
    }
    if (resolve_clear_needed) {
      any_work = true;
    }
    const std::vector<Transfer>& transfers = render_target_transfers[i];
    if (transfers.empty()) {
      continue;
    }
    RenderTargetKey dest_key = dest_rt->key();
    const std::vector<TransferRectanglePlan>* target_transfer_rectangles = nullptr;
    if (prepared_draw_pass_transfer_plans) {
      const PendingDrawPassTransferPlan& prepared_plan =
          prepared_draw_pass_transfer_plans[i];
      if (prepared_plan.render_target == dest_rt &&
          prepared_plan.rejection_reason ==
              DrawPassTransferRejectionReason::kNone &&
          prepared_plan.transfer_rectangles.size() == transfers.size()) {
        target_transfer_rectangles = &prepared_plan.transfer_rectangles;
      }
    }
    if (!target_transfer_rectangles) {
      std::vector<TransferRectanglePlan>& local_transfer_rectangles =
          local_transfer_rectangle_plans[i];
      if (!BuildTransferRectanglePlans(dest_key, transfers,
                                       resolve_clear_rectangle, false,
                                       local_transfer_rectangles)) {
        continue;
      }
      target_transfer_rectangles = &local_transfer_rectangles;
    }
    transfer_rectangle_plans[i] = target_transfer_rectangles;
    if (target_transfer_rectangles->empty()) {
      continue;
    }
    for (const TransferRectanglePlan& transfer_plan :
         *target_transfer_rectangles) {
      const Transfer& transfer = transfers[transfer_plan.transfer_index];
      if (dest_key.is_depth && transfer.host_depth_source == dest_rt) {
        host_depth_store_needed = true;
      }
    }
    any_work = true;
  }
  if (!any_work) {
    ++telemetry_.perform_transfer_no_work_calls;
    return true;
  }
  ++telemetry_.perform_transfer_work_calls;
  if (use_active_render_encoder && host_depth_store_needed) {
    return false;
  }

  MTL::CommandBuffer* cmd = command_buffer;
  if (use_active_render_encoder) {
    cmd = command_processor_.GetCurrentCommandBuffer();
  } else if (!cmd) {
    // RequestTransferCommandBuffer ends any active render encoder and ensures
    // transfer work has a command buffer. It does not require a standalone
    // command-buffer submission if the current one can be reused.
    cmd = command_processor_.RequestTransferCommandBuffer(
        MetalCommandProcessor::TransferRequestSource::kRenderTargetTransfer);
  } else {
    // An externally-provided command buffer still requires the render
    // encoder to be ended before transfer work can proceed.
    command_processor_.EndRenderEncoder();
  }
  if (!cmd) {
    XELOGE(
        "MetalRenderTargetCache::PerformTransfersAndResolveClears: no command "
        "buffer");
    return false;
  }

  uint32_t scale_x = draw_resolution_scale_x();
  uint32_t scale_y = draw_resolution_scale_y();
  uint32_t tile_width_samples =
      xenos::kEdramTileWidthSamples * draw_resolution_scale_x();
  uint32_t tile_height_samples =
      xenos::kEdramTileHeightSamples * draw_resolution_scale_y();
  uint32_t depth_round = (!::cvars::depth_float24_convert_in_pixel_shader &&
                          ::cvars::depth_float24_round)
                             ? 1u
                             : 0u;

  // Host depth store pass (dest depth where host depth source == dest).
  // Use a single compute encoder for all depth store dispatches.
  bool host_depth_store_dispatched = false;
  if (host_depth_store_needed) {
    MTL::ComputeCommandEncoder* depth_store_encoder = nullptr;
    for (uint32_t i = 0; i < render_target_count; ++i) {
      RenderTarget* dest_rt = render_targets[i];
      if (!dest_rt) {
        continue;
      }
      RenderTargetKey dest_key = dest_rt->key();
      if (!dest_key.is_depth) {
        continue;
      }
      const std::vector<Transfer>& depth_transfers = render_target_transfers[i];
      const std::vector<TransferRectanglePlan>* depth_transfer_rectangles =
          transfer_rectangle_plans[i];
      if (!depth_transfer_rectangles) {
        continue;
      }
      for (const TransferRectanglePlan& transfer_plan :
           *depth_transfer_rectangles) {
        const Transfer& transfer = depth_transfers[transfer_plan.transfer_index];
        if (transfer.host_depth_source != dest_rt) {
          continue;
        }
        auto* dest_metal_rt = static_cast<MetalRenderTarget*>(dest_rt);
        MTL::Texture* depth_texture = dest_metal_rt->texture();
        if (!depth_texture || !edram_buffer_) {
          continue;
        }
        size_t pipeline_index = size_t(dest_key.msaa_samples);
        if (pipeline_index >= xe::countof(host_depth_store_pipelines_) ||
            !host_depth_store_pipelines_[pipeline_index]) {
          XELOGE(
              "MetalRenderTargetCache::PerformTransfersAndResolveClears: "
              "missing host depth store pipeline for msaa={}",
              uint32_t(dest_key.msaa_samples));
          continue;
        }
        if (!transfer_plan.rectangle_count) {
          continue;
        }
        HostDepthStoreRenderTargetConstant render_target_constant =
            GetHostDepthStoreRenderTargetConstant(dest_key.pitch_tiles_at_32bpp,
                                                  msaa_2x_supported_);
        if (!depth_store_encoder) {
          EndSharedMemoryUploadBlitEncoderForCommandBuffer(command_processor_,
                                                           cmd);
          depth_store_encoder = cmd->computeCommandEncoder();
          if (!depth_store_encoder) {
            XELOGE(
                "MetalRenderTargetCache::PerformTransfersAndResolveClears: "
                "failed to create host depth store encoder");
            break;
          }
          depth_store_encoder->setLabel(NS::String::string(
              "XeniaHostDepthStoreEncoder", NS::UTF8StringEncoding));
          depth_store_encoder->setComputePipelineState(
              host_depth_store_pipelines_[pipeline_index]);
          depth_store_encoder->setBuffer(edram_buffer_, 0, 1);
          depth_store_encoder->setTexture(depth_texture, 0);
          depth_store_encoder->useResource(edram_buffer_,
                                           MTL::ResourceUsageWrite);
          depth_store_encoder->useResource(depth_texture,
                                           MTL::ResourceUsageRead);
        }
        for (uint32_t rect_index = 0; rect_index < transfer_plan.rectangle_count;
             ++rect_index) {
          uint32_t group_count_x = 0;
          uint32_t group_count_y = 0;
          HostDepthStoreRectangleConstant rectangle_constant;
          GetHostDepthStoreRectangleInfo(
              transfer_plan.rectangles[rect_index], dest_key.msaa_samples,
              rectangle_constant, group_count_x, group_count_y);
          if (!group_count_x || !group_count_y) {
            continue;
          }
          HostDepthStoreConstants constants = {};
          constants.rectangle = rectangle_constant;
          constants.render_target = render_target_constant;
          depth_store_encoder->setBytes(&constants, sizeof(constants), 0);
          depth_store_encoder->dispatchThreadgroups(
              MTL::Size::Make(group_count_x, group_count_y, 1),
              MTL::Size::Make(8, 8, 1));
          host_depth_store_dispatched = true;
        }
      }
      break;
    }
    if (depth_store_encoder) {
      depth_store_encoder->endEncoding();
    }
  }

  bool any_transfers_done = false;

  for (uint32_t i = 0; i < render_target_count; ++i) {
    RenderTarget* dest_rt = render_targets[i];
    if (!dest_rt) {
      continue;
    }

    const std::vector<Transfer>& transfers = render_target_transfers[i];
    const std::vector<TransferRectanglePlan>* target_transfer_rectangles =
        transfer_rectangle_plans[i];
    if ((!target_transfer_rectangles || target_transfer_rectangles->empty()) &&
        !resolve_clear_needed) {
      continue;
    }

    auto* dest_metal_rt = static_cast<MetalRenderTarget*>(dest_rt);
    if (dest_metal_rt->needs_initial_clear()) {
      dest_metal_rt->SetNeedsInitialClear(false);
      MarkRenderPassDescriptorDirty(
          RenderPassDescriptorDirtyReason::
              kStandaloneTransferInitialClearConsumed);
    }
    RenderTargetKey dest_key = dest_metal_rt->key();
    bool dest_is_depth = dest_key.is_depth;
    uint32_t active_color_attachment_index = 0;

    bool dest_is_uint = false;
    MTL::PixelFormat dest_pixel_format =
        dest_is_depth ? GetDepthPixelFormat(dest_key.GetDepthFormat())
                      : GetColorOwnershipTransferPixelFormat(
                            dest_key.GetColorFormat(), &dest_is_uint);

    MTL::Texture* dest_texture = dest_is_depth
                                     ? dest_metal_rt->texture()
                                     : dest_metal_rt->transfer_texture();
    if (!dest_texture) {
      XELOGW(
          "MetalRenderTargetCache::PerformTransfersAndResolveClears: "
          "Destination RT {} has no texture",
          i);
      continue;
    }
    if (dest_is_depth) {
      assert_true(dest_texture->pixelFormat() == dest_pixel_format,
                  "Transfer depth must use resource pixel format");
    } else {
      assert_true(dest_texture->pixelFormat() == dest_pixel_format,
                  "Transfer color must use ownership pixel format");
    }
    if (use_active_render_encoder) {
      if (dest_is_depth) {
        if (i != 0 ||
            active_attachment_formats.depth_attachment_format !=
                dest_pixel_format) {
          return false;
        }
        auto* depth_attachment =
            active_render_pass_descriptor->depthAttachment();
        MTL::Texture* depth_texture =
            depth_attachment ? depth_attachment->texture() : nullptr;
        if (depth_texture != dest_metal_rt->draw_texture()) {
          return false;
        }
        if (dest_pixel_format == MTL::PixelFormatDepth32Float_Stencil8 ||
            dest_pixel_format == MTL::PixelFormatDepth24Unorm_Stencil8) {
          auto* stencil_attachment =
              active_render_pass_descriptor->stencilAttachment();
          MTL::Texture* stencil_texture =
              stencil_attachment ? stencil_attachment->texture() : nullptr;
          if (stencil_texture != depth_texture ||
              active_attachment_formats.stencil_attachment_format !=
                  dest_pixel_format) {
            return false;
          }
        }
      } else {
        if (i == 0 || i > xenos::kMaxColorRenderTargets) {
          return false;
        }
        active_color_attachment_index = i - 1;
        if (active_attachment_formats
                .color_attachment_formats[active_color_attachment_index] !=
            dest_pixel_format) {
          return false;
        }
      }
    }

    uint32_t dest_sample_count = MsaaSamplesToCount(dest_key.msaa_samples);
    bool transfer_use_sample_id_default =
        dest_sample_count > 1 && ::cvars::metal_transfer_msaa_sample_id;
    uint32_t dest_width = uint32_t(dest_texture->width());
    uint32_t dest_height = uint32_t(dest_texture->height());

    auto get_scaled_rect = [&](const Transfer::Rectangle& rect,
                               uint32_t& scaled_x, uint32_t& scaled_y,
                               uint32_t& scaled_width,
                               uint32_t& scaled_height) -> bool {
      uint32_t rect_x = rect.x_pixels * scale_x;
      uint32_t rect_y = rect.y_pixels * scale_y;
      uint32_t rect_width = rect.width_pixels * scale_x;
      uint32_t rect_height = rect.height_pixels * scale_y;
      if (rect_x >= dest_width || rect_y >= dest_height) {
        return false;
      }
      rect_width = std::min(rect_width, dest_width - rect_x);
      rect_height = std::min(rect_height, dest_height - rect_y);
      if (!rect_width || !rect_height) {
        return false;
      }
      scaled_x = rect_x;
      scaled_y = rect_y;
      scaled_width = rect_width;
      scaled_height = rect_height;
      return true;
    };

    auto set_rect_viewport = [&](MTL::RenderCommandEncoder* encoder,
                                 const Transfer::Rectangle& rect) -> bool {
      uint32_t scaled_x = 0;
      uint32_t scaled_y = 0;
      uint32_t scaled_width = 0;
      uint32_t scaled_height = 0;
      if (!get_scaled_rect(rect, scaled_x, scaled_y, scaled_width,
                           scaled_height)) {
        return false;
      }
      MTL::Viewport vp;
      vp.originX = double(scaled_x);
      vp.originY = double(scaled_y);
      vp.width = double(scaled_width);
      vp.height = double(scaled_height);
      vp.znear = 0.0;
      vp.zfar = 1.0;
      encoder->setViewport(vp);
      mark_active_encoder_mutation(kDrawPassTransferEncoderMutationViewport);
      MTL::ScissorRect scissor;
      scissor.x = scaled_x;
      scissor.y = scaled_y;
      scissor.width = scaled_width;
      scissor.height = scaled_height;
      encoder->setScissorRect(scissor);
      mark_active_encoder_mutation(kDrawPassTransferEncoderMutationScissor);
      return true;
    };

    std::vector<uint32_t> all_transfer_plan_indices;
    if (target_transfer_rectangles) {
      all_transfer_plan_indices.reserve(target_transfer_rectangles->size());
      for (uint32_t transfer_plan_index = 0;
           transfer_plan_index < target_transfer_rectangles->size();
           ++transfer_plan_index) {
        all_transfer_plan_indices.push_back(transfer_plan_index);
      }
    }
    std::vector<uint32_t> filtered_transfer_plan_indices;
    bool used_blit = false;
    MTL::BlitCommandEncoder* blit_encoder = nullptr;
    auto ensure_blit_encoder = [&]() -> MTL::BlitCommandEncoder* {
      if (!blit_encoder) {
        EndSharedMemoryUploadBlitEncoderForCommandBuffer(command_processor_,
                                                         cmd);
        blit_encoder = cmd->blitCommandEncoder();
        if (blit_encoder) {
          blit_encoder->setLabel(NS::String::string(
              "XeniaRTTransferBlitEncoder", NS::UTF8StringEncoding));
        }
      }
      return blit_encoder;
    };

    // Fast path: when source/dest share compatible EDRAM layout and format,
    // use a blit instead of shader-based transfers.
    if (!use_active_render_encoder && target_transfer_rectangles &&
        !target_transfer_rectangles->empty()) {
      auto try_blit_transfer = [&](uint32_t transfer_plan_index) -> bool {
        const TransferRectanglePlan& transfer_plan =
            (*target_transfer_rectangles)[transfer_plan_index];
        const Transfer& transfer = transfers[transfer_plan.transfer_index];
        auto* source_rt = static_cast<MetalRenderTarget*>(transfer.source);
        if (!source_rt || transfer.host_depth_source) {
          return false;
        }

        RenderTargetKey source_key = source_rt->key();
        if (dest_is_depth != source_key.is_depth) {
          return false;
        }
        if (source_key.resource_format != dest_key.resource_format ||
            source_key.msaa_samples != dest_key.msaa_samples ||
            source_key.pitch_tiles_at_32bpp != dest_key.pitch_tiles_at_32bpp) {
          return false;
        }

        bool base_tiles_match = source_key.base_tiles == dest_key.base_tiles;
        // Different-base color transfers need shader transfer address
        // semantics.
        if (!base_tiles_match) {
          return false;
        }

        MTL::Texture* source_texture = dest_is_depth
                                           ? source_rt->texture()
                                           : source_rt->transfer_texture();
        if (!source_texture) {
          return false;
        }
        if (!dest_is_depth) {
          MTL::PixelFormat expected_format =
              GetColorOwnershipTransferPixelFormat(source_key.GetColorFormat(),
                                                   nullptr);
          assert_true(source_texture->pixelFormat() == expected_format,
                      "Transfer source must use ownership pixel format");
        }
        if (source_texture->pixelFormat() != dest_texture->pixelFormat() ||
            source_texture->sampleCount() != dest_texture->sampleCount() ||
            source_texture->width() != dest_width ||
            source_texture->height() != dest_height) {
          return false;
        }

        if (!transfer_plan.rectangle_count) {
          return false;
        }

        MTL::BlitCommandEncoder* blit = ensure_blit_encoder();
        if (!blit) {
          return false;
        }

        for (uint32_t rect_index = 0; rect_index < transfer_plan.rectangle_count;
             ++rect_index) {
          uint32_t scaled_x = 0;
          uint32_t scaled_y = 0;
          uint32_t scaled_width = 0;
          uint32_t scaled_height = 0;
          if (!get_scaled_rect(transfer_plan.rectangles[rect_index], scaled_x,
                               scaled_y, scaled_width, scaled_height)) {
            continue;
          }
          MTL::Origin origin = MTL::Origin::Make(scaled_x, scaled_y, 0);
          MTL::Size size = MTL::Size::Make(scaled_width, scaled_height, 1);
          blit->copyFromTexture(source_texture, 0, 0, origin, size,
                                dest_texture, 0, 0, origin);
        }

        used_blit = true;
        any_transfers_done = true;
        return true;
      };

      for (uint32_t transfer_plan_index : all_transfer_plan_indices) {
        if (!try_blit_transfer(transfer_plan_index)) {
          filtered_transfer_plan_indices.push_back(transfer_plan_index);
        }
      }
    }

    if (blit_encoder) {
      blit_encoder->endEncoding();
    }

    const bool disable_transfer_shaders = false;
    const std::vector<uint32_t>& transfer_plan_indices_for_shaders =
        used_blit ? filtered_transfer_plan_indices : all_transfer_plan_indices;

    auto is_full_target_rectangle =
        [&](const Transfer::Rectangle& rect) -> bool {
      uint32_t scaled_x = 0;
      uint32_t scaled_y = 0;
      uint32_t scaled_width = 0;
      uint32_t scaled_height = 0;
      if (!get_scaled_rect(rect, scaled_x, scaled_y, scaled_width,
                           scaled_height)) {
        return false;
      }
      return !scaled_x && !scaled_y && scaled_width == dest_width &&
             scaled_height == dest_height;
    };

    auto transfers_fully_overwrite_target = [&]() -> bool {
      if (transfer_plan_indices_for_shaders.empty() ||
          !target_transfer_rectangles) {
        return false;
      }
      for (uint32_t transfer_plan_index : transfer_plan_indices_for_shaders) {
        const TransferRectanglePlan& transfer_plan =
            (*target_transfer_rectangles)[transfer_plan_index];
        if (transfer_plan.rectangle_count != 1 ||
            !is_full_target_rectangle(transfer_plan.rectangles[0])) {
          return false;
        }
      }
      return true;
    };

    bool resolve_clear_fully_overwrites_target = false;
    if (resolve_clear_needed && resolve_clear_rectangle) {
      resolve_clear_fully_overwrites_target =
          is_full_target_rectangle(*resolve_clear_rectangle);
    }

    bool resolve_clear_via_load_action = false;
    MTL::ClearColor resolve_clear_color = MTL::ClearColor(0.0, 0.0, 0.0, 0.0);
    double resolve_clear_depth = 1.0;
    uint32_t resolve_clear_stencil = 0;
    if (resolve_clear_needed && resolve_clear_fully_overwrites_target) {
      const uint64_t clear_value = render_target_resolve_clear_values[i];
      if (dest_is_depth) {
        uint32_t depth_guest_clear_value =
            (uint32_t(clear_value) >> 8) & 0xFFFFFF;
        switch (dest_key.GetDepthFormat()) {
          case xenos::DepthRenderTargetFormat::kD24S8:
            resolve_clear_depth = xenos::UNorm24To32(depth_guest_clear_value);
            resolve_clear_via_load_action = true;
            break;
          case xenos::DepthRenderTargetFormat::kD24FS8:
            resolve_clear_depth =
                xenos::Float20e4To32(depth_guest_clear_value) * 0.5f;
            resolve_clear_via_load_action = true;
            break;
        }
        resolve_clear_stencil = uint32_t(clear_value) & 0xFF;
      } else {
        TransferClearColorFloatConstants float_constants = {};
        bool clear_via_drawing = false;
        switch (dest_key.GetColorFormat()) {
          case xenos::ColorRenderTargetFormat::k_8_8_8_8: {
            for (uint32_t j = 0; j < 4; ++j) {
              float_constants.color[j] =
                  ((clear_value >> (j * 8)) & 0xFF) * (1.0f / 0xFF);
            }
          } break;
          case xenos::ColorRenderTargetFormat::k_8_8_8_8_GAMMA: {
            for (uint32_t j = 0; j < 4; ++j) {
              float_constants.color[j] =
                  ((clear_value >> (j * 8)) & 0xFF) * (1.0f / 0xFF);
            }
            if (gamma_render_target_as_unorm16_) {
              for (uint32_t j = 0; j < 3; ++j) {
                float_constants.color[j] =
                    xenos::PWLGammaToLinear(float_constants.color[j]);
              }
            }
          } break;
          case xenos::ColorRenderTargetFormat::k_2_10_10_10:
          case xenos::ColorRenderTargetFormat::k_2_10_10_10_AS_10_10_10_10: {
            for (uint32_t j = 0; j < 3; ++j) {
              float_constants.color[j] =
                  ((clear_value >> (j * 10)) & 0x3FF) * (1.0f / 0x3FF);
            }
            float_constants.color[3] =
                ((clear_value >> 30) & 0x3) * (1.0f / 0x3);
          } break;
          case xenos::ColorRenderTargetFormat::k_2_10_10_10_FLOAT:
          case xenos::ColorRenderTargetFormat::
              k_2_10_10_10_FLOAT_AS_16_16_16_16: {
            for (uint32_t j = 0; j < 3; ++j) {
              float_constants.color[j] =
                  xenos::Float7e3To32((clear_value >> (j * 10)) & 0x3FF);
            }
            float_constants.color[3] =
                ((clear_value >> 30) & 0x3) * (1.0f / 0x3);
          } break;
          case xenos::ColorRenderTargetFormat::k_16_16:
          case xenos::ColorRenderTargetFormat::k_16_16_FLOAT: {
            for (uint32_t j = 0; j < 2; ++j) {
              float_constants.color[j] =
                  float((clear_value >> (j * 16)) & 0xFFFF);
            }
          } break;
          case xenos::ColorRenderTargetFormat::k_16_16_16_16:
          case xenos::ColorRenderTargetFormat::k_16_16_16_16_FLOAT: {
            for (uint32_t j = 0; j < 4; ++j) {
              float_constants.color[j] =
                  float((clear_value >> (j * 16)) & 0xFFFF);
            }
          } break;
          case xenos::ColorRenderTargetFormat::k_32_FLOAT: {
            float_constants.color[0] = float(uint32_t(clear_value));
            if (uint64_t(float_constants.color[0]) != uint32_t(clear_value)) {
              clear_via_drawing = true;
            }
          } break;
          case xenos::ColorRenderTargetFormat::k_32_32_FLOAT: {
            float_constants.color[0] = float(uint32_t(clear_value));
            float_constants.color[1] = float(uint32_t(clear_value >> 32));
            if (uint64_t(float_constants.color[0]) != uint32_t(clear_value) ||
                uint64_t(float_constants.color[1]) !=
                    uint32_t(clear_value >> 32)) {
              clear_via_drawing = true;
            }
          } break;
        }

        bool clear_is_uint = false;
        GetColorOwnershipTransferPixelFormat(dest_key.GetColorFormat(),
                                             &clear_is_uint);
        if (!clear_is_uint && !clear_via_drawing) {
          resolve_clear_color = MTL::ClearColor(
              float_constants.color[0], float_constants.color[1],
              float_constants.color[2], float_constants.color[3]);
          resolve_clear_via_load_action = true;
        }
      }
    }
    if (resolve_clear_via_load_action &&
        transfer_plan_indices_for_shaders.empty()) {
      resolve_clear_via_load_action = false;
    }

    // Depth transfers that fully overwrite the destination still need a clean
    // stencil surface before the per-bit stencil draws run. A load-action
    // clear is cheaper than a separate clear draw in that case.
    bool transfer_stencil_clear_via_load_action =
        dest_is_depth && !transfer_plan_indices_for_shaders.empty() &&
        transfers_fully_overwrite_target() && !resolve_clear_via_load_action;
    if (transfer_stencil_clear_via_load_action) {
      resolve_clear_depth = 0.0;
      resolve_clear_stencil = 0;
    }

    // Prefer DontCare on transfer-pass loads only when destination contents are
    // provably fully overwritten by this pass.
    bool transfer_pass_load_dontcare = false;
    if (resolve_clear_fully_overwrites_target &&
        !resolve_clear_via_load_action) {
      transfer_pass_load_dontcare = true;
    }
    if (!transfer_pass_load_dontcare && !resolve_clear_needed &&
        !transfer_stencil_clear_via_load_action) {
      transfer_pass_load_dontcare = transfers_fully_overwrite_target();
    }
    MTL::LoadAction transfer_load_action = MTL::LoadActionLoad;
    if (resolve_clear_via_load_action ||
        transfer_stencil_clear_via_load_action) {
      transfer_load_action = MTL::LoadActionClear;
    } else if (transfer_pass_load_dontcare) {
      transfer_load_action = MTL::LoadActionDontCare;
    }

    MTL::RenderCommandEncoder* transfer_encoder = nullptr;
    auto ensure_transfer_encoder = [&]() -> MTL::RenderCommandEncoder* {
      if (transfer_encoder) {
        return transfer_encoder;
      }
      if (use_active_render_encoder) {
        transfer_encoder = active_render_encoder;
        if (dest_is_depth) {
        } else {
        }
        return transfer_encoder;
      }
      MTL::RenderPassDescriptor* rp =
          MTL::RenderPassDescriptor::renderPassDescriptor();
      if (dest_is_depth) {
        auto* da = rp->depthAttachment();
        da->setTexture(dest_texture);
        da->setLoadAction(transfer_load_action);
        da->setStoreAction(MTL::StoreActionStore);
        if (transfer_load_action == MTL::LoadActionClear) {
          da->setClearDepth(resolve_clear_depth);
        }
        if (dest_pixel_format == MTL::PixelFormatDepth32Float_Stencil8 ||
            dest_pixel_format == MTL::PixelFormatDepth24Unorm_Stencil8) {
          auto* sa = rp->stencilAttachment();
          sa->setTexture(dest_texture);
          sa->setLoadAction(transfer_load_action);
          sa->setStoreAction(MTL::StoreActionStore);
          if (transfer_load_action == MTL::LoadActionClear) {
            sa->setClearStencil(resolve_clear_stencil);
          }
        }
      } else {
        auto* ca = rp->colorAttachments()->object(0);
        ca->setTexture(dest_texture);
        ca->setLoadAction(transfer_load_action);
        ca->setStoreAction(MTL::StoreActionStore);
        if (transfer_load_action == MTL::LoadActionClear) {
          ca->setClearColor(resolve_clear_color);
        }
      }
      EndSharedMemoryUploadBlitEncoderForCommandBuffer(command_processor_, cmd);
      transfer_encoder = cmd->renderCommandEncoder(rp);
      if (transfer_encoder) {
        transfer_encoder->setLabel(
            NS::String::string("XeniaTransferEncoder", NS::UTF8StringEncoding));
      }
      return transfer_encoder;
    };

    if (!transfer_plan_indices_for_shaders.empty() && disable_transfer_shaders) {
      static uint32_t transfer_shader_skip_log_count = 0;
      if (transfer_shader_skip_log_count < 8) {
        ++transfer_shader_skip_log_count;
        XELOGW(
            "MetalRenderTargetCache::PerformTransfersAndResolveClears: "
            "transfer shaders disabled; skipping {} transfers for RT {}",
            transfer_plan_indices_for_shaders.size(), i);
      }
    } else if (!transfer_plan_indices_for_shaders.empty() &&
               target_transfer_rectangles) {
      bool need_stencil_bit_draws = dest_is_depth;
      bool stencil_clear_needed =
          need_stencil_bit_draws && !transfer_stencil_clear_via_load_action;

      transfer_invocations_.clear();
      transfer_invocations_.reserve(transfer_plan_indices_for_shaders.size() *
                                    (need_stencil_bit_draws ? 2 : 1));

      for (uint32_t transfer_plan_index : transfer_plan_indices_for_shaders) {
        const TransferRectanglePlan& transfer_plan =
            (*target_transfer_rectangles)[transfer_plan_index];
        const Transfer& transfer = transfers[transfer_plan.transfer_index];
        if (transfer.source) {
          auto* source = static_cast<MetalRenderTarget*>(transfer.source);
          source->SetTemporarySortIndex(UINT32_MAX);
        }
        if (transfer.host_depth_source) {
          auto* host_depth =
              static_cast<MetalRenderTarget*>(transfer.host_depth_source);
          host_depth->SetTemporarySortIndex(UINT32_MAX);
        }
      }

      uint32_t rt_sort_index = 0;
      auto ensure_sort_index = [&](MetalRenderTarget* rt) {
        if (rt && rt->temporary_sort_index() == UINT32_MAX) {
          rt->SetTemporarySortIndex(rt_sort_index++);
        }
      };

      for (uint32_t pass = 0; pass <= uint32_t(need_stencil_bit_draws);
           ++pass) {
        for (uint32_t transfer_plan_index : transfer_plan_indices_for_shaders) {
          const TransferRectanglePlan& transfer_plan =
              (*target_transfer_rectangles)[transfer_plan_index];
          const Transfer& transfer = transfers[transfer_plan.transfer_index];
          if (!transfer.source) {
            continue;
          }
          auto* source_rt = static_cast<MetalRenderTarget*>(transfer.source);
          auto* host_depth_rt =
              pass
                  ? nullptr
                  : static_cast<MetalRenderTarget*>(transfer.host_depth_source);
          ensure_sort_index(source_rt);
          ensure_sort_index(host_depth_rt);

          RenderTargetKey source_key = source_rt->key();
          bool host_depth_is_copy = host_depth_rt == dest_metal_rt;
          RenderTargetKey host_depth_key;
          if (host_depth_rt) {
            host_depth_key = host_depth_rt->key();
          }
          TransferShaderKey shader_key = GetTransferShaderKey(
              source_key, dest_key, host_depth_rt ? &host_depth_key : nullptr,
              host_depth_is_copy, pass != 0, transfer_use_sample_id_default);

          transfer_invocations_.emplace_back(transfer, shader_key,
                                             &transfer_plan);
          if (pass) {
            transfer_invocations_.back().transfer.host_depth_source = nullptr;
          }
        }
      }

      std::sort(transfer_invocations_.begin(), transfer_invocations_.end());

      if (stencil_clear_needed) {
        MTL::RenderPipelineState* clear_pipeline =
            GetOrCreateTransferClearPipeline(dest_pixel_format, false, true,
                                             dest_sample_count, 0,
                                             use_active_render_encoder
                                                 ? &active_attachment_formats
                                                        .color_attachment_formats
                                                 : nullptr,
                                             use_active_render_encoder
                                                 ? active_attachment_formats
                                                        .depth_attachment_format
                                                 : MTL::PixelFormatInvalid,
                                             use_active_render_encoder
                                                 ? active_attachment_formats
                                                        .stencil_attachment_format
                                                 : MTL::PixelFormatInvalid);
        MTL::DepthStencilState* stencil_clear_state =
            GetTransferStencilClearState();
        if (clear_pipeline && stencil_clear_state) {
          MTL::RenderCommandEncoder* encoder = ensure_transfer_encoder();
          if (encoder) {
            TransferClearDepthConstants constants = {};
            constants.depth = 0.0f;
            encoder->setRenderPipelineState(clear_pipeline);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationPipeline);
            encoder->setDepthStencilState(stencil_clear_state);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationDepthStencil);
            encoder->setStencilReferenceValue(0);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationStencilReference);
            encoder->setFragmentBytes(&constants, sizeof(constants), 0);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationFragmentSlot0);
            for (uint32_t transfer_plan_index :
                 transfer_plan_indices_for_shaders) {
              const TransferRectanglePlan& transfer_plan =
                  (*target_transfer_rectangles)[transfer_plan_index];
              for (uint32_t rect_index = 0;
                   rect_index < transfer_plan.rectangle_count;
                   ++rect_index) {
                if (!set_rect_viewport(encoder,
                                       transfer_plan.rectangles[rect_index])) {
                  continue;
                }
                uint32_t scaled_x = 0;
                uint32_t scaled_y = 0;
                uint32_t scaled_width = 0;
                uint32_t scaled_height = 0;
                if (get_scaled_rect(transfer_plan.rectangles[rect_index],
                                    scaled_x, scaled_y, scaled_width,
                                    scaled_height)) {
                }
                encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                                        NS::UInteger(0), NS::UInteger(3));
              }
            }
          }
        }
      }
      MTL::RenderCommandEncoder* encoder = ensure_transfer_encoder();
      if (encoder) {
        bool transfer_viewport_full_set = false;
        MTL::ScissorRect last_transfer_scissor = {};
        bool last_transfer_scissor_valid = false;
        MTL::RenderPipelineState* last_transfer_pipeline = nullptr;
        MTL::DepthStencilState* last_transfer_depth_state = nullptr;
        MTL::Buffer* last_transfer_fragment_buffer_1 = nullptr;
        std::array<MTL::Texture*, 3> last_transfer_fragment_textures = {
            nullptr, nullptr, nullptr};
        bool last_transfer_stencil_reference_valid = false;
        uint32_t last_transfer_stencil_reference = 0;
        bool transfer_constants_valid = false;
        TransferShaderConstants last_transfer_constants = {};
        bool last_transfer_vertex_bytes_1_valid = false;
        TransferRectInstance last_transfer_vertex_bytes_1 = {};
        auto bind_transfer_pipeline = [&](MTL::RenderPipelineState* pipeline) {
          if (last_transfer_pipeline != pipeline) {
            encoder->setRenderPipelineState(pipeline);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationPipeline);
            last_transfer_pipeline = pipeline;
          }
        };
        auto bind_transfer_depth_state = [&](MTL::DepthStencilState* state) {
          if (last_transfer_depth_state != state) {
            encoder->setDepthStencilState(state);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationDepthStencil);
            last_transfer_depth_state = state;
          }
        };
        auto bind_transfer_fragment_texture = [&](uint32_t index,
                                                  MTL::Texture* texture) {
          if (index >= last_transfer_fragment_textures.size()) {
            return;
          }
          if (last_transfer_fragment_textures[index] != texture) {
            encoder->setFragmentTexture(texture, index);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationFragmentTextures);
            last_transfer_fragment_textures[index] = texture;
          }
        };
        auto bind_transfer_fragment_buffer_1 = [&](MTL::Buffer* buffer) {
          if (last_transfer_fragment_buffer_1 != buffer) {
            encoder->setFragmentBuffer(buffer, 0, 1);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationFragmentSlot1);
            last_transfer_fragment_buffer_1 = buffer;
          }
        };
        auto bind_transfer_stencil_reference = [&](uint32_t reference) {
          if (!last_transfer_stencil_reference_valid ||
              last_transfer_stencil_reference != reference) {
            encoder->setStencilReferenceValue(reference);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationStencilReference);
            last_transfer_stencil_reference = reference;
            last_transfer_stencil_reference_valid = true;
          }
        };
        auto bind_transfer_constants =
            [&](const TransferShaderConstants& constants) {
              if (!transfer_constants_valid ||
                  std::memcmp(&last_transfer_constants, &constants,
                              sizeof(constants)) != 0) {
                encoder->setVertexBytes(&constants, sizeof(constants), 0);
                encoder->setFragmentBytes(&constants, sizeof(constants), 0);
                mark_active_encoder_mutation(
                    kDrawPassTransferEncoderMutationVertexSlot0 |
                    kDrawPassTransferEncoderMutationFragmentSlot0);
                last_transfer_constants = constants;
                transfer_constants_valid = true;
              }
            };
        auto bind_transfer_scissor = [&](const MTL::ScissorRect& scissor) {
          if (!last_transfer_scissor_valid ||
              last_transfer_scissor.x != scissor.x ||
              last_transfer_scissor.y != scissor.y ||
              last_transfer_scissor.width != scissor.width ||
              last_transfer_scissor.height != scissor.height) {
            encoder->setScissorRect(scissor);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationScissor);
            last_transfer_scissor = scissor;
            last_transfer_scissor_valid = true;
          }
        };
        auto bind_transfer_vertex_bytes_1 =
            [&](const TransferRectInstance& rect_instance) {
              if (!last_transfer_vertex_bytes_1_valid ||
                  std::memcmp(&last_transfer_vertex_bytes_1, &rect_instance,
                              sizeof(rect_instance)) != 0) {
                encoder->setVertexBytes(&rect_instance, sizeof(rect_instance),
                                        1);
                mark_active_encoder_mutation(
                    kDrawPassTransferEncoderMutationVertexSlot1);
                last_transfer_vertex_bytes_1 = rect_instance;
                last_transfer_vertex_bytes_1_valid = true;
              }
            };
        auto bind_transfer_vertex_bytes_1_span =
            [&](const TransferRectInstance* rect_instances,
                uint32_t rect_instance_count) {
              if (!rect_instances || !rect_instance_count) {
                return;
              }
              encoder->setVertexBytes(
                  rect_instances,
                  size_t(rect_instance_count) * sizeof(TransferRectInstance),
                  1);
              mark_active_encoder_mutation(
                  kDrawPassTransferEncoderMutationVertexSlot1);
              last_transfer_vertex_bytes_1_valid = false;
            };
        auto set_full_transfer_viewport_scissor = [&]() {
          if (!transfer_viewport_full_set) {
            MTL::Viewport vp;
            vp.originX = 0.0;
            vp.originY = 0.0;
            vp.width = double(dest_width);
            vp.height = double(dest_height);
            vp.znear = 0.0;
            vp.zfar = 1.0;
            encoder->setViewport(vp);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationViewport);
            transfer_viewport_full_set = true;
          }
          MTL::ScissorRect scissor;
          scissor.x = 0;
          scissor.y = 0;
          scissor.width = dest_width;
          scissor.height = dest_height;
          bind_transfer_scissor(scissor);
        };

        std::vector<Transfer::Rectangle> merged_transfer_rectangles;
        for (size_t invocation_index = 0;
             invocation_index < transfer_invocations_.size();) {
          const auto& invocation = transfer_invocations_[invocation_index];
          size_t merged_invocation_end = invocation_index + 1;
          while (merged_invocation_end < transfer_invocations_.size() &&
                 invocation.CanBeMergedIntoOneDraw(
                     transfer_invocations_[merged_invocation_end])) {
            ++merged_invocation_end;
          }

          merged_transfer_rectangles.clear();
          merged_transfer_rectangles.reserve(
              (merged_invocation_end - invocation_index) *
              Transfer::kMaxRectanglesWithCutout);
          for (size_t merged_index = invocation_index;
               merged_index < merged_invocation_end; ++merged_index) {
            const TransferRectanglePlan* rectangle_plan =
                transfer_invocations_[merged_index].rectangle_plan;
            if (!rectangle_plan) {
              continue;
            }
            for (uint32_t rect_index = 0;
                 rect_index < rectangle_plan->rectangle_count; ++rect_index) {
              merged_transfer_rectangles.push_back(
                  rectangle_plan->rectangles[rect_index]);
            }
          }
          invocation_index = merged_invocation_end;
          if (merged_transfer_rectangles.empty()) {
            continue;
          }
          uint64_t merged_transfer_pixels = 0;
          for (const Transfer::Rectangle& rect : merged_transfer_rectangles) {
            uint32_t scaled_x = 0;
            uint32_t scaled_y = 0;
            uint32_t scaled_width = 0;
            uint32_t scaled_height = 0;
            if (get_scaled_rect(rect, scaled_x, scaled_y, scaled_width,
                                scaled_height)) {
              merged_transfer_pixels +=
                  uint64_t(scaled_width) * uint64_t(scaled_height);
            }
          }

          const Transfer& transfer = invocation.transfer;
          const TransferShaderKey& shader_key = invocation.shader_key;
          const TransferModeInfo& mode_info =
              kTransferModeInfos[size_t(shader_key.mode)];
          bool is_stencil_bit = mode_info.output == TransferOutput::kStencilBit;
          bool needs_source_depth =
              !mode_info.source_is_color &&
              mode_info.output != TransferOutput::kStencilBit;
          bool needs_source_stencil =
              !mode_info.source_is_color &&
              (mode_info.output == TransferOutput::kColor ||
               mode_info.output == TransferOutput::kStencilBit);

          auto* source_rt = static_cast<MetalRenderTarget*>(transfer.source);
          if (!source_rt) {
            continue;
          }

          RenderTargetKey source_key = source_rt->key();
          bool source_is_uint = false;
          MTL::PixelFormat source_transfer_format = MTL::PixelFormatInvalid;
          if (mode_info.source_is_color) {
            source_transfer_format = GetColorOwnershipTransferPixelFormat(
                source_key.GetColorFormat(), &source_is_uint);
          }

          if (is_stencil_bit) {
            // Depth/stencil state set per-bit below.
          } else if (dest_is_depth) {
            bind_transfer_depth_state(GetTransferDepthStencilState(true));
          } else {
            MTL::DepthStencilState* no_depth_state =
                GetTransferNoDepthStencilState();
            if (!no_depth_state) {
              continue;
            }
            bind_transfer_depth_state(no_depth_state);
          }

          // Bind source textures.
          if (mode_info.source_is_color) {
            MTL::Texture* source_texture = source_rt->transfer_texture();
            if (!source_texture) {
              continue;
            }
            assert_true(source_texture->pixelFormat() == source_transfer_format,
                        "Transfer source must use ownership pixel format");
            bind_transfer_fragment_texture(0, source_texture);
          } else {
            if (needs_source_depth) {
              MTL::Texture* depth_texture = source_rt->texture();
              if (!depth_texture) {
                continue;
              }
              bind_transfer_fragment_texture(0, depth_texture);
            }
            if (needs_source_stencil) {
              MTL::Texture* stencil_texture = GetStencilTextureView(source_rt);
              if (!stencil_texture) {
                continue;
              }
              bind_transfer_fragment_texture(1, stencil_texture);
            }
          }

          // Bind host depth source if needed.
          if (mode_info.uses_host_depth) {
            uint32_t host_depth_index = mode_info.source_is_color ? 1 : 2;
            if (shader_key.host_depth_source_is_copy) {
              if (edram_buffer_) {
                bind_transfer_fragment_buffer_1(edram_buffer_);
              } else {
                MTL::Buffer* dummy = GetTransferDummyBuffer();
                if (dummy) {
                  bind_transfer_fragment_buffer_1(dummy);
                }
              }
              MTL::Texture* dummy_host_depth = GetTransferDummyDepthTexture(1);
              if (!dummy_host_depth) {
                continue;
              }
              bind_transfer_fragment_texture(host_depth_index,
                                             dummy_host_depth);
            } else {
              MTL::Buffer* dummy = GetTransferDummyBuffer();
              if (!dummy) {
                continue;
              }
              bind_transfer_fragment_buffer_1(dummy);
              auto* host_depth_rt =
                  static_cast<MetalRenderTarget*>(transfer.host_depth_source);
              MTL::Texture* host_depth_texture =
                  host_depth_rt ? host_depth_rt->texture() : nullptr;
              if (!host_depth_texture) {
                continue;
              }
              bind_transfer_fragment_texture(host_depth_index,
                                             host_depth_texture);
            }
          }

          TransferShaderConstants constants = {};
          constants.address.dest_pitch = dest_key.GetPitchTiles();
          constants.address.source_pitch = source_key.GetPitchTiles();
          constants.address.source_to_dest =
              int32_t(dest_key.base_tiles) - int32_t(source_key.base_tiles);
          constants.host_depth_address = {};
          if (mode_info.uses_host_depth &&
              !shader_key.host_depth_source_is_copy) {
            auto* host_depth_rt =
                static_cast<MetalRenderTarget*>(transfer.host_depth_source);
            if (host_depth_rt) {
              RenderTargetKey host_depth_key = host_depth_rt->key();
              constants.host_depth_address.dest_pitch =
                  dest_key.GetPitchTiles();
              constants.host_depth_address.source_pitch =
                  host_depth_key.GetPitchTiles();
              constants.host_depth_address.source_to_dest =
                  int32_t(dest_key.base_tiles) -
                  int32_t(host_depth_key.base_tiles);
            }
          }
          constants.source_format = source_key.resource_format;
          constants.dest_format = dest_key.resource_format;
          constants.source_is_depth = source_key.is_depth ? 1 : 0;
          constants.dest_is_depth = dest_key.is_depth ? 1 : 0;
          constants.source_is_uint = source_is_uint ? 1 : 0;
          constants.dest_is_uint = dest_is_uint ? 1 : 0;
          // Don't treat gamma-as-unorm16 as 64bpp for guest coordinate math.
          // It's 64bpp in storage, but represents a single pixel, not two
          // packed 32bpp halves.
          constants.source_is_64bpp = source_key.Is64bpp() ? 1 : 0;
          constants.dest_is_64bpp = dest_key.Is64bpp() ? 1 : 0;
          constants.source_msaa_samples =
              MsaaSamplesToCount(source_key.msaa_samples);
          constants.dest_msaa_samples =
              MsaaSamplesToCount(dest_key.msaa_samples);
          constants.host_depth_source_msaa_samples =
              MsaaSamplesToCount(shader_key.host_depth_source_msaa_samples);
          constants.host_depth_source_is_copy =
              shader_key.host_depth_source_is_copy ? 1 : 0;
          constants.depth_round = depth_round;
          constants.msaa_2x_supported = msaa_2x_supported_ ? 1 : 0;
          constants.tile_width_samples = tile_width_samples;
          constants.tile_height_samples = tile_height_samples;
          uint32_t dest_tile_width_pixels =
              tile_width_samples >>
              ((constants.dest_is_64bpp != 0u) +
               (constants.dest_msaa_samples >= 4u ? 1u : 0u));
          uint32_t dest_tile_height_pixels =
              tile_height_samples >>
              (constants.dest_msaa_samples >= 2u ? 1u : 0u);
          constants.dest_tile_width_pixels = dest_tile_width_pixels;
          constants.dest_tile_height_pixels = dest_tile_height_pixels;
          constants.dest_tile_width_pixels_inv =
              dest_tile_width_pixels ? (1.0f / float(dest_tile_width_pixels))
                                     : 0.0f;
          constants.dest_tile_height_pixels_inv =
              dest_tile_height_pixels ? (1.0f / float(dest_tile_height_pixels))
                                      : 0.0f;
          uint32_t source_pitch_tiles = source_key.GetPitchTiles();
          constants.source_pitch_tiles_inv =
              source_pitch_tiles ? (1.0f / float(source_pitch_tiles)) : 0.0f;
          constants.host_depth_source_pitch_tiles_inv = 0.0f;
          if (mode_info.uses_host_depth &&
              !shader_key.host_depth_source_is_copy) {
            auto* host_depth_rt =
                static_cast<MetalRenderTarget*>(transfer.host_depth_source);
            if (host_depth_rt) {
              uint32_t host_pitch_tiles = host_depth_rt->key().GetPitchTiles();
              constants.host_depth_source_pitch_tiles_inv =
                  host_pitch_tiles ? (1.0f / float(host_pitch_tiles)) : 0.0f;
            }
          }
          constants.dest_pixel_to_ndc_x =
              dest_width ? (2.0f / float(dest_width)) : 0.0f;
          constants.dest_pixel_to_ndc_y =
              dest_height ? (2.0f / float(dest_height)) : 0.0f;
          constants.dest_sample_id = 0;

          const uint32_t rectangle_count =
              uint32_t(merged_transfer_rectangles.size());

          std::vector<TransferRectInstance> rect_instance_fallback;
          rect_instance_fallback.reserve(rectangle_count);
          for (uint32_t rect_index = 0; rect_index < rectangle_count;
               ++rect_index) {
            uint32_t scaled_x = 0;
            uint32_t scaled_y = 0;
            uint32_t scaled_width = 0;
            uint32_t scaled_height = 0;
            if (!get_scaled_rect(merged_transfer_rectangles[rect_index],
                                 scaled_x, scaled_y, scaled_width,
                                 scaled_height) ||
                !scaled_width || !scaled_height) {
              continue;
            }
            TransferRectInstance rect_instance = {};
            rect_instance.origin_x = float(scaled_x);
            rect_instance.origin_y = float(scaled_y);
            rect_instance.size_x = float(scaled_width);
            rect_instance.size_y = float(scaled_height);
            rect_instance_fallback.push_back(rect_instance);
          }

          MTL::DepthStencilState* native_stencil_output_state =
              is_stencil_bit && ::cvars::metal_transfer_native_stencil_output
                  ? GetTransferStencilOutputState()
                  : nullptr;
          bool use_native_stencil_output =
              native_stencil_output_state != nullptr;
          MTL::RenderPipelineState* pipeline = GetOrCreateTransferPipelines(
              shader_key, dest_pixel_format, dest_is_uint,
              use_native_stencil_output, active_color_attachment_index,
              use_active_render_encoder
                  ? &active_attachment_formats.color_attachment_formats
                  : nullptr,
              use_active_render_encoder
                  ? active_attachment_formats.depth_attachment_format
                  : MTL::PixelFormatInvalid,
              use_active_render_encoder
                  ? active_attachment_formats.stencil_attachment_format
                  : MTL::PixelFormatInvalid);
          if (!pipeline && use_native_stencil_output) {
            use_native_stencil_output = false;
            native_stencil_output_state = nullptr;
            pipeline = GetOrCreateTransferPipelines(
                shader_key, dest_pixel_format, dest_is_uint, false,
                active_color_attachment_index,
                use_active_render_encoder
                    ? &active_attachment_formats.color_attachment_formats
                    : nullptr,
                use_active_render_encoder
                    ? active_attachment_formats.depth_attachment_format
                    : MTL::PixelFormatInvalid,
                use_active_render_encoder
                    ? active_attachment_formats.stencil_attachment_format
                    : MTL::PixelFormatInvalid);
          }
          if (!pipeline) {
            continue;
          }
          bind_transfer_pipeline(pipeline);

          bool use_sample_id_for_invocation =
              shader_key.dest_sample_id_from_sample != 0;
          auto draw_transfer_samples = [&](auto&& draw_fn) {
            if (use_sample_id_for_invocation || dest_sample_count <= 1) {
              draw_fn(0);
              return;
            }
            for (uint32_t sample_id = 0; sample_id < dest_sample_count;
                 ++sample_id) {
              draw_fn(sample_id);
            }
          };

          auto draw_transfer = [&](uint32_t sample_id) {
            constants.dest_sample_id = sample_id;
            bind_transfer_constants(constants);
            if (rect_instance_fallback.empty()) {
              return;
            }
            set_full_transfer_viewport_scissor();
            constexpr uint32_t kTransferRectInlineBatchMax = 240;
            const TransferRectInstance* rect_instances =
                rect_instance_fallback.data();
            uint32_t rect_instances_remaining =
                uint32_t(rect_instance_fallback.size());
            while (rect_instances_remaining) {
              uint32_t batch_count = std::min(rect_instances_remaining,
                                              kTransferRectInlineBatchMax);
              if (batch_count == 1) {
                bind_transfer_vertex_bytes_1(*rect_instances);
              } else {
                bind_transfer_vertex_bytes_1_span(rect_instances, batch_count);
              }
              encoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip,
                                      NS::UInteger(0), NS::UInteger(4),
                                      NS::UInteger(batch_count));
              rect_instances += batch_count;
              rect_instances_remaining -= batch_count;
            }
          };

          if (is_stencil_bit) {
            if (use_native_stencil_output) {
              if (!native_stencil_output_state) {
                use_native_stencil_output = false;
              } else {
                constants.stencil_mask = 0xFFu;
                constants.stencil_clear = 0;
                bind_transfer_depth_state(native_stencil_output_state);
                bind_transfer_stencil_reference(0);
                draw_transfer_samples(draw_transfer);
              }
            }
            if (!use_native_stencil_output) {
              for (uint32_t bit = 0; bit < 8; ++bit) {
                MTL::DepthStencilState* stencil_state =
                    GetTransferStencilBitState(bit);
                if (!stencil_state) {
                  continue;
                }
                constants.stencil_mask = uint32_t(1) << bit;
                constants.stencil_clear = 0;
                bind_transfer_depth_state(stencil_state);
                bind_transfer_stencil_reference(uint32_t(1) << bit);
                draw_transfer_samples(draw_transfer);
              }
            }
          } else {
            constants.stencil_mask = 0;
            constants.stencil_clear = 0;
            draw_transfer_samples(draw_transfer);
          }
          any_transfers_done = true;
        }
      }
    }

    if (resolve_clear_needed && !resolve_clear_via_load_action) {
      uint64_t clear_value = render_target_resolve_clear_values[i];
      if (dest_is_depth) {
        uint32_t depth_guest_clear_value =
            (uint32_t(clear_value) >> 8) & 0xFFFFFF;
        float depth_host_clear_value = 0.0f;
        switch (dest_key.GetDepthFormat()) {
          case xenos::DepthRenderTargetFormat::kD24S8:
            depth_host_clear_value =
                xenos::UNorm24To32(depth_guest_clear_value);
            break;
          case xenos::DepthRenderTargetFormat::kD24FS8:
            depth_host_clear_value =
                xenos::Float20e4To32(depth_guest_clear_value) * 0.5f;
            break;
        }
        MTL::RenderPipelineState* clear_pipeline =
            GetOrCreateTransferClearPipeline(dest_pixel_format, false, true,
                                             dest_sample_count);
        MTL::DepthStencilState* clear_state = GetTransferDepthClearState();
        if (clear_pipeline && clear_state) {
          MTL::RenderCommandEncoder* clear_encoder = ensure_transfer_encoder();
          if (clear_encoder) {
            TransferClearDepthConstants constants = {};
            constants.depth = depth_host_clear_value;
            clear_encoder->setRenderPipelineState(clear_pipeline);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationPipeline);
            clear_encoder->setDepthStencilState(clear_state);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationDepthStencil);
            clear_encoder->setStencilReferenceValue(uint32_t(clear_value) &
                                                    0xFF);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationStencilReference);
            clear_encoder->setFragmentBytes(&constants, sizeof(constants), 0);
            mark_active_encoder_mutation(
                kDrawPassTransferEncoderMutationFragmentSlot0);
            Transfer::Rectangle clear_rect = *resolve_clear_rectangle;
            if (set_rect_viewport(clear_encoder, clear_rect)) {
              uint32_t scaled_x = 0;
              uint32_t scaled_y = 0;
              uint32_t scaled_width = 0;
              uint32_t scaled_height = 0;
              if (get_scaled_rect(clear_rect, scaled_x, scaled_y, scaled_width,
                                  scaled_height)) {
              }
              clear_encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                                            NS::UInteger(0), NS::UInteger(3));
            }
          }
        }
      } else {
        TransferClearColorFloatConstants float_constants = {};
        TransferClearColorUintConstants uint_constants = {};
        bool clear_via_drawing = false;
        switch (dest_key.GetColorFormat()) {
          case xenos::ColorRenderTargetFormat::k_8_8_8_8: {
            for (uint32_t j = 0; j < 4; ++j) {
              float_constants.color[j] =
                  ((clear_value >> (j * 8)) & 0xFF) * (1.0f / 0xFF);
            }
          } break;
          case xenos::ColorRenderTargetFormat::k_8_8_8_8_GAMMA: {
            for (uint32_t j = 0; j < 4; ++j) {
              float_constants.color[j] =
                  ((clear_value >> (j * 8)) & 0xFF) * (1.0f / 0xFF);
            }
            if (gamma_render_target_as_unorm16_) {
              for (uint32_t j = 0; j < 3; ++j) {
                float_constants.color[j] =
                    xenos::PWLGammaToLinear(float_constants.color[j]);
              }
            }
          } break;
          case xenos::ColorRenderTargetFormat::k_2_10_10_10:
          case xenos::ColorRenderTargetFormat::k_2_10_10_10_AS_10_10_10_10: {
            for (uint32_t j = 0; j < 3; ++j) {
              float_constants.color[j] =
                  ((clear_value >> (j * 10)) & 0x3FF) * (1.0f / 0x3FF);
            }
            float_constants.color[3] =
                ((clear_value >> 30) & 0x3) * (1.0f / 0x3);
          } break;
          case xenos::ColorRenderTargetFormat::k_2_10_10_10_FLOAT:
          case xenos::ColorRenderTargetFormat::
              k_2_10_10_10_FLOAT_AS_16_16_16_16: {
            for (uint32_t j = 0; j < 3; ++j) {
              float_constants.color[j] =
                  xenos::Float7e3To32((clear_value >> (j * 10)) & 0x3FF);
            }
            float_constants.color[3] =
                ((clear_value >> 30) & 0x3) * (1.0f / 0x3);
          } break;
          case xenos::ColorRenderTargetFormat::k_16_16:
          case xenos::ColorRenderTargetFormat::k_16_16_FLOAT: {
            for (uint32_t j = 0; j < 2; ++j) {
              float_constants.color[j] =
                  float((clear_value >> (j * 16)) & 0xFFFF);
            }
          } break;
          case xenos::ColorRenderTargetFormat::k_16_16_16_16:
          case xenos::ColorRenderTargetFormat::k_16_16_16_16_FLOAT: {
            for (uint32_t j = 0; j < 4; ++j) {
              float_constants.color[j] =
                  float((clear_value >> (j * 16)) & 0xFFFF);
            }
          } break;
          case xenos::ColorRenderTargetFormat::k_32_FLOAT: {
            float_constants.color[0] = float(uint32_t(clear_value));
            if (uint64_t(float_constants.color[0]) != uint32_t(clear_value)) {
              clear_via_drawing = true;
            }
          } break;
          case xenos::ColorRenderTargetFormat::k_32_32_FLOAT: {
            float_constants.color[0] = float(uint32_t(clear_value));
            float_constants.color[1] = float(uint32_t(clear_value >> 32));
            if (uint64_t(float_constants.color[0]) != uint32_t(clear_value) ||
                uint64_t(float_constants.color[1]) !=
                    uint32_t(clear_value >> 32)) {
              clear_via_drawing = true;
            }
          } break;
        }

        bool clear_is_uint = false;
        MTL::PixelFormat clear_format = GetColorOwnershipTransferPixelFormat(
            dest_key.GetColorFormat(), &clear_is_uint);
        MTL::Texture* clear_texture = dest_metal_rt->transfer_texture();
        bool clear_use_uint = clear_is_uint;

        if (clear_use_uint) {
          switch (dest_key.GetColorFormat()) {
            case xenos::ColorRenderTargetFormat::k_16_16:
            case xenos::ColorRenderTargetFormat::k_16_16_FLOAT:
              uint_constants.color[0] = uint32_t(clear_value) & 0xFFFF;
              uint_constants.color[1] = (uint32_t(clear_value) >> 16) & 0xFFFF;
              uint_constants.color[2] = 0;
              uint_constants.color[3] = 0;
              break;
            case xenos::ColorRenderTargetFormat::k_16_16_16_16:
            case xenos::ColorRenderTargetFormat::k_16_16_16_16_FLOAT:
              uint_constants.color[0] = uint32_t(clear_value) & 0xFFFF;
              uint_constants.color[1] = (uint32_t(clear_value) >> 16) & 0xFFFF;
              uint_constants.color[2] = (uint32_t(clear_value >> 32)) & 0xFFFF;
              uint_constants.color[3] =
                  (uint32_t(clear_value >> 32) >> 16) & 0xFFFF;
              break;
            case xenos::ColorRenderTargetFormat::k_32_FLOAT:
              uint_constants.color[0] = uint32_t(clear_value);
              uint_constants.color[1] = 0;
              uint_constants.color[2] = 0;
              uint_constants.color[3] = 0;
              break;
            case xenos::ColorRenderTargetFormat::k_32_32_FLOAT:
              uint_constants.color[0] = uint32_t(clear_value);
              uint_constants.color[1] = uint32_t(clear_value >> 32);
              uint_constants.color[2] = 0;
              uint_constants.color[3] = 0;
              break;
            default:
              break;
          }
        }

        if (clear_via_drawing && clear_use_uint) {
          uint_constants.color[0] = uint32_t(clear_value);
          uint_constants.color[1] = uint32_t(clear_value >> 32);
          uint_constants.color[2] = 0;
          uint_constants.color[3] = 0;
        }

        if (clear_texture) {
          MTL::RenderPipelineState* clear_pipeline =
              GetOrCreateTransferClearPipeline(clear_format, clear_use_uint,
                                               false, dest_sample_count);
          if (clear_pipeline) {
            MTL::RenderCommandEncoder* clear_encoder =
                ensure_transfer_encoder();
            if (clear_encoder) {
              MTL::DepthStencilState* no_depth_state =
                  GetTransferNoDepthStencilState();
              if (!no_depth_state) {
                continue;
              }
              clear_encoder->setRenderPipelineState(clear_pipeline);
              mark_active_encoder_mutation(
                  kDrawPassTransferEncoderMutationPipeline);
              clear_encoder->setDepthStencilState(no_depth_state);
              mark_active_encoder_mutation(
                  kDrawPassTransferEncoderMutationDepthStencil);
              if (clear_use_uint) {
                clear_encoder->setFragmentBytes(&uint_constants,
                                                sizeof(uint_constants), 0);
              } else {
                clear_encoder->setFragmentBytes(&float_constants,
                                                sizeof(float_constants), 0);
              }
              mark_active_encoder_mutation(
                  kDrawPassTransferEncoderMutationFragmentSlot0);
              Transfer::Rectangle clear_rect = *resolve_clear_rectangle;
              if (set_rect_viewport(clear_encoder, clear_rect)) {
                uint32_t scaled_x = 0;
                uint32_t scaled_y = 0;
                uint32_t scaled_width = 0;
                uint32_t scaled_height = 0;
                if (get_scaled_rect(clear_rect, scaled_x, scaled_y,
                                    scaled_width, scaled_height)) {
                }
                clear_encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                                              NS::UInteger(0), NS::UInteger(3));
              }
            }
          }
        }
      }
    }

    if (transfer_encoder && !use_active_render_encoder) {
      transfer_encoder->endEncoding();
    }
  }
  return true;
}

MTL::RenderPipelineState* MetalRenderTargetCache::GetOrCreateTransferPipelines(
    const TransferShaderKey& key, MTL::PixelFormat dest_format,
    bool dest_is_uint, bool native_stencil_output,
    uint32_t color_attachment_index,
    const TransferColorAttachmentFormats* color_attachment_formats,
    MTL::PixelFormat depth_attachment_format,
    MTL::PixelFormat stencil_attachment_format) {
  const TransferModeInfo& mode_info = kTransferModeInfos[size_t(key.mode)];
  TransferOutput output = mode_info.output;
  bool source_is_color = mode_info.source_is_color;
  bool has_host_depth = mode_info.uses_host_depth;
  native_stencil_output =
      native_stencil_output && output == TransferOutput::kStencilBit;

  xenos::ColorRenderTargetFormat source_color_format =
      xenos::ColorRenderTargetFormat(key.source_resource_format);
  xenos::ColorRenderTargetFormat dest_color_format =
      xenos::ColorRenderTargetFormat(key.dest_resource_format);
  xenos::DepthRenderTargetFormat source_depth_format =
      xenos::DepthRenderTargetFormat(key.source_resource_format);
  xenos::DepthRenderTargetFormat dest_depth_format =
      xenos::DepthRenderTargetFormat(key.dest_resource_format);

  bool source_is_uint = false;
  if (source_is_color) {
    GetColorOwnershipTransferPixelFormat(source_color_format, &source_is_uint);
  }
  bool source_is_64bpp = false;
  if (source_is_color) {
    source_is_64bpp =
        xenos::IsColorRenderTargetFormat64bpp(source_color_format);
  }
  bool dest_is_depth = output != TransferOutput::kColor;
  TransferPipelineKey pipeline_key = {};
  pipeline_key.shader_key = key;
  pipeline_key.shader_key.host_depth_source_is_copy = 0;
  pipeline_key.native_stencil_output = native_stencil_output ? 1u : 0u;
  if (output == TransferOutput::kColor) {
    if (color_attachment_index >= xenos::kMaxColorRenderTargets) {
      return nullptr;
    }
    pipeline_key.color_attachment_index = color_attachment_index;
    if (color_attachment_formats) {
      pipeline_key.color_attachment_formats = *color_attachment_formats;
    } else {
      pipeline_key.color_attachment_formats.fill(MTL::PixelFormatInvalid);
      pipeline_key.color_attachment_formats[color_attachment_index] =
          dest_format;
    }
    if (pipeline_key.color_attachment_formats[color_attachment_index] !=
        dest_format) {
      return nullptr;
    }
    pipeline_key.depth_attachment_format = depth_attachment_format;
    pipeline_key.stencil_attachment_format = stencil_attachment_format;
  } else {
    pipeline_key.color_attachment_index = 0;
    if (color_attachment_formats) {
      pipeline_key.color_attachment_formats = *color_attachment_formats;
    } else {
      pipeline_key.color_attachment_formats.fill(MTL::PixelFormatInvalid);
    }
    pipeline_key.depth_attachment_format =
        depth_attachment_format != MTL::PixelFormatInvalid
            ? depth_attachment_format
            : dest_format;
    if (pipeline_key.depth_attachment_format != dest_format) {
      return nullptr;
    }
    if (dest_format == MTL::PixelFormatDepth32Float_Stencil8 ||
        dest_format == MTL::PixelFormatDepth24Unorm_Stencil8) {
      pipeline_key.stencil_attachment_format =
          stencil_attachment_format != MTL::PixelFormatInvalid
              ? stencil_attachment_format
              : dest_format;
      if (pipeline_key.stencil_attachment_format != dest_format) {
        return nullptr;
      }
    } else {
      pipeline_key.stencil_attachment_format = stencil_attachment_format;
    }
  }

  auto it = transfer_pipelines_.find(pipeline_key);
  if (it != transfer_pipelines_.end()) {
    return it->second;
  }

  bool dest_is_64bpp = false;
  bool dest_is_gamma_unorm16 = false;
  if (!dest_is_depth) {
    dest_is_64bpp = xenos::IsColorRenderTargetFormat64bpp(dest_color_format);
    dest_is_gamma_unorm16 =
        dest_color_format == xenos::ColorRenderTargetFormat::k_8_8_8_8_GAMMA &&
        gamma_render_target_as_unorm16_ && !dest_is_uint;
  }
  bool source_needs_depth =
      !source_is_color && output != TransferOutput::kStencilBit;
  bool source_needs_stencil =
      !source_is_color && (output == TransferOutput::kColor ||
                           output == TransferOutput::kStencilBit);

  uint32_t dest_component_count = 1;
  if (output == TransferOutput::kColor) {
    dest_component_count =
        xenos::GetColorRenderTargetFormatComponentCount(dest_color_format);
  }

  bool source_is_multisample =
      key.source_msaa_samples != xenos::MsaaSamples::k1X;
  bool dest_is_multisample = key.dest_msaa_samples != xenos::MsaaSamples::k1X;
  bool host_depth_is_multisample =
      key.host_depth_source_msaa_samples != xenos::MsaaSamples::k1X;
  uint32_t host_depth_texture_index = source_is_color ? 1 : 2;

  auto append_define = [](std::string& source, const char* name,
                          uint32_t value) {
    source.append("#define ");
    source.append(name);
    source.push_back(' ');
    source.append(std::to_string(value));
    source.push_back('\n');
  };

  std::string source;
  source.reserve(16384);
  append_define(source, "XE_TRANSFER_SOURCE_IS_COLOR", source_is_color ? 1 : 0);
  append_define(source, "XE_TRANSFER_SOURCE_IS_DEPTH", source_is_color ? 0 : 1);
  append_define(source, "XE_TRANSFER_SOURCE_NEEDS_DEPTH",
                source_needs_depth ? 1 : 0);
  append_define(source, "XE_TRANSFER_SOURCE_NEEDS_STENCIL",
                source_needs_stencil ? 1 : 0);
  append_define(source, "XE_TRANSFER_SOURCE_IS_UINT", source_is_uint ? 1 : 0);
  append_define(source, "XE_TRANSFER_SOURCE_IS_64BPP", source_is_64bpp ? 1 : 0);
  append_define(source, "XE_TRANSFER_SOURCE_IS_MULTISAMPLE",
                source_is_multisample ? 1 : 0);
  append_define(source, "XE_TRANSFER_DEST_IS_UINT", dest_is_uint ? 1 : 0);
  append_define(source, "XE_TRANSFER_DEST_IS_DEPTH", dest_is_depth ? 1 : 0);
  append_define(source, "XE_TRANSFER_DEST_IS_64BPP", dest_is_64bpp ? 1 : 0);
  append_define(source, "XE_TRANSFER_DEST_IS_GAMMA_UNORM16",
                dest_is_gamma_unorm16 ? 1 : 0);
  append_define(source, "XE_TRANSFER_DEST_COMPONENTS", dest_component_count);
  append_define(source, "XE_TRANSFER_DEST_IS_MULTISAMPLE",
                dest_is_multisample ? 1 : 0);
  append_define(source, "XE_TRANSFER_DEST_SAMPLE_ID_FROM_SAMPLE",
                key.dest_sample_id_from_sample ? 1 : 0);
  append_define(source, "XE_TRANSFER_COLOR_ATTACHMENT_INDEX",
                pipeline_key.color_attachment_index);
  append_define(source, "XE_TRANSFER_HAS_HOST_DEPTH", has_host_depth ? 1 : 0);
  append_define(source, "XE_TRANSFER_HOST_DEPTH_IS_MULTISAMPLE",
                host_depth_is_multisample ? 1 : 0);
  append_define(source, "XE_TRANSFER_SOURCE_FORMAT",
                key.source_resource_format);
  append_define(source, "XE_TRANSFER_DEST_FORMAT", key.dest_resource_format);
  append_define(source, "XE_TRANSFER_SOURCE_MSAA_SAMPLES",
                MsaaSamplesToCount(key.source_msaa_samples));
  append_define(source, "XE_TRANSFER_DEST_MSAA_SAMPLES",
                MsaaSamplesToCount(key.dest_msaa_samples));
  append_define(source, "XE_TRANSFER_HOST_DEPTH_MSAA_SAMPLES",
                MsaaSamplesToCount(key.host_depth_source_msaa_samples));
  append_define(source, "XE_TRANSFER_SOURCE_TEXTURE_INDEX", 0);
  append_define(source, "XE_TRANSFER_STENCIL_TEXTURE_INDEX", 1);
  append_define(source, "XE_TRANSFER_HOST_DEPTH_TEXTURE_INDEX",
                host_depth_texture_index);
  append_define(source, "XE_TRANSFER_OUTPUT_COLOR",
                output == TransferOutput::kColor ? 1 : 0);
  append_define(source, "XE_TRANSFER_OUTPUT_DEPTH",
                output == TransferOutput::kDepth ? 1 : 0);
  append_define(source, "XE_TRANSFER_OUTPUT_STENCIL_BIT",
                output == TransferOutput::kStencilBit ? 1 : 0);
  append_define(source, "XE_TRANSFER_NATIVE_STENCIL_OUTPUT",
                native_stencil_output ? 1 : 0);
  append_define(source, "XE_TRANSFER_FAST_DIVMOD",
                ::cvars::metal_transfer_fast_divmod ? 1 : 0);
  append_define(source, "XE_FMT_8_8_8_8",
                uint32_t(xenos::ColorRenderTargetFormat::k_8_8_8_8));
  append_define(source, "XE_FMT_8_8_8_8_GAMMA",
                uint32_t(xenos::ColorRenderTargetFormat::k_8_8_8_8_GAMMA));
  append_define(source, "XE_FMT_2_10_10_10",
                uint32_t(xenos::ColorRenderTargetFormat::k_2_10_10_10));
  append_define(
      source, "XE_FMT_2_10_10_10_AS_10_10_10_10",
      uint32_t(xenos::ColorRenderTargetFormat::k_2_10_10_10_AS_10_10_10_10));
  append_define(source, "XE_FMT_2_10_10_10_FLOAT",
                uint32_t(xenos::ColorRenderTargetFormat::k_2_10_10_10_FLOAT));
  append_define(
      source, "XE_FMT_2_10_10_10_FLOAT_AS_16_16_16_16",
      uint32_t(
          xenos::ColorRenderTargetFormat::k_2_10_10_10_FLOAT_AS_16_16_16_16));
  append_define(source, "XE_FMT_16_16",
                uint32_t(xenos::ColorRenderTargetFormat::k_16_16));
  append_define(source, "XE_FMT_16_16_FLOAT",
                uint32_t(xenos::ColorRenderTargetFormat::k_16_16_FLOAT));
  append_define(source, "XE_FMT_16_16_16_16",
                uint32_t(xenos::ColorRenderTargetFormat::k_16_16_16_16));
  append_define(source, "XE_FMT_16_16_16_16_FLOAT",
                uint32_t(xenos::ColorRenderTargetFormat::k_16_16_16_16_FLOAT));
  append_define(source, "XE_FMT_32_FLOAT",
                uint32_t(xenos::ColorRenderTargetFormat::k_32_FLOAT));
  append_define(source, "XE_FMT_32_32_FLOAT",
                uint32_t(xenos::ColorRenderTargetFormat::k_32_32_FLOAT));
  append_define(source, "XE_FMT_D24S8",
                uint32_t(xenos::DepthRenderTargetFormat::kD24S8));
  append_define(source, "XE_FMT_D24FS8",
                uint32_t(xenos::DepthRenderTargetFormat::kD24FS8));
  append_define(source, "XE_GAMMA_RT_AS_UNORM16",
                gamma_render_target_as_unorm16_ ? 1 : 0);

  static const char kTransferShaderSource[] = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct TransferAddressConstants {
  uint dest_pitch;
  uint source_pitch;
  int source_to_dest;
};

struct TransferShaderConstants {
  TransferAddressConstants address;
  TransferAddressConstants host_depth_address;
  uint source_format;
  uint dest_format;
  uint source_is_depth;
  uint dest_is_depth;
  uint source_is_uint;
  uint dest_is_uint;
  uint source_is_64bpp;
  uint dest_is_64bpp;
  uint source_msaa_samples;
  uint dest_msaa_samples;
  uint host_depth_source_msaa_samples;
  uint host_depth_source_is_copy;
  uint depth_round;
  uint msaa_2x_supported;
  uint tile_width_samples;
  uint tile_height_samples;
  uint dest_tile_width_pixels;
  uint dest_tile_height_pixels;
  float dest_tile_width_pixels_inv;
  float dest_tile_height_pixels_inv;
  float source_pitch_tiles_inv;
  float host_depth_source_pitch_tiles_inv;
  float dest_pixel_to_ndc_x;
  float dest_pixel_to_ndc_y;
  uint dest_sample_id;
  uint stencil_mask;
  uint stencil_clear;
};

struct TransferRectInstance {
  float2 origin;
  float2 size;
};

constant uint kEdramTileCount = 2048u;

inline uint XeBitFieldMask(uint count) {
  if (count >= 32u) {
    return 0xFFFFFFFFu;
  }
  return (1u << count) - 1u;
}

inline uint XeBitFieldInsert(uint base, uint insert, uint offset, uint count) {
  uint mask = XeBitFieldMask(count) << offset;
  return (base & ~mask) | ((insert << offset) & mask);
}

inline uint XeBitFieldExtract(uint value, uint offset, uint count) {
  return (value >> offset) & XeBitFieldMask(count);
}

inline uint XeRoundToNearestEven(float value) {
  float floor_value = floor(value);
  float frac = value - floor_value;
  uint result = uint(floor_value);
  if (frac > 0.5f || (frac == 0.5f && (result & 1u))) {
    result += 1u;
  }
  return result;
}

inline uint XePackUnorm(float value, float scale) {
  return uint(clamp(value, 0.0f, 1.0f) * scale + 0.5f);
}

inline float XeSaturateNoNaN(float value) {
  float clamped = clamp(value, 0.0f, 1.0f);
  return (clamped == clamped) ? clamped : 0.0f;
}

inline float XeTruncToFloat(float value) {
  return trunc(value);
}

inline float XePWLGammaToLinear(float value) {
  float clamped = XeSaturateNoNaN(value);
  float scale;
  float offset;
  if (clamped >= (96.0f / 255.0f)) {
    if (clamped >= (192.0f / 255.0f)) {
      scale = 8.0f / 1024.0f;
      offset = -1024.0f;
    } else {
      scale = 4.0f / 1024.0f;
      offset = -256.0f;
    }
  } else {
    if (clamped >= (64.0f / 255.0f)) {
      scale = 2.0f / 1024.0f;
      offset = -64.0f;
    } else {
      scale = 1.0f / 1024.0f;
      offset = 0.0f;
    }
  }
  float linear = clamped * (255.0f * 1024.0f) * scale + offset;
  linear += XeTruncToFloat(linear * scale);
  return linear * (1.0f / 1023.0f);
}

inline void XeFastDivMod(uint x, uint w, float inv_w, thread uint& q,
                         thread uint& r) {
  q = uint(float(x) * inv_w);
  r = x - q * w;
  if (r >= w) {
    r -= w;
    q += 1u;
  } else if (r > x) {
    r += w;
    q -= 1u;
  }
}

inline float XeLinearToPWLGamma(float value) {
  float clamped = XeSaturateNoNaN(value);
  float scale;
  float offset;
  if (clamped >= (128.0f / 1023.0f)) {
    if (clamped >= (512.0f / 1023.0f)) {
      scale = 1023.0f / 8.0f;
      offset = 128.0f / 255.0f;
    } else {
      scale = 1023.0f / 4.0f;
      offset = 64.0f / 255.0f;
    }
  } else {
    if (clamped >= (64.0f / 1023.0f)) {
      scale = 1023.0f / 2.0f;
      offset = 32.0f / 255.0f;
    } else {
      scale = 1023.0f;
      offset = 0.0f;
    }
  }
  return XeTruncToFloat(clamped * scale) * (1.0f / 255.0f) + offset;
}

inline float3 XePWLGammaToLinear3(float3 v) {
  return float3(XePWLGammaToLinear(v.r), XePWLGammaToLinear(v.g),
                XePWLGammaToLinear(v.b));
}

inline float3 XeLinearToPWLGamma3(float3 v) {
  return float3(XeLinearToPWLGamma(v.r), XeLinearToPWLGamma(v.g),
                XeLinearToPWLGamma(v.b));
}

inline float XePWLGammaByteToLinearMidpoint(uint byte_value) {
  byte_value &= 0xFFu;
  float recip = 1.0f / 1023.0f;
  float offset = 0.5f / 1023.0f;
  if (byte_value >= 64u) {
    recip = 1.0f / 511.5f;
    offset = -31.5f / 511.5f;
  }
  if (byte_value >= 96u) {
    recip = 1.0f / 255.75f;
    offset = -63.5f / 255.75f;
  }
  if (byte_value >= 192u) {
    recip = 1.0f / 127.875f;
    offset = -127.5f / 127.875f;
  }
  return float(byte_value) * recip + offset;
}

inline float4 XePWLGammaPackedRGBA8ToLinearMidpoint(uint packed) {
  return float4(
      XePWLGammaByteToLinearMidpoint(packed),
      XePWLGammaByteToLinearMidpoint(packed >> 8u),
      XePWLGammaByteToLinearMidpoint(packed >> 16u),
      float((packed >> 24u) & 0xFFu) * (1.0f / 255.0f));
}

uint XePreClampedFloat32To7e3(float value) {
  uint f32 = as_type<uint>(value);
  uint biased_f32;
  if (f32 < 0x3E800000u) {
    uint f32_exp = f32 >> 23u;
    uint shift = 125u - f32_exp;
    shift = min(shift, 24u);
    uint mantissa = (f32 & 0x7FFFFFu) | 0x800000u;
    biased_f32 = mantissa >> shift;
  } else {
    biased_f32 = f32 + 0xC2000000u;
  }
  uint round_bit = (biased_f32 >> 16u) & 1u;
  uint f10 = biased_f32 + 0x7FFFu + round_bit;
  return (f10 >> 16u) & 0x3FFu;
}

uint XeUnclampedFloat32To7e3(float value) {
  float clamped = min(max(value, 0.0f), 31.875f);
  return XePreClampedFloat32To7e3(clamped);
}

float XeFloat7e3To32(uint f10) {
  f10 &= 0x3FFu;
  if (f10 == 0u) {
    return 0.0f;
  }
  uint mantissa = f10 & 0x7Fu;
  uint exponent = f10 >> 7u;
  if (exponent == 0u) {
    uint mantissa_lzcnt = clz(mantissa) - 24u;
    exponent = uint(int(1) - int(mantissa_lzcnt));
    mantissa = (mantissa << mantissa_lzcnt) & 0x7Fu;
  }
  uint f32 = ((exponent + 124u) << 23u) | (mantissa << 16u);
  return as_type<float>(f32);
}

uint XeFloat32To20e4(float value, bool round_to_nearest_even) {
  uint f32 = as_type<uint>(value);
  f32 = min((f32 <= 0x7FFFFFFFu) ? f32 : 0u, 0x3FFFFFF8u);
  uint denormalized =
      ((f32 & 0x7FFFFFu) | 0x800000u) >> min(113u - (f32 >> 23u), 24u);
  uint f24 = (f32 < 0x38800000u) ? denormalized : (f32 + 0xC8000000u);
  if (round_to_nearest_even) {
    f24 += 3u + ((f24 >> 3u) & 1u);
  }
  return (f24 >> 3u) & 0xFFFFFFu;
}

float XeFloat20e4To32(uint f24, bool remap_to_0_to_0_5) {
  if (f24 == 0u) {
    return 0.0f;
  }
  uint mantissa = f24 & 0xFFFFFu;
  uint exponent = f24 >> 20u;
  if (exponent == 0u) {
    uint msb = 31u - clz(mantissa);
    uint mantissa_lzcnt = 20u - msb;
    exponent = 1u - mantissa_lzcnt;
    mantissa = (mantissa << mantissa_lzcnt) & 0xFFFFFu;
  }
  uint bias = remap_to_0_to_0_5 ? 111u : 112u;
  uint f32 = ((exponent + bias) << 23u) | (mantissa << 3u);
  return as_type<float>(f32);
}

float XeUnorm24To32(uint n24) {
  return float(n24 + (n24 >> 23u)) * (1.0f / 16777216.0f);
}

uint XePackColorRGBA8(float4 color) {
  uint r = XePackUnorm(color.r, 255.0f);
  uint g = XePackUnorm(color.g, 255.0f);
  uint b = XePackUnorm(color.b, 255.0f);
  uint a = XePackUnorm(color.a, 255.0f);
  return r | (g << 8u) | (b << 16u) | (a << 24u);
}

uint XePackColorRGB10A2(float4 color) {
  uint r = XePackUnorm(color.r, 1023.0f);
  uint g = XePackUnorm(color.g, 1023.0f);
  uint b = XePackUnorm(color.b, 1023.0f);
  uint a = XePackUnorm(color.a, 3.0f);
  return r | (g << 10u) | (b << 20u) | (a << 30u);
}

uint XePackColorRGB10A2Float(float4 color) {
  uint r = XeUnclampedFloat32To7e3(color.r);
  uint g = XeUnclampedFloat32To7e3(color.g);
  uint b = XeUnclampedFloat32To7e3(color.b);
  uint a = XePackUnorm(color.a, 3.0f);
  return (r & 0x3FFu) | ((g & 0x3FFu) << 10u) |
         ((b & 0x3FFu) << 20u) | ((a & 0x3u) << 30u);
}

struct VSOut {
  float4 position [[position]];
};

vertex VSOut transfer_vs(uint vid [[vertex_id]]) {
  float2 pt = float2((vid << 1) & 2, vid & 2);
  VSOut out;
  out.position = float4(pt * 2.0f - 1.0f, 0.0f, 1.0f);
  return out;
}

vertex VSOut transfer_rect_vs(uint vid [[vertex_id]],
                              uint iid [[instance_id]],
                              constant TransferShaderConstants& constants
                                  [[buffer(0)]],
                              constant TransferRectInstance* instances
                                  [[buffer(1)]]) {
  float2 quad = float2(float(vid & 1), float(vid >> 1));
  TransferRectInstance inst = instances[iid];
  float2 pos_pixel = inst.origin + quad * inst.size;
  float2 ndc;
  ndc.x = pos_pixel.x * constants.dest_pixel_to_ndc_x - 1.0f;
  ndc.y = 1.0f - pos_pixel.y * constants.dest_pixel_to_ndc_y;
  VSOut out;
  out.position = float4(ndc, 0.0f, 1.0f);
  return out;
}

#if XE_TRANSFER_SOURCE_IS_COLOR
  #if XE_TRANSFER_SOURCE_IS_UINT
    #if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      #define XE_TRANSFER_SOURCE_PARAMS \
          , texture2d_ms<uint, access::read> xe_transfer_source \
              [[texture(XE_TRANSFER_SOURCE_TEXTURE_INDEX)]]
    #else
      #define XE_TRANSFER_SOURCE_PARAMS \
          , texture2d<uint, access::read> xe_transfer_source \
              [[texture(XE_TRANSFER_SOURCE_TEXTURE_INDEX)]]
    #endif
  #else
    #if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      #define XE_TRANSFER_SOURCE_PARAMS \
          , texture2d_ms<float, access::read> xe_transfer_source \
              [[texture(XE_TRANSFER_SOURCE_TEXTURE_INDEX)]]
    #else
      #define XE_TRANSFER_SOURCE_PARAMS \
          , texture2d<float, access::read> xe_transfer_source \
              [[texture(XE_TRANSFER_SOURCE_TEXTURE_INDEX)]]
    #endif
  #endif
#else
  #if XE_TRANSFER_SOURCE_NEEDS_DEPTH && XE_TRANSFER_SOURCE_NEEDS_STENCIL
    #if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      #define XE_TRANSFER_SOURCE_PARAMS \
          , texture2d_ms<float, access::read> xe_transfer_source_depth \
              [[texture(XE_TRANSFER_SOURCE_TEXTURE_INDEX)]], \
            texture2d_ms<uint, access::read> xe_transfer_source_stencil \
              [[texture(XE_TRANSFER_STENCIL_TEXTURE_INDEX)]]
    #else
      #define XE_TRANSFER_SOURCE_PARAMS \
          , texture2d<float, access::read> xe_transfer_source_depth \
              [[texture(XE_TRANSFER_SOURCE_TEXTURE_INDEX)]], \
            texture2d<uint, access::read> xe_transfer_source_stencil \
              [[texture(XE_TRANSFER_STENCIL_TEXTURE_INDEX)]]
    #endif
  #elif XE_TRANSFER_SOURCE_NEEDS_DEPTH
    #if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      #define XE_TRANSFER_SOURCE_PARAMS \
          , texture2d_ms<float, access::read> xe_transfer_source_depth \
              [[texture(XE_TRANSFER_SOURCE_TEXTURE_INDEX)]]
    #else
      #define XE_TRANSFER_SOURCE_PARAMS \
          , texture2d<float, access::read> xe_transfer_source_depth \
              [[texture(XE_TRANSFER_SOURCE_TEXTURE_INDEX)]]
    #endif
  #elif XE_TRANSFER_SOURCE_NEEDS_STENCIL
    #if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      #define XE_TRANSFER_SOURCE_PARAMS \
          , texture2d_ms<uint, access::read> xe_transfer_source_stencil \
              [[texture(XE_TRANSFER_STENCIL_TEXTURE_INDEX)]]
    #else
      #define XE_TRANSFER_SOURCE_PARAMS \
          , texture2d<uint, access::read> xe_transfer_source_stencil \
              [[texture(XE_TRANSFER_STENCIL_TEXTURE_INDEX)]]
    #endif
  #else
    #define XE_TRANSFER_SOURCE_PARAMS
  #endif
#endif

#if XE_TRANSFER_HAS_HOST_DEPTH
  #define XE_TRANSFER_HOST_DEPTH_BUFFER_PARAM \
      , device const uint* xe_transfer_host_depth_buffer [[buffer(1)]]
#else
  #define XE_TRANSFER_HOST_DEPTH_BUFFER_PARAM
#endif

#if XE_TRANSFER_HAS_HOST_DEPTH
  #if XE_TRANSFER_HOST_DEPTH_IS_MULTISAMPLE
    #define XE_TRANSFER_HOST_DEPTH_TEXTURE_PARAM \
        , texture2d_ms<float, access::read> xe_transfer_host_depth \
            [[texture(XE_TRANSFER_HOST_DEPTH_TEXTURE_INDEX)]]
  #else
    #define XE_TRANSFER_HOST_DEPTH_TEXTURE_PARAM \
        , texture2d<float, access::read> xe_transfer_host_depth \
            [[texture(XE_TRANSFER_HOST_DEPTH_TEXTURE_INDEX)]]
  #endif
#else
  #define XE_TRANSFER_HOST_DEPTH_TEXTURE_PARAM
#endif

#if XE_TRANSFER_DEST_IS_MULTISAMPLE && XE_TRANSFER_DEST_SAMPLE_ID_FROM_SAMPLE
  #define XE_TRANSFER_SAMPLE_ID_PARAM , uint xe_sample_id [[sample_id]]
#else
  #define XE_TRANSFER_SAMPLE_ID_PARAM
#endif

#if XE_TRANSFER_OUTPUT_COLOR
  #if XE_TRANSFER_DEST_IS_UINT
    #if XE_TRANSFER_DEST_COMPONENTS == 1
      typedef uint XeTransferColorOutType;
    #elif XE_TRANSFER_DEST_COMPONENTS == 2
      typedef uint2 XeTransferColorOutType;
    #else
      typedef uint4 XeTransferColorOutType;
    #endif
  #else
    #if XE_TRANSFER_DEST_COMPONENTS == 1
      typedef float XeTransferColorOutType;
    #elif XE_TRANSFER_DEST_COMPONENTS == 2
      typedef float2 XeTransferColorOutType;
    #else
      typedef float4 XeTransferColorOutType;
    #endif
  #endif

struct TransferColorOut {
  XeTransferColorOutType color [[color(XE_TRANSFER_COLOR_ATTACHMENT_INDEX)]];
#if XE_TRANSFER_DEST_IS_MULTISAMPLE
  uint sample_mask [[sample_mask]];
#endif
};

fragment TransferColorOut transfer_ps(
    VSOut in [[stage_in]],
    constant TransferShaderConstants& constants [[buffer(0)]]
    XE_TRANSFER_HOST_DEPTH_BUFFER_PARAM
    XE_TRANSFER_SOURCE_PARAMS
    XE_TRANSFER_HOST_DEPTH_TEXTURE_PARAM
    XE_TRANSFER_SAMPLE_ID_PARAM) {
  uint2 dest_pixel = uint2(in.position.xy);
  uint dest_sample_id = 0u;
#if XE_TRANSFER_DEST_IS_MULTISAMPLE
  #if XE_TRANSFER_DEST_SAMPLE_ID_FROM_SAMPLE
    dest_sample_id = xe_sample_id;
  #else
    dest_sample_id = constants.dest_sample_id;
  #endif
#endif

  uint tile_width_samples = constants.tile_width_samples;
  uint tile_height_samples = constants.tile_height_samples;
  uint dest_tile_width_pixels = constants.dest_tile_width_pixels;
  uint dest_tile_height_pixels = constants.dest_tile_height_pixels;

  uint dest_tile_pixel_x = 0u;
  uint dest_tile_pixel_y = 0u;
  uint dest_tile_index = 0u;
  uint dest_tile_index_x = 0u;
  uint dest_tile_index_y = 0u;
#if XE_TRANSFER_FAST_DIVMOD
  XeFastDivMod(dest_pixel.x, dest_tile_width_pixels,
               constants.dest_tile_width_pixels_inv, dest_tile_index_x,
               dest_tile_pixel_x);
  XeFastDivMod(dest_pixel.y, dest_tile_height_pixels,
               constants.dest_tile_height_pixels_inv, dest_tile_index_y,
               dest_tile_pixel_y);
#else
  dest_tile_index_x = dest_pixel.x / dest_tile_width_pixels;
  dest_tile_pixel_x = dest_pixel.x % dest_tile_width_pixels;
  dest_tile_index_y = dest_pixel.y / dest_tile_height_pixels;
  dest_tile_pixel_y = dest_pixel.y % dest_tile_height_pixels;
#endif

  dest_tile_index =
      dest_tile_index_x +
      dest_tile_index_y * constants.address.dest_pitch;

  uint source_sample_id = dest_sample_id;
  uint source_tile_pixel_x = dest_tile_pixel_x;
  uint source_tile_pixel_y = dest_tile_pixel_y;
  uint source_color_half = 0u;
  bool source_color_half_valid = false;

  bool source_is_64bpp = XE_TRANSFER_SOURCE_IS_64BPP != 0u;
  bool dest_is_64bpp = XE_TRANSFER_DEST_IS_64BPP != 0u;
  uint source_msaa = XE_TRANSFER_SOURCE_MSAA_SAMPLES;
  uint dest_msaa = XE_TRANSFER_DEST_MSAA_SAMPLES;
  bool msaa_2x_supported = constants.msaa_2x_supported != 0u;

  if (!source_is_64bpp && dest_is_64bpp) {
    if (source_msaa >= 4u) {
      if (dest_msaa >= 4u) {
        source_sample_id = dest_sample_id & 2u;
        source_tile_pixel_x =
            XeBitFieldInsert(dest_sample_id, dest_tile_pixel_x, 1u, 31u);
      } else if (dest_msaa == 2u) {
        if (msaa_2x_supported) {
          source_sample_id = (dest_sample_id ^ 1u) << 1u;
        } else {
          source_sample_id = dest_sample_id & 2u;
        }
      } else {
        source_sample_id =
            XeBitFieldInsert(0u, dest_tile_pixel_y, 1u, 1u);
        source_tile_pixel_y = dest_tile_pixel_y >> 1u;
      }
    } else {
      if (dest_msaa >= 4u) {
        source_tile_pixel_x = XeBitFieldInsert(
            dest_tile_pixel_x << 2u, dest_sample_id, 1u, 1u);
      } else {
        source_tile_pixel_x = dest_tile_pixel_x << 1u;
      }
    }
  } else if (source_is_64bpp && !dest_is_64bpp) {
    if (dest_msaa >= 4u) {
      if (source_msaa >= 4u) {
        source_sample_id =
            XeBitFieldInsert(dest_sample_id, dest_tile_pixel_x, 0u, 1u);
        source_tile_pixel_x = dest_tile_pixel_x >> 1u;
      }
      source_color_half = dest_sample_id & 1u;
      source_color_half_valid = true;
    } else {
      if (source_msaa >= 4u) {
        source_sample_id = XeBitFieldExtract(dest_tile_pixel_x, 1u, 1u);
        if (dest_msaa == 2u) {
          if (msaa_2x_supported) {
            source_sample_id = XeBitFieldInsert(
                source_sample_id, dest_sample_id ^ 1u, 1u, 1u);
          } else {
            source_sample_id = XeBitFieldInsert(
                dest_sample_id, source_sample_id, 0u, 1u);
          }
        } else {
          source_sample_id = XeBitFieldInsert(
              source_sample_id, dest_tile_pixel_y, 1u, 1u);
          source_tile_pixel_y = dest_tile_pixel_y >> 1u;
        }
        source_tile_pixel_x = dest_tile_pixel_x >> 2u;
      } else {
        source_tile_pixel_x = dest_tile_pixel_x >> 1u;
      }
      source_color_half = dest_tile_pixel_x & 1u;
      source_color_half_valid = true;
    }
  } else {
    if (source_msaa != dest_msaa) {
      if (source_msaa >= 4u) {
        if (dest_msaa == 2u) {
          if (msaa_2x_supported) {
            source_sample_id = XeBitFieldInsert(
                dest_tile_pixel_x, dest_sample_id ^ 1u, 1u, 31u);
          } else {
            source_sample_id = XeBitFieldInsert(
                dest_sample_id, dest_tile_pixel_x, 0u, 1u);
          }
          source_tile_pixel_x = dest_tile_pixel_x >> 1u;
        } else {
          source_sample_id = XeBitFieldInsert(
              dest_tile_pixel_x & 1u, dest_tile_pixel_y, 1u, 1u);
          source_tile_pixel_x = dest_tile_pixel_x >> 1u;
          source_tile_pixel_y = dest_tile_pixel_y >> 1u;
        }
      } else if (dest_msaa >= 4u) {
        source_tile_pixel_x = XeBitFieldInsert(
            dest_sample_id, dest_tile_pixel_x, 1u, 31u);
      }
    }
  }

  if (source_msaa < 4u && source_msaa != dest_msaa) {
    if (dest_msaa >= 4u) {
      if (source_msaa == 2u) {
        source_sample_id = dest_sample_id >> 1u;
        if (msaa_2x_supported) {
          source_sample_id ^= 1u;
        } else {
          source_sample_id = XeBitFieldInsert(
              source_sample_id, source_sample_id, 1u, 1u);
        }
      } else {
        source_tile_pixel_y = XeBitFieldInsert(
            dest_sample_id >> 1u, dest_tile_pixel_y, 1u, 31u);
      }
    } else {
      if (source_msaa == 2u) {
        source_sample_id = dest_tile_pixel_y & 1u;
        if (msaa_2x_supported) {
          source_sample_id ^= 1u;
        } else {
          source_sample_id = XeBitFieldInsert(
              source_sample_id, source_sample_id, 1u, 1u);
        }
        source_tile_pixel_y = dest_tile_pixel_y >> 1u;
      } else {
        if (msaa_2x_supported) {
          source_tile_pixel_y = XeBitFieldInsert(
              dest_sample_id ^ 1u, dest_tile_pixel_y, 1u, 31u);
        } else {
          source_tile_pixel_y = XeBitFieldInsert(
              dest_sample_id >> 1u, dest_tile_pixel_y, 1u, 31u);
        }
      }
    }
  }

  uint source_pixel_width_dwords_log2 =
      (source_msaa >= 4u ? 1u : 0u) + (source_is_64bpp ? 1u : 0u);

  if ((XE_TRANSFER_SOURCE_IS_DEPTH != 0u) != (XE_TRANSFER_DEST_IS_DEPTH != 0u)) {
    uint source_32bpp_tile_half_pixels =
        tile_width_samples >> (1u + source_pixel_width_dwords_log2);
    if (source_tile_pixel_x < source_32bpp_tile_half_pixels) {
      source_tile_pixel_x += source_32bpp_tile_half_pixels;
    } else {
      source_tile_pixel_x -= source_32bpp_tile_half_pixels;
    }
  }

  uint source_pixel_x = 0u;
  uint source_pixel_y = 0u;
  uint source_tile_index =
      uint(int(dest_tile_index) + constants.address.source_to_dest) &
      (kEdramTileCount - 1u);
  uint source_pitch_tiles = constants.address.source_pitch;
  uint source_tile_index_y = 0u;
  uint source_tile_index_x = 0u;
  XeFastDivMod(source_tile_index, source_pitch_tiles,
               constants.source_pitch_tiles_inv, source_tile_index_y,
               source_tile_index_x);
  source_pixel_x =
      source_tile_index_x *
          (tile_width_samples >> source_pixel_width_dwords_log2) +
      source_tile_pixel_x;
  source_pixel_y =
      source_tile_index_y *
          (tile_height_samples >> (source_msaa >= 2u ? 1u : 0u)) +
      source_tile_pixel_y;

  bool load_two = !source_is_64bpp && dest_is_64bpp;
  uint source_pixel_x1 = source_pixel_x;
  uint source_sample_id1 = source_sample_id;
  if (load_two) {
    if (source_msaa >= 4u) {
      source_sample_id1 = source_sample_id | 1u;
    } else {
      source_pixel_x1 = source_pixel_x | 1u;
    }
  }

#if XE_TRANSFER_SOURCE_IS_COLOR
  #if XE_TRANSFER_SOURCE_IS_UINT
  uint4 source_color0 =
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      xe_transfer_source.read(uint2(source_pixel_x, source_pixel_y),
                              source_sample_id);
#else
      xe_transfer_source.read(uint2(source_pixel_x, source_pixel_y));
#endif
  uint4 source_color1 = source_color0;
  if (load_two) {
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
    source_color1 = xe_transfer_source.read(
        uint2(source_pixel_x1, source_pixel_y), source_sample_id1);
#else
    source_color1 = xe_transfer_source.read(
        uint2(source_pixel_x1, source_pixel_y));
#endif
  }
  #else
  float4 source_color0 =
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      xe_transfer_source.read(uint2(source_pixel_x, source_pixel_y),
                              source_sample_id);
#else
      xe_transfer_source.read(uint2(source_pixel_x, source_pixel_y));
#endif
  float4 source_color1 = source_color0;
  if (load_two) {
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
    source_color1 = xe_transfer_source.read(
        uint2(source_pixel_x1, source_pixel_y), source_sample_id1);
#else
    source_color1 = xe_transfer_source.read(
        uint2(source_pixel_x1, source_pixel_y));
#endif
  }
  #endif
#else
#if XE_TRANSFER_SOURCE_NEEDS_DEPTH
  float source_depth0 =
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      xe_transfer_source_depth.read(uint2(source_pixel_x, source_pixel_y),
                                    source_sample_id).r;
#else
      xe_transfer_source_depth.read(uint2(source_pixel_x, source_pixel_y)).r;
#endif
  float source_depth1 = source_depth0;
  if (load_two) {
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
    source_depth1 = xe_transfer_source_depth.read(
        uint2(source_pixel_x1, source_pixel_y), source_sample_id1).r;
#else
    source_depth1 = xe_transfer_source_depth.read(
        uint2(source_pixel_x1, source_pixel_y)).r;
#endif
  }
#endif
#if XE_TRANSFER_SOURCE_NEEDS_STENCIL
  uint source_stencil0 =
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      xe_transfer_source_stencil.read(uint2(source_pixel_x, source_pixel_y),
                                      source_sample_id).r;
#else
      xe_transfer_source_stencil.read(uint2(source_pixel_x, source_pixel_y)).r;
#endif
  uint source_stencil1 = source_stencil0;
  if (load_two) {
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
    source_stencil1 = xe_transfer_source_stencil.read(
        uint2(source_pixel_x1, source_pixel_y), source_sample_id1).r;
#else
    source_stencil1 = xe_transfer_source_stencil.read(
        uint2(source_pixel_x1, source_pixel_y)).r;
#endif
  }
#else
  uint source_stencil0 = 0u;
  uint source_stencil1 = 0u;
#endif
#endif

#if XE_TRANSFER_SOURCE_IS_COLOR
  if (source_is_64bpp && !dest_is_64bpp && source_color_half_valid) {
    uint source_component_count = 0u;
    switch (XE_TRANSFER_SOURCE_FORMAT) {
      case XE_FMT_32_FLOAT:
        source_component_count = 1u;
        break;
      case XE_FMT_16_16:
      case XE_FMT_16_16_FLOAT:
      case XE_FMT_32_32_FLOAT:
        source_component_count = 2u;
        break;
      default:
        source_component_count = 4u;
        break;
    }
    if (source_component_count == 2u) {
  #if XE_TRANSFER_SOURCE_IS_UINT
      source_color0[0] = source_color_half != 0u ? source_color0[1]
                                                 : source_color0[0];
  #else
      source_color0[0] = source_color_half != 0u ? source_color0[1]
                                                 : source_color0[0];
  #endif
    } else if (source_color_half != 0u) {
  #if XE_TRANSFER_SOURCE_IS_UINT
      source_color0[0] = source_color0[2];
      source_color0[1] = source_color0[3];
  #else
      source_color0[0] = source_color0[2];
      source_color0[1] = source_color0[3];
  #endif
    }
  }
#endif

#if XE_TRANSFER_DEST_IS_UINT
  uint4 out_color = uint4(0u);
#else
  float4 out_color = float4(0.0f);
#endif

  if (dest_is_64bpp) {
    uint2 packed64 = uint2(0u);
#if XE_TRANSFER_SOURCE_IS_COLOR
  #if XE_TRANSFER_SOURCE_IS_UINT
    switch (XE_TRANSFER_SOURCE_FORMAT) {
      case XE_FMT_16_16:
      case XE_FMT_16_16_FLOAT:
        packed64.x = source_color0[0] | (source_color0[1] << 16u);
        packed64.y = source_color1[0] | (source_color1[1] << 16u);
        break;
      case XE_FMT_16_16_16_16:
      case XE_FMT_16_16_16_16_FLOAT:
        packed64.x = source_color0[0] | (source_color0[1] << 16u);
        packed64.y = source_color0[2] | (source_color0[3] << 16u);
        break;
      case XE_FMT_32_FLOAT:
        packed64.x = source_color0[0];
        packed64.y = source_color1[0];
        break;
      case XE_FMT_32_32_FLOAT:
        packed64.x = source_color0[0];
        packed64.y = source_color0[1];
        break;
      default:
        packed64.x = source_color0[0];
        packed64.y = source_color1[0];
        break;
    }
  #else
    switch (XE_TRANSFER_SOURCE_FORMAT) {
      case XE_FMT_8_8_8_8_GAMMA: {
#if XE_GAMMA_RT_AS_UNORM16
        float4 gamma_color0 = source_color0;
        float4 gamma_color1 = source_color1;
        gamma_color0.rgb = XeLinearToPWLGamma3(gamma_color0.rgb);
        gamma_color1.rgb = XeLinearToPWLGamma3(gamma_color1.rgb);
        packed64.x = XePackColorRGBA8(gamma_color0);
        packed64.y = XePackColorRGBA8(gamma_color1);
#else
        packed64.x = XePackColorRGBA8(source_color0);
        packed64.y = XePackColorRGBA8(source_color1);
#endif
      } break;
      case XE_FMT_8_8_8_8:
        packed64.x = XePackColorRGBA8(source_color0);
        packed64.y = XePackColorRGBA8(source_color1);
        break;
      case XE_FMT_2_10_10_10:
      case XE_FMT_2_10_10_10_AS_10_10_10_10:
        packed64.x = XePackColorRGB10A2(source_color0);
        packed64.y = XePackColorRGB10A2(source_color1);
        break;
      case XE_FMT_2_10_10_10_FLOAT:
      case XE_FMT_2_10_10_10_FLOAT_AS_16_16_16_16:
        packed64.x = XePackColorRGB10A2Float(source_color0);
        packed64.y = XePackColorRGB10A2Float(source_color1);
        break;
      case XE_FMT_32_FLOAT:
        packed64.x = as_type<uint>(source_color0[0]);
        packed64.y = as_type<uint>(source_color1[0]);
        break;
      case XE_FMT_32_32_FLOAT:
        packed64.x = as_type<uint>(source_color0[0]);
        packed64.y = as_type<uint>(source_color0[1]);
        break;
      default:
        packed64.x = as_type<uint>(source_color0[0]);
        packed64.y = as_type<uint>(source_color1[0]);
        break;
    }
  #endif
#else
    uint depth24_0 = 0u;
    uint depth24_1 = 0u;
    if (XE_TRANSFER_SOURCE_FORMAT == XE_FMT_D24FS8) {
      bool round_depth = constants.depth_round != 0u;
      depth24_0 = XeFloat32To20e4(source_depth0 * 2.0f, round_depth);
      depth24_1 = XeFloat32To20e4(source_depth1 * 2.0f, round_depth);
    } else {
      depth24_0 = XeRoundToNearestEven(
          clamp(source_depth0, 0.0f, 1.0f) * 16777215.0f);
      depth24_1 = XeRoundToNearestEven(
          clamp(source_depth1, 0.0f, 1.0f) * 16777215.0f);
    }
    packed64.x = (depth24_0 << 8u) | (source_stencil0 & 0xFFu);
    packed64.y = (depth24_1 << 8u) | (source_stencil1 & 0xFFu);
#endif

    if (XE_TRANSFER_DEST_FORMAT == XE_FMT_32_32_FLOAT) {
#if XE_TRANSFER_DEST_IS_UINT
      out_color = uint4(packed64.x, packed64.y, 0u, 0u);
#else
      out_color = float4(as_type<float>(packed64.x),
                         as_type<float>(packed64.y), 0.0f, 0.0f);
#endif
    } else {
      uint4 components = uint4(packed64.x & 0xFFFFu, packed64.x >> 16u,
                               packed64.y & 0xFFFFu, packed64.y >> 16u);
#if XE_TRANSFER_DEST_IS_UINT
      out_color = components;
#else
      out_color = float4(components);
#endif
    }
  } else {
    bool wrote_direct = false;
    uint packed32 = 0u;
#if XE_TRANSFER_SOURCE_IS_COLOR
  #if XE_TRANSFER_SOURCE_IS_UINT
    switch (XE_TRANSFER_SOURCE_FORMAT) {
      case XE_FMT_16_16:
      case XE_FMT_16_16_FLOAT:
        if (XE_TRANSFER_DEST_FORMAT == XE_FMT_16_16 ||
            XE_TRANSFER_DEST_FORMAT == XE_FMT_16_16_FLOAT) {
#if XE_TRANSFER_DEST_IS_UINT
          out_color = uint4(source_color0[0], source_color0[1], 0u, 0u);
#else
          out_color = float4(float(source_color0[0]),
                             float(source_color0[1]), 0.0f, 0.0f);
#endif
          wrote_direct = true;
        } else {
          packed32 = source_color0[0] | (source_color0[1] << 16u);
        }
        break;
      case XE_FMT_16_16_16_16:
      case XE_FMT_16_16_16_16_FLOAT:
        if (XE_TRANSFER_DEST_FORMAT == XE_FMT_16_16 ||
            XE_TRANSFER_DEST_FORMAT == XE_FMT_16_16_FLOAT) {
#if XE_TRANSFER_DEST_IS_UINT
          out_color = uint4(source_color0[0], source_color0[1], 0u, 0u);
#else
          out_color = float4(float(source_color0[0]),
                             float(source_color0[1]), 0.0f, 0.0f);
#endif
          wrote_direct = true;
        } else {
          packed32 = source_color0[0] | (source_color0[1] << 16u);
        }
        break;
      case XE_FMT_32_FLOAT:
      case XE_FMT_32_32_FLOAT:
        packed32 = source_color0[0];
        break;
      default:
        packed32 = source_color0[0];
        break;
    }
  #else
    switch (XE_TRANSFER_SOURCE_FORMAT) {
      case XE_FMT_8_8_8_8:
      case XE_FMT_8_8_8_8_GAMMA: {
        float4 color = source_color0;
#if XE_GAMMA_RT_AS_UNORM16
        if ((XE_TRANSFER_SOURCE_FORMAT == XE_FMT_8_8_8_8_GAMMA ||
             XE_TRANSFER_DEST_FORMAT == XE_FMT_8_8_8_8_GAMMA) &&
            (XE_TRANSFER_DEST_FORMAT == XE_FMT_8_8_8_8 ||
             XE_TRANSFER_DEST_FORMAT == XE_FMT_8_8_8_8_GAMMA) &&
            XE_TRANSFER_DEST_FORMAT != XE_TRANSFER_SOURCE_FORMAT) {
          if (XE_TRANSFER_DEST_FORMAT == XE_FMT_8_8_8_8) {
            color.rgb = XeLinearToPWLGamma3(color.rgb);
          } else {
            color.rgb = XePWLGammaToLinear3(color.rgb);
          }
        }
#endif
#if !XE_TRANSFER_DEST_IS_UINT
        if (XE_TRANSFER_DEST_FORMAT == XE_FMT_8_8_8_8 ||
            XE_TRANSFER_DEST_FORMAT == XE_FMT_8_8_8_8_GAMMA) {
          out_color = color;
          wrote_direct = true;
        } else
#endif
        {
#if XE_GAMMA_RT_AS_UNORM16
          if (XE_TRANSFER_SOURCE_FORMAT == XE_FMT_8_8_8_8_GAMMA) {
            color.rgb = XeLinearToPWLGamma3(color.rgb);
          }
#endif
          uint packed_component_offset = 0u;
          if (XE_TRANSFER_DEST_IS_DEPTH != 0u) {
            packed_component_offset = 1u;
          }
          packed32 =
              XePackUnorm(color[packed_component_offset], 255.0f);
          if (XE_TRANSFER_DEST_IS_DEPTH == 0u) {
            packed32 |= XePackUnorm(color[packed_component_offset + 1],
                                    255.0f) << 8u;
            packed32 |= XePackUnorm(color[packed_component_offset + 2],
                                    255.0f) << 16u;
            packed32 |= XePackUnorm(color[packed_component_offset + 3],
                                    255.0f) << 24u;
          }
        }
      } break;
      case XE_FMT_2_10_10_10:
      case XE_FMT_2_10_10_10_AS_10_10_10_10:
#if !XE_TRANSFER_DEST_IS_UINT
        if (XE_TRANSFER_DEST_FORMAT == XE_FMT_2_10_10_10 ||
            XE_TRANSFER_DEST_FORMAT == XE_FMT_2_10_10_10_AS_10_10_10_10) {
          out_color = source_color0;
          wrote_direct = true;
        } else
#endif
        {
          packed32 = XePackUnorm(source_color0[0], 1023.0f);
          if (XE_TRANSFER_DEST_IS_DEPTH == 0u) {
            packed32 |= XePackUnorm(source_color0[1], 1023.0f) << 10u;
            packed32 |= XePackUnorm(source_color0[2], 1023.0f) << 20u;
            packed32 |= XePackUnorm(source_color0[3], 3.0f) << 30u;
          }
        }
        break;
      case XE_FMT_2_10_10_10_FLOAT:
      case XE_FMT_2_10_10_10_FLOAT_AS_16_16_16_16:
#if !XE_TRANSFER_DEST_IS_UINT
        if (XE_TRANSFER_DEST_FORMAT == XE_FMT_2_10_10_10_FLOAT ||
            XE_TRANSFER_DEST_FORMAT == XE_FMT_2_10_10_10_FLOAT_AS_16_16_16_16) {
          out_color = source_color0;
          wrote_direct = true;
        } else
#endif
        {
          packed32 = XeUnclampedFloat32To7e3(source_color0[0]);
          if (XE_TRANSFER_DEST_IS_DEPTH == 0u) {
            packed32 |= XeUnclampedFloat32To7e3(source_color0[1]) << 10u;
            packed32 |= XeUnclampedFloat32To7e3(source_color0[2]) << 20u;
            packed32 |= XePackUnorm(source_color0[3], 3.0f) << 30u;
          }
        }
        break;
      case XE_FMT_32_FLOAT:
      case XE_FMT_32_32_FLOAT:
        packed32 = as_type<uint>(source_color0[0]);
        break;
      default:
        packed32 = as_type<uint>(source_color0[0]);
        break;
    }
  #endif
#else
    if (XE_TRANSFER_DEST_IS_DEPTH != 0u &&
        XE_TRANSFER_DEST_FORMAT == XE_TRANSFER_SOURCE_FORMAT) {
      TransferColorOut out;
      out.color = XeTransferColorOutType(source_depth0);
#if XE_TRANSFER_DEST_IS_MULTISAMPLE
      out.sample_mask = 1u << dest_sample_id;
#endif
      return out;
    }
    if (XE_TRANSFER_SOURCE_FORMAT == XE_FMT_D24FS8) {
      bool round_depth = constants.depth_round != 0u;
      packed32 = XeFloat32To20e4(source_depth0 * 2.0f, round_depth);
    } else {
      packed32 = XeRoundToNearestEven(
          clamp(source_depth0, 0.0f, 1.0f) * 16777215.0f);
    }
    if (XE_TRANSFER_DEST_IS_DEPTH == 0u) {
      packed32 = (packed32 << 8u) | (source_stencil0 & 0xFFu);
    }
#endif

    if (!wrote_direct) {
      switch (XE_TRANSFER_DEST_FORMAT) {
        case XE_FMT_8_8_8_8: {
#if XE_TRANSFER_DEST_IS_UINT
          out_color = uint4(packed32, 0u, 0u, 0u);
#else
          out_color = float4(
              float((packed32 >> 0u) & 0xFFu) * (1.0f / 255.0f),
              float((packed32 >> 8u) & 0xFFu) * (1.0f / 255.0f),
              float((packed32 >> 16u) & 0xFFu) * (1.0f / 255.0f),
              float((packed32 >> 24u) & 0xFFu) * (1.0f / 255.0f));
#endif
        } break;
        case XE_FMT_8_8_8_8_GAMMA: {
#if XE_TRANSFER_DEST_IS_UINT
          out_color = uint4(packed32, 0u, 0u, 0u);
#else
#if XE_TRANSFER_DEST_IS_GAMMA_UNORM16
          out_color = XePWLGammaPackedRGBA8ToLinearMidpoint(packed32);
#else
          float4 color = float4(
              float((packed32 >> 0u) & 0xFFu) * (1.0f / 255.0f),
              float((packed32 >> 8u) & 0xFFu) * (1.0f / 255.0f),
              float((packed32 >> 16u) & 0xFFu) * (1.0f / 255.0f),
              float((packed32 >> 24u) & 0xFFu) * (1.0f / 255.0f));
#if XE_GAMMA_RT_AS_UNORM16
          color.rgb = XePWLGammaToLinear3(color.rgb);
#endif
          out_color = color;
#endif
#endif
        } break;
        case XE_FMT_2_10_10_10:
        case XE_FMT_2_10_10_10_AS_10_10_10_10: {
#if XE_TRANSFER_DEST_IS_UINT
          out_color = uint4(packed32, 0u, 0u, 0u);
#else
          out_color = float4(
              float((packed32 >> 0u) & 0x3FFu) * (1.0f / 1023.0f),
              float((packed32 >> 10u) & 0x3FFu) * (1.0f / 1023.0f),
              float((packed32 >> 20u) & 0x3FFu) * (1.0f / 1023.0f),
              float((packed32 >> 30u) & 0x3u) * (1.0f / 3.0f));
#endif
        } break;
        case XE_FMT_2_10_10_10_FLOAT:
        case XE_FMT_2_10_10_10_FLOAT_AS_16_16_16_16: {
#if XE_TRANSFER_DEST_IS_UINT
          out_color = uint4(packed32, 0u, 0u, 0u);
#else
          out_color = float4(
              XeFloat7e3To32((packed32 >> 0u) & 0x3FFu),
              XeFloat7e3To32((packed32 >> 10u) & 0x3FFu),
              XeFloat7e3To32((packed32 >> 20u) & 0x3FFu),
              float((packed32 >> 30u) & 0x3u) * (1.0f / 3.0f));
#endif
        } break;
        case XE_FMT_16_16:
        case XE_FMT_16_16_FLOAT: {
#if XE_TRANSFER_DEST_IS_UINT
          out_color = uint4(packed32 & 0xFFFFu, packed32 >> 16u, 0u, 0u);
#else
          out_color = float4(float(packed32 & 0xFFFFu),
                             float(packed32 >> 16u), 0.0f, 0.0f);
#endif
        } break;
        case XE_FMT_32_FLOAT: {
#if XE_TRANSFER_DEST_IS_UINT
          out_color = uint4(packed32, 0u, 0u, 0u);
#else
          out_color = float4(as_type<float>(packed32), 0.0f, 0.0f, 0.0f);
#endif
        } break;
        default:
          break;
      }
    }
  }

  TransferColorOut out;
#if XE_TRANSFER_DEST_COMPONENTS == 1
  out.color = out_color.x;
#elif XE_TRANSFER_DEST_COMPONENTS == 2
  out.color = out_color.xy;
#else
  out.color = out_color;
#endif
#if XE_TRANSFER_DEST_IS_MULTISAMPLE
  out.sample_mask = 1u << dest_sample_id;
#endif
  return out;
}
#elif XE_TRANSFER_OUTPUT_DEPTH || XE_TRANSFER_OUTPUT_STENCIL_BIT
struct TransferDepthOut {
#if XE_TRANSFER_OUTPUT_STENCIL_BIT && XE_TRANSFER_NATIVE_STENCIL_OUTPUT
  uint stencil [[stencil]];
#else
  float depth [[depth(any)]];
#endif
#if XE_TRANSFER_DEST_IS_MULTISAMPLE
  uint sample_mask [[sample_mask]];
#endif
};

fragment TransferDepthOut transfer_ps(
    VSOut in [[stage_in]],
    constant TransferShaderConstants& constants [[buffer(0)]]
    XE_TRANSFER_HOST_DEPTH_BUFFER_PARAM
    XE_TRANSFER_SOURCE_PARAMS
    XE_TRANSFER_HOST_DEPTH_TEXTURE_PARAM
    XE_TRANSFER_SAMPLE_ID_PARAM) {
  uint2 dest_pixel = uint2(in.position.xy);
  uint dest_sample_id = 0u;
#if XE_TRANSFER_DEST_IS_MULTISAMPLE
  #if XE_TRANSFER_DEST_SAMPLE_ID_FROM_SAMPLE
    dest_sample_id = xe_sample_id;
  #else
    dest_sample_id = constants.dest_sample_id;
  #endif
#endif

  uint tile_width_samples = constants.tile_width_samples;
  uint tile_height_samples = constants.tile_height_samples;
  uint dest_tile_width_pixels = constants.dest_tile_width_pixels;
  uint dest_tile_height_pixels = constants.dest_tile_height_pixels;

  uint dest_tile_pixel_x = 0u;
  uint dest_tile_pixel_y = 0u;
  uint dest_tile_index = 0u;
  uint dest_tile_index_x = 0u;
  uint dest_tile_index_y = 0u;
#if XE_TRANSFER_FAST_DIVMOD
  XeFastDivMod(dest_pixel.x, dest_tile_width_pixels,
               constants.dest_tile_width_pixels_inv, dest_tile_index_x,
               dest_tile_pixel_x);
  XeFastDivMod(dest_pixel.y, dest_tile_height_pixels,
               constants.dest_tile_height_pixels_inv, dest_tile_index_y,
               dest_tile_pixel_y);
#else
  dest_tile_index_x = dest_pixel.x / dest_tile_width_pixels;
  dest_tile_pixel_x = dest_pixel.x % dest_tile_width_pixels;
  dest_tile_index_y = dest_pixel.y / dest_tile_height_pixels;
  dest_tile_pixel_y = dest_pixel.y % dest_tile_height_pixels;
#endif

  dest_tile_index =
      dest_tile_index_x +
      dest_tile_index_y * constants.address.dest_pitch;

  uint source_sample_id = dest_sample_id;
  uint source_tile_pixel_x = dest_tile_pixel_x;
  uint source_tile_pixel_y = dest_tile_pixel_y;

  bool source_is_64bpp = XE_TRANSFER_SOURCE_IS_64BPP != 0u;
  bool dest_is_64bpp = XE_TRANSFER_DEST_IS_64BPP != 0u;
  uint source_msaa = XE_TRANSFER_SOURCE_MSAA_SAMPLES;
  uint dest_msaa = XE_TRANSFER_DEST_MSAA_SAMPLES;
  bool msaa_2x_supported = constants.msaa_2x_supported != 0u;

  if (!source_is_64bpp && dest_is_64bpp) {
    if (source_msaa >= 4u) {
      if (dest_msaa >= 4u) {
        source_sample_id = dest_sample_id & 2u;
        source_tile_pixel_x =
            XeBitFieldInsert(dest_sample_id, dest_tile_pixel_x, 1u, 31u);
      } else if (dest_msaa == 2u) {
        if (msaa_2x_supported) {
          source_sample_id = (dest_sample_id ^ 1u) << 1u;
        } else {
          source_sample_id = dest_sample_id & 2u;
        }
      } else {
        source_sample_id =
            XeBitFieldInsert(0u, dest_tile_pixel_y, 1u, 1u);
        source_tile_pixel_y = dest_tile_pixel_y >> 1u;
      }
    } else {
      if (dest_msaa >= 4u) {
        source_tile_pixel_x = XeBitFieldInsert(
            dest_tile_pixel_x << 2u, dest_sample_id, 1u, 1u);
      } else {
        source_tile_pixel_x = dest_tile_pixel_x << 1u;
      }
    }
  } else if (source_is_64bpp && !dest_is_64bpp) {
    if (dest_msaa >= 4u) {
      if (source_msaa >= 4u) {
        source_sample_id =
            XeBitFieldInsert(dest_sample_id, dest_tile_pixel_x, 0u, 1u);
        source_tile_pixel_x = dest_tile_pixel_x >> 1u;
      }
    } else {
      if (source_msaa >= 4u) {
        source_sample_id = XeBitFieldExtract(dest_tile_pixel_x, 1u, 1u);
        if (dest_msaa == 2u) {
          if (msaa_2x_supported) {
            source_sample_id = XeBitFieldInsert(
                source_sample_id, dest_sample_id ^ 1u, 1u, 1u);
          } else {
            source_sample_id = XeBitFieldInsert(
                dest_sample_id, source_sample_id, 0u, 1u);
          }
        } else {
          source_sample_id = XeBitFieldInsert(
              source_sample_id, dest_tile_pixel_y, 1u, 1u);
          source_tile_pixel_y = dest_tile_pixel_y >> 1u;
        }
        source_tile_pixel_x = dest_tile_pixel_x >> 2u;
      } else {
        source_tile_pixel_x = dest_tile_pixel_x >> 1u;
      }
    }
  } else {
    if (source_msaa != dest_msaa) {
      if (source_msaa >= 4u) {
        if (dest_msaa == 2u) {
          if (msaa_2x_supported) {
            source_sample_id = XeBitFieldInsert(
                dest_tile_pixel_x, dest_sample_id ^ 1u, 1u, 31u);
          } else {
            source_sample_id = XeBitFieldInsert(
                dest_sample_id, dest_tile_pixel_x, 0u, 1u);
          }
          source_tile_pixel_x = dest_tile_pixel_x >> 1u;
        } else {
          source_sample_id = XeBitFieldInsert(
              dest_tile_pixel_x & 1u, dest_tile_pixel_y, 1u, 1u);
          source_tile_pixel_x = dest_tile_pixel_x >> 1u;
          source_tile_pixel_y = dest_tile_pixel_y >> 1u;
        }
      } else if (dest_msaa >= 4u) {
        source_tile_pixel_x = XeBitFieldInsert(
            dest_sample_id, dest_tile_pixel_x, 1u, 31u);
      }
    }
  }

  if (source_msaa < 4u && source_msaa != dest_msaa) {
    if (dest_msaa >= 4u) {
      if (source_msaa == 2u) {
        source_sample_id = dest_sample_id >> 1u;
        if (msaa_2x_supported) {
          source_sample_id ^= 1u;
        } else {
          source_sample_id = XeBitFieldInsert(
              source_sample_id, source_sample_id, 1u, 1u);
        }
      } else {
        source_tile_pixel_y = XeBitFieldInsert(
            dest_sample_id >> 1u, dest_tile_pixel_y, 1u, 31u);
      }
    } else {
      if (source_msaa == 2u) {
        source_sample_id = dest_tile_pixel_y & 1u;
        if (msaa_2x_supported) {
          source_sample_id ^= 1u;
        } else {
          source_sample_id = XeBitFieldInsert(
              source_sample_id, source_sample_id, 1u, 1u);
        }
        source_tile_pixel_y = dest_tile_pixel_y >> 1u;
      } else {
        if (msaa_2x_supported) {
          source_tile_pixel_y = XeBitFieldInsert(
              dest_sample_id ^ 1u, dest_tile_pixel_y, 1u, 31u);
        } else {
          source_tile_pixel_y = XeBitFieldInsert(
              dest_sample_id >> 1u, dest_tile_pixel_y, 1u, 31u);
        }
      }
    }
  }

  uint source_pixel_width_dwords_log2 =
      (source_msaa >= 4u ? 1u : 0u) + (source_is_64bpp ? 1u : 0u);

  if ((XE_TRANSFER_SOURCE_IS_DEPTH != 0u) != (XE_TRANSFER_DEST_IS_DEPTH != 0u)) {
    uint source_32bpp_tile_half_pixels =
        tile_width_samples >> (1u + source_pixel_width_dwords_log2);
    if (source_tile_pixel_x < source_32bpp_tile_half_pixels) {
      source_tile_pixel_x += source_32bpp_tile_half_pixels;
    } else {
      source_tile_pixel_x -= source_32bpp_tile_half_pixels;
    }
  }

  uint source_pixel_x = 0u;
  uint source_pixel_y = 0u;
  uint source_tile_index =
      uint(int(dest_tile_index) + constants.address.source_to_dest) &
      (kEdramTileCount - 1u);
  uint source_pitch_tiles = constants.address.source_pitch;
  uint source_tile_index_y = 0u;
  uint source_tile_index_x = 0u;
  XeFastDivMod(source_tile_index, source_pitch_tiles,
               constants.source_pitch_tiles_inv, source_tile_index_y,
               source_tile_index_x);
  source_pixel_x =
      source_tile_index_x *
          (tile_width_samples >> source_pixel_width_dwords_log2) +
      source_tile_pixel_x;
  source_pixel_y =
      source_tile_index_y *
          (tile_height_samples >> (source_msaa >= 2u ? 1u : 0u)) +
      source_tile_pixel_y;

  bool load_two = !source_is_64bpp && dest_is_64bpp;
  uint source_pixel_x1 = source_pixel_x;
  uint source_sample_id1 = source_sample_id;
  if (load_two) {
    if (source_msaa >= 4u) {
      source_sample_id1 = source_sample_id | 1u;
    } else {
      source_pixel_x1 = source_pixel_x | 1u;
    }
  }

#if XE_TRANSFER_SOURCE_IS_COLOR
  #if XE_TRANSFER_SOURCE_IS_UINT
  uint4 source_color0 =
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      xe_transfer_source.read(uint2(source_pixel_x, source_pixel_y),
                              source_sample_id);
#else
      xe_transfer_source.read(uint2(source_pixel_x, source_pixel_y));
#endif
  uint4 source_color1 = source_color0;
  if (load_two) {
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
    source_color1 = xe_transfer_source.read(
        uint2(source_pixel_x1, source_pixel_y), source_sample_id1);
#else
    source_color1 = xe_transfer_source.read(
        uint2(source_pixel_x1, source_pixel_y));
#endif
  }
  #else
  float4 source_color0 =
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      xe_transfer_source.read(uint2(source_pixel_x, source_pixel_y),
                              source_sample_id);
#else
      xe_transfer_source.read(uint2(source_pixel_x, source_pixel_y));
#endif
  float4 source_color1 = source_color0;
  if (load_two) {
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
    source_color1 = xe_transfer_source.read(
        uint2(source_pixel_x1, source_pixel_y), source_sample_id1);
#else
    source_color1 = xe_transfer_source.read(
        uint2(source_pixel_x1, source_pixel_y));
#endif
  }
  #endif
#else
#if XE_TRANSFER_SOURCE_NEEDS_DEPTH
  float source_depth0 =
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      xe_transfer_source_depth.read(uint2(source_pixel_x, source_pixel_y),
                                    source_sample_id).r;
#else
      xe_transfer_source_depth.read(uint2(source_pixel_x, source_pixel_y)).r;
#endif
  float source_depth1 = source_depth0;
  if (load_two) {
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
    source_depth1 = xe_transfer_source_depth.read(
        uint2(source_pixel_x1, source_pixel_y), source_sample_id1).r;
#else
    source_depth1 = xe_transfer_source_depth.read(
        uint2(source_pixel_x1, source_pixel_y)).r;
#endif
  }
#endif
#if XE_TRANSFER_SOURCE_NEEDS_STENCIL
  uint source_stencil0 =
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
      xe_transfer_source_stencil.read(uint2(source_pixel_x, source_pixel_y),
                                      source_sample_id).r;
#else
      xe_transfer_source_stencil.read(uint2(source_pixel_x, source_pixel_y)).r;
#endif
  uint source_stencil1 = source_stencil0;
  if (load_two) {
#if XE_TRANSFER_SOURCE_IS_MULTISAMPLE
    source_stencil1 = xe_transfer_source_stencil.read(
        uint2(source_pixel_x1, source_pixel_y), source_sample_id1).r;
#else
    source_stencil1 = xe_transfer_source_stencil.read(
        uint2(source_pixel_x1, source_pixel_y)).r;
#endif
  }
#else
  uint source_stencil0 = 0u;
  uint source_stencil1 = 0u;
#endif
#endif

  uint packed = 0u;
#if !XE_TRANSFER_OUTPUT_STENCIL_BIT
  bool packed_only_depth = false;
#endif
#if XE_TRANSFER_SOURCE_IS_COLOR
  #if XE_TRANSFER_SOURCE_IS_UINT
  switch (XE_TRANSFER_SOURCE_FORMAT) {
    case XE_FMT_16_16:
    case XE_FMT_16_16_FLOAT:
    case XE_FMT_16_16_16_16:
    case XE_FMT_16_16_16_16_FLOAT:
      packed = source_color0[0] | (source_color0[1] << 16u);
      break;
    case XE_FMT_32_FLOAT:
    case XE_FMT_32_32_FLOAT:
      packed = source_color0[0];
      break;
    default:
      packed = source_color0[0];
      break;
  }
  #else
  switch (XE_TRANSFER_SOURCE_FORMAT) {
    case XE_FMT_8_8_8_8:
    case XE_FMT_8_8_8_8_GAMMA: {
      float4 color = source_color0;
      if (XE_TRANSFER_SOURCE_FORMAT == XE_FMT_8_8_8_8_GAMMA) {
#if XE_GAMMA_RT_AS_UNORM16 || XE_TRANSFER_OUTPUT_STENCIL_BIT
        color.rgb = XeLinearToPWLGamma3(color.rgb);
#endif
      }
      // Match D3D12: color -> depth transfers derive guest depth from bits
      // 8:31 of the packed 32bpp color word rather than treating depth as a
      // standalone component.
      packed = XePackColorRGBA8(color);
    } break;
    case XE_FMT_2_10_10_10:
    case XE_FMT_2_10_10_10_AS_10_10_10_10: {
      packed = XePackColorRGB10A2(source_color0);
    } break;
    case XE_FMT_2_10_10_10_FLOAT:
    case XE_FMT_2_10_10_10_FLOAT_AS_16_16_16_16: {
      packed = XePackColorRGB10A2Float(source_color0);
    } break;
    case XE_FMT_32_FLOAT:
    case XE_FMT_32_32_FLOAT:
      packed = as_type<uint>(source_color0[0]);
      break;
    default:
      packed = as_type<uint>(source_color0[0]);
      break;
  }
  #endif
#else
#if XE_TRANSFER_OUTPUT_STENCIL_BIT
  packed = source_stencil0;
#else
  if (XE_TRANSFER_SOURCE_FORMAT == XE_FMT_D24FS8) {
    bool round_depth = constants.depth_round != 0u;
    packed = XeFloat32To20e4(source_depth0 * 2.0f, round_depth);
  } else {
    packed = XeRoundToNearestEven(
        clamp(source_depth0, 0.0f, 1.0f) * 16777215.0f);
  }
  if (XE_TRANSFER_DEST_IS_DEPTH != 0u) {
#if !XE_TRANSFER_OUTPUT_STENCIL_BIT
    packed_only_depth = true;
#endif
  } else {
    packed = (packed << 8u) | (source_stencil0 & 0xFFu);
  }
#endif
#endif

#if XE_TRANSFER_OUTPUT_STENCIL_BIT
#if XE_TRANSFER_NATIVE_STENCIL_OUTPUT
  TransferDepthOut out;
  out.stencil = packed & 0xFFu;
#if XE_TRANSFER_DEST_IS_MULTISAMPLE
  out.sample_mask = 1u << dest_sample_id;
#endif
  return out;
#else
  if (constants.stencil_clear == 0u) {
    if ((packed & constants.stencil_mask) == 0u) {
      discard_fragment();
    }
  }
  TransferDepthOut out;
  out.depth = 0.0f;
#if XE_TRANSFER_DEST_IS_MULTISAMPLE
  out.sample_mask = 1u << dest_sample_id;
#endif
  return out;
#endif
#else
  uint guest_depth24 = packed;
  if (!packed_only_depth) {
    guest_depth24 = packed >> 8u;
  }

  float host_depth32 = 0.0f;
  bool has_host_depth = false;

#if XE_TRANSFER_HAS_HOST_DEPTH
  if (constants.host_depth_source_is_copy == 0u) {
    uint host_tile_pixel_x = dest_tile_pixel_x;
    uint host_tile_pixel_y = dest_tile_pixel_y;
    uint host_sample_id = dest_sample_id;
    uint host_msaa = XE_TRANSFER_HOST_DEPTH_MSAA_SAMPLES;

    if (host_msaa != dest_msaa) {
      if (host_msaa >= 4u) {
        if (dest_msaa == 2u) {
          if (msaa_2x_supported) {
            host_sample_id = XeBitFieldInsert(
                dest_tile_pixel_x, dest_sample_id ^ 1u, 1u, 31u);
          } else {
            host_sample_id = XeBitFieldInsert(
                dest_sample_id, dest_tile_pixel_x, 0u, 1u);
          }
          host_tile_pixel_x = dest_tile_pixel_x >> 1u;
        } else {
          host_sample_id = XeBitFieldInsert(
              dest_tile_pixel_x & 1u, dest_tile_pixel_y, 1u, 1u);
          host_tile_pixel_x = dest_tile_pixel_x >> 1u;
          host_tile_pixel_y = dest_tile_pixel_y >> 1u;
        }
      } else if (dest_msaa >= 4u) {
        host_tile_pixel_x = XeBitFieldInsert(
            dest_sample_id, dest_tile_pixel_x, 1u, 31u);
      }

      if (host_msaa < 4u) {
        if (dest_msaa >= 4u) {
          if (host_msaa == 2u) {
            host_sample_id = dest_sample_id >> 1u;
            if (msaa_2x_supported) {
              host_sample_id ^= 1u;
            } else {
              host_sample_id = XeBitFieldInsert(
                  host_sample_id, host_sample_id, 1u, 1u);
            }
          } else {
            host_tile_pixel_y = XeBitFieldInsert(
                dest_sample_id >> 1u, dest_tile_pixel_y, 1u, 31u);
          }
        } else {
          if (host_msaa == 2u) {
            host_sample_id = dest_tile_pixel_y & 1u;
            if (msaa_2x_supported) {
              host_sample_id ^= 1u;
            } else {
              host_sample_id = XeBitFieldInsert(
                  host_sample_id, host_sample_id, 1u, 1u);
            }
            host_tile_pixel_y = dest_tile_pixel_y >> 1u;
          } else {
            if (msaa_2x_supported) {
              host_tile_pixel_y = XeBitFieldInsert(
                  dest_sample_id ^ 1u, dest_tile_pixel_y, 1u, 31u);
            } else {
              host_tile_pixel_y = XeBitFieldInsert(
                  dest_sample_id >> 1u, dest_tile_pixel_y, 1u, 31u);
            }
          }
        }
      }
    }

    uint host_pixel_x = 0u;
    uint host_pixel_y = 0u;
    uint host_tile_index =
        uint(int(dest_tile_index) +
             constants.host_depth_address.source_to_dest) &
        (kEdramTileCount - 1u);
    uint host_pitch_tiles = constants.host_depth_address.source_pitch;
    uint host_tile_index_y = 0u;
    uint host_tile_index_x = 0u;
    XeFastDivMod(host_tile_index, host_pitch_tiles,
                 constants.host_depth_source_pitch_tiles_inv,
                 host_tile_index_y, host_tile_index_x);
    host_pixel_x =
        host_tile_index_x *
            (tile_width_samples >> (host_msaa >= 4u ? 1u : 0u)) +
        host_tile_pixel_x;
    host_pixel_y =
        host_tile_index_y *
            (tile_height_samples >> (host_msaa >= 2u ? 1u : 0u)) +
        host_tile_pixel_y;

#if XE_TRANSFER_HOST_DEPTH_IS_MULTISAMPLE
    host_depth32 = xe_transfer_host_depth.read(
        uint2(host_pixel_x, host_pixel_y), host_sample_id).r;
#else
    host_depth32 =
        xe_transfer_host_depth.read(uint2(host_pixel_x, host_pixel_y)).r;
#endif
  } else {
    uint dest_tile_sample_x = dest_tile_pixel_x;
    uint dest_tile_sample_y = dest_tile_pixel_y;
    if (dest_msaa >= 2u) {
      if (dest_msaa >= 4u) {
        dest_tile_sample_x = XeBitFieldInsert(
            dest_sample_id, dest_tile_pixel_x, 1u, 31u);
      }
      uint vert_sample = 0u;
      if (dest_msaa == 2u && msaa_2x_supported) {
        vert_sample = dest_sample_id ^ 1u;
      } else {
        vert_sample = dest_sample_id >> 1u;
      }
      dest_tile_sample_y = XeBitFieldInsert(
          vert_sample, dest_tile_pixel_y, 1u, 31u);
    }
    uint host_depth_offset =
        (tile_width_samples * tile_height_samples) * dest_tile_index +
        tile_width_samples * dest_tile_sample_y + dest_tile_sample_x;
    host_depth32 =
        as_type<float>(xe_transfer_host_depth_buffer[host_depth_offset]);
  }
  has_host_depth = true;
#endif

  float fragment_depth = 0.0f;
  if (XE_TRANSFER_DEST_FORMAT == XE_FMT_D24FS8) {
    float guest_depth32 = XeFloat20e4To32(guest_depth24, true);
    fragment_depth = guest_depth32;
  } else {
    fragment_depth = XeUnorm24To32(guest_depth24);
  }

  if (has_host_depth) {
    uint host_depth24 = 0u;
    if (XE_TRANSFER_DEST_FORMAT == XE_FMT_D24FS8) {
      bool round_depth = constants.depth_round != 0u;
      host_depth24 = XeFloat32To20e4(host_depth32 * 2.0f, round_depth);
    } else {
      host_depth24 = XeRoundToNearestEven(
          clamp(host_depth32, 0.0f, 1.0f) * 16777215.0f);
    }
    if (host_depth24 == guest_depth24) {
      fragment_depth = host_depth32;
    }
  }

  TransferDepthOut out;
  out.depth = fragment_depth;
#if XE_TRANSFER_DEST_IS_MULTISAMPLE
  out.sample_mask = 1u << dest_sample_id;
#endif
  return out;
#endif
}
#endif
)METAL";

  source.append(kTransferShaderSource);

  NS::Error* error = nullptr;
  auto src_str = NS::String::string(source.c_str(), NS::UTF8StringEncoding);
  MTL::CompileOptions* compile_options = MTL::CompileOptions::alloc()->init();
  compile_options->setFastMathEnabled(true);
  compile_options->setLanguageVersion(MTL::LanguageVersion2_4);
  MTL::Library* lib = device_->newLibrary(src_str, compile_options, &error);
  compile_options->release();
  if (!lib) {
    XELOGE(
        "GetOrCreateTransferPipelines: failed to compile transfer MSL "
        "(mode={}): "
        "{}",
        int(key.mode),
        error && error->localizedDescription()
            ? error->localizedDescription()->utf8String()
            : "unknown error");
    return nullptr;
  }

  auto vs_name = NS::String::string("transfer_rect_vs", NS::UTF8StringEncoding);
  auto ps_name = NS::String::string("transfer_ps", NS::UTF8StringEncoding);
  MTL::Function* vs = lib->newFunction(vs_name);
  MTL::Function* ps = lib->newFunction(ps_name);
  if (!vs || !ps) {
    XELOGE("GetOrCreateTransferPipelines: failed to get transfer shader entry");
    if (vs) {
      vs->release();
    }
    if (ps) {
      ps->release();
    }
    lib->release();
    return nullptr;
  }

  MTL::RenderPipelineDescriptor* desc =
      MTL::RenderPipelineDescriptor::alloc()->init();
  desc->setVertexFunction(vs);
  desc->setFragmentFunction(ps);

  if (output == TransferOutput::kColor) {
    for (uint32_t i = 0; i < xenos::kMaxColorRenderTargets; ++i) {
      auto* color_attachment = desc->colorAttachments()->object(i);
      color_attachment->setPixelFormat(
          pipeline_key.color_attachment_formats[i]);
      color_attachment->setWriteMask(i == pipeline_key.color_attachment_index
                                         ? MTL::ColorWriteMaskAll
                                         : MTL::ColorWriteMaskNone);
    }
    desc->setDepthAttachmentPixelFormat(pipeline_key.depth_attachment_format);
    desc->setStencilAttachmentPixelFormat(
        pipeline_key.stencil_attachment_format);
  } else {
    for (uint32_t i = 0; i < xenos::kMaxColorRenderTargets; ++i) {
      auto* color_attachment = desc->colorAttachments()->object(i);
      color_attachment->setPixelFormat(
          pipeline_key.color_attachment_formats[i]);
      color_attachment->setWriteMask(MTL::ColorWriteMaskNone);
    }
    desc->setDepthAttachmentPixelFormat(pipeline_key.depth_attachment_format);
    desc->setStencilAttachmentPixelFormat(
        pipeline_key.stencil_attachment_format);
  }

  uint32_t sample_count = 1;
  if (key.dest_msaa_samples == xenos::MsaaSamples::k2X) {
    sample_count = 2;
  } else if (key.dest_msaa_samples == xenos::MsaaSamples::k4X) {
    sample_count = 4;
  }
  desc->setSampleCount(sample_count);

  MTL::RenderPipelineState* pipeline =
      device_->newRenderPipelineState(desc, &error);

  desc->release();
  vs->release();
  ps->release();
  lib->release();

  if (!pipeline) {
    XELOGE(
        "GetOrCreateTransferPipelines: failed to create pipeline (mode={}): {}",
        int(key.mode),
        error && error->localizedDescription()
            ? error->localizedDescription()->utf8String()
            : "unknown error");
    return nullptr;
  }

  transfer_pipelines_.emplace(pipeline_key, pipeline);

  return pipeline;
}

MTL::Library* MetalRenderTargetCache::GetOrCreateTransferLibrary() {
  if (transfer_library_) {
    return transfer_library_;
  }
  static const char kTransferLibrarySource[] = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct VSOut {
  float4 position [[position]];
};

struct TransferClearColorFloatConstants {
  float4 color;
};

struct TransferClearColorUintConstants {
  uint4 color;
};

struct TransferClearDepthConstants {
  float4 depth;
};

vertex VSOut transfer_clear_vs(uint vid [[vertex_id]]) {
  float2 pt = float2((vid << 1) & 2, vid & 2);
  VSOut out;
  out.position = float4(pt * 2.0f - 1.0f, 0.0f, 1.0f);
  return out;
}

fragment float4 transfer_clear_color_float_ps(
    VSOut in [[stage_in]],
    constant TransferClearColorFloatConstants& constants [[buffer(0)]]) {
  return constants.color;
}

fragment uint4 transfer_clear_color_uint_ps(
    VSOut in [[stage_in]],
    constant TransferClearColorUintConstants& constants [[buffer(0)]]) {
  return constants.color;
}

struct TransferDepthOut {
  float depth [[depth(any)]];
};

fragment TransferDepthOut transfer_clear_depth_ps(
    VSOut in [[stage_in]],
    constant TransferClearDepthConstants& constants [[buffer(0)]]) {
  TransferDepthOut out;
  out.depth = constants.depth.x;
  return out;
}
)METAL";

  NS::Error* error = nullptr;
  auto source_str =
      NS::String::string(kTransferLibrarySource, NS::UTF8StringEncoding);
  transfer_library_ = device_->newLibrary(source_str, nullptr, &error);
  if (!transfer_library_) {
    XELOGE("GetOrCreateTransferLibrary: failed to compile transfer library: {}",
           error && error->localizedDescription()
               ? error->localizedDescription()->utf8String()
               : "unknown error");
  }
  return transfer_library_;
}

MTL::RenderPipelineState*
MetalRenderTargetCache::GetOrCreateTransferClearPipeline(
    MTL::PixelFormat dest_format, bool dest_is_uint, bool is_depth,
    uint32_t sample_count, uint32_t color_attachment_index,
    const TransferColorAttachmentFormats* color_attachment_formats,
    MTL::PixelFormat depth_attachment_format,
    MTL::PixelFormat stencil_attachment_format) {
  TransferClearPipelineKey key = {};
  key.sample_count = sample_count ? sample_count : 1;
  key.dest_is_uint = dest_is_uint ? 1u : 0u;
  key.is_depth = is_depth ? 1u : 0u;
  key.color_attachment_formats.fill(MTL::PixelFormatInvalid);
  if (color_attachment_formats) {
    key.color_attachment_formats = *color_attachment_formats;
  }
  if (is_depth) {
    key.depth_attachment_format =
        depth_attachment_format != MTL::PixelFormatInvalid
            ? depth_attachment_format
            : dest_format;
    if (key.depth_attachment_format != dest_format) {
      return nullptr;
    }
    if (dest_format == MTL::PixelFormatDepth32Float_Stencil8 ||
        dest_format == MTL::PixelFormatDepth24Unorm_Stencil8) {
      key.stencil_attachment_format =
          stencil_attachment_format != MTL::PixelFormatInvalid
              ? stencil_attachment_format
              : dest_format;
      if (key.stencil_attachment_format != dest_format) {
        return nullptr;
      }
    } else {
      key.stencil_attachment_format = stencil_attachment_format;
    }
  } else {
    if (color_attachment_index >= xenos::kMaxColorRenderTargets) {
      return nullptr;
    }
    key.color_attachment_index = color_attachment_index;
    if (!color_attachment_formats) {
      key.color_attachment_formats[color_attachment_index] = dest_format;
    }
    if (key.color_attachment_formats[color_attachment_index] != dest_format) {
      return nullptr;
    }
    key.depth_attachment_format = depth_attachment_format;
    key.stencil_attachment_format = stencil_attachment_format;
  }
  auto it = transfer_clear_pipelines_.find(key);
  if (it != transfer_clear_pipelines_.end()) {
    return it->second;
  }

  MTL::Library* lib = GetOrCreateTransferLibrary();
  if (!lib) {
    return nullptr;
  }

  auto vs_name =
      NS::String::string("transfer_clear_vs", NS::UTF8StringEncoding);
  const char* ps_name_cstr = nullptr;
  if (is_depth) {
    ps_name_cstr = "transfer_clear_depth_ps";
  } else {
    ps_name_cstr = dest_is_uint ? "transfer_clear_color_uint_ps"
                                : "transfer_clear_color_float_ps";
  }
  auto ps_name = NS::String::string(ps_name_cstr, NS::UTF8StringEncoding);

  MTL::Function* vs = lib->newFunction(vs_name);
  MTL::Function* ps = lib->newFunction(ps_name);
  if (!vs || !ps) {
    XELOGE(
        "GetOrCreateTransferClearPipeline: missing transfer clear functions");
    if (vs) {
      vs->release();
    }
    if (ps) {
      ps->release();
    }
    return nullptr;
  }

  MTL::RenderPipelineDescriptor* desc =
      MTL::RenderPipelineDescriptor::alloc()->init();
  desc->setVertexFunction(vs);
  desc->setFragmentFunction(ps);
  desc->setSampleCount(key.sample_count);

  for (uint32_t i = 0; i < xenos::kMaxColorRenderTargets; ++i) {
    auto* color_attachment = desc->colorAttachments()->object(i);
    color_attachment->setPixelFormat(key.color_attachment_formats[i]);
    color_attachment->setWriteMask(!is_depth && i == key.color_attachment_index
                                       ? MTL::ColorWriteMaskAll
                                       : MTL::ColorWriteMaskNone);
  }
  desc->setDepthAttachmentPixelFormat(key.depth_attachment_format);
  desc->setStencilAttachmentPixelFormat(key.stencil_attachment_format);

  NS::Error* error = nullptr;
  MTL::RenderPipelineState* pipeline =
      device_->newRenderPipelineState(desc, &error);

  desc->release();
  vs->release();
  ps->release();

  if (!pipeline) {
    XELOGE("GetOrCreateTransferClearPipeline: failed to create pipeline: {}",
           error && error->localizedDescription()
               ? error->localizedDescription()->utf8String()
               : "unknown error");
    return nullptr;
  }

  transfer_clear_pipelines_.emplace(key, pipeline);
  return pipeline;
}

MTL::Texture* MetalRenderTargetCache::GetTransferDummyTexture(
    MTL::PixelFormat format, uint32_t sample_count) {
  if (!device_) {
    return nullptr;
  }
  MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
  desc->setWidth(1);
  desc->setHeight(1);
  desc->setPixelFormat(format);
  desc->setTextureType(sample_count > 1 ? MTL::TextureType2DMultisample
                                        : MTL::TextureType2D);
  desc->setSampleCount(sample_count ? sample_count : 1);
  MTL::TextureUsage usage = MTL::TextureUsageShaderRead;
  if (format == MTL::PixelFormatDepth32Float_Stencil8 ||
      format == MTL::PixelFormatDepth24Unorm_Stencil8) {
    usage |= MTL::TextureUsagePixelFormatView;
  }
  desc->setUsage(usage);
  desc->setStorageMode(MTL::StorageModePrivate);
  MTL::Texture* tex = nullptr;
  if (render_target_heap_pool_) {
    tex = render_target_heap_pool_->CreateTexture(desc);
  }
  if (!tex) {
    tex = device_->newTexture(desc);
  }
  desc->release();
  return tex;
}

MTL::Texture* MetalRenderTargetCache::GetTransferDummyColorFloatTexture(
    uint32_t sample_count) {
  size_t index = sample_count >= 4 ? 2 : (sample_count == 2 ? 1 : 0);
  if (!transfer_dummy_color_float_[index]) {
    transfer_dummy_color_float_[index] =
        GetTransferDummyTexture(MTL::PixelFormatRGBA8Unorm, sample_count);
  }
  return transfer_dummy_color_float_[index];
}

MTL::Texture* MetalRenderTargetCache::GetTransferDummyColorUintTexture(
    uint32_t sample_count) {
  size_t index = sample_count >= 4 ? 2 : (sample_count == 2 ? 1 : 0);
  if (!transfer_dummy_color_uint_[index]) {
    transfer_dummy_color_uint_[index] =
        GetTransferDummyTexture(MTL::PixelFormatRGBA8Uint, sample_count);
  }
  return transfer_dummy_color_uint_[index];
}

MTL::Texture* MetalRenderTargetCache::GetTransferDummyDepthTexture(
    uint32_t sample_count) {
  size_t index = sample_count >= 4 ? 2 : (sample_count == 2 ? 1 : 0);
  if (!transfer_dummy_depth_[index]) {
    transfer_dummy_depth_[index] = GetTransferDummyTexture(
        MTL::PixelFormatDepth32Float_Stencil8, sample_count);
  }
  return transfer_dummy_depth_[index];
}

MTL::Texture* MetalRenderTargetCache::GetTransferDummyStencilTexture(
    uint32_t sample_count) {
  size_t index = sample_count >= 4 ? 2 : (sample_count == 2 ? 1 : 0);
  if (!transfer_dummy_stencil_[index]) {
    MTL::Texture* depth_tex = GetTransferDummyDepthTexture(sample_count);
    if (!depth_tex) {
      return nullptr;
    }
    transfer_dummy_stencil_[index] =
        depth_tex->newTextureView(MTL::PixelFormatX32_Stencil8);
  }
  return transfer_dummy_stencil_[index];
}

MTL::Buffer* MetalRenderTargetCache::GetTransferDummyBuffer() {
  if (!transfer_dummy_buffer_ && device_) {
    transfer_dummy_buffer_ =
        device_->newBuffer(sizeof(uint32_t), MTL::ResourceStorageModeShared);
    if (transfer_dummy_buffer_) {
    }
    if (transfer_dummy_buffer_) {
      std::memset(transfer_dummy_buffer_->contents(), 0, sizeof(uint32_t));
    }
  }
  return transfer_dummy_buffer_;
}

MTL::DepthStencilState* MetalRenderTargetCache::GetTransferDepthStencilState(
    bool depth_write) {
  if (transfer_depth_state_) {
    return transfer_depth_state_;
  }
  MTL::DepthStencilDescriptor* desc =
      MTL::DepthStencilDescriptor::alloc()->init();
  desc->setDepthCompareFunction(::cvars::depth_transfer_not_equal_test
                                    ? MTL::CompareFunctionNotEqual
                                    : MTL::CompareFunctionAlways);
  desc->setDepthWriteEnabled(depth_write);
  transfer_depth_state_ = device_->newDepthStencilState(desc);
  desc->release();
  return transfer_depth_state_;
}

MTL::DepthStencilState*
MetalRenderTargetCache::GetTransferNoDepthStencilState() {
  if (transfer_depth_state_none_) {
    return transfer_depth_state_none_;
  }
  MTL::DepthStencilDescriptor* desc =
      MTL::DepthStencilDescriptor::alloc()->init();
  desc->setDepthCompareFunction(MTL::CompareFunctionAlways);
  desc->setDepthWriteEnabled(false);
  transfer_depth_state_none_ = device_->newDepthStencilState(desc);
  desc->release();
  return transfer_depth_state_none_;
}

MTL::DepthStencilState* MetalRenderTargetCache::GetTransferDepthClearState() {
  if (transfer_depth_clear_state_) {
    return transfer_depth_clear_state_;
  }
  MTL::DepthStencilDescriptor* desc =
      MTL::DepthStencilDescriptor::alloc()->init();
  desc->setDepthCompareFunction(MTL::CompareFunctionAlways);
  desc->setDepthWriteEnabled(true);
  MTL::StencilDescriptor* stencil = MTL::StencilDescriptor::alloc()->init();
  stencil->setStencilCompareFunction(MTL::CompareFunctionAlways);
  stencil->setStencilFailureOperation(MTL::StencilOperationKeep);
  stencil->setDepthFailureOperation(MTL::StencilOperationKeep);
  stencil->setDepthStencilPassOperation(MTL::StencilOperationReplace);
  stencil->setReadMask(0xFF);
  stencil->setWriteMask(0xFF);
  desc->setFrontFaceStencil(stencil);
  desc->setBackFaceStencil(stencil);
  transfer_depth_clear_state_ = device_->newDepthStencilState(desc);
  stencil->release();
  desc->release();
  return transfer_depth_clear_state_;
}

MTL::DepthStencilState* MetalRenderTargetCache::GetTransferStencilClearState() {
  if (transfer_stencil_clear_state_) {
    return transfer_stencil_clear_state_;
  }
  MTL::DepthStencilDescriptor* desc =
      MTL::DepthStencilDescriptor::alloc()->init();
  desc->setDepthCompareFunction(MTL::CompareFunctionAlways);
  desc->setDepthWriteEnabled(false);
  MTL::StencilDescriptor* stencil = MTL::StencilDescriptor::alloc()->init();
  stencil->setStencilCompareFunction(MTL::CompareFunctionAlways);
  stencil->setStencilFailureOperation(MTL::StencilOperationKeep);
  stencil->setDepthFailureOperation(MTL::StencilOperationKeep);
  stencil->setDepthStencilPassOperation(MTL::StencilOperationReplace);
  stencil->setReadMask(0xFF);
  stencil->setWriteMask(0xFF);
  desc->setFrontFaceStencil(stencil);
  desc->setBackFaceStencil(stencil);
  transfer_stencil_clear_state_ = device_->newDepthStencilState(desc);
  stencil->release();
  desc->release();
  return transfer_stencil_clear_state_;
}

MTL::DepthStencilState*
MetalRenderTargetCache::GetTransferStencilOutputState() {
  if (transfer_stencil_output_state_) {
    return transfer_stencil_output_state_;
  }
  MTL::DepthStencilDescriptor* desc =
      MTL::DepthStencilDescriptor::alloc()->init();
  desc->setDepthCompareFunction(MTL::CompareFunctionAlways);
  desc->setDepthWriteEnabled(false);
  MTL::StencilDescriptor* stencil = MTL::StencilDescriptor::alloc()->init();
  stencil->setStencilCompareFunction(MTL::CompareFunctionAlways);
  stencil->setStencilFailureOperation(MTL::StencilOperationKeep);
  stencil->setDepthFailureOperation(MTL::StencilOperationKeep);
  stencil->setDepthStencilPassOperation(MTL::StencilOperationReplace);
  stencil->setReadMask(0xFF);
  stencil->setWriteMask(0xFF);
  desc->setFrontFaceStencil(stencil);
  desc->setBackFaceStencil(stencil);
  transfer_stencil_output_state_ = device_->newDepthStencilState(desc);
  stencil->release();
  desc->release();
  return transfer_stencil_output_state_;
}

MTL::DepthStencilState* MetalRenderTargetCache::GetTransferStencilBitState(
    uint32_t bit) {
  if (bit >= 8) {
    return nullptr;
  }
  if (transfer_stencil_bit_states_[bit]) {
    return transfer_stencil_bit_states_[bit];
  }
  uint32_t mask = uint32_t(1) << bit;
  MTL::DepthStencilDescriptor* desc =
      MTL::DepthStencilDescriptor::alloc()->init();
  desc->setDepthCompareFunction(MTL::CompareFunctionAlways);
  desc->setDepthWriteEnabled(false);
  MTL::StencilDescriptor* stencil = MTL::StencilDescriptor::alloc()->init();
  stencil->setStencilCompareFunction(MTL::CompareFunctionAlways);
  stencil->setStencilFailureOperation(MTL::StencilOperationKeep);
  stencil->setDepthFailureOperation(MTL::StencilOperationKeep);
  stencil->setDepthStencilPassOperation(MTL::StencilOperationReplace);
  stencil->setReadMask(0xFF);
  stencil->setWriteMask(uint32_t(mask));
  desc->setFrontFaceStencil(stencil);
  desc->setBackFaceStencil(stencil);
  transfer_stencil_bit_states_[bit] = device_->newDepthStencilState(desc);
  stencil->release();
  desc->release();
  return transfer_stencil_bit_states_[bit];
}

}  // namespace metal
}  // namespace gpu
}  // namespace xe
