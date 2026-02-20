#ifndef CAMERA_H_
#define CAMERA_H_

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// #include "Settings.h"

// View matrix is updated immediately on position change.
// Projection matrix is recomputed lazily via update().
class Camera
{
public:
  Camera();

  void      setPosSPH(glm::vec3 pos);
  glm::vec3 getPosSPH() const { return positionSPH; }

  void      setPosCAR(glm::vec3 pos);
  glm::vec3 getPosCAR() const { return positionCAR; }

  void      setViewDirection(glm::vec3 dir);
  glm::vec3 getViewDirection() const { return eyeVector; }

  void  setScrollScaling(float s) { scrollScaling = s; }
  float getScrollScaling()  const { return scrollScaling; }

  glm::mat4 getViewMatrix()       const { return viewMatrix; }
  glm::mat4 getProjectionMatrix() const { return projectionMatrix; }

  // Settings::ProjectionType& getProjectionType_M() { return type; }
  float& getFOV_M()    { return fov; }
  float& getAspect_M() { return aspectRatio; }
  float& getNear_M()   { return nearClip; }
  float& getFar_M()    { return farClip; }

  void deferUpdate()        { needsUpdate = true; }
  bool requiresUpdate() { return needsUpdate; }
  void update(); // recomputes projection matrix

  // Input callbacks
  void OnScroll(double xoffset, double yoffset);
  void OnMouseButton(int action, int mods);

private:
  float scrollScaling = 0.5f;
  bool  needsUpdate   = true;

  glm::vec3 positionSPH = glm::vec3(5.0f, 0.0f, glm::half_pi<float>());
  glm::vec3 positionCAR = glm::vec3(0.0f);
  glm::vec3 eyeVector   = glm::vec3(0.0f);

  glm::mat4 viewMatrix       = glm::mat4(1.0f);
  glm::mat4 projectionMatrix = glm::mat4(1.0f);

  // Settings::ProjectionType type = Settings::PERSPEC;
  float aspectRatio = 16.0f / 9.0f;
  float fov         = glm::radians(45.0f);
  float nearClip    = 0.1f;
  float farClip     = 100.0f;

  void convertSPHtoCAR();
  void convertCARtoSPH();
  void setViewMatrix();
};

#endif
