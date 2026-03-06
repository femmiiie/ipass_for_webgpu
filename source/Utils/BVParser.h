#ifndef BVPARSER_H_
#define BVPARSER_H_

#include <vector>
#include <fstream>
#include <iostream>

#include <glm/glm.hpp>

#include "Utils.h"

using utils::Vertex3D;
using Patch = std::vector<Vertex3D>;

class BVParser
{
public:

  BVParser() {};
  void Parse(std::string filepath);
  std::vector<Patch>    Get();
  std::vector<Vertex3D> GetFlat();


private:  
  std::ifstream file;
  std::vector<Patch> patches;

  void ParsePatch();
  // void ParsePolyhedron();
  void ParseSquareTensorPatch();
  void ParseRectTensorPatch();
  void ParseTensorPatch(glm::u32 degU, glm::u32 degV);

  std::vector<glm::u32> getRowU32(int cols);
  std::vector<glm::f32> getRowF32(int cols);
  std::vector<glm::f32> getRowF32();
};

#endif