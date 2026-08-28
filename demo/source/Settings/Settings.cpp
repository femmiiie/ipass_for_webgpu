#include "Settings.h"

#include <glm/gtc/matrix_transform.hpp>

void Settings::checkUpdates()
{
  if (translation != prevTranslation ||
      rotation    != prevRotation    ||
      scale       != prevScale)
  {
    prevTranslation = translation;
    prevRotation    = rotation;
    prevScale       = scale;

    glm::mat4 T  = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), glm::vec3(1, 0, 0));
    glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), glm::vec3(0, 1, 0));
    glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.z), glm::vec3(0, 0, 1));
    glm::mat4 S  = glm::scale(glm::mat4(1.0f), scale);

    MVP& m = mvp.modify();
    m.M     = T * Rz * Ry * Rx * S;
    m.M_inv = glm::inverse(m.M);
    mvp.notify();
  }

  if (clearColor != prevClearColor)
  {
    clearColorNeedsUpdate = true;
    prevClearColor  = clearColor;
  }

  if (tessellation.pending()) tessellation.notify();
  if (patches.pending())      patches.notify();
  if (shadingMode.pending()) shadingMode.notify();
  if (presentMode.pending()) presentMode.notify();
}
