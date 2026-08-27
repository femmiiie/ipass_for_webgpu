#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>

#ifdef __EMSCRIPTEN__
#include <unordered_map>
#include "embedded_shaders.h"
#endif

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

  inline wgpu::ShaderModule LoadShaderFromSource(wgpu::Device& device, const std::string& source)
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

  inline wgpu::ShaderModule LoadShader(wgpu::Device& device, std::string filepath)
  {
#ifdef __EMSCRIPTEN__
    static const std::unordered_map<std::string, const std::string*> embedded = {
  {"ipass.wgsl",     &embedded_shaders::ipass},
  {"tess-calc.wgsl", &embedded_shaders::tess_calc},
  {"tess-scan.wgsl", &embedded_shaders::tess_scan},
  {"tess-gen.wgsl",  &embedded_shaders::tess_gen},
    };

    auto pos = filepath.find_last_of("/\\");
    std::string name = (pos != std::string::npos) ? filepath.substr(pos + 1) : filepath;
    auto it = embedded.find(name);
    if (it != embedded.end())
    {
      return LoadShaderFromSource(device, *it->second);
    }
#endif

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
