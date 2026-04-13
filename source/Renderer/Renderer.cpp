#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>

#include "Renderer.h"
#include "Camera.h"
#include "Settings.h"
#include "IPass.h"
#include "TessellatorPass.h"
#include "Elevation.h"

#include "Platform.h"

//find better way of doing this
wgpu::PresentMode ToWGPUPresentMode(PresentModeSetting mode)
{
  switch (mode)
  {
    case PresentModeSetting::Immediate:
      return wgpu::PresentMode::Immediate;
    case PresentModeSetting::Mailbox:
      return wgpu::PresentMode::Mailbox;
    case PresentModeSetting::Fifo:
    default:
      return wgpu::PresentMode::Fifo;
  }
}

Renderer::Renderer()
{
  if (!glfwInit())
  {
    throw RendererException("Failed to initialize GLFW!");
  }
  this->context.size = platform::GetInitialWindowSize();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  this->window = glfwCreateWindow(this->context.size.x, this->context.size.y, "iPASS for WebGPU", nullptr, nullptr);
  if (!this->window)
  {
    glfwTerminate();
    throw RendererException("Failed to create GLFW window!");
  }

  this->Initialize();

  platform::SetupCanvasResize(this->window, [this](int w, int h) { this->OnResize(w, h); });

  // compute initial viewport (to the right of the ui panel)
  this->UpdateSceneViewport();

  uint32_t patchLimit = 64; //compute patch limit based on device allowance
  {
    wgpu::Limits limits = wgpu::Default;
    if (this->context.device.getLimits(&limits))
      patchLimit = tess::ComputeMaxPatches(
        std::min(limits.maxBufferSize, limits.maxStorageBufferBindingSize));
  }
  this->iPass = new IPass(this->context, patchLimit);
  this->iPass->SetViewportWidth(this->context.sceneViewport.width);
  this->tessPass = new TessellatorPass(this->context, this->iPass->patchesBuffer, patchLimit);
  this->scenePass = new SceneRenderPass(this->context);
  this->uiPass = new UIRenderPass(this->context, "fonts/Inter-VariableFont.ttf");

  this->stagingSize = this->iPass->GetOutputBuffer().getSize();
  wgpu::BufferDescriptor stagingDesc{};
  stagingDesc.size            = this->stagingSize;
  stagingDesc.usage           = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  stagingDesc.mappedAtCreation = false;
  this->stagingBuffer = this->context.device.createBuffer(stagingDesc);

  wgpu::BufferDescriptor triCountStagingDesc{};
  triCountStagingDesc.size            = sizeof(uint32_t);
  triCountStagingDesc.usage           = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  triCountStagingDesc.mappedAtCreation = false;
  this->triCountStagingBuffer = this->context.device.createBuffer(triCountStagingDesc);

  Settings::mvp.subscribe([this](const MVP& m) {
    glm::mat4 mvp = m.data.P * m.data.V * m.data.M;
    this->iPass->SetMVP(mvp);
  });

  Settings::parser.subscribe([this](const BVParser& p) {
    this->LoadParser(p);

    Settings::tessOutput.modify() = {
      this->tessPass->GetOutputBuffer(), 
      this->tessPass->GetMaxVertexCount(), 
      this->tessPass->GetControlPointBuffer(), 
      this->tessPass->GetPatchCount()
    };
    Settings::tessOutput.notify();
  });

  Settings::presentMode.subscribe([this](const PresentModeSetting&) {
    this->ConfigureSurface();
  });

  if (!Settings::parser.get().Get().empty())
    this->LoadParser(Settings::parser.get());

  std::cout << "Renderer initialized successfully" << std::endl;
}



Renderer::~Renderer()
{
  if (this->stagingBuffer) this->stagingBuffer.release();
  if (this->triCountStagingBuffer) this->triCountStagingBuffer.release();
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

  wgpu::Limits supported = wgpu::Default;
  if (adapter.getLimits(&supported)) //request max buffer size
  {
    wgpu::Limits required = wgpu::Default;
    required.maxBufferSize = supported.maxBufferSize;
    required.maxStorageBufferBindingSize = supported.maxStorageBufferBindingSize;
    deviceDesc.requiredLimits = &required;
    this->context.device = adapter.requestDevice(deviceDesc);
  }
  else
  {
    this->context.device = adapter.requestDevice(deviceDesc);
  }
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

  wgpu::SurfaceCapabilities capabilities;
  this->context.surface.getCapabilities(adapter, &capabilities);
  this->context.surfaceFormat = capabilities.formats[0];
  capabilities.freeMembers();
  #ifdef __EMSCRIPTEN__
  this->context.colorFormat = wgpu::TextureFormat::BGRA8UnormSrgb;
  #else
  this->context.colorFormat = this->context.surfaceFormat;
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
  #ifdef __EMSCRIPTEN__
  surfConfig.presentMode = wgpu::PresentMode::Fifo;
  #else
  surfConfig.presentMode = ToWGPUPresentMode(Settings::presentMode.get());
  #endif
  surfConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;

  #ifdef __EMSCRIPTEN__
  surfConfig.viewFormatCount = 1;
  surfConfig.viewFormats = &this->context.colorFormat.m_raw;
  #else
  surfConfig.viewFormatCount = 0;
  surfConfig.viewFormats = nullptr;
  #endif

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
    glm::uvec2 fbSize = platform::GetFramebufferSize(this->window);
    OnResize((int)fbSize.x, (int)fbSize.y);
    return nullptr;
  }
  if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
      surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)
  {
    return nullptr;
  }
  this->texture = surfaceTexture.texture;

  wgpu::TextureViewDescriptor viewDescriptor;
  viewDescriptor.label = WGPU_STRING_VIEW_INIT;
  #ifdef __EMSCRIPTEN__
  viewDescriptor.format = this->context.colorFormat;
  #else
  viewDescriptor.format = texture.getFormat();
  #endif
  viewDescriptor.dimension = wgpu::TextureViewDimension::_2D;
  viewDescriptor.baseMipLevel = 0;
  viewDescriptor.mipLevelCount = 1;
  viewDescriptor.baseArrayLayer = 0;
  viewDescriptor.arrayLayerCount = 1;
  viewDescriptor.aspect = wgpu::TextureAspect::All;
  wgpu::TextureView targetView = texture.createView(viewDescriptor);

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
  if (this->iPass)     { this->iPass->SetViewportWidth(context.sceneViewport.width); }
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

