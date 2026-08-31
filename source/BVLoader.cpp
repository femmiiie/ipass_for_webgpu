#include "ipass/ipass.h"
#include "Elevation.h"
#include "Shader.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace ipass {

using utils::Vertex3D;
using Patch = std::vector<Vertex3D>;

// simple recursive-descent reader for the BezierView (.bv) patch file format.
class Parser
{
public:
  std::vector<Patch> patches;
  std::vector<std::pair<glm::u32, glm::u32>> dims;

  void Parse(const std::string& filepath)
  {
    this->file = std::ifstream(filepath);
    if (!this->file.is_open())
    {
      std::cerr << "[BVLoader] failed to open " << filepath << std::endl;
      return;
    }
    while (this->file.good()) { this->ParsePatch(); }
  }

private:
  std::ifstream file;

  void ParsePatch()
  {
    while (this->file.good())
    {
      if (!this->file.good()) return;

      // patches are always positive ints, skip any line that doesn't start with that
      if (!std::isdigit(this->file.peek()))
      {
        std::string line;
        std::getline(this->file, line);
        continue;
      }

      auto row = this->getRowU32(1);
      if (row.empty()) return;

      switch (row[0])
      {
        case 4:
          return this->ParseSquareTensorPatch();
        case 5:
          return this->ParseRectTensorPatch(3);
        case 8:
          return this->ParseRectTensorPatch(4); // rational: xyzw control points
        default:
          return;
      }
    }
  }

  void ParseSquareTensorPatch()
  {
    auto row = this->getRowU32(1);
    if (row.empty()) return;
    glm::u32 deg = row[0];
    this->ParseTensorPatch(deg, deg, 3);
  }

  void ParseRectTensorPatch(int coordDim)
  {
    auto row = this->getRowU32(2);
    if (row.size() < 2) return;
    this->ParseTensorPatch(row[0], row[1], coordDim);
  }

  void ParseTensorPatch(glm::u32 degU, glm::u32 degV, int coordDim)
  {
    this->dims.emplace_back(degU + 1, degV + 1);
    Patch patch;
    for (glm::u32 i = 0; i <= degU; i++)
    {
      for (glm::u32 j = 0; j <= degV; j++)
      {
        auto pos = this->getRowF32(coordDim);
        if (pos.size() < 3) continue;

        glm::vec4 glm_pos = coordDim >= 4
          ? glm::vec4(pos[0], pos[1], pos[2], pos[3])
          : glm::vec4(pos[0], pos[1], pos[2], 1.0f);

        Vertex3D vert = {
          .pos   = glm_pos,
          .color = glm::vec4(1),
          .tex   = glm::vec2(1),
          ._pad  = glm::vec2(0)
        };
        patch.emplace_back(vert);
      }
    }
    this->patches.emplace_back(patch);
  }

  // reads n whitespace separated unsigned ints
  std::vector<glm::u32> getRowU32(int cols)
  {
    std::vector<glm::u32> vec(cols);
    for (int i = 0; i < cols; i++)
    {
      if (!(this->file >> vec[i])) { return {}; }
    }
    return vec;
  }

  // reads n whitespace separated floats
  std::vector<glm::f32> getRowF32(int cols)
  {
    std::vector<glm::f32> vec(cols);
    for (int i = 0; i < cols; i++)
    {
      if (!(this->file >> vec[i])) { return {}; }
    }
    return vec;
  }
};


// tessellator requires corner data to allow for even tessellation on shared boundaries
// so we weld close enough points together to record which indices are corners
std::vector<uint32_t> ComputeCornerIndices(
    const std::vector<Patch>& patches,
    const std::vector<std::pair<glm::u32, glm::u32>>& dims)
{
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

  std::vector<uint32_t> cornerIndices;
  cornerIndices.reserve(patches.size() * 4);
  std::unordered_map<size_t, uint32_t> vertexMap;
  vertexMap.reserve(patches.size() * 4);
  uint32_t nextVertId = 0;

  for (size_t pi = 0; pi < patches.size(); pi++) {
    const Patch& patch = patches[pi];
    if (patch.empty()) continue;

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
      cornerIndices.push_back(it->second);
    }
  }
  return cornerIndices;
}


PatchData LoadBV(const std::string& filepath, Status* status, uint32_t max_patches)
{
    Parser parser;
    parser.Parse(filepath);

    if (parser.patches.empty()) {
        std::cerr << "[BVLoader] No patches loaded from " << filepath << std::endl;
        if (status) *status = Status::LoadFailed;
        return {};
    }

    PatchData result;
    uint32_t count = 0;
    uint32_t limit = (max_patches > 0) ? max_patches : static_cast<uint32_t>(parser.patches.size());

    // elevate all patches to bicubic and collect control points
    for (size_t pi = 0; pi < parser.patches.size() && count < limit; pi++) {
        if (parser.patches[pi].empty()) continue;
        auto elevated = elevation::elevatePatchPositions(
            parser.patches[pi], parser.dims[pi].first, parser.dims[pi].second);
        result.control_points.insert(result.control_points.end(),
            elevated.begin(), elevated.end());
        count++;
    }

    result.num_patches = count;
    if (count == 0) {
        if (status) *status = Status::LoadFailed;
        return result;
    }

    result.corner_indices = ComputeCornerIndices(parser.patches, parser.dims);
    result.corner_indices.resize(count * 4);

    if (status) *status = Status::Success;
    return result;
}

}