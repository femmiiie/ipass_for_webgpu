#include "iPass.h"
#include "Utils.h"
#include "Settings.h"
#include <webgpu/webgpu.h>
#include <vector>

using Vertex3D = utils::Vertex3D;

// elevate a point from degree m to degree m+1
static std::vector<glm::vec4> elevate1D(const std::vector<glm::vec4>& pts)
{
  uint32_t m = static_cast<uint32_t>(pts.size()) - 1;
  std::vector<glm::vec4> result(m + 2);
  result[0] = pts[0];
  for (uint32_t i = 1; i <= m; i++) {
    float t  = static_cast<float>(i) / static_cast<float>(m + 1);
    result[i] = t * pts[i - 1] + (1.0f - t) * pts[i];
  }
  result[m + 1] = pts[m];
  return result;
}

// repeatedly elevate a point until it is the target degree
static std::vector<glm::vec4> elevateTo(const std::vector<glm::vec4>& pts, uint32_t targetDeg)
{
  std::vector<glm::vec4> cur = pts;
  while (static_cast<uint32_t>(cur.size()) - 1 < targetDeg)
    cur = elevate1D(cur);
  return cur;
}


static std::vector<Vertex3D> elevatePatch(
  const std::vector<Vertex3D>& patch,
  uint32_t rows, uint32_t cols)
{
  // unpack into parallel arrays
  std::vector<glm::vec4> pos_in(rows * cols);
  std::vector<glm::vec4> col_in(rows * cols);
  std::vector<glm::vec4> tex_in(rows * cols); // tex + pad
  for (uint32_t r = 0; r < rows; r++) {
    for (uint32_t c = 0; c < cols; c++) {
      const Vertex3D& v = patch[r * cols + c];
      pos_in[r * cols + c] = v.pos;
      col_in[r * cols + c] = v.color;
      tex_in[r * cols + c] = glm::vec4(v.tex, v._pad);
    }
  }

  // elevate in v direction
  std::vector<glm::vec4> pos1(rows * 4), col1(rows * 4), tex1(rows * 4);
  for (uint32_t r = 0; r < rows; r++) {
    std::vector<glm::vec4> rp(cols), rc(cols), rt(cols);
    for (uint32_t c = 0; c < cols; c++) {
      rp[c] = pos_in[r * cols + c];
      rc[c] = col_in[r * cols + c];
      rt[c] = tex_in[r * cols + c];
    }
    auto ep = elevateTo(rp, 3);
    auto ec = elevateTo(rc, 3);
    auto et = elevateTo(rt, 3);
    for (uint32_t c = 0; c < 4; c++) {
      pos1[r * 4 + c] = ep[c];
      col1[r * 4 + c] = ec[c];
      tex1[r * 4 + c] = et[c];
    }
  }

  // elevate in u direction
  std::vector<glm::vec4> pos2(16), col2(16), tex2(16);
  for (uint32_t c = 0; c < 4; c++) {
    std::vector<glm::vec4> cp(rows), cc(rows), ct(rows);
    for (uint32_t r = 0; r < rows; r++) {
      cp[r] = pos1[r * 4 + c];
      cc[r] = col1[r * 4 + c];
      ct[r] = tex1[r * 4 + c];
    }
    auto ep = elevateTo(cp, 3);
    auto ec = elevateTo(cc, 3);
    auto et = elevateTo(ct, 3);
    for (uint32_t r = 0; r < 4; r++) {
      pos2[r * 4 + c] = ep[r];
      col2[r * 4 + c] = ec[r];
      tex2[r * 4 + c] = et[r];
    }
  }

  // repack
  std::vector<Vertex3D> result(16);
  for (uint32_t i = 0; i < 16; i++) {
    result[i].pos   = pos2[i];
    result[i].color = col2[i];
    result[i].tex   = glm::vec2(tex2[i].x, tex2[i].y);
    result[i]._pad  = glm::vec2(tex2[i].z, tex2[i].w);
  }
  return result;
}

// placeholder vertex/patch counts
// resize these buffers
static constexpr uint32_t INITIAL_VERTEX_COUNT = 512;
static constexpr uint32_t INITIAL_PATCH_COUNT  = 1024;

