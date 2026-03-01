#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>


#include "Renderer.h"
#include "Camera.h"

Renderer::Renderer(glm::uvec2 size)
{
  this->context.screenSize = size;

  if (!glfwInit())
  {
    throw RendererException("Failed to initialize GLFW!");
  }


  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  this->window = glfwCreateWindow(size.x, size.y, "iPASS for WebGPU", nullptr, nullptr);
  if (!this->window)
  {
    glfwTerminate();
    throw RendererException("Failed to create GLFW window!");
  }

  this->Initialize();

  this->uiPass = new UIRenderPass(this->context);
  this->scenePass = new SceneRenderPass(this->context);

  glfwSetWindowUserPointer(this->window, this);
  glfwSetFramebufferSizeCallback(this->window, [](GLFWwindow* w, int width, int height) {
    auto* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(w));
    renderer->OnResize(width, height);
  });

  std::cout << "Renderer initialized successfully" << std::endl;
}

void Renderer::SetCamera(Camera* cam)
{
  if (scenePass) { scenePass->SetCamera(cam); }
}


Renderer::~Renderer()
{
  delete this->uiPass;
  delete this->scenePass;

  glfwDestroyWindow(this->window);
  glfwTerminate();

  this->context.surface.unconfigure();
  this->context.queue.release();
  this->context.surface.release();
  this->context.device.release();
}

void Renderer::Initialize()
{
  this->instance = wgpu::createInstance();
  if (!this->instance)
  {
    throw RendererException("Failed to create WebGPU instance!");
  }

  wgpu::RequestAdapterOptions adapterOpts;
  adapterOpts.setDefault();
  this->adapter = this->instance.requestAdapter(adapterOpts);
  if (!this->adapter)
  {
    throw RendererException("Failed to get adapter!");
  }

  wgpu::DeviceDescriptor deviceDesc;
  deviceDesc.setDefault();
  deviceDesc.requiredFeatures = nullptr;
  deviceDesc.requiredFeatureCount = 0;

  this->context.device = this->adapter.requestDevice(deviceDesc);
  if (!this->context.device)
  {
    throw RendererException("Failed to get device!");
  }

  this->GenerateSurface();
  this->GetSurfaceFormat();
  this->ConfigureSurface();

  this->adapter.release();
  this->instance.release();
}

void Renderer::ConfigureSurface()
{
  wgpu::SurfaceConfiguration surfConfig;
  surfConfig.setDefault();
  surfConfig.device = this->context.device;
  surfConfig.width  = this->context.screenSize.x;
  surfConfig.height = this->context.screenSize.y;
  surfConfig.format = this->context.surfaceFormat;
  surfConfig.usage = wgpu::TextureUsage::RenderAttachment;
  surfConfig.presentMode = wgpu::PresentMode::Fifo;
  surfConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;
  surfConfig.viewFormatCount = 0;
  surfConfig.viewFormats = nullptr;

  this->context.surface.configure(surfConfig);
  this->context.queue = this->context.device.getQueue();
}

bool Renderer::isRunning()
{
  return !glfwWindowShouldClose(this->window);
}

wgpu::TextureView Renderer::GetNextTextureView()
{
  wgpu::SurfaceTexture surfaceTexture;
  this->context.surface.getCurrentTexture(&surfaceTexture);
  if (surfaceTexture.status == wgpu::SurfaceGetCurrentTextureStatus::Outdated ||
      surfaceTexture.status == wgpu::SurfaceGetCurrentTextureStatus::Lost)
  {
    int w, h;
    glfwGetFramebufferSize(this->window, &w, &h);
    OnResize(w, h);
    return nullptr;
  }
  if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
      surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)
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
  texture.release();
#endif
  return targetView;
}

void Renderer::GenerateSurface()
{
  WGPUSurface rawSurface = glfwCreateWindowWGPUSurface(this->instance, this->window);
  if (!rawSurface)
  {
    throw RendererException("Failed to create surface!");
  }
  this->context.surface = wgpu::Surface(rawSurface);
}

void Renderer::GetSurfaceFormat()
{
#ifdef WEBGPU_BACKEND_EMDAWNWEBGPU
  this->surfaceFormat = wgpu::TextureFormat::BGRA8Unorm;
#else
  wgpu::SurfaceCapabilities capabilities;
  this->context.surface.getCapabilities(this->adapter, &capabilities);
  this->context.surfaceFormat = capabilities.formats[0];
#endif
}

void Renderer::Present()
{
#ifndef WEBGPU_BACKEND_EMDAWNWEBGPU
  this->context.surface.present();
#endif
}

void Renderer::DevicePoll()
{
#if defined(WEBGPU_BACKEND_DAWN)
  this->context.device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
  this->context.device.poll(false, nullptr);
#endif
}

void Renderer::OnResize(int w, int h)
{
  if (w == 0 || h == 0) { return; }
  context.screenSize = { (uint32_t)w, (uint32_t)h };
  ConfigureSurface();
  if (uiPass) { uiPass->UpdateProjection(context.screenSize); }
  for (auto& cb : resizeCallbacks) { cb(w, h); }
}

void Renderer::AddResizeCallback(std::function<void(int, int)> cb)
{
  resizeCallbacks.emplace_back(std::move(cb));
}

nk_context *Renderer::getUIContext()
{
  return this->uiPass ? this->uiPass->getUIContext() : nullptr;
}

void Renderer::MainLoop()
{
  wgpu::TextureView targetView = this->GetNextTextureView();
  if (!targetView) { return; }

  wgpu::CommandEncoderDescriptor encoderDesc;
  utils::InitLabel(encoderDesc.label);
  wgpu::CommandEncoder encoder = this->context.device.createCommandEncoder(encoderDesc);

  wgpu::RenderPassColorAttachment renderPassColorAttachment;
  renderPassColorAttachment.view = targetView;
  renderPassColorAttachment.resolveTarget = nullptr;
  renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
  renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
  renderPassColorAttachment.clearValue = WGPUColor{0.0, 0.0, 0.2, 0.0};
#ifndef WEBGPU_BACKEND_WGPU
  renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

  wgpu::RenderPassDescriptor renderPassDesc;
  renderPassDesc.colorAttachmentCount = 1;
  renderPassDesc.colorAttachments = &renderPassColorAttachment;
  renderPassDesc.depthStencilAttachment = nullptr;
  renderPassDesc.timestampWrites = nullptr;

  wgpu::RenderPassEncoder passEncoder = encoder.beginRenderPass(renderPassDesc);

  this->scenePass->Execute(passEncoder);
  this->uiPass->Execute(passEncoder);

  passEncoder.end();
  passEncoder.release();

  wgpu::CommandBufferDescriptor cmdBufferDescriptor = {};
  utils::InitLabel(cmdBufferDescriptor.label);

  wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
  encoder.release();

  this->context.queue.submit(1, &command);
  command.release();

  targetView.release();
  this->Present();
  this->DevicePoll();
}