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