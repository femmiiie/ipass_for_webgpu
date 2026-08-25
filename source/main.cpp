#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "Renderer.h"
#include "InputManager.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

int main()
{
  Renderer renderer;

  InputManager::Initialize(renderer.getWindow(), renderer.getUIContext());
  InputManager::BeginInput();

  auto loop = [](void *arg)
  {
    Renderer *renderer = reinterpret_cast<Renderer *>(arg);

    InputManager::EndInput();
    InputManager::PollInputs();

    renderer->MainLoop();

    InputManager::BeginInput();
    glfwPollEvents();
  };

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(loop, &renderer, 0, true);
#else
  while (renderer.isRunning())
  {
    loop(&renderer);
  }
#endif

  return 0;
}
