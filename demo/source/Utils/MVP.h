#pragma once

#include <glm/glm.hpp>

struct MVP
{
  glm::mat4 M         = glm::mat4(1.0f);
  glm::mat4 M_inv     = glm::mat4(1.0f);
  glm::mat4 VP        = glm::mat4(1.0f);
  glm::vec4 cameraPos = glm::vec4(0.0f);
};
