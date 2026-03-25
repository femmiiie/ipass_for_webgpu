#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstring>
#include <vector>
#include <iostream>

#include "tesselation.h"

#include "TessellatorPass.h"
#include "BVParser.h"
#include "Settings.h"
#include "Utils.h"
#include "Elevation.h"

using Vertex3D = utils::Vertex3D;

TessellatorPass::TessellatorPass(Context& ctx, wgpu::Buffer ipass_levels_buf) : ComputePass(ctx)
{
  tess = new Tesselator();

  if (!tess->init(ctx.device, ctx.queue, MAX_PATCHES, ipass_levels_buf)) {
    std::cerr << "[TessellatorPass] Failed to initialise Tesselator." << std::endl;
    delete tess;
    tess = nullptr;
    return;
  }

  initialized = true;

  Settings::parser.subscribe([this](const BVParser& p) {
    this->LoadBV(p);
  });
}

TessellatorPass::~TessellatorPass()
{
  if (tess) {
    tess->terminate();
    delete tess;
    tess = nullptr;
  }
}

void TessellatorPass::LoadBV(const BVParser& parser)
{
  if (!tess || !initialized)
    return;

  const auto& patches = parser.Get();
  const auto& dims    = parser.GetDims();

  if (patches.empty()) {
    num_quads = 0;
    return;
  }

  // elevate every patch to bi-3
  std::vector<glm::vec4> bicubicControlPts;
  bicubicControlPts.reserve(patches.size() * 16);

  uint32_t count = 0;
  for (size_t pi = 0; pi < patches.size() && count < MAX_PATCHES; pi++) {
    if (patches[pi].empty()) continue;
    auto elevated = elevation::elevatePatchPositions(patches[pi], dims[pi].first, dims[pi].second);
    bicubicControlPts.insert(bicubicControlPts.end(), elevated.begin(), elevated.end());
    count++;
  }

  num_quads = count;

  if (num_quads == 0) return;

  constexpr uint32_t corner_cp_offsets[4] = {0, 12, 15, 3};
  constexpr float WELD_EPS = 1e-5f;

  std::vector<std::pair<glm::vec3, uint32_t>> vertexMap;
  uint32_t nextVertId = 0;
  std::vector<uint32_t> indices(num_quads * 4);

  for (uint32_t i = 0; i < num_quads; i++) {
    for (int c = 0; c < 4; c++) {
      glm::vec3 p(bicubicControlPts[i * 16 + corner_cp_offsets[c]]);
      uint32_t vid = nextVertId;
      bool found = false;
      for (const auto& [vpos, existingId] : vertexMap) {
        if (glm::distance(p, vpos) < WELD_EPS) {
          vid = existingId;
          found = true;
          break;
        }
      }
      if (!found) {
        vertexMap.push_back({p, nextVertId});
        vid = nextVertId;
        nextVertId++;
      }
      indices[i * 4 + c] = vid;
    }
  }

  tess->upload(bicubicControlPts.data(), indices.data(), num_quads);

  std::cout << "[TessellatorPass] Uploaded " << num_quads
            << " bicubic patch(es) for GPU tessellation." << std::endl;

  if (onGeometryReady)
    onGeometryReady(tess->get_verts_out(), GetMaxVertexCount());
}

uint32_t TessellatorPass::Execute(wgpu::CommandEncoder& encoder)
{
  if (!tess || !initialized || num_quads == 0)
    return 0;

  if (!tess->exec(encoder, num_quads)) {
    std::cerr << "[TessellatorPass] exec() failed." << std::endl;
    return 0;
  }

  return GetMaxVertexCount();
}

wgpu::Buffer TessellatorPass::GetOutputBuffer() const
{
  if (!tess) return nullptr;
  return tess->get_verts_out();
}

uint32_t TessellatorPass::GetMaxVertexCount() const
{
  return num_quads * MAX_TRIS_PER_PATCH * 3;
}
