#include "IPass.h"
#include "Shader.h"
#include <webgpu/webgpu.h>
#include <vector>

using Vertex3D = utils::Vertex3D;

// placeholder vertex/patch counts
// resize these buffers
static constexpr uint32_t INITIAL_VERTEX_COUNT = 512;
static constexpr uint32_t INITIAL_PATCH_COUNT  = 1024;
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

  this->verticesBuffer = this->CreateBuffer(resizedVerticesSize, storageReadUsage, false);

  std::vector<wgpu::BindGroupEntry> resizedStorageBindings = {
    this->CreateBinding(0, this->verticesBuffer),
    this->CreateBinding(1, this->patchesBuffer),
  };
  this->storageBindGroup = this->CreateBindGroup(resizedStorageBindings, this->storageBindGroupLayout);
}

IPass::IPass(GPUContext& ctx) : ComputePass(ctx)
{
  wgpu::ShaderModule shaderModule = utils::LoadShader(this->context.device, "Pass/ComputePass/ipass.wgsl");
  if (!shaderModule)
  {
    throw new ComputePassException("Failed to load shader module.");
  }

  const wgpu::BufferUsage storageReadUsage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
  const wgpu::BufferUsage storageReadWriteUsage = storageReadUsage | wgpu::BufferUsage::CopySrc;

  this->vertexCapacity = INITIAL_VERTEX_COUNT;
  this->patchCapacity = std::max(INITIAL_PATCH_COUNT, tess::MAX_PATCHES);

  const uint64_t verticesSize = static_cast<uint64_t>(this->vertexCapacity) * sizeof(Vertex3D);
  const uint64_t patchesSize  = static_cast<uint64_t>(this->patchCapacity)  * sizeof(float);

  this->verticesBuffer = this->CreateBuffer(verticesSize, storageReadUsage, false);
  this->patchesBuffer  = this->CreateBuffer(patchesSize,  storageReadWriteUsage, false);

  std::vector<wgpu::BindGroupEntry> storageBindings = {
    this->CreateBinding(0, this->verticesBuffer),
    this->CreateBinding(1, this->patchesBuffer),
  };

  this->storageBindGroupLayout = this->CreateBindGroupLayout({
    this->CreateBufferLayout(0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::ReadOnlyStorage, verticesSize),
    this->CreateBufferLayout(1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage, patchesSize),
  });

  this->storageBindGroup = this->CreateBindGroup(storageBindings, this->storageBindGroupLayout);


  const wgpu::BufferUsage uniformUsage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;

  wgpu::BindGroupLayout uniformLayout = this->CreateBindGroupLayout({
    this->CreateBufferLayout(0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform, static_cast<uint64_t>(sizeof(glm::mat4))),
    this->CreateBufferLayout(1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform, static_cast<uint64_t>(sizeof(uint32_t))),
    this->CreateBufferLayout(2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform, static_cast<uint64_t>(sizeof(float))),
  });

  this->mvpBuffer       = this->CreateBuffer(sizeof(glm::mat4), uniformUsage, false);
  this->vertCountBuffer = this->CreateBuffer(sizeof(glm::u32), uniformUsage, false);
  this->pixelSizeBuffer = this->CreateBuffer(sizeof(glm::f32), uniformUsage, false);

  std::vector<wgpu::BindGroupEntry> uniformBindings = {
    this->CreateBinding(0, this->mvpBuffer),
    this->CreateBinding(1, this->vertCountBuffer),
    this->CreateBinding(2, this->pixelSizeBuffer),
  };

  this->uniformBindGroup = this->CreateBindGroup(uniformBindings, uniformLayout);

  const std::vector<wgpu::BindGroupLayout> bindGroupLayouts = { this->storageBindGroupLayout, uniformLayout };

  wgpu::PipelineLayoutDescriptor layoutDesc;
  layoutDesc.bindGroupLayoutCount = 2;
  layoutDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout const*>(bindGroupLayouts.data());

  wgpu::PipelineLayout pipelineLayout = this->context.device.createPipelineLayout(layoutDesc);

  wgpu::ComputePipelineDescriptor pipelineDesc;
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.compute.module = shaderModule;
  pipelineDesc.compute.entryPoint = {"ipass", 5};
  this->pipeline = this->context.device.createComputePipeline(pipelineDesc);
}

void IPass::SetMVP(const glm::mat4& mvp)
{
  this->context.queue.writeBuffer(this->mvpBuffer, 0, &mvp, sizeof(glm::mat4));
}

void IPass::UploadVertices(const std::vector<Vertex3D>& bicubicVerts)
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
    this->context.queue.writeBuffer(this->verticesBuffer, 0, bicubicVerts.data(), sizeof(Vertex3D) * count);
  this->context.queue.writeBuffer(this->vertCountBuffer, 0, &count, sizeof(uint32_t));
}

IPass::~IPass()
{
  this->storageBindGroupLayout.release();
  this->storageBindGroup.release();
  this->uniformBindGroup.release();
  this->pipeline.release();
}

void IPass::Execute(wgpu::CommandEncoder& encoder)
{
  if (this->currentVertCount == 0) return;

  float pixelSize = 2.0f / this->viewportWidth;
  this->context.queue.writeBuffer(this->pixelSizeBuffer, 0, &pixelSize, sizeof(float));

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
}