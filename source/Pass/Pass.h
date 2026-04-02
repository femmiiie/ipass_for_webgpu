#pragma once

#include "Shader.h"
#include "GPUContext.h"

#include <vector>
#include <string>
#include <exception>


class Pass
{
public:
  Pass(GPUContext& ctx) : context(ctx) {}
  virtual ~Pass() = default;

  GPUContext& context;

  wgpu::PipelineLayout layout;
  wgpu::BindGroup bindGroup;
  wgpu::BindGroupLayout bindGroupLayout;

  wgpu::Buffer CreateBuffer(uint64_t size, wgpu::BufferUsage usage, bool mapped = false);

  wgpu::BindGroupLayoutEntry CreateTextureLayout(uint16_t binding, wgpu::ShaderStage visibility);
  wgpu::BindGroupLayoutEntry CreateSamplerLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::SamplerBindingType type);
  wgpu::BindGroupLayoutEntry CreateBufferLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::BufferBindingType type, uint64_t minBindingSize);
  wgpu::BindGroupLayout CreateBindGroupLayout(std::vector<wgpu::BindGroupLayoutEntry> entries);

  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::Buffer& buffer);
  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::TextureView& view);
  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::Sampler& sampler);
  wgpu::BindGroup CreateBindGroup(std::vector<wgpu::BindGroupEntry> bindings);
  wgpu::BindGroup CreateBindGroup(std::vector<wgpu::BindGroupEntry> bindings, wgpu::BindGroupLayout& layout);

  void ClearBuffer(wgpu::CommandEncoder& encoder, wgpu::Buffer& buffer);
};


class PassException : public std::exception
{
private:
  std::string message;

public:
  PassException(std::string m) : message(m) {}
  PassException(const char* m) { this->message = m; }
  const char* what() const noexcept override { return this->message.c_str(); }
};
