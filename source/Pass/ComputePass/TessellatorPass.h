#pragma once

#include "Context.h"

#include <webgpu/webgpu.hpp>
#include <cstdint>
#include <functional>

class Tesselator;
class BVParser;

//hacky class to make for an easy connection to the rest of the renderer
//need to make this extend ComputePass, and either contain each pass entirely within
//or as a list of ComputePasses that get called on execute

//also, moving shader code out to separate files

class TessellatorPass
{
public:
  static constexpr uint32_t MAX_PATCHES       = 128;
  static constexpr uint32_t MAX_TRIS_PER_PATCH = 8192;

  TessellatorPass(Context& ctx, wgpu::Buffer ipass_levels_buf);
  ~TessellatorPass();

  void LoadBV(const BVParser& parser);

  uint32_t Execute(wgpu::CommandEncoder& encoder);

  wgpu::Buffer GetOutputBuffer() const;

  uint32_t GetMaxVertexCount() const;

  std::function<void(wgpu::Buffer, uint32_t)> onGeometryReady;

private:
  Context&    context;
  Tesselator* tess = nullptr;

  uint32_t num_quads    = 0;  // actual patch count after last LoadBV
  bool     initialized  = false;
};
