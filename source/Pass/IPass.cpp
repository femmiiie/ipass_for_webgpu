#include "IPass.h"
#include "Shader.h"
#include <webgpu/webgpu.h>
#include <stdexcept>
#include <vector>

using Vertex3D = utils::Vertex3D;

// placeholder vertex/patch counts
// resize these buffers
static constexpr uint32_t INITIAL_VERTEX_COUNT = 512;
static constexpr uint32_t VERTS_PER_PATCH = 16;
static constexpr uint32_t IPASS_WORKGROUP_SIZE = 256;

void IPass::EnsureStorageCapacity(uint32_t requiredVertices)
{
  if (requiredVertices <= this->vertexCapacity)
    return;

  auto nextPow2 = [](uint32_t v) {
    if (v == 0) return 1u;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1;
  };

  this->vertexCapacity = std::max(this->vertexCapacity, nextPow2(requiredVertices));

  const wgpu::BufferUsage storageReadUsage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;

  const uint64_t resizedVerticesSize = static_cast<uint64_t>(this->vertexCapacity) * sizeof(Vertex3D);

  this->storageBindGroup.release();
  this->verticesBuffer.destroy();

  this->verticesBuffer = utils::CreateBuffer(this->device, resizedVerticesSize, storageReadUsage, false);

  std::vector<wgpu::BindGroupEntry> resizedStorageBindings = {
    utils::CreateBinding(0, this->verticesBuffer),
    utils::CreateBinding(1, this->patchesBuffer),
  };
  this->storageBindGroup = utils::CreateBindGroup(this->device, resizedStorageBindings, this->storageBindGroupLayout);
}

IPass::IPass(wgpu::Device device, wgpu::Queue queue, uint32_t patchLimit)
  : device(device), queue(queue), patchCapacity(patchLimit)
{
  wgpu::ShaderModule shaderModule = utils::LoadShader(this->device, "Pass/ipass.wgsl");
  if (!shaderModule)
  {
    throw std::runtime_error("Failed to load shader module.");
  }

  const wgpu::BufferUsage storageReadUsage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
  const wgpu::BufferUsage storageReadWriteUsage = storageReadUsage | wgpu::BufferUsage::CopySrc;

  this->vertexCapacity = INITIAL_VERTEX_COUNT;

  const uint64_t verticesSize = static_cast<uint64_t>(this->vertexCapacity) * sizeof(Vertex3D);
  const uint64_t patchesSize  = static_cast<uint64_t>(this->patchCapacity)  * sizeof(float);

  this->verticesBuffer = utils::CreateBuffer(this->device, verticesSize, storageReadUsage, false);
  this->patchesBuffer  = utils::CreateBuffer(this->device, patchesSize,  storageReadWriteUsage, false);

  std::vector<wgpu::BindGroupEntry> storageBindings = {
    utils::CreateBinding(0, this->verticesBuffer),
    utils::CreateBinding(1, this->patchesBuffer),
  };

  this->storageBindGroupLayout = utils::CreateBindGroupLayout(this->device, {
    utils::CreateBufferLayout(0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::ReadOnlyStorage, verticesSize),
    utils::CreateBufferLayout(1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage, patchesSize),
  });

  this->storageBindGroup = utils::CreateBindGroup(this->device, storageBindings, this->storageBindGroupLayout);


  const wgpu::BufferUsage uniformUsage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;

  wgpu::BindGroupLayout uniformLayout = utils::CreateBindGroupLayout(this->device, {
    utils::CreateBufferLayout(0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform, static_cast<uint64_t>(sizeof(glm::mat4))),
    utils::CreateBufferLayout(1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform, static_cast<uint64_t>(sizeof(uint32_t))),
    utils::CreateBufferLayout(2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform, static_cast<uint64_t>(sizeof(float))),
  });

  this->mvpBuffer       = utils::CreateBuffer(this->device, sizeof(glm::mat4), uniformUsage, false);
  this->vertCountBuffer = utils::CreateBuffer(this->device, sizeof(glm::u32), uniformUsage, false);
  this->pixelSizeBuffer = utils::CreateBuffer(this->device, sizeof(glm::f32), uniformUsage, false);

  std::vector<wgpu::BindGroupEntry> uniformBindings = {
    utils::CreateBinding(0, this->mvpBuffer),
    utils::CreateBinding(1, this->vertCountBuffer),
    utils::CreateBinding(2, this->pixelSizeBuffer),
  };

  this->uniformBindGroup = utils::CreateBindGroup(this->device, uniformBindings, uniformLayout);

  const std::vector<wgpu::BindGroupLayout> bindGroupLayouts = { this->storageBindGroupLayout, uniformLayout };

  wgpu::PipelineLayoutDescriptor layoutDesc;
  layoutDesc.bindGroupLayoutCount = 2;
  layoutDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout const*>(bindGroupLayouts.data());

  wgpu::PipelineLayout pipelineLayout = this->device.createPipelineLayout(layoutDesc);

  wgpu::ComputePipelineDescriptor pipelineDesc;
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.compute.module = shaderModule;
  pipelineDesc.compute.entryPoint = {"ipass", 5};
  this->pipeline = this->device.createComputePipeline(pipelineDesc);
}

void IPass::SetMVP(const glm::mat4& mvp)
{
  this->queue.writeBuffer(this->mvpBuffer, 0, &mvp, sizeof(glm::mat4));
}

void IPass::UploadPatches(const std::vector<Vertex3D>& bicubicVerts)
{
  uint32_t count = static_cast<uint32_t>(bicubicVerts.size());
  uint32_t patchCount = (count + VERTS_PER_PATCH - 1) / VERTS_PER_PATCH;

  if (patchCount > this->patchCapacity)
  {
    count = this->patchCapacity * VERTS_PER_PATCH;
    patchCount = this->patchCapacity;
    std::cerr << "[IPass] Clamped patches to " << this->patchCapacity
              << " to match the fixed tessellation levels buffer capacity." << std::endl;
  }

  this->currentVertCount = count;
  this->EnsureStorageCapacity(count);

  if (count > 0)
    this->queue.writeBuffer(this->verticesBuffer, 0, bicubicVerts.data(), sizeof(Vertex3D) * count);
  this->queue.writeBuffer(this->vertCountBuffer, 0, &count, sizeof(uint32_t));
}

IPass::~IPass()
{
  this->storageBindGroupLayout.release();
  this->storageBindGroup.release();
  this->uniformBindGroup.release();
  this->pipeline.release();
}

bool IPass::Execute(wgpu::CommandEncoder& encoder)
{
  if (this->currentVertCount == 0) return true;

  float pSize = 2.0f / this->viewportWidth;
  if (pSize != this->pixelSize)
  {
    this->pixelSize = pSize;
    this->queue.writeBuffer(this->pixelSizeBuffer, 0, &pixelSize, sizeof(float));
  }

  wgpu::ComputePassDescriptor desc;
  desc.timestampWrites = nullptr;
  wgpu::ComputePassEncoder pass = encoder.beginComputePass(desc);

  pass.setPipeline(this->pipeline);
  pass.setBindGroup(0, this->storageBindGroup, 0, nullptr);
  pass.setBindGroup(1, this->uniformBindGroup, 0, nullptr);
  uint32_t workgroups = (this->currentVertCount + IPASS_WORKGROUP_SIZE - 1) / IPASS_WORKGROUP_SIZE;
  pass.dispatchWorkgroups(workgroups, 1, 1);

  pass.end();
  pass.release();
  return true;
}
