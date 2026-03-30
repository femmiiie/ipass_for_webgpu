#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>

#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace utils
{
  struct Vertex3D
  {
    glm::vec4 pos;
    glm::vec4 color;
    glm::vec2 tex;
    glm::vec2 _pad; // padding
  };

  struct Vertex2D
  {
    glm::vec2 pos;
    glm::vec2 tex;
    glm::vec4 color;
  };

  inline void SetEntryPoint(WGPUStringView& entryPoint, const char* name)
  {
    entryPoint.data = name;
    entryPoint.length = strlen(name);
  }

  template <typename T>
  uint32_t aligned_size(const T& value, uint32_t alignment = 256)
  {
    (void)value; //removes unused parameter warning
    uint32_t size = sizeof(T);
    return ((size + alignment - 1) / alignment) * alignment;
  }

  inline wgpu::ShaderModule LoadShader(wgpu::Device& device, std::string filepath)
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