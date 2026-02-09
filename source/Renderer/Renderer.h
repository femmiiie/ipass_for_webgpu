#ifndef RENDERER_H
#define RENDERER_H

#include <webgpu/webgpu.hpp>

#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>
#include <array>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "UIRenderPass.h"
#include "SceneRenderPass.h"
#include "RenderPass.h"
#include "RenderContext.h"
#include "Utils.h"

class Renderer
{
public:
  Renderer(RenderContext &context);
  ~Renderer();
  void MainLoop();

  nk_context* getUIContext();

private:

  SceneRenderPass* scenePass = nullptr;
  UIRenderPass* uiPass = nullptr;
  RenderContext &context;

};

class RendererException : public std::exception
{
private:
  std::string message;

public:
  RendererException(std::string m) : message(m) {}
  RendererException(const char *m) { this->message = m; }
  const char *what() const noexcept override { return this->message.c_str(); }
};

#endif