#pragma once
#include <glm/glm.hpp>
#include <functional>
#include <GLFW/glfw3.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

namespace platform::detail
{
  inline std::function<void(int, int)> g_onResize;

#ifdef __EMSCRIPTEN__
  inline GLFWwindow *g_window = nullptr;

  inline EM_BOOL OnWindowResize(int, const EmscriptenUiEvent *, void *)
  {
    glfwSetWindowSize(g_window,
                      EM_ASM_INT({ return window.innerWidth; }),
                      EM_ASM_INT({ return window.innerHeight; }));
    return EM_FALSE;
  }
#endif
}

namespace platform
{
  inline void SetWindowHints()
  {
#ifdef __EMSCRIPTEN__
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE); // let glfw handle devicePixelRatio
#endif
  }

  inline glm::uvec2 GetInitialWindowSize()
  {
#ifdef __EMSCRIPTEN__
    return {static_cast<unsigned>(EM_ASM_INT({ return window.innerWidth; })),
            static_cast<unsigned>(EM_ASM_INT({ return window.innerHeight; }))};
#else //set as 2/3 monitor size
    const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    return {static_cast<unsigned>(mode->width / 1.5),
            static_cast<unsigned>(mode->height / 1.5)};
#endif
  }

  inline glm::uvec2 GetFramebufferSize(GLFWwindow *window)
  {
    glm::ivec2 size = {0, 0};
    glfwGetFramebufferSize(window, &size.x, &size.y);
    return {static_cast<unsigned>(size.x), static_cast<unsigned>(size.y)};
  }

  inline void SetupCanvasResize(GLFWwindow *window, std::function<void(int, int)> onResize)
  {
    detail::g_onResize = std::move(onResize);
    glfwSetWindowUserPointer(window, &detail::g_onResize);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow *w, int width, int height) {
      auto *cb = static_cast<std::function<void(int, int)> *>(glfwGetWindowUserPointer(w));
      (*cb)(width, height);
    });

#ifdef __EMSCRIPTEN__
    detail::g_window = window;
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,
                                   nullptr, false, detail::OnWindowResize);
#endif
  }
}
