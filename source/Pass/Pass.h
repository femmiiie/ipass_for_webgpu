#ifndef PASS_H_
#define PASS_H_

#include "Utils.h"
#include "Context.h"

#include <vector>
#include <string>
#include <exception>


class Pass
{
public:
  Pass(Context& ctx) : context(ctx) {}
  virtual ~Pass() = default;

  Context& context;

  wgpu::PipelineLayout layout;
  wgpu::BindGroup bindGroup;
  wgpu::BindGroupLayout bindGroupLayout;

  wgpu::Buffer CreateBuffer(uint64_t size, wgpu::BufferUsage usage, bool mapped);

  wgpu::BindGroupLayoutEntry CreateBindingLayout(uint16_t binding, wgpu::ShaderStage visibility, uint64_t minBindingSize);
  wgpu::BindGroupLayoutEntry CreateBindingLayout(uint16_t binding, wgpu::ShaderStage visibility);
  wgpu::BindGroupLayoutEntry CreateBindingLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::SamplerBindingType type);
  wgpu::BindGroupLayoutEntry CreateBindingLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::BufferBindingType type);
  wgpu::BindGroupLayout CreateBindGroupLayout(std::vector<wgpu::BindGroupLayoutEntry> entries);

  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::Buffer& buffer, glm::u32 size);
  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::Buffer& buffer, uint64_t size, wgpu::BufferUsage usage);
  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::TextureView& view);
  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::Sampler& sampler);
  wgpu::BindGroup CreateBindGroup(std::vector<wgpu::BindGroupEntry> bindings);
  wgpu::BindGroup CreateBindGroup(std::vector<wgpu::BindGroupEntry> bindings, wgpu::BindGroupLayout& layout);
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

#endif
