#pragma once

#include "GPU.h"
#include "Shader.h"
#include "TessConstants.h"
#include <cstdint>


class IPass
{
public:

  IPass(wgpu::Device device, wgpu::Queue queue, uint32_t patchLimit);
  ~IPass();
  bool Execute(wgpu::CommandEncoder& encoder);

  uint32_t GetPatchCount() { return patchCount; }
  wgpu::Buffer& GetOutputBuffer() { return patchesBuffer; }

  void SetMVP(const glm::mat4& mvp);
  void SetViewportWidth(float width) { viewportWidth = width; }
  void UploadPatches(const std::vector<utils::Vertex3D>& bicubicVerts);
  void SetPatchCount(uint32_t count) { patchCount = count; }

  wgpu::Device device;
  wgpu::Queue queue;

  wgpu::ComputePipeline pipeline;

  // group 0 storage buffers
  wgpu::Buffer verticesBuffer;
  wgpu::Buffer patchesBuffer;

  // group 1 uniform buffers
  wgpu::Buffer mvpBuffer;
  wgpu::Buffer vertCountBuffer;
  wgpu::Buffer pixelSizeBuffer;

private:
  void EnsureStorageCapacity(uint32_t requiredVertices);

  wgpu::BindGroupLayout storageBindGroupLayout;
  wgpu::BindGroup storageBindGroup;
  wgpu::BindGroup uniformBindGroup;

  uint32_t patchCount = 0;

  uint32_t vertexCapacity = 0;
  uint32_t patchCapacity = 0;
  uint32_t currentVertCount = 0;
  float viewportWidth = 1.0f;
  float pixelSize     = -1.0f;
};
