#include "Pass.h"

wgpu::Buffer Pass::CreateBuffer(uint64_t size, wgpu::BufferUsage usage, bool mapped)
{
  wgpu::BufferDescriptor desc;
  desc.size = size;
  desc.usage = usage;
  desc.mappedAtCreation = mapped;
  return this->context.device.createBuffer(desc);
}

wgpu::BindGroupLayoutEntry Pass::CreateBindingLayout(uint16_t binding, wgpu::ShaderStage visibility, uint64_t minBindingSize)
{
  wgpu::BindGroupLayoutEntry entry;
  entry.binding = binding;
  entry.visibility = visibility;
  entry.buffer.type = wgpu::BufferBindingType::Uniform;
  entry.buffer.minBindingSize = minBindingSize;
  return entry;
}

wgpu::BindGroupLayoutEntry Pass::CreateBindingLayout(uint16_t binding, wgpu::ShaderStage visibility)
{
  wgpu::BindGroupLayoutEntry entry;
  entry.binding = binding;
  entry.visibility = visibility;
  entry.texture.sampleType = wgpu::TextureSampleType::Float;
  entry.texture.viewDimension = wgpu::TextureViewDimension::_2D;
  return entry;
}

wgpu::BindGroupLayoutEntry Pass::CreateBindingLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::SamplerBindingType type)
{
  wgpu::BindGroupLayoutEntry entry;
  entry.binding = binding;
  entry.visibility = visibility;
  entry.sampler.type = type;
  return entry;
}

wgpu::BindGroupLayout Pass::CreateBindGroupLayout(std::vector<wgpu::BindGroupLayoutEntry> entries)
{
  wgpu::BindGroupLayoutDescriptor desc;
  desc.entryCount = entries.size();
  desc.entries = entries.data();
  return this->context.device.createBindGroupLayout(desc);
}

wgpu::BindGroupEntry Pass::CreateBinding(uint16_t entry, wgpu::Buffer& buffer, glm::u32 size)
{
  buffer = this->CreateBuffer(
    size,
    wgpu::BufferUsage(static_cast<WGPUBufferUsage>(wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform)),
    false
  );

  wgpu::BindGroupEntry binding = wgpu::Default;
  binding.binding = entry;
  binding.buffer = buffer;
  binding.offset = 0;
  binding.size = size;
  return binding;
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

wgpu::BindGroupLayoutEntry Pass::CreateBindingLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::BufferBindingType type)
{
  wgpu::BindGroupLayoutEntry entry;
  entry.binding = binding;
  entry.visibility = visibility;
  entry.buffer.type = type;
  return entry;
}

wgpu::BindGroupEntry Pass::CreateBinding(uint16_t entry, wgpu::Buffer& buffer, uint64_t size, wgpu::BufferUsage usage)
{
  buffer = this->CreateBuffer(size, usage, false);

  wgpu::BindGroupEntry binding = wgpu::Default;
  binding.binding = entry;
  binding.buffer = buffer;
  binding.offset = 0;
  binding.size = size;
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

wgpu::BindGroup Pass::CreateBindGroup(std::vector<wgpu::BindGroupEntry> bindings, wgpu::BindGroupLayout& layout)
{
  wgpu::BindGroupDescriptor desc;
  desc.layout = layout;
  desc.entryCount = bindings.size();
  desc.entries = bindings.data();
  return this->context.device.createBindGroup(desc);
}
