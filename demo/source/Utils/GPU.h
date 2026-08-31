#pragma once

#include <webgpu/webgpu.hpp>

#include <vector>

namespace utils
{
  static inline wgpu::Buffer CreateBuffer(wgpu::Device device, uint64_t size, wgpu::BufferUsage usage, bool mapped = false)
  {
    wgpu::BufferDescriptor desc;
    desc.size = size;
    desc.usage = usage;
    desc.mappedAtCreation = mapped;
    return device.createBuffer(desc);
  }

  static inline wgpu::BindGroupLayoutEntry CreateTextureLayout(uint16_t binding, wgpu::ShaderStage visibility)
  {
    wgpu::BindGroupLayoutEntry entry;
    entry.binding = binding;
    entry.visibility = visibility;
    entry.texture.sampleType = wgpu::TextureSampleType::Float;
    entry.texture.viewDimension = wgpu::TextureViewDimension::_2D;
    return entry;
  }

  static inline wgpu::BindGroupLayoutEntry CreateSamplerLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::SamplerBindingType type)
  {
    wgpu::BindGroupLayoutEntry entry;
    entry.binding = binding;
    entry.visibility = visibility;
    entry.sampler.type = type;
    return entry;
  }

  static inline wgpu::BindGroupLayoutEntry CreateBufferLayout(uint16_t binding, wgpu::ShaderStage visibility, wgpu::BufferBindingType type, uint64_t minBindingSize)
  {
    wgpu::BindGroupLayoutEntry entry;
    entry.binding = binding;
    entry.visibility = visibility;
    entry.buffer.type = type;
    entry.buffer.minBindingSize = minBindingSize;
    return entry;
  }

  static inline wgpu::BindGroupLayout CreateBindGroupLayout(wgpu::Device device, std::vector<wgpu::BindGroupLayoutEntry> entries)
  {
    wgpu::BindGroupLayoutDescriptor desc;
    desc.entryCount = entries.size();
    desc.entries = entries.data();
    return device.createBindGroupLayout(desc);
  }

  static inline wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::TextureView& view)
  {
    wgpu::BindGroupEntry binding = wgpu::Default;
    binding.binding = entry;
    binding.textureView = view;
    return binding;
  }

  static inline wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::Sampler& sampler)
  {
    wgpu::BindGroupEntry binding = wgpu::Default;
    binding.binding = entry;
    binding.sampler = sampler;
    return binding;
  }

  static inline wgpu::BindGroupEntry CreateBinding(uint16_t entry, wgpu::Buffer& buffer)
  {
    wgpu::BindGroupEntry binding = wgpu::Default;
    binding.binding = entry;
    binding.buffer = buffer;
    binding.offset = 0;
    binding.size = WGPU_WHOLE_SIZE;
    return binding;
  }

  static inline wgpu::BindGroup CreateBindGroup(wgpu::Device device, std::vector<wgpu::BindGroupEntry> bindings, wgpu::BindGroupLayout& bindLayout)
  {
    wgpu::BindGroupDescriptor desc;
    desc.layout = bindLayout;
    desc.entryCount = bindings.size();
    desc.entries = bindings.data();
    return device.createBindGroup(desc);
  }

  static inline void ClearBuffer(wgpu::CommandEncoder& encoder, wgpu::Buffer& buffer)
  {
    const uint64_t size = buffer.getSize();
    if (size == 0) return;
    encoder.clearBuffer(buffer, 0, size);
  }
}
