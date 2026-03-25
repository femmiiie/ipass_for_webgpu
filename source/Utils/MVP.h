#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct MVP
{
  glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
  glm::vec3 rotation    = { 0.0f, 0.0f, 0.0f }; // euler degrees, applied zyx
  glm::vec3 scale       = { 1.0f, 1.0f, 1.0f };

  struct GPUData //for passing directly to gpu
  {
    glm::mat4 M     = glm::mat4(1.0f);
    glm::mat4 M_inv = glm::mat4(1.0f);
    glm::mat4 V     = glm::mat4(1.0f);
    glm::mat4 P     = glm::mat4(1.0f);
  } data;

  void setModel()
  {
    glm::mat4 T  = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), glm::vec3(1, 0, 0));
    glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), glm::vec3(0, 1, 0));
    glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.z), glm::vec3(0, 0, 1));
    glm::mat4 S  = glm::scale(glm::mat4(1.0f), scale);
    data.M     = T * Rz * Ry * Rx * S;
    data.M_inv = glm::inverse(data.M);
  }

  void setView(const glm::mat4& v) { data.V = v; }

  void setLookAt(glm::vec3 eye, glm::vec3 center, glm::vec3 up)
  {
    data.V = glm::lookAt(eye, center, up);
  }

  void setProjection(const glm::mat4& p) { data.P = p; }

  void setPerspective(float fovRad, float aspect, float nearClip, float farClip)
  {
    data.P = glm::perspective(fovRad, aspect, nearClip, farClip);
  }
};
