#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

std::vector<glm::ivec4> buildQuadConnectivity(const uint32_t* indices, int num_indices);
