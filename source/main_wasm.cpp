#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <emscripten.h>

#include "Renderer.h"
#include "WebRenderContext.h"

int main()
{
  WebRenderContext context = WebRenderContext({1920, 1080});
  Renderer renderer(context);

  auto callback = [](void *arg)
  {
    Renderer *renderer = reinterpret_cast<Renderer *>(arg);
    renderer->MainLoop();
  };
  emscripten_set_main_loop_arg(callback, &renderer, 0, true);

  return 0;
}