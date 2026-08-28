#pragma once

#include <algorithm>
#include <cstdint>

namespace tess {
  inline constexpr uint32_t MAX_TRIS_PER_PATCH = 8192;
  inline constexpr uint64_t BYTES_PER_PATCH    = 2 * 1024 * 1024;

  inline uint32_t ComputeMaxPatches(uint64_t maxBufferBytes)
  {
    uint64_t count = maxBufferBytes / BYTES_PER_PATCH;
    return static_cast<uint32_t>(std::max<uint64_t>(count, 1));
  }
}
