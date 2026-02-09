#ifndef RENDERPASS_H_
#define RENDERPASS_H_

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Utils.h"


#include "RenderContext.h" 

class RenderPass
{
public:
  RenderPass(RenderContext& ctx) : context(ctx) {};
  virtual ~RenderPass() = default;
  virtual void Execute(wgpu::RenderPassEncoder& pass) = 0;

  RenderContext &context;

  wgpu::RenderPipeline pipeline;
  uint32_t vertexCount;
  wgpu::Buffer vertexBuffer;

  wgpu::PipelineLayout layout;
  wgpu::BindGroup bindGroup;
  wgpu::BindGroupLayout bindGroupLayout;

  wgpu::Buffer CreateBuffer(uint64_t size, wgpu::BufferUsage usage, bool mapped);

  wgpu::BindGroupLayoutEntry CreateBindingLayout(uint16_t binding, wgpu::ShaderStage visibility, uint64_t minBindingSize);
  wgpu::BindGroupLayoutEntry CreateBindingLayout(uint16_t binding, wgpu::ShaderStage visibility);
  wgpu::BindGroupLayoutEntry CreateBindingLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::SamplerBindingType type);
  wgpu::BindGroupLayout CreateBindGroupLayout(std::vector<wgpu::BindGroupLayoutEntry>& entries);

  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::Buffer& buffer, glm::u32 size);
  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::TextureView& view);
  wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::Sampler& sampler);
  wgpu::BindGroup CreateBindGroup(std::vector<wgpu::BindGroupEntry>& bindings);
};

class RenderPassException : public std::exception
{
private:
  std::string message;

public:
  RenderPassException(std::string m) : message(m) {}
  RenderPassException(const char *m) { this->message = m; }
  const char *what() const noexcept override { return this->message.c_str(); }
};

#endif