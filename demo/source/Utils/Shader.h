#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

#include "embedded_shaders.h"

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

  static inline wgpu::ShaderModule LoadShaderFromSource(wgpu::Device& device, const std::string& source)
  {
    wgpu::ShaderModuleDescriptor shaderDesc;
    wgpu::ShaderSourceWGSL shaderCodeDesc;

    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = wgpu::SType::ShaderSourceWGSL;

    shaderDesc.nextInChain = &shaderCodeDesc.chain;
    shaderCodeDesc.code.data = source.c_str();
    shaderCodeDesc.code.length = source.length();

    return device.createShaderModule(shaderDesc);
  }

  static inline wgpu::ShaderModule LoadShader(wgpu::Device& device, std::string filepath)
  {
    static const std::unordered_map<std::string, const std::string*> embedded = {
  {"blinn_phong.wgsl",      &embedded_shaders::blinn_phong},
  {"flat.wgsl",             &embedded_shaders::flat},
  {"parametric_error.wgsl", &embedded_shaders::parametric_error},
  {"triangle_size.wgsl",    &embedded_shaders::triangle_size},
  {"ui.wgsl",               &embedded_shaders::ui},
    };

    auto pos = filepath.find_last_of("/\\");
    std::string name = (pos != std::string::npos) ? filepath.substr(pos + 1) : filepath;
    auto it = embedded.find(name);
    if (it != embedded.end())
    {
      return LoadShaderFromSource(device, *it->second);
    }

    std::ifstream fs(filepath);

    if (!fs.is_open())
    {
      std::cout << "Failed to open file " << filepath << std::endl;
      return nullptr;
    }

    std::ostringstream ss;
    ss << fs.rdbuf();
    std::string shaderSource = ss.str();

    return LoadShaderFromSource(device, shaderSource);
  }
}