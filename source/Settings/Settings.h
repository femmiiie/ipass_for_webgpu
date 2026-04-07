#pragma once

#include <functional>
#include <vector>
#include <variant>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "BVParser.h"
#include "MVP.h"

struct TessOutput {
  wgpu::Buffer buffer = nullptr;
  uint32_t vertexCount = 0;
  wgpu::Buffer controlPoints = nullptr;
};

enum class ShadingMode { BlinnPhong = 0, Flat = 1, ParametricError = 2, TriangleSize = 3 };

class Settings
{
public:
  template<typename T>
  struct Setting
  {
  private:
    T value;
    bool needsUpdate = false;
    std::vector<std::function<void(const T&)>> subscribers;

  public:
    Setting() {};
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

    void subscribe(std::function<void(const T&)> callback) {
      this->subscribers.push_back(std::move(callback));
    }

    void notify() {
      if (!this->needsUpdate) return;
      this->needsUpdate = false;
      for (auto& cb : this->subscribers)
        cb(this->value);
    }

  };

  static inline Setting<bool> wireframe    = {false};
  static inline Setting<bool> tessellation = {false};
  static inline Setting<bool> perfWindow   = {false};

  static inline Setting<BVParser> parser;

  static inline Setting<TessOutput> tessOutput;

  static inline Setting<MVP> mvp = {MVP()};

  static inline Setting<ShadingMode>        shadingMode = {ShadingMode::BlinnPhong};

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
