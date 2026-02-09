#include "RenderContext.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glfw3webgpu.h>

class GLFWRenderContext final : public RenderContext
{
public:
  GLFWRenderContext(glm::vec2 size);
  ~GLFWRenderContext();

  bool isRunning();
  GLFWwindow* getWindow() { return window; }

  void GenerateSurface() override;
  void GetSurfaceFormat() override;
  wgpu::ShaderModule CreateShaderModuleFromSource(std::string &shaderSource) override;

  wgpu::TextureView GetNextTextureView() override;
  void Present() override;
  void DevicePoll() override;

private:
  GLFWwindow *window;
};
