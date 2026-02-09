#include "GLFWRenderContext.h"

#include <iostream>
#include <cstring>

GLFWRenderContext::GLFWRenderContext(glm::vec2 size) : RenderContext(size)
{
  if (!glfwInit())
  {
    throw RenderContextException("Failed to initialize GLFW!");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  this->window = glfwCreateWindow(this->screenSize.x, this->screenSize.y, "iPASS for WebGPU", nullptr, nullptr);
  if (!this->window)
  {
    glfwTerminate();
    throw RenderContextException("Failed to create GLFW window!");
  }

  this->Initialize();

  std::cout << "GLFWRenderContext initialized successfully" << std::endl;
}

GLFWRenderContext::~GLFWRenderContext()
{
  glfwDestroyWindow(this->window);
  glfwTerminate();
}

bool GLFWRenderContext::isRunning()
{
  return !glfwWindowShouldClose(this->window);
}

wgpu::TextureView GLFWRenderContext::GetNextTextureView()
{
  wgpu::SurfaceTexture surfaceTexture;
  this->surface.getCurrentTexture(&surfaceTexture);
  if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal)
  {
    return nullptr;
  }
  wgpu::Texture texture = surfaceTexture.texture;

  wgpu::TextureViewDescriptor viewDescriptor;
  viewDescriptor.label = WGPU_STRING_VIEW_INIT;
  viewDescriptor.format = texture.getFormat();
  viewDescriptor.dimension = wgpu::TextureViewDimension::_2D;
  viewDescriptor.baseMipLevel = 0;
  viewDescriptor.mipLevelCount = 1;
  viewDescriptor.baseArrayLayer = 0;
  viewDescriptor.arrayLayerCount = 1;
  viewDescriptor.aspect = wgpu::TextureAspect::All;
  wgpu::TextureView targetView = texture.createView(viewDescriptor);

#ifndef WEBGPU_BACKEND_WGPU
  wgpu::Texture(surfaceTexture.texture).release();
#endif

  return targetView;
}

void GLFWRenderContext::GenerateSurface()
{
  WGPUSurface rawSurface = glfwCreateWindowWGPUSurface(this->instance, this->window);
  if (!rawSurface)
  {
    throw RenderContextException("Failed to create surface!");
  }
  this->surface = wgpu::Surface(rawSurface);
}

void GLFWRenderContext::GetSurfaceFormat()
{
  wgpu::SurfaceCapabilities capabilities;
  this->surface.getCapabilities(this->adapter, &capabilities);
  this->surfaceFormat = capabilities.formats[0];
}

wgpu::ShaderModule GLFWRenderContext::CreateShaderModuleFromSource(std::string &shaderSource)
{
  wgpu::ShaderModuleDescriptor shaderDesc;
  wgpu::ShaderSourceWGSL shaderCodeDesc;

  shaderCodeDesc.chain.next = nullptr;
  shaderCodeDesc.chain.sType = wgpu::SType::ShaderSourceWGSL;

  shaderDesc.nextInChain = &shaderCodeDesc.chain;
  shaderCodeDesc.code.data = shaderSource.c_str();
  shaderCodeDesc.code.length = shaderSource.length();

  return this->device.createShaderModule(shaderDesc);
}

void GLFWRenderContext::Present()
{
  this->surface.present();
}

void GLFWRenderContext::DevicePoll()
{
#ifdef WEBGPU_BACKEND_DAWN
  this->device.tick();
#else
  this->device.poll(false, nullptr);
#endif
}

