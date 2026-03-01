#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <webgpu/webgpu.hpp>

struct RenderContext
{
  wgpu::Device device;
  wgpu::Queue queue;

  wgpu::Surface surface;
  wgpu::TextureFormat surfaceFormat;

  glm::uvec2 screenSize;
};