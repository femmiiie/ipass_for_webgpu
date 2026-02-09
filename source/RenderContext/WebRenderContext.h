#ifndef WEBRENDERCONTEXT
#define WEBRENDERCONTEXT

#include "RenderContext.h"

class WebRenderContext : public RenderContext
{
public:
  WebRenderContext(glm::vec2 size);

  void GenerateSurface() override;
  void GetSurfaceFormat() override;
  wgpu::ShaderModule CreateShaderModuleFromSource(std::string &shaderSource) override;

  wgpu::TextureView GetNextTextureView() override;
  void Present() override {};
  void DevicePoll() override {};
};

#endif