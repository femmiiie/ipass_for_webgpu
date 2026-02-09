#include "WebRenderContext.h"
#include <emscripten.h>
#include <iostream>

WebRenderContext::WebRenderContext(glm::vec2 size) : RenderContext(size)
{
  this->Initialize();

  std::cout << "WebRenderContext initialized successfully" << std::endl;
}

wgpu::TextureView WebRenderContext::GetNextTextureView()
{
  wgpu::SurfaceTexture surfaceTexture;
  this->surface.getCurrentTexture(&surfaceTexture);

  if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal)
  {
    std::cerr << "Failed to get current surface texture!" << std::endl;
    return nullptr;
  }

  wgpu::TextureViewDescriptor viewDesc;
  viewDesc.setDefault();
  viewDesc.format = this->surfaceFormat;
  viewDesc.dimension = wgpu::TextureViewDimension::_2D;
  viewDesc.baseMipLevel = 0;
  viewDesc.mipLevelCount = 1;
  viewDesc.baseArrayLayer = 0;
  viewDesc.arrayLayerCount = 1;
  viewDesc.aspect = wgpu::TextureAspect::All;

  wgpu::Texture texture(surfaceTexture.texture);
  return texture.createView(viewDesc);
}

void WebRenderContext::GenerateSurface()
{
  wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc;
  canvasDesc.setDefault();
  canvasDesc.selector = {"#canvas", strlen("#canvas")};

  wgpu::SurfaceDescriptor surfDesc;
  surfDesc.setDefault();
  surfDesc.nextInChain = &canvasDesc.chain;

  this->surface = this->instance.createSurface(surfDesc);
  if (!this->surface)
  {
    throw RenderContextException("Failed to create surface!");
  }
}

void WebRenderContext::GetSurfaceFormat()
{
  this->surfaceFormat = wgpu::TextureFormat::BGRA8Unorm;
}

wgpu::ShaderModule WebRenderContext::CreateShaderModuleFromSource(std::string &shaderSource)
{
  wgpu::ShaderModuleDescriptor shaderDesc;
  wgpu::ShaderSourceWGSL wgslDesc;
  wgslDesc.setDefault();
  wgslDesc.code.data = shaderSource.c_str();
  wgslDesc.code.length = shaderSource.length();
  shaderDesc.nextInChain = &wgslDesc.chain;

  return this->device.createShaderModule(shaderDesc);
}