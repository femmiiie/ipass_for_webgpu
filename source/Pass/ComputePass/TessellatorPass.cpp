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

using Vertex3D = utils::Vertex3D;

//these are taken straight from ipass.cpp,
//likely provide this to be available to both as a utility namespace
static std::vector<glm::vec4> tp_elevate1D(const std::vector<glm::vec4>& pts)
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

static std::vector<glm::vec4> tp_elevateTo(const std::vector<glm::vec4>& pts, uint32_t targetDeg)
{
  std::vector<glm::vec4> cur = pts;
  while (static_cast<uint32_t>(cur.size()) - 1 < targetDeg)
    cur = tp_elevate1D(cur);
  return cur;
}

//only thing needed here is position
static std::vector<glm::vec4> tp_elevatePatch(
  const std::vector<Vertex3D>& patch,
  uint32_t rows, uint32_t cols)
{
  std::vector<glm::vec4> pos(rows * cols);
  for (uint32_t r = 0; r < rows; r++)
    for (uint32_t c = 0; c < cols; c++)
      pos[r * cols + c] = patch[r * cols + c].pos;

  std::vector<glm::vec4> pos1(rows * 4);
  for (uint32_t r = 0; r < rows; r++) {
    std::vector<glm::vec4> row(cols);
    for (uint32_t c = 0; c < cols; c++)
      row[c] = pos[r * cols + c];
    auto ep = tp_elevateTo(row, 3);
    for (uint32_t c = 0; c < 4; c++)
      pos1[r * 4 + c] = ep[c];
  }

  std::vector<glm::vec4> pos2(16);
  for (uint32_t c = 0; c < 4; c++) {
    std::vector<glm::vec4> col(rows);
    for (uint32_t r = 0; r < rows; r++)
      col[r] = pos1[r * 4 + c];
    auto ep = tp_elevateTo(col, 3);
    for (uint32_t r = 0; r < 4; r++)
      pos2[r * 4 + c] = ep[r];
  }

  return pos2;
}

TessellatorPass::TessellatorPass(Context& ctx, wgpu::Buffer ipass_levels_buf) : context(ctx)
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
    auto elevated = tp_elevatePatch(patches[pi], dims[pi].first, dims[pi].second);
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
