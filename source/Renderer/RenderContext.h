#pragma once

#include <glm/glm.hpp>
#include <webgpu/webgpu.hpp>

struct RenderContext
{
  wgpu::Device device;
  wgpu::Queue queue;

  wgpu::Surface surface;
  wgpu::TextureFormat surfaceFormat;

  glm::uvec2 screenSize;
};