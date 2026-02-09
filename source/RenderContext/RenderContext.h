#ifndef RENDERCONTEXT
#define RENDERCONTEXT

#include <glm/glm.hpp>
#include <exception>

#include <webgpu/webgpu.hpp>

struct RenderContext
{
  wgpu::Adapter adapter;
  wgpu::Instance instance;
  wgpu::Device device;
  wgpu::Queue queue;
  wgpu::Surface surface;
  wgpu::TextureFormat surfaceFormat;

  glm::uvec2 screenSize;

  void Initialize();
  void ConfigureSurface();

  wgpu::ShaderModule LoadShader(std::string filepath);

  virtual void GenerateSurface() = 0;
  virtual void GetSurfaceFormat() = 0;
  virtual wgpu::ShaderModule CreateShaderModuleFromSource(std::string &shaderSource) = 0;

  virtual wgpu::TextureView GetNextTextureView() = 0;
  virtual void Present() = 0;
  virtual void DevicePoll() = 0;

  RenderContext(glm::uvec2 size) : screenSize(size) {}
  ~RenderContext();
};

class RenderContextException : public std::exception
{
private:
  std::string message;

public:
  RenderContextException(std::string m) : message(m) {}
  RenderContextException(const char *m) { this->message = m; }
  const char *what() const noexcept override { return this->message.c_str(); }
};

#endif