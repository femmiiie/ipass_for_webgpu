#pragma once

#include <vector>
#include <cstdint>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "Utils.h"

namespace elevation {

// Elevate a 1D Bezier curve from degree m to degree m+1
std::vector<glm::vec4> elevate1D(const std::vector<glm::vec4>& pts);

// Repeatedly elevate a 1D curve until it reaches the target degree
std::vector<glm::vec4> elevateTo(const std::vector<glm::vec4>& pts, uint32_t targetDeg);

// Elevate a tensor-product patch to bi-cubic (4x4), preserving all vertex attributes
std::vector<utils::Vertex3D> elevatePatchFull(
  const std::vector<utils::Vertex3D>& patch,
  uint32_t rows, uint32_t cols);

// Elevate a tensor-product patch to bi-cubic (4x4), returning positions only
std::vector<glm::vec4> elevatePatchPositions(
  const std::vector<utils::Vertex3D>& patch,
  uint32_t rows, uint32_t cols);

} // namespace elevation
