#ifndef RENDERER_H
#define RENDERER_H

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <webgpu/webgpu.hpp>

#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>
#include <array>
#include <string>
#include <exception>
#include <functional>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "UIRenderPass.h"
#include "SceneRenderPass.h"
#include "RenderPass.h"
#include "Utils.h"
#include "RenderContext.h"

class Camera;

class Renderer
{
public:
  wgpu::Adapter adapter;
  wgpu::Instance instance;

  RenderContext context;

  GLFWwindow *window;
  GLFWwindow *getWindow() { return this->window; }


  Renderer(glm::uvec2 size);
  ~Renderer();

  void MainLoop();
  nk_context *getUIContext();
  void SetCamera(Camera* cam);

  bool isRunning();
  wgpu::TextureView GetNextTextureView();
  void Present();
  void DevicePoll();

  void OnResize(int w, int h);
  void AddResizeCallback(std::function<void(int, int)> cb);

private:
  SceneRenderPass *scenePass = nullptr;
  UIRenderPass *uiPass = nullptr;

  std::vector<std::function<void(int, int)>> resizeCallbacks;

  void Initialize();
  void ConfigureSurface();
  void GenerateSurface();
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