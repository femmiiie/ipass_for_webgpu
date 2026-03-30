#pragma once

#include <webgpu/webgpu.hpp>

struct GPUContext
{
  wgpu::Device device;
  wgpu::Queue queue;
};
