#include "Camera.h"
#include "InputManager.h" // for GetCursorDelta()

#include <GLFW/glfw3.h>

Camera::Camera()
{
  convertSPHtoCAR();
  update();
}

void Camera::setPosCAR(glm::vec3 pos)
{
  positionCAR = pos;
  convertCARtoSPH();
}

void Camera::setPosSPH(glm::vec3 pos)
{
  positionSPH = pos;
  if (positionSPH.x < 0.1f) { positionSPH.x = 0.1f; }
  convertSPHtoCAR();
}

void Camera::setViewDirection(glm::vec3 dir)
{
  eyeVector = dir;
  setViewMatrix();
}

void Camera::convertSPHtoCAR()
{
  float r     = positionSPH.x;
  float theta = positionSPH.y;
  float phi   = positionSPH.z;

  positionCAR = glm::vec3(
    r * glm::sin(phi) * glm::cos(theta),
    r * glm::cos(phi),
    r * glm::sin(phi) * glm::sin(theta)
  ) + eyeVector;

  setViewMatrix();
}

void Camera::convertCARtoSPH()
{
  glm::vec3 offset = positionCAR - eyeVector;

  float r     = glm::length(offset);
  float theta = std::atan2(offset.z, offset.x);
  float phi   = std::acos(glm::clamp(offset.y / r, -1.0f, 1.0f));

  positionSPH = glm::vec3(r, theta, phi);
  setViewMatrix();
}

void Camera::setViewMatrix()
{
  viewMatrix = glm::lookAt(positionCAR, eyeVector, glm::vec3(0.0f, 1.0f, 0.0f));
}


void Camera::update()
{
  // if (type == Settings::PERSPEC)
  if (true)
  {
    projectionMatrix = glm::perspective(fov, aspectRatio, nearClip, farClip);
  }
  else
  {
    float halfH = positionSPH.x * glm::tan(fov * 0.5f);
    float halfW = halfH * aspectRatio;
    projectionMatrix = glm::ortho(-halfW, halfW, -halfH, halfH, nearClip, farClip);
  }
  needsUpdate = false;
}


// Input Callbacks
void Camera::OnScroll(double, double yoffset)
{
  glm::vec3 pos = getPosSPH();
  pos.x -= static_cast<float>(yoffset) * scrollScaling;
  setPosSPH(pos);
}

void Camera::OnMouseButton(int action, int mods)
{
  if (action != GLFW_REPEAT) { return; }

  glm::dvec2 delta = InputManager::GetCursorDelta();

  if (!(mods & GLFW_MOD_SHIFT)) // regular orbiting
  {
    glm::vec3 sph = getPosSPH();
    sph.y += static_cast<float>(delta.x) * 0.005f;
    sph.z  = glm::clamp(sph.z - static_cast<float>(delta.y) * 0.005f,
                         0.0005f, glm::pi<float>() - 0.0005f);
    setPosSPH(sph);
  }

  else // panning the camera around
  {
    glm::vec3 camPos = getPosCAR();
    glm::vec3 target = getViewDirection();

    glm::vec3 forward = glm::normalize(camPos  - target);
    glm::vec3 right   = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), forward));
    glm::vec3 up      = glm::normalize(glm::cross(forward, right));

    float scale = 0.0005f * getPosSPH().x;

    glm::vec3 offset =
        static_cast<float>(-delta.x) * right * scale +
        static_cast<float>( delta.y) * up    * scale;

    setPosCAR(camPos + offset);
    setViewDirection(target + offset);
  }
}
