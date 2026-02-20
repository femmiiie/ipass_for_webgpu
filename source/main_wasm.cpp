#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <emscripten.h>
#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "InputManager.h"

int main()
{
  Renderer renderer({1920, 1080});

  InputManager::Initialize(renderer.getWindow(), renderer.getUIContext());
  InputManager::BeginInput();

  auto callback = [](void *arg)
  {
    Renderer *renderer = reinterpret_cast<Renderer *>(arg);

    InputManager::EndInput();
    InputManager::PollInputs();

    renderer->MainLoop();

    InputManager::BeginInput();
    glfwPollEvents();
  };
  emscripten_set_main_loop_arg(callback, &renderer, 0, true);

  return 0;
}