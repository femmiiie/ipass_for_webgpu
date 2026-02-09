#ifndef UTILS_H_
#define UTILS_H_

#include <cstdint>
#include <cstring>
#include <webgpu/webgpu.hpp>

namespace utils
{
  inline void SetEntryPoint(WGPUStringView& entryPoint, const char* name)
  {
    entryPoint.data = name;
    entryPoint.length = strlen(name);
  }
  inline void InitLabel(WGPUStringView& label) { label = WGPU_STRING_VIEW_INIT; }
  template <typename T>
  uint32_t aligned_size(const T& value, uint32_t alignment = 256)
  {
    (void)value; //removes unused parameter warning
    uint32_t size = sizeof(T);
    return ((size + alignment - 1) / alignment) * alignment;
  }
}

#endif