#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstring>
#include <vector>
#include <iostream>

#include "Tessellator.h"

#include "TessellatorPass.h"

TessellatorPass::TessellatorPass(GPUContext& ctx, wgpu::Buffer ipass_levels_buf, uint32_t patchLimit) 
  : ComputePass(ctx), maxPatchLimit(patchLimit)
{
  tess = new Tessellator(ctx);
  if (!tess->Init(this->maxPatchLimit, ipass_levels_buf)) {
    std::cerr << "[TessellatorPass] Failed to initialise Tessellator." << std::endl;
    delete tess;
    tess = nullptr;
    return;
  }

  initialized = true;
}

TessellatorPass::~TessellatorPass()
{
  if (tess) {
    tess->Terminate();
    delete tess;
    tess = nullptr;
  }
}

void TessellatorPass::Load(const std::vector<glm::vec4>& controlPoints,
                           const std::vector<uint32_t>& cornerIndices)
{
  if (!tess || !initialized)
    return;

  const uint32_t availablePatches = static_cast<uint32_t>(cornerIndices.size() / 4);
  num_quads = std::min(availablePatches, this->maxPatchLimit);

  if (num_quads == 0) {
    tess->ClearBuffers();
    return;
  }

  tess->Upload(controlPoints.data(), cornerIndices.data(), num_quads);

  std::cout << "[TessellatorPass] Uploaded " << num_quads
            << " bicubic patch(es) for GPU tessellation." << std::endl;
}

void TessellatorPass::Execute(wgpu::CommandEncoder& encoder)
{
  if (!tess || !initialized || num_quads == 0) { return; }

  if (!tess->Execute(encoder, num_quads)) {
    std::cerr << "[TessellatorPass] Execute() failed." << std::endl;
  }
}

wgpu::Buffer TessellatorPass::GetOutputBuffer() const
{
  return tess ? tess->GetVertexOutput() : nullptr;
}

wgpu::Buffer TessellatorPass::GetControlPointBuffer() const
{
  return tess ? tess->GetControlPointBuffer() : nullptr;
}

wgpu::Buffer TessellatorPass::GetTriCountBuffer() const
{
  return tess ? tess->GetTriCountBuffer() : nullptr;
}

uint32_t TessellatorPass::GetMaxVertexCount() const
{
  return num_quads * tess::MAX_TRIS_PER_PATCH * 3;
}
