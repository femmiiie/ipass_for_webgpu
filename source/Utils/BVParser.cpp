#include "BVParser.h"

const std::vector<Patch>& BVParser::Get() const { return this->patches; }
const std::vector<std::pair<glm::u32,glm::u32>>& BVParser::GetDims() const { return this->dims; }

using utils::Vertex3D;
const std::vector<Vertex3D> BVParser::GetFlat() const
{
  std::vector<Vertex3D> vec;
  for (const Patch& patch : this->patches) { vec.insert(vec.end(), patch.begin(), patch.end()); }
  return vec;
}

void BVParser::Parse(std::string filepath)
{
  this->patches.clear();
  this->dims.clear();
  this->file = std::ifstream(filepath);

  if (!this->file.is_open())
  {
    std::cerr << "BVParser: failed to open " << filepath << std::endl;
    return;
  }

  while (this->file.good()) { this->ParsePatch(); }
}

void BVParser::ParsePatch()
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

void BVParser::ParseSquareTensorPatch()
{
  auto row = this->getRowU32(1);
  if (row.empty()) return;
  glm::u32 deg = row[0];
  this->ParseTensorPatch(deg, deg, 3);
}

void BVParser::ParseRectTensorPatch(int coordDim)
{
  auto row = this->getRowU32(2);
  if (row.size() < 2) return;
  this->ParseTensorPatch(row[0], row[1], coordDim);
}

void BVParser::ParseTensorPatch(glm::u32 degU, glm::u32 degV, int coordDim)
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
std::vector<glm::u32> BVParser::getRowU32(int cols)
{
  std::vector<glm::u32> vec(cols);
  for (int i = 0; i < cols; i++)
  {
    if (!(this->file >> vec[i])) { return {}; }
  }
  return vec;
}

// reads n whitespace separated floats
std::vector<glm::f32> BVParser::getRowF32(int cols)
{
  std::vector<glm::f32> vec(cols);
  for (int i = 0; i < cols; i++)
  {
    if (!(this->file >> vec[i])) { return {}; }
  }
  return vec;
}

// reads all floats on the next line
std::vector<glm::f32> BVParser::getRowF32()
{
  std::string line;
  this->file >> std::ws;
  std::getline(this->file, line);

  std::istringstream iss(line);
  return { std::istream_iterator<glm::f32>(iss), std::istream_iterator<glm::f32>() };
}
