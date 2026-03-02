#ifndef RENDERER_H
#define RENDERER_H

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>
#include <array>
#include <string>
#include <exception>
#include <functional>

#include "RenderContext.h"
#include "RenderPass.h"
#include "SceneRenderPass.h"
#include "UIRenderPass.h"
#include "Utils.h"

class Renderer
{
public:
  RenderContext context;

  GLFWwindow *window;
  GLFWwindow *getWindow() { return this->window; }


  Renderer();
  ~Renderer();

  void MainLoop();
  nk_context *getUIContext();

  bool isRunning();
  wgpu::TextureView GetNextTextureView();
  void Present();
  void DevicePoll();

  void OnResize(int w, int h);

private:
  SceneRenderPass *scenePass = nullptr;
  UIRenderPass *uiPass = nullptr;



  void UpdateSceneViewport();

  void Initialize();
  void ConfigureSurface();
  void GetSurfaceFormat();
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