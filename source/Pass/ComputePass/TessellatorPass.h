#pragma once

#include "ComputePass.h"
#include "TessConstants.h"

#include <cstdint>
#include <functional>

class Tessellator;
class BVParser;

class TessellatorPass : public ComputePass
{
public:
  static constexpr uint32_t DEFAULT_PATCH_LIMIT = 64;
  static_assert(DEFAULT_PATCH_LIMIT <= tess::MAX_PATCHES);

  TessellatorPass(GPUContext& ctx, wgpu::Buffer ipass_levels_buf);
  ~TessellatorPass();

  void LoadBV(const BVParser& parser);

  void Execute(wgpu::CommandEncoder& encoder) override;

  wgpu::Buffer GetOutputBuffer() const;

  uint32_t GetMaxVertexCount() const;

private:
  Tessellator* tess = nullptr;

  uint32_t num_quads    = 0;  // actual patch count after last LoadBV
  bool     initialized  = false;
};