IPass::IPass(Context& ctx) : ComputePass(ctx)
{
  wgpu::ShaderModule shaderModule = utils::LoadShader(this->context.device, "Pass/ComputePass/ipass.wgsl");
  if (!shaderModule)
  {
    throw new ComputePassException("Failed to load shader module.");
  }

  const wgpu::BufferUsage storageReadUsage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
  const wgpu::BufferUsage storageReadWriteUsage = storageReadUsage | wgpu::BufferUsage::CopySrc;

  const uint64_t verticesSize = INITIAL_VERTEX_COUNT * sizeof(Vertex3D);
  const uint64_t patchesSize  = INITIAL_PATCH_COUNT  * sizeof(float);

  this->verticesBuffer = this->CreateBuffer(verticesSize, storageReadUsage, false);
  this->patchesBuffer  = this->CreateBuffer(patchesSize,  storageReadWriteUsage, false);

  std::vector<wgpu::BindGroupEntry> storageBindings = {
    this->CreateBinding(0, this->verticesBuffer),
    this->CreateBinding(1, this->patchesBuffer),
  };

  wgpu::BindGroupLayout storageLayout = this->CreateBindGroupLayout({
    this->CreateBufferLayout(0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::ReadOnlyStorage, verticesSize),
    this->CreateBufferLayout(1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage, patchesSize),
  });

  this->storageBindGroup = this->CreateBindGroup(storageBindings, storageLayout);


  const wgpu::BufferUsage uniformUsage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;

  wgpu::BindGroupLayout uniformLayout = this->CreateBindGroupLayout({
    this->CreateBufferLayout(0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform, static_cast<uint64_t>(sizeof(glm::mat4))),
    this->CreateBufferLayout(1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform, static_cast<uint64_t>(sizeof(uint32_t))),
    this->CreateBufferLayout(2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform, static_cast<uint64_t>(sizeof(float))),
  });

  this->mvpBuffer       = this->CreateBuffer(sizeof(MVP::GPUData), uniformUsage, false);
  this->vertCountBuffer = this->CreateBuffer(sizeof(glm::u32), uniformUsage, false);
  this->pixelSizeBuffer = this->CreateBuffer(sizeof(glm::f32), uniformUsage, false);

  std::vector<wgpu::BindGroupEntry> uniformBindings = {
    this->CreateBinding(0, this->mvpBuffer),
    this->CreateBinding(1, this->vertCountBuffer),
    this->CreateBinding(2, this->pixelSizeBuffer),
  };

  this->uniformBindGroup = this->CreateBindGroup(uniformBindings, uniformLayout);

  const std::vector<wgpu::BindGroupLayout> bindGroupLayouts = { storageLayout, uniformLayout };

  wgpu::PipelineLayoutDescriptor layoutDesc;
  layoutDesc.bindGroupLayoutCount = 2;
  layoutDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout const*>(bindGroupLayouts.data());

  wgpu::PipelineLayout pipelineLayout = this->context.device.createPipelineLayout(layoutDesc);

  wgpu::ComputePipelineDescriptor pipelineDesc;
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.compute.module = shaderModule;
  pipelineDesc.compute.entryPoint = {"ipass", 5};
  this->pipeline = this->context.device.createComputePipeline(pipelineDesc);

  Settings::mvp.subscribe([this](const MVP& m) {
    glm::mat4 mvp = m.data.P * m.data.V * m.data.M;
    this->context.queue.writeBuffer(this->mvpBuffer, 0, &mvp, sizeof(glm::mat4));
  });

  Settings::parser.subscribe([this](const BVParser& p) {
    const auto& patches = p.Get();
    const auto& dims    = p.GetDims();

    // elevate to force all patches to be bi-cubic
    std::vector<Vertex3D> bicubicVerts;
    bicubicVerts.reserve(patches.size() * 16);
    for (size_t pi = 0; pi < patches.size(); pi++) {
      if (patches[pi].empty()) continue;
      auto elevated = elevatePatch(patches[pi], dims[pi].first, dims[pi].second);
      bicubicVerts.insert(bicubicVerts.end(), elevated.begin(), elevated.end());
    }

    uint32_t count = static_cast<uint32_t>(bicubicVerts.size()); // == numPatches * 16
    this->context.queue.writeBuffer(this->verticesBuffer, 0, bicubicVerts.data(), sizeof(Vertex3D) * count);
    this->context.queue.writeBuffer(this->vertCountBuffer, 0, &count, sizeof(uint32_t));
  });
}

IPass::~IPass()
{
  this->storageBindGroup.release();
  this->uniformBindGroup.release();
  this->pipeline.release();
}

wgpu::Buffer& IPass::Execute(wgpu::ComputePassEncoder& encoder)
{
  float pixelSize = 2.0f / this->context.sceneViewport.width;
  this->context.queue.writeBuffer(this->pixelSizeBuffer, 0, &pixelSize, sizeof(float));

  encoder.setPipeline(this->pipeline);
  encoder.setBindGroup(0, this->storageBindGroup, 0, nullptr);
  encoder.setBindGroup(1, this->uniformBindGroup, 0, nullptr);
  encoder.dispatchWorkgroups(256, 1, 1);
  
  return this->patchesBuffer;
}