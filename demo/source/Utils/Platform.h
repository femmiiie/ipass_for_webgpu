#pragma once
#include <glm/glm.hpp>
#include <functional>
#include <GLFW/glfw3.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

namespace platform::detail
{
  inline std::function<void(int, int)> g_onResize;

  inline EM_BOOL OnWindowResize(int, const EmscriptenUiEvent *, void *)
  {
    glm::dvec2 size = {0.0, 0.0};
    if (emscripten_get_element_css_size("#canvas", &size.x, &size.y) != EMSCRIPTEN_RESULT_SUCCESS)
    {
      size.x = static_cast<double>(EM_ASM_INT({ return window.innerWidth; }));
      size.y = static_cast<double>(EM_ASM_INT({ return window.innerHeight; }));
    }

    glm::ivec2 css_size = {
      static_cast<int>(size.x + 0.5),
      static_cast<int>(size.y + 0.5)
    };

    if (css_size.x <= 0 || css_size.y <= 0)
    {
      return EM_FALSE;
    }

    double pixel_ratio = EM_ASM_DOUBLE({ return window.devicePixelRatio || 1.0; });
    glm::ivec2 fb_size = {
      static_cast<int>(css_size.x * pixel_ratio),
      static_cast<int>(css_size.y * pixel_ratio)
    };
    

    if (fb_size.x <= 0 || fb_size.y <= 0)
    {
      return EM_FALSE;
    }

    emscripten_set_element_css_size("#canvas", static_cast<double>(css_size.x), static_cast<double>(css_size.y));
    emscripten_set_canvas_element_size("#canvas", fb_size.x, fb_size.y);
    if (g_onResize)
    {
      g_onResize(fb_size.x, fb_size.y);
    }
    return EM_FALSE;
  }
}

namespace platform
{
  inline glm::uvec2 GetInitialWindowSize()
  {
    return {static_cast<unsigned>(EM_ASM_INT({ return window.innerWidth; })),
            static_cast<unsigned>(EM_ASM_INT({ return window.innerHeight; }))};
  }

  inline glm::uvec2 GetFramebufferSize(GLFWwindow *window)
  {
    (void)window;
    return {
        static_cast<unsigned>(EM_ASM_INT({
          const c = Module['canvas'];
          return c ? c.width : 0;
        })),
        static_cast<unsigned>(EM_ASM_INT({
          const c = Module['canvas'];
          return c ? c.height : 0;
        }))};
  }

  inline void SetupCanvasResize(GLFWwindow *window, std::function<void(int, int)> onResize)
  {
    (void)window;
    detail::g_onResize = std::move(onResize);
    detail::OnWindowResize(0, nullptr, nullptr);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,
                                   nullptr, false, detail::OnWindowResize);
  }
}

#else

namespace platform::detail
{
  inline std::function<void(int, int)> g_onResize;
}

namespace platform
{
  inline glm::uvec2 GetInitialWindowSize() //set as 2/3 monitor size
  {
    const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    return {static_cast<unsigned>(mode->width / 1.5),
            static_cast<unsigned>(mode->height / 1.5)};
  }

  inline glm::uvec2 GetFramebufferSize(GLFWwindow *window)
  {
    glm::ivec2 size = {0.0, 0.0};
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
  }
}
#endif