void Renderer::LoadParser(const BVParser& parser)
{
  const auto& patches = parser.Get();
  const auto& dims    = parser.GetDims();

  std::vector<utils::Vertex3D> bicubicVerts;
  bicubicVerts.reserve(patches.size() * 16);
  for (size_t pi = 0; pi < patches.size(); pi++) {
    if (patches[pi].empty()) continue;
    auto elevated = elevation::elevatePatchFull(patches[pi], dims[pi].first, dims[pi].second);
    bicubicVerts.insert(bicubicVerts.end(), elevated.begin(), elevated.end());
  }

  this->iPass->UploadVertices(bicubicVerts);
  this->tessPass->Load(bicubicVerts, patches, dims);
}

void Renderer::MainLoop()
{
  this->context.tick();
  Settings::checkUpdates();

  wgpu::TextureView targetView = this->GetNextTextureView();
  if (!targetView) { return; }

  wgpu::CommandEncoderDescriptor encoderDesc;
  encoderDesc.label = WGPU_STRING_VIEW_INIT;
  wgpu::CommandEncoder encoder = this->context.device.createCommandEncoder(encoderDesc);

  if (Settings::tessellation.get())
  {
    if (this->iPass) { this->iPass->Execute(encoder); }
    if (this->tessPass) { this->tessPass->Execute(encoder); }
  }

  if (!this->stagingBusy)
    encoder.copyBufferToBuffer(this->iPass->GetOutputBuffer(), 0, this->stagingBuffer, 0, this->stagingSize);

  if (Settings::tessellation.get() && !this->triCountStagingBusy)
  {
    wgpu::Buffer triCountBuf = this->tessPass->GetTriCountBuffer();
    if (triCountBuf)
      encoder.copyBufferToBuffer(triCountBuf, 0, this->triCountStagingBuffer, 0, sizeof(uint32_t));
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

  if (!this->stagingBusy)
  {
    this->stagingBusy = true;
    this->MapBufferForRead(this->stagingBuffer, this->stagingSize, [this]() {
      const void* mapped = this->stagingBuffer.getMappedRange(0, this->stagingSize);
      if (mapped)
      {
        const float* data = static_cast<const float*>(mapped);
        this->debugReadback.assign(data, data + this->stagingSize / sizeof(float));
        this->uiPass->SetDebugData(std::move(this->debugReadback));
      }
      this->stagingBuffer.unmap();
      this->stagingBusy = false;
    });
  }

  if (Settings::tessellation.get() && !this->triCountStagingBusy)
  {
    this->triCountStagingBusy = true;
    this->MapBufferForRead(this->triCountStagingBuffer, sizeof(uint32_t), [this]() {
      const void* mapped = this->triCountStagingBuffer.getMappedRange(0, sizeof(uint32_t));
      if (mapped)
        Settings::tessOutput.get().triangleCount = *static_cast<const uint32_t*>(mapped);
      this->triCountStagingBuffer.unmap();
      this->triCountStagingBusy = false;
    });
  }

  targetView.release();
  this->Present();
  this->texture.release();
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

void Renderer::MapBufferForRead(wgpu::Buffer& buffer, uint64_t size, std::function<void()> onSuccess)
{
#ifdef WEBGPU_BACKEND_EMDAWNWEBGPU
  auto* cb = new std::function<void()>(std::move(onSuccess));
  buffer.mapAsync(wgpu::MapMode::Read, 0, size, wgpu::CallbackMode::AllowProcessEvents,
    [cb](wgpu::MapAsyncStatus status, wgpu::StringView /*message*/) {
      if (status == wgpu::MapAsyncStatus::Success) (*cb)();
      delete cb;
    }
  );
#else
  auto* cb = new std::function<void()>(std::move(onSuccess));
  wgpu::BufferMapCallbackInfo callbackInfo;
  callbackInfo.nextInChain = nullptr;
  callbackInfo.mode        = wgpu::CallbackMode::AllowProcessEvents;
  callbackInfo.userdata1   = cb;
  callbackInfo.userdata2   = nullptr;
  callbackInfo.callback    = [](WGPUMapAsyncStatus status, WGPUStringView /*message*/,
                                void* userdata1, void* /*userdata2*/)
  {
    auto* fn = static_cast<std::function<void()>*>(userdata1);
    if (status == WGPUMapAsyncStatus_Success) (*fn)();
    delete fn;
  };
  buffer.mapAsync(wgpu::MapMode::Read, 0, size, callbackInfo);
#endif
}