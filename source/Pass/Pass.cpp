#include "Pass.h"

wgpu::Buffer Pass::CreateBuffer(uint64_t size, wgpu::BufferUsage usage, bool mapped)
{
  wgpu::BufferDescriptor desc;
  desc.size = size;
  desc.usage = usage;
  desc.mappedAtCreation = mapped;
  return this->context.device.createBuffer(desc);
}

wgpu::BindGroupLayoutEntry Pass::CreateTextureLayout(uint16_t binding, wgpu::ShaderStage visibility)
{
  wgpu::BindGroupLayoutEntry entry;
  entry.binding = binding;
  entry.visibility = visibility;
  entry.texture.sampleType = wgpu::TextureSampleType::Float;
  entry.texture.viewDimension = wgpu::TextureViewDimension::_2D;
  return entry;
}

wgpu::BindGroupLayoutEntry Pass::CreateSamplerLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::SamplerBindingType type)
{
  wgpu::BindGroupLayoutEntry entry;
  entry.binding = binding;
  entry.visibility = visibility;
  entry.sampler.type = type;
  return entry;
}

wgpu::BindGroupLayoutEntry Pass::CreateBufferLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::BufferBindingType type, uint64_t minBindingSize)
{
  wgpu::BindGroupLayoutEntry entry;
  entry.binding = binding;
  entry.visibility = visibility;
  entry.buffer.type = type;
  entry.buffer.minBindingSize = minBindingSize;
  return entry;
}

wgpu::BindGroupLayout Pass::CreateBindGroupLayout(std::vector<wgpu::BindGroupLayoutEntry> entries)
{
  wgpu::BindGroupLayoutDescriptor desc;
  desc.entryCount = entries.size();
  desc.entries = entries.data();
  return this->context.device.createBindGroupLayout(desc);
}

wgpu::BindGroupEntry Pass::CreateBinding(uint16_t entry, wgpu::TextureView& view)
{
  wgpu::BindGroupEntry binding = wgpu::Default;
  binding.binding = entry;
  binding.textureView = view;
  return binding;
}

wgpu::BindGroupEntry Pass::CreateBinding(uint16_t entry, wgpu::Sampler& sampler)
{
  wgpu::BindGroupEntry binding = wgpu::Default;
  binding.binding = entry;
  binding.sampler = sampler;
  return binding;
}

wgpu::BindGroupEntry Pass::CreateBinding(uint16_t entry, wgpu::Buffer& buffer)
{
  wgpu::BindGroupEntry binding = wgpu::Default;
  binding.binding = entry;
  binding.buffer = buffer;
  binding.offset = 0;
  binding.size = WGPU_WHOLE_SIZE;
  return binding;
}

wgpu::BindGroup Pass::CreateBindGroup(std::vector<wgpu::BindGroupEntry> bindings)
{
  wgpu::BindGroupDescriptor desc;
  desc.layout = this->bindGroupLayout;
  desc.entryCount = bindings.size();
  desc.entries = bindings.data();
  return this->context.device.createBindGroup(desc);
}

wgpu::BindGroup Pass::CreateBindGroup(std::vector<wgpu::BindGroupEntry> bindings, wgpu::BindGroupLayout& bindLayout)
{
  wgpu::BindGroupDescriptor desc;
  desc.layout = bindLayout;
  desc.entryCount = bindings.size();
  desc.entries = bindings.data();
  return this->context.device.createBindGroup(desc);
}

void Pass::ClearBuffer(wgpu::CommandEncoder& encoder, wgpu::Buffer& buffer)
{
  const uint64_t size = buffer.getSize();
  if (size == 0) return;
  encoder.clearBuffer(buffer, 0, size);
}
