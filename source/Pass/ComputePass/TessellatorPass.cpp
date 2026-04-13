#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <iostream>

#include "Tessellator.h"

#include "TessellatorPass.h"

using Vertex3D = utils::Vertex3D;

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

void TessellatorPass::Load(const std::vector<utils::Vertex3D>& bicubicVerts,
                           const std::vector<Patch>& patches,
                           const std::vector<std::pair<glm::u32, glm::u32>>& dims)
{
  if (!tess || !initialized)
    return;

  if (patches.empty()) {
    tess->ClearBuffers();
    num_quads = 0;
    return;
  }

  constexpr float WELD_EPS = 1e-5f;
  const float invEps = 1.0f / WELD_EPS;
  auto quantise = [&](glm::vec3 p) -> size_t {
    int64_t xi = static_cast<int64_t>(std::round(p.x * invEps));
    int64_t yi = static_cast<int64_t>(std::round(p.y * invEps));
    int64_t zi = static_cast<int64_t>(std::round(p.z * invEps));
    size_t h = (size_t)14695981039346656037ull; //FNV-1a hash for O(1) lookup
    auto mix = [&](int64_t v) { h ^= static_cast<size_t>(v); h *= 1099511628211ull; };
    mix(xi); mix(yi); mix(zi);
    return h;
  };

  std::vector<uint32_t> indices;
  indices.reserve(patches.size() * 4);
  std::unordered_map<size_t, uint32_t> vertexMap;
  vertexMap.reserve(patches.size() * 4);
  uint32_t nextVertId = 0;
  uint32_t count = 0;

  for (size_t pi = 0; pi < patches.size() && count < this->maxPatchLimit; pi++) {
    if (patches[pi].empty()) continue;

    const auto& patch = patches[pi];
    const uint32_t rows = dims[pi].first;
    const uint32_t cols = dims[pi].second;

    const glm::vec3 corners[4] = {
      glm::vec3(patch[0].pos),
      glm::vec3(patch[(rows - 1) * cols].pos),
      glm::vec3(patch[(rows - 1) * cols + (cols - 1)].pos),
      glm::vec3(patch[cols - 1].pos),
    };
    for (const glm::vec3& p : corners) {
      auto [it, inserted] = vertexMap.emplace(quantise(p), nextVertId);
      if (inserted) nextVertId++;
      indices.push_back(it->second);
    }
    count++;
  }

  num_quads = count;

  if (num_quads == 0) {
    tess->ClearBuffers();
    return;
  }

  std::vector<glm::vec4> positions(num_quads * 16);
  for (size_t i = 0; i < positions.size(); i++)
    positions[i] = bicubicVerts[i].pos;

  tess->Upload(positions.data(), indices.data(), num_quads);

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

uint32_t TessellatorPass::GetMaxVertexCount() const
{
  return num_quads * tess::MAX_TRIS_PER_PATCH * 3;
}
