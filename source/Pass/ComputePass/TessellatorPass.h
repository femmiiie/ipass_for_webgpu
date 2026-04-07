#pragma once

#include "GPUContext.h"
#include "ComputePass.h"
#include "TessConstants.h"

#include <cstdint>
#include <functional>

class Tessellator;
class BVParser;

class TessellatorPass : public ComputePass
{
public:
  TessellatorPass(GPUContext& ctx, wgpu::Buffer ipass_levels_buf, uint32_t patchLimit);
  ~TessellatorPass();

  void LoadBV(const BVParser& parser);

  void Execute(wgpu::CommandEncoder& encoder) override;

  wgpu::Buffer GetOutputBuffer() const;
  wgpu::Buffer GetControlPointBuffer() const;
  uint32_t GetMaxVertexCount() const;

private:
  Tessellator* tess = nullptr;

  uint32_t maxPatchLimit = 64; // fallback if device limit query fails
  uint32_t num_quads    = 0;  // actual patch count after last LoadBV
  bool     initialized  = false;
};
