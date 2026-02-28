#ifndef UTILS_H_
#define UTILS_H_

#include <cstdint>
#include <cstring>

#include <fstream>
#include <sstream>

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

  inline wgpu::ShaderModule LoadShader(wgpu::Device device, std::string filepath)
  {
    std::ifstream fs(filepath);

    if (!fs.is_open())
    {
      std::cout << "Failed to open file " << filepath << std::endl;
      return nullptr;
    }

    std::ostringstream ss;
    ss << fs.rdbuf();
    std::string shaderSource = ss.str();

    wgpu::ShaderModuleDescriptor shaderDesc;
    wgpu::ShaderSourceWGSL shaderCodeDesc;

    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = wgpu::SType::ShaderSourceWGSL;

    shaderDesc.nextInChain = &shaderCodeDesc.chain;
    shaderCodeDesc.code.data = shaderSource.c_str();
    shaderCodeDesc.code.length = shaderSource.length();

    return device.createShaderModule(shaderDesc);
  }
}

#endif