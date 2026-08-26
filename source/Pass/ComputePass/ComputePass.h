#pragma once

#include "Shader.h"
#include "GPUContext.h"

#include <vector>
#include <string>
#include <exception>


class ComputePass
{
public:
  ComputePass(GPUContext& ctx) : context(ctx) {}
  virtual ~ComputePass() = default;
  virtual bool Execute(wgpu::CommandEncoder& encoder) = 0;

  GPUContext& context;

  wgpu::ComputePipeline pipeline;

  wgpu::Buffer CreateBuffer(uint64_t size, wgpu::BufferUsage usage, bool mapped = false);

  wgpu::BindGroupLayoutEntry CreateTextureLayout(uint16_t binding, wgpu::ShaderStage visibility);
  wgpu::BindGroupLayoutEntry CreateSamplerLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::SamplerBindingType type);
  wgpu::BindGroupLayoutEntry CreateBufferLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::BufferBindingType type, uint64_t minBindingSize);
  wgpu::BindGroupLayout CreateBindGroupLayout(std::vector<wgpu::BindGroupLayoutEntry> entries);

  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::Buffer& buffer);
  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::TextureView& view);
  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::Sampler& sampler);
  wgpu::BindGroup CreateBindGroup(std::vector<wgpu::BindGroupEntry> bindings, wgpu::BindGroupLayout& layout);

  void ClearBuffer(wgpu::CommandEncoder& encoder, wgpu::Buffer& buffer);
};

class ComputePassException : public std::exception
{
private:
  std::string message;

public:
  ComputePassException(std::string m) : message(m) {}
  ComputePassException(const char* m) { this->message = m; }
  const char* what() const noexcept override { return this->message.c_str(); }
};
