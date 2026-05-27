/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/gpu/metal/metal_shared_memory.h"

#include <algorithm>
#include <cstring>

#include "xenia/base/logging.h"
#include "xenia/base/math.h"
#include "xenia/base/memory.h"
#include "xenia/gpu/metal/metal_command_processor.h"

namespace xe {
namespace gpu {
namespace metal {

MetalSharedMemory::MetalSharedMemory(MetalCommandProcessor& command_processor,
                                     Memory& memory)
    : SharedMemory(memory), command_processor_(command_processor) {}

MetalSharedMemory::~MetalSharedMemory() { Shutdown(); }

bool MetalSharedMemory::Initialize() {
  if (!InitializeCommon()) {
    return false;
  }

  const ui::metal::MetalProvider& provider =
      command_processor_.GetMetalProvider();
  MTL::Device* device = provider.GetDevice();

  if (!device) {
    XELOGE("Metal device is null in MetalSharedMemory::Initialize");
    return false;
  }

  // Create Metal buffer - similar to D3D12's approach
  // On Apple Silicon, ResourceStorageModeShared gives CPU/GPU access
  void* xbox_ram = memory().TranslatePhysical(0);
  if (!xbox_ram) {
    XELOGE("Metal shared memory: Xbox RAM is null");
    return false;
  }

  buffer_ = device->newBuffer(kBufferSize, MTL::ResourceStorageModeShared);
  if (!buffer_) {
    XELOGE("Failed to create Metal shared memory buffer");
    return false;
  }

  upload_buffer_pool_ = std::make_unique<MetalUploadBufferPool>(
      device, xe::align(ui::GraphicsUploadBufferPool::kDefaultPageSize,
                        size_t(1) << page_size_log2()));

  return true;
}

void MetalSharedMemory::ClearCache() {
  SharedMemory::ClearCache();

  if (upload_buffer_pool_) {
    upload_buffer_pool_->ClearCache();
  }
}

bool MetalSharedMemory::UploadRanges(
    const std::pair<uint32_t, uint32_t>* upload_page_ranges,
    uint32_t num_upload_ranges) {
  static bool first_upload = true;
  if (first_upload) {
    first_upload = false;
    const uint32_t page_size = 1u << page_size_log2();
    XELOGD("MetalSharedMemory::UploadRanges: page_size={}, {} ranges to upload",
           page_size, num_upload_ranges);
    for (uint32_t i = 0; i < std::min(5u, num_upload_ranges); i++) {
      uint32_t start_byte = upload_page_ranges[i].first * page_size;
      uint32_t length_bytes = upload_page_ranges[i].second * page_size;
      XELOGD("  Range[{}]: page={} count={} -> byte offset=0x{:08X} length={}",
             i, upload_page_ranges[i].first, upload_page_ranges[i].second,
             start_byte, length_bytes);
    }
  }

  if (!buffer_ || num_upload_ranges == 0) {
    return true;
  }
  if (!upload_buffer_pool_) {
    XELOGE("MetalSharedMemory::UploadRanges: upload buffer pool is null");
    return false;
  }
  command_processor_.RecordSharedMemoryUploadRangeBatch(num_upload_ranges);

  void* xbox_ram = memory().TranslatePhysical(0);
  if (!xbox_ram) {
    XELOGE("MetalSharedMemory::UploadRanges: Xbox RAM is null");
    return false;
  }
  uint8_t* xbox_data = static_cast<uint8_t*>(xbox_ram);

  const uint32_t page_size = 1u << page_size_log2();
  upload_buffer_pool_->Reclaim(command_processor_.GetCompletedSubmission());

  MTL::BlitCommandEncoder* blit_encoder = nullptr;
  auto get_blit_encoder = [&]() -> MTL::BlitCommandEncoder* {
    if (!blit_encoder) {
      blit_encoder = command_processor_.GetSharedMemoryUploadBlitEncoder();
    }
    return blit_encoder;
  };

  uint32_t merged_start = 0;
  uint32_t merged_end = 0;
  bool have_merged = false;

  auto flush_merged_range = [&](uint32_t start, uint32_t end) -> bool {
    if (end <= start) {
      return true;
    }
    uint32_t offset = start;
    uint32_t remaining = end - start;
    while (remaining) {
      MTL::BlitCommandEncoder* encoder = get_blit_encoder();
      if (!encoder) {
        XELOGE("MetalSharedMemory::UploadRanges: failed to get blit encoder");
        return false;
      }

      MTL::Buffer* upload_buffer = nullptr;
      size_t upload_offset = 0;
      uint64_t upload_gpu_address = 0;
      size_t upload_size = 0;
      uint8_t* upload_mapping = upload_buffer_pool_->RequestPartial(
          command_processor_.GetCurrentSubmission(), remaining, page_size,
          &upload_buffer, upload_offset, upload_gpu_address, upload_size);
      if (!upload_mapping || !upload_buffer || !upload_size) {
        XELOGE(
            "MetalSharedMemory::UploadRanges: failed to allocate upload "
            "staging buffer");
        return false;
      }

      MakeRangeValid(offset, static_cast<uint32_t>(upload_size), false);
      if (upload_size < (1ULL << 32) && upload_size > 8192) {
        memory::vastcpy(upload_mapping, xbox_data + offset,
                        static_cast<uint32_t>(upload_size));
        swcache::WriteFence();
      } else {
        std::memcpy(upload_mapping, xbox_data + offset, upload_size);
      }
      encoder->copyFromBuffer(
          upload_buffer, static_cast<NS::UInteger>(upload_offset), buffer_,
          static_cast<NS::UInteger>(offset),
          static_cast<NS::UInteger>(upload_size));

      offset += static_cast<uint32_t>(upload_size);
      remaining -= static_cast<uint32_t>(upload_size);
    }
    return true;
  };

  for (uint32_t i = 0; i < num_upload_ranges; ++i) {
    const auto& range = upload_page_ranges[i];
    uint32_t start = range.first * page_size;
    uint32_t end = start + range.second * page_size;
    if (start >= kBufferSize) {
      continue;
    }
    if (end > kBufferSize) {
      end = kBufferSize;
    }

    if (!have_merged) {
      merged_start = start;
      merged_end = end;
      have_merged = true;
      continue;
    }

    // Merge overlapping/adjacent ranges.
    if (start <= merged_end) {
      if (end > merged_end) {
        merged_end = end;
      }
    } else {
      if (!flush_merged_range(merged_start, merged_end)) {
        command_processor_.EndSharedMemoryUploadBlitEncoder();
        return false;
      }
      merged_start = start;
      merged_end = end;
    }
  }

  if (have_merged) {
    if (!flush_merged_range(merged_start, merged_end)) {
      command_processor_.EndSharedMemoryUploadBlitEncoder();
      return false;
    }
  }

  XELOGD("MetalSharedMemory::UploadRanges: Staged {} ranges to Metal buffer",
         num_upload_ranges);

  return true;
}

void MetalSharedMemory::Shutdown() {
  upload_buffer_pool_.reset();
  if (buffer_) {
    buffer_->release();
    buffer_ = nullptr;
  }

  ShutdownCommon();  // Base class cleanup
}

}  // namespace metal
}  // namespace gpu
}  // namespace xe
