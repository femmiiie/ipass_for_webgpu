#pragma once

#include <cctype>
#include <cstdint>
#include <vector>
#include <utility>
#include <fstream>
#include <sstream>
#include <iterator>
#include <iostream>

#include <glm/glm.hpp>

#include "Shader.h"

using utils::Vertex3D;
using Patch = std::vector<Vertex3D>;

class BVParser
{
public:

  // BVParser() {};
  void Parse(std::string filepath);
  const std::vector<Patch>&    Get() const;
  const std::vector<Vertex3D> GetFlat() const;
  const std::vector<std::pair<glm::u32,glm::u32>>& GetDims() const;
  const std::vector<uint32_t>& GetCornerIndices() const;


private:
  std::ifstream file;
  std::vector<Patch> patches;
  std::vector<std::pair<glm::u32,glm::u32>> dims;
  std::vector<uint32_t> cornerIndices;

  void ParsePatch();
  void ParseSquareTensorPatch();
  void ParseRectTensorPatch(int coordDim);
  void ParseTensorPatch(glm::u32 degU, glm::u32 degV, int coordDim);
  void ComputeCornerIndices();

  std::vector<glm::u32> getRowU32(int cols);
  std::vector<glm::f32> getRowF32(int cols);
  std::vector<glm::f32> getRowF32();
};
