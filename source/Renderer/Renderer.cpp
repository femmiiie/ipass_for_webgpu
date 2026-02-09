#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>

#include "Renderer.h"

Renderer::Renderer(RenderContext &context) : context(context)
{
  this->uiPass = new UIRenderPass(this->context);
  this->scenePass = new SceneRenderPass(this->context);
}

Renderer::~Renderer(){}

nk_context* Renderer::getUIContext()
{
  return this->uiPass ? this->uiPass->getUIContext() : nullptr;
}

void Renderer::MainLoop()
{
  wgpu::TextureView targetView = this->context.GetNextTextureView();
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
  this->context.Present();
  this->context.DevicePoll();
}