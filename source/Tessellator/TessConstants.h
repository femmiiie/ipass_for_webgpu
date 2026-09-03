#pragma once

#include <webgpu/webgpu.hpp>

#include <algorithm>
#include <cstdint>

namespace patch {
  inline constexpr uint64_t MAX_TRIS = 8192;
  inline constexpr uint64_t QUAD_BYTES         = 16 * 16;  // buf_quads: 16 vec4 control points
  inline constexpr uint64_t TESS_FACTOR_BYTES  = 4;        // buf_tess_factors: 1 float
  inline constexpr uint64_t TRI_COUNT_BYTES    = 4;        // buf_tri_counts: 1 uint32
  inline constexpr uint64_t TRI_OFFSET_BYTES   = 4;        // buf_tri_offsets: 1 uint32
  inline constexpr uint64_t CONNECTIVITY_BYTES = 2 * 16;   // buf_connectivity: 2 ivec4
  inline constexpr uint64_t VERTS_OUT_BYTES    = MAX_TRIS * 3 * 4 * 16;

  inline constexpr uint64_t BYTES_PER_PATCH =
      QUAD_BYTES + TESS_FACTOR_BYTES + TRI_COUNT_BYTES +
      TRI_OFFSET_BYTES + CONNECTIVITY_BYTES + VERTS_OUT_BYTES;

  inline uint32_t ComputeMax(uint64_t maxBufferBytes)
  {
    uint64_t count = maxBufferBytes / BYTES_PER_PATCH;
    return static_cast<uint32_t>(std::max<uint64_t>(count, 1));
  }

  inline uint32_t ResolveMax(wgpu::Device device)
  {
    wgpu::Limits limits = wgpu::Default;
    if (device.getLimits(&limits))
      return ComputeMax(std::min<uint64_t>(limits.maxBufferSize, limits.maxStorageBufferBindingSize));

    return 64;
  }
}
