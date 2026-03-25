#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>

#include "Renderer.h"
#include "Camera.h"
#include "Settings.h"
#include "iPass.h"
#include "TessellatorPass.h"

Renderer::Renderer()
{
  if (!glfwInit())
  {
    throw RendererException("Failed to initialize GLFW!");
  }

  // sets initial size to 2/3 monitor resolution
  const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
  this->context.size = glm::uvec2(mode->width / 1.5, mode->height / 1.5);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  this->window = glfwCreateWindow(this->context.size.x, this->context.size.y, "iPASS for WebGPU", nullptr, nullptr);
  if (!this->window)
  {
    glfwTerminate();
    throw RendererException("Failed to create GLFW window!");
  }

  this->Initialize();

  // compute initial viewport (to the right of the ui panel)
  this->UpdateSceneViewport();

  this->iPass = new IPass(this->context);
  this->tessPass = new TessellatorPass(this->context, this->iPass->patchesBuffer);
  this->scenePass = new SceneRenderPass(this->context);
  this->uiPass = new UIRenderPass(this->context);

  if (!Settings::parser.get().Get().empty()) {
    this->tessPass->LoadBV(Settings::parser.get());
  }

  glfwSetWindowUserPointer(this->window, this);
  glfwSetFramebufferSizeCallback(this->window, [](GLFWwindow* w, int width, int height) {
    auto* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(w));
    renderer->OnResize(width, height);
  });

  std::cout << "Renderer initialized successfully" << std::endl;
}



Renderer::~Renderer()
{
  delete this->iPass;
  delete this->tessPass;
  delete this->scenePass;
  delete this->uiPass;

  glfwDestroyWindow(this->window);
  glfwTerminate();

  this->context.surface.unconfigure();
  this->context.queue.release();
  this->context.surface.release();
  this->context.device.release();
}

void Renderer::Initialize()
{
  wgpu::Instance instance = wgpu::createInstance();
  if (!instance)
  {
    throw RendererException("Failed to create WebGPU instance!");
  }

  wgpu::RequestAdapterOptions adapterOpts;
  adapterOpts.setDefault();
  wgpu::Adapter adapter = instance.requestAdapter(adapterOpts);
  if (!adapter)
  {
    throw RendererException("Failed to get adapter!");
  }

  wgpu::DeviceDescriptor deviceDesc;
  deviceDesc.setDefault();
  deviceDesc.requiredFeatures = nullptr;
  deviceDesc.requiredFeatureCount = 0;

  this->context.device = adapter.requestDevice(deviceDesc);
  if (!this->context.device)
  {
    throw RendererException("Failed to get device!");
  }

  WGPUSurface rawSurface = glfwCreateWindowWGPUSurface(instance, this->window);
  if (!rawSurface)
  {
    throw RendererException("Failed to create surface!");
  }
  this->context.surface = wgpu::Surface(rawSurface);


#ifdef WEBGPU_BACKEND_EMDAWNWEBGPU
  this->surfaceFormat = wgpu::TextureFormat::BGRA8Unorm;
#else
  wgpu::SurfaceCapabilities capabilities;
  this->context.surface.getCapabilities(adapter, &capabilities);
  this->context.surfaceFormat = capabilities.formats[0];
#endif

  this->ConfigureSurface();

  adapter.release();
  instance.release();
}

void Renderer::ConfigureSurface()
{
  wgpu::SurfaceConfiguration surfConfig;
  surfConfig.setDefault();
  surfConfig.device = this->context.device;
  surfConfig.width  = this->context.size.x;
  surfConfig.height = this->context.size.y;
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
  this->context.size = { (uint32_t)w, (uint32_t)h };
  this->ConfigureSurface();
  this->UpdateSceneViewport();
  if (this->uiPass)    { this->uiPass->UpdateProjection(context.size); }
  if (this->scenePass) { this->scenePass->OnResize(context.size); }
}

nk_context *Renderer::getUIContext()
{
  return this->uiPass ? this->uiPass->getUIContext() : nullptr;
}

void Renderer::UpdateSceneViewport()
{
  float menuWidth = (float)context.size.x / 4.0f;
  context.sceneViewport.x      = menuWidth;
  context.sceneViewport.y      = 0.0f;
  context.sceneViewport.width  = (float)context.size.x - menuWidth;
  context.sceneViewport.height = (float)context.size.y;
}

