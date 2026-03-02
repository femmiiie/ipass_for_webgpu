#ifndef SETTINGS_H_
#define SETTINGS_H_

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class Settings
{
public:
  static inline bool wireframeEnabled      = false;
  static inline bool tessEnabled           = false;
  static inline bool showPerformanceWindow = false;

  static inline glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
  static inline glm::vec3 rotation    = { 0.0f, 0.0f, 0.0f }; // Euler degrees
  static inline glm::vec3 scale       = { 1.0f, 1.0f, 1.0f };

  static inline glm::vec4 clearColor  = { 0.0f, 0.0f, 0.1f, 1.0f };

  static void checkUpdates();

  //get and reset flag
  static bool resetTransformUpdate();
  static bool resetClearColorUpdate();

  // get without resetting flag
  static bool transformRequiresUpdate()  { return transformNeedsUpdate; }
  static bool clearColorRequiresUpdate() { return clearColorNeedsUpdate; }

private:
  // snapshots for checkUpdates
  static inline glm::vec3 prevTranslation = translation;
  static inline glm::vec3 prevRotation    = rotation;
  static inline glm::vec3 prevScale       = scale;
  static inline glm::vec4 prevClearColor  = clearColor;

  // update flags
  static inline bool transformNeedsUpdate  = true;
  static inline bool clearColorNeedsUpdate = true;
};

#endif