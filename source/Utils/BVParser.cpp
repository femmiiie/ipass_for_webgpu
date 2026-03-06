#include "BVParser.h"

std::vector<Patch> BVParser::Get() { return this->patches; }

using utils::Vertex3D;
std::vector<Vertex3D> BVParser::GetFlat()
{
  std::vector<Vertex3D> vec;
  for (Patch& patch : this->patches) { vec.insert(vec.end(), patch.begin(), patch.end()); }
  return vec;
}

void BVParser::Parse(std::string filepath)
{
  this->patches.clear();
  this->file = std::ifstream(filepath);
  //check opens properly?

  while (!this->file.eof()) { this->ParsePatch(); }
  this->patches.emplace_back(Patch());
}

void BVParser::ParsePatch()
{
  switch (this->getRowU32(1)[0])
  {
    // case 1: return this->ParsePolyhedron();
    case 4: 
      return this->ParseSquareTensorPatch();
    case 5: 
    case 8: 
      return this->ParseRectTensorPatch();
    default:
      // std::cout << "iPASS for WebGPU only supports quad tensor patches and polyhedrons; skipping.";
      return;
  }
}

// void BVParser::ParsePolyhedron() {}

void BVParser::ParseSquareTensorPatch()
{
  glm::u32 deg = this->getRowU32(1)[0];
  this->ParseTensorPatch(deg, deg); // same degree in both 
}

void BVParser::ParseRectTensorPatch()
{
  std::vector<glm::u32> degs = this->getRowU32(2);
  this->ParseTensorPatch(degs[0], degs[1]);
}

void BVParser::ParseTensorPatch(glm::u32 degU, glm::u32 degV)
{
  Patch patch;
  for (glm::u32 i = 0; i <= degU; i++)
  {
    for (glm::u32 j = 0; j <= degV; j++)
    {
      // std::vector<glm::f32> pos = this->getRowF32(3);
      std::vector<glm::f32> pos = this->getRowF32();
      
      glm::vec4 glm_pos = pos.size() == 3 ?
        glm::vec4(pos[0], pos[1], pos[2], 1.0f) :
        glm::vec4(pos[0], pos[1], pos[2], pos[3]);

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

// parses a row of a .bv file expecting n values of ints
std::vector<glm::u32> BVParser::getRowU32(int cols)
{
  std::vector<glm::u32> vec(cols);
  for (int i = 0; i < cols; i++)
  {
    this->file >> vec[i];

    //this is an incredibly dirty way of handling comments, but will work for our purposes
    if (this->file.rdstate() & std::ifstream::failbit) {
      char _[256];
      this->file.clear();
      this->file.getline(_, 256, '\n');
      return {0};
    }

  }
  return vec;
}

// will parse either ints or floats, but stores as floats
std::vector<glm::f32> BVParser::getRowF32(int cols)
{
  std::vector<glm::f32> vec(cols);
  for (int i = 0; i < cols; i++)
  {
    this->file >> vec[i];
  }
  return vec;
}

std::vector<glm::f32> BVParser::getRowF32()
{
  std::vector<glm::f32> vec;
  char buffer[64];
  this->file >> std::ws;
  this->file.getline(buffer, 64, '\n');
  
  char* token = strtok(buffer, " ");
  while (token)
  {
    
    vec.emplace_back(std::stof(token));
    token = strtok(NULL, " ");
  }

  return vec;
}