void Renderer::MainLoop()
{
  this->context.tick();
  Settings::checkUpdates();

  if (this->pendingDebugInspect.has_value() && this->debugMapped)
  {
    auto& di = this->pendingDebugInspect.value();
    const void* mapped = di.buffer.getMappedRange(0, di.size);
    if (mapped)
    {
      const float* data = static_cast<const float*>(mapped);
      this->debugReadback.assign(data, data + di.size / sizeof(float));
      this->uiPass->SetDebugData(this->debugReadback);
    }
    di.buffer.unmap();
    di.buffer.release();
    this->pendingDebugInspect.reset();
    this->debugMapped = false;
  }

  wgpu::TextureView targetView = this->GetNextTextureView();
  if (!targetView) { return; }

  wgpu::CommandEncoderDescriptor encoderDesc;
  encoderDesc.label = WGPU_STRING_VIEW_INIT;
  wgpu::CommandEncoder encoder = this->context.device.createCommandEncoder(encoderDesc);

  this->iPass->Execute(encoder);

  if (this->tessPass)
  {
    this->tessPass->Execute(encoder);
  }

  wgpu::Buffer stagingBuffer;
  uint64_t stagingSize = 0;
  if (!this->pendingDebugInspect.has_value())
  {
    wgpu::Buffer& debugBuf = this->iPass->GetOutputBuffer();
    stagingSize = debugBuf.getSize();
    wgpu::BufferDescriptor desc;
    desc.size = stagingSize;
    desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    desc.mappedAtCreation = false;
    stagingBuffer = this->context.device.createBuffer(desc);
    encoder.copyBufferToBuffer(debugBuf, 0, stagingBuffer, 0, stagingSize);
  }


  wgpu::RenderPassColorAttachment colorAttachment;
  wgpu::RenderPassDepthStencilAttachment depthStencilAttachment;
  wgpu::RenderPassDescriptor renderPassDesc = this->GetRenderDescriptor(targetView, colorAttachment, depthStencilAttachment);
  wgpu::RenderPassEncoder renderEncoder = encoder.beginRenderPass(renderPassDesc);

  this->scenePass->Execute(renderEncoder);

  renderEncoder.setViewport(0, 0, (float)context.size.x, (float)context.size.y, 0.0f, 1.0f);
  renderEncoder.setScissorRect(0, 0, context.size.x, context.size.y);

  this->uiPass->Execute(renderEncoder);

  renderEncoder.end();
  renderEncoder.release();

  wgpu::CommandBufferDescriptor cmdBufferDescriptor = {};
  cmdBufferDescriptor.label = WGPU_STRING_VIEW_INIT;

  wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
  encoder.release();

  this->context.queue.submit(1, &command);
  command.release();

  // Initiate async readback after submission
  // The DevicePoll() call below will trigger the callback when the GPU finishes
  if (stagingSize > 0)
  {
    this->pendingDebugInspect = DebugInspect{stagingBuffer, stagingSize};
    bool* mappedFlag = &this->debugMapped;
    wgpu::BufferMapCallbackInfo callbackInfo;
    callbackInfo.nextInChain = nullptr;
    callbackInfo.mode = wgpu::CallbackMode::AllowProcessEvents;
    callbackInfo.userdata1 = mappedFlag;
    callbackInfo.userdata2 = nullptr;
    callbackInfo.callback = [](
      WGPUMapAsyncStatus status,
      WGPUStringView /*message*/,
      void* userdata1,
      void* /*userdata2*/
    ) {
      bool* flag = static_cast<bool*>(userdata1);
      *flag = (status == WGPUMapAsyncStatus_Success);
    };
    this->pendingDebugInspect.value().buffer.mapAsync(
      wgpu::MapMode::Read, 0, stagingSize, callbackInfo
    );
  }

  targetView.release();
  this->Present();
  this->DevicePoll();

  this->context.measure();
}

wgpu::RenderPassDescriptor Renderer::GetRenderDescriptor(wgpu::TextureView& view,
  wgpu::RenderPassColorAttachment& renderPassColorAttachment,
  wgpu::RenderPassDepthStencilAttachment& depthStencilAttachment)
{
  renderPassColorAttachment.view = view;
  renderPassColorAttachment.resolveTarget = nullptr;
  renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
  renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
  renderPassColorAttachment.clearValue = WGPUColor{
    Settings::clearColor.r, 
    Settings::clearColor.g, 
    Settings::clearColor.b, 
    Settings::clearColor.a
  };
#ifndef WEBGPU_BACKEND_WGPU
  renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

  depthStencilAttachment.view              = this->scenePass->GetDepthTextureView();
  depthStencilAttachment.depthLoadOp       = wgpu::LoadOp::Clear;
  depthStencilAttachment.depthStoreOp      = wgpu::StoreOp::Store;
  depthStencilAttachment.depthClearValue   = 1.0f;
  depthStencilAttachment.depthReadOnly     = false;
  depthStencilAttachment.stencilLoadOp     = wgpu::LoadOp::Undefined;
  depthStencilAttachment.stencilStoreOp    = wgpu::StoreOp::Undefined;
  depthStencilAttachment.stencilReadOnly   = true;

  wgpu::RenderPassDescriptor renderPassDesc;
  renderPassDesc.colorAttachmentCount = 1;
  renderPassDesc.colorAttachments = &renderPassColorAttachment;
  renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
  renderPassDesc.timestampWrites = nullptr;
  return renderPassDesc;
}