#pragma once

#include "GPUContext.h"
#include "ComputePass.h"
#include "TessConstants.h"
#include "Shader.h"

#include <cstdint>
#include <functional>
#include <vector>
#include <utility>

#include <glm/glm.hpp>

class Tessellator;
using Patch = std::vector<utils::Vertex3D>;

class TessellatorPass : public ComputePass
{
public:
  TessellatorPass(GPUContext& ctx, wgpu::Buffer ipass_levels_buf, uint32_t patchLimit);
  ~TessellatorPass();

  void Load(const std::vector<utils::Vertex3D>& bicubicVerts,
            const std::vector<Patch>& patches,
            const std::vector<std::pair<glm::u32, glm::u32>>& dims);

  void Execute(wgpu::CommandEncoder& encoder) override;

  wgpu::Buffer GetOutputBuffer() const;
  wgpu::Buffer GetControlPointBuffer() const;
  wgpu::Buffer GetTriCountBuffer() const;
  uint32_t GetMaxVertexCount() const;
  uint32_t GetPatchCount() const { return num_quads; }

private:
  Tessellator* tess = nullptr;

  uint32_t maxPatchLimit = 64; // fallback if device limit query fails
  uint32_t num_quads    = 0;  // actual patch count after last LoadBV
  bool     initialized  = false;
};
