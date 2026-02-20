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

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "UIRenderPass.h"
#include "SceneRenderPass.h"
#include "RenderPass.h"
#include "Utils.h"

class Camera;

class Renderer
{
public:
  wgpu::Adapter adapter;
  wgpu::Instance instance;
  wgpu::Device device;
  wgpu::Queue queue;
  wgpu::Surface surface;
  wgpu::TextureFormat surfaceFormat;

  GLFWwindow *window;
  GLFWwindow *getWindow() { return this->window; }

  glm::uvec2 screenSize;

  Renderer(glm::uvec2 size);
  ~Renderer();

  void MainLoop();
  nk_context *getUIContext();
  void SetCamera(Camera* cam);

  bool isRunning();
  wgpu::ShaderModule LoadShader(std::string filepath);
  wgpu::TextureView GetNextTextureView();
  void Present();
  void DevicePoll();

private:
  SceneRenderPass *scenePass = nullptr;
  UIRenderPass *uiPass = nullptr;

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