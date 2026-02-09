#include "RenderContext.h"

#include <fstream>
#include <sstream>

RenderContext::~RenderContext()
{
  this->surface.unconfigure();
  this->queue.release();
  this->surface.release();
  this->device.release();
}

void RenderContext::Initialize()
{
  this->instance = wgpu::createInstance();
  if (!this->instance)
  {
    throw RenderContextException("Failed to create WebGPU instance!");
  }

  wgpu::RequestAdapterOptions adapterOpts;
  adapterOpts.setDefault();
  this->adapter = this->instance.requestAdapter(adapterOpts);
  if (!adapter)
  {
    throw RenderContextException("Failed to get adapter!");
  }

  wgpu::DeviceDescriptor deviceDesc;
  deviceDesc.setDefault();
  deviceDesc.requiredFeatures = nullptr;
  deviceDesc.requiredFeatureCount = 0;

  this->device = adapter.requestDevice(deviceDesc);
  if (!this->device)
  {
    throw RenderContextException("Failed to get device!");
  }

  this->GenerateSurface();
  this->ConfigureSurface();

  this->adapter.release();
  this->instance.release();
}

void RenderContext::ConfigureSurface()
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

wgpu::ShaderModule RenderContext::LoadShader(std::string filepath)
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

  return this->CreateShaderModuleFromSource(shaderSource);
}