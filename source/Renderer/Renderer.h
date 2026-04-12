#pragma once

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

#include "Context.h"
#include "IPass.h"
#include "TessellatorPass.h"
#include "SceneRenderPass.h"
#include "UIRenderPass.h"
#include "File.h"

class Renderer
{
public:
  Context context;

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
  IPass *iPass = nullptr;
  TessellatorPass *tessPass = nullptr;

  void UpdateSceneViewport();
  wgpu::RenderPassDescriptor GetRenderDescriptor(wgpu::TextureView& view,
    wgpu::RenderPassColorAttachment& colorAttachment,
    wgpu::RenderPassDepthStencilAttachment& depthStencilAttachment
  );
  void Initialize();
  void ConfigureSurface();
  void MapBufferForRead(wgpu::Buffer& buffer, uint64_t size, std::function<void()> onSuccess);
  void GetSurfaceFormat();

  wgpu::Texture texture;

  wgpu::Buffer stagingBuffer;
  uint64_t     stagingSize  = 0;
  bool         stagingBusy  = false;
  std::vector<glm::f32> debugReadback;
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
