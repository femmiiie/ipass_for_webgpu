#pragma once

#include <webgpu/webgpu.hpp>

#include <algorithm>
#include <cstdint>

namespace patch {
  inline constexpr uint64_t MAX_TRIS   = 8192;
  inline constexpr uint64_t CHUNK_TRIS = 64;                     // triangles per Draw instance, must match the shaders
  inline constexpr uint64_t MAX_CHUNKS = MAX_TRIS / CHUNK_TRIS;  // chunk index is packed in 7 bits
  inline constexpr uint32_t MAX_SCAN   = 256 * 256;              // two level prefix scan capacity

  // Tessellator base
  inline constexpr uint64_t QUAD_BYTES         = 16 * 16;  // buf_quads: 16 vec4 control points
  inline constexpr uint64_t TESS_FACTOR_BYTES  = 4;        // buf_tess_factors: 1 float
  inline constexpr uint64_t CONNECTIVITY_BYTES = 2 * 16;   // buf_connectivity: 2 ivec4

  // ComputeTessellator
  inline constexpr uint64_t TRI_COUNT_BYTES    = 4;        // buf_tri_counts: 1 uint32
  inline constexpr uint64_t TRI_OFFSET_BYTES   = 4;        // buf_tri_offsets: 1 uint32
  inline constexpr uint64_t VERTS_OUT_BYTES    = MAX_TRIS * 3 * 3 * 16;

  // VertexTessellator
  inline constexpr uint64_t PATCH_INFO_BYTES   = 8;              // buf_patch_info: packed levels + tri count
  inline constexpr uint64_t CHUNK_COUNT_BYTES  = 4;              // buf_chunk_counts: 1 uint32
  inline constexpr uint64_t CHUNK_OFFSET_BYTES = 4;              // buf_chunk_offsets: 1 uint32
  inline constexpr uint64_t CHUNKS_BYTES       = MAX_CHUNKS * 4; // buf_chunks: worst case chunk records

  // total bytes per patch of each user of the patch data
  inline constexpr uint64_t INPUT_BYTES    = QUAD_BYTES + TESS_FACTOR_BYTES + CONNECTIVITY_BYTES;
  inline constexpr uint64_t GEOMETRY_BYTES = INPUT_BYTES + TRI_COUNT_BYTES + TRI_OFFSET_BYTES + VERTS_OUT_BYTES;
  inline constexpr uint64_t DIRECT_BYTES   = INPUT_BYTES + PATCH_INFO_BYTES + CHUNK_COUNT_BYTES + CHUNK_OFFSET_BYTES + CHUNKS_BYTES;


  inline uint32_t ResolveMax(wgpu::Device device, uint32_t max_patches, uint64_t bytesPerPatch)
  {
    if (max_patches) {
      return std::min(max_patches, MAX_SCAN);
    }

    wgpu::Limits limits = wgpu::Default;
    if (device.getLimits(&limits)) {
      uint64_t maxBufBytes = std::min(limits.maxBufferSize, limits.maxStorageBufferBindingSize);
      uint64_t count = std::clamp<uint64_t>(maxBufBytes / bytesPerPatch, 1, MAX_SCAN);
      return static_cast<uint32_t>(count);
    }

    return 64;
  }
}
