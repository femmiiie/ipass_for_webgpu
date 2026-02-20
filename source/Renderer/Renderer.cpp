#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>
#include <fstream>
#include <sstream>

#include "Renderer.h"
#include "Camera.h"

Renderer::Renderer(glm::uvec2 size) : screenSize(size)
{
  if (!glfwInit())
  {
    throw RendererException("Failed to initialize GLFW!");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  this->window = glfwCreateWindow(this->screenSize.x, this->screenSize.y, "iPASS for WebGPU", nullptr, nullptr);
  if (!this->window)
  {
    glfwTerminate();
    throw RendererException("Failed to create GLFW window!");
  }

  this->Initialize();

  this->uiPass = new UIRenderPass(*this);
  this->scenePass = new SceneRenderPass(*this);

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

  this->surface.unconfigure();
  this->queue.release();
  this->surface.release();
  this->device.release();
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

  this->device = this->adapter.requestDevice(deviceDesc);
  if (!this->device)
  {
    throw RendererException("Failed to get device!");
  }

  this->GenerateSurface();
  this->ConfigureSurface();

  this->adapter.release();
  this->instance.release();
}

void Renderer::ConfigureSurface()
{
  this->GetSurfaceFormat();

  wgpu::SurfaceConfiguration surfConfig;
  surfConfig.setDefault();
  surfConfig.device = this->device;
  surfConfig.width = this->screenSize.x;
  surfConfig.height = this->screenSize.y;
  surfConfig.format = this->surfaceFormat;
  surfConfig.usage = wgpu::TextureUsage::RenderAttachment;
  surfConfig.presentMode = wgpu::PresentMode::Fifo;
  surfConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;
  surfConfig.viewFormatCount = 0;
  surfConfig.viewFormats = nullptr;

  this->surface.configure(surfConfig);

  this->queue = this->device.getQueue();
}

bool Renderer::isRunning()
{
  return !glfwWindowShouldClose(this->window);
}

wgpu::TextureView Renderer::GetNextTextureView()
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
  texture.release();
#endif
  return targetView;
}

wgpu::ShaderModule Renderer::LoadShader(std::string filepath)
{
  std::ifstream fs(filepath);

  if (!fs.is_open())
  {
    std::cout << "Failed to open file " << filepath << std::endl;
    return nullptr;
  }

  std::ostringstream ss;
  ss << fs.rdbuf();
  std::string shaderSource = ss.str();

  wgpu::ShaderModuleDescriptor shaderDesc;
  wgpu::ShaderSourceWGSL shaderCodeDesc;

  shaderCodeDesc.chain.next = nullptr;
  shaderCodeDesc.chain.sType = wgpu::SType::ShaderSourceWGSL;

  shaderDesc.nextInChain = &shaderCodeDesc.chain;
  shaderCodeDesc.code.data = shaderSource.c_str();
  shaderCodeDesc.code.length = shaderSource.length();

  return this->device.createShaderModule(shaderDesc);
}

void Renderer::GenerateSurface()
{
  WGPUSurface rawSurface = glfwCreateWindowWGPUSurface(this->instance, this->window);
  if (!rawSurface)
  {
    throw RendererException("Failed to create surface!");
  }
  this->surface = wgpu::Surface(rawSurface);
}

void Renderer::GetSurfaceFormat()
{
#ifdef WEBGPU_BACKEND_EMDAWNWEBGPU
  this->surfaceFormat = wgpu::TextureFormat::BGRA8Unorm;
#else
  wgpu::SurfaceCapabilities capabilities;
  this->surface.getCapabilities(this->adapter, &capabilities);
  this->surfaceFormat = capabilities.formats[0];
#endif
}

void Renderer::Present()
{
#ifndef WEBGPU_BACKEND_EMDAWNWEBGPU
  this->surface.present();
#endif
}

void Renderer::DevicePoll()
{
#if defined(WEBGPU_BACKEND_DAWN)
  this->device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
  this->device.poll(false, nullptr);
#endif
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
  wgpu::CommandEncoder encoder = this->device.createCommandEncoder(encoderDesc);

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

  this->queue.submit(1, &command);
  command.release();

  targetView.release();
  this->Present();
  this->DevicePoll();
}