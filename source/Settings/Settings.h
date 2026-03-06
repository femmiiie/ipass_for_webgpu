#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <variant>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "BVParser.h"
#include "MVP.h"

class Settings
{
public:
  static inline BVParser parser;

  template<typename T>
  struct Setting
  {
  private:
    T value;
    bool needsUpdate = false;

  public:
    Setting(T val) : value(val) {} 
    T& get() { return this->value; }
    void mark() { this->needsUpdate = true; }

    T& modify()  {
      this->needsUpdate = true;
      return this->value;
    }  

    bool observe() {
      bool state = this->needsUpdate;
      this->needsUpdate = false;
      return state;
    }

  };

  static inline Setting<bool> wireframe    = {false};
  static inline Setting<bool> tessellation = {false};
  static inline Setting<bool> perfWindow   = {false};

  static inline Setting<MVP> mvp = {MVP()};  // use modify()/observe()/get().data

  static inline glm::vec4 clearColor  = { 0.0f, 0.0f, 0.1f, 1.0f };

  static void checkUpdates();

private:
  // snapshots for checkUpdates
  static inline glm::vec3 prevTranslation = mvp.get().translation;
  static inline glm::vec3 prevRotation    = mvp.get().rotation;
  static inline glm::vec3 prevScale       = mvp.get().scale;
  static inline glm::vec4 prevClearColor  = clearColor;

  // update flags
  static inline bool clearColorNeedsUpdate = true;
  static inline bool surfaceNeedsUpdate    = true;
};

#endif