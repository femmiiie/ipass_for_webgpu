#pragma once

#include <chrono>

#include <webgpu/webgpu.hpp>

#include "BVParser.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using std::chrono::steady_clock;
using std::chrono::duration;

struct Context
{
  glm::uvec2 size;

  wgpu::Device device;
  wgpu::Queue queue;
  wgpu::Surface surface;
  wgpu::TextureFormat surfaceFormat;

  void tick();
  void measure();

  struct Viewport
  {
    float x      = 0.0f;
    float y      = 0.0f;
    float width  = 0.0f;
    float height = 0.0f;
  } sceneViewport;

  struct PerformanceMetrics
  {
    steady_clock::time_point prev_frametime = steady_clock::now();
    float since_last  = 0.0f;
    int   frame_count = 0;

    float avg_frametime = 0.0f;
    float acc_frametime = 0.0f;

    float fps = 0.0f;

    const float UPDATE_INTERVAL = 0.5f;
  } performance;
};

inline void Context::tick()
{
  Context::PerformanceMetrics& perf = this->performance;
  steady_clock::time_point now = steady_clock::now();
  perf.since_last += duration<float>(now - perf.prev_frametime).count();
  perf.prev_frametime = now;

  perf.frame_count++;
  if (perf.since_last >= perf.UPDATE_INTERVAL)
  {
    perf.fps = (float)perf.frame_count / perf.since_last;
    if (perf.frame_count > 0)
      perf.avg_frametime = perf.acc_frametime / (float)perf.frame_count;
    perf.since_last       = 0.0f;
    perf.frame_count      = 0;
    perf.acc_frametime   = 0.0f;
  }
}

inline void Context::measure()
{
  Context::PerformanceMetrics& perf = this->performance;
  auto frame_start = perf.prev_frametime;
  auto frame_end   = steady_clock::now();

  perf.acc_frametime += duration<float, std::milli>(frame_end - frame_start).count();
}
