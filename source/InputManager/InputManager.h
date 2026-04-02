#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <nuklear.h>

#include <functional>
#include <map>
#include <set>
#include <vector>

using KeyCallback    = std::function<void(int action, int mods)>;
using ScrollCallback = std::function<void(double xoffset, double yoffset)>;
using CursorCallback = std::function<void(double xpos, double ypos)>;

class InputManager
{
public:
  static void Initialize(GLFWwindow* window, nk_context* nkCtx);

  static void AddKeyCallback(int key, KeyCallback callback);
  static void AddMouseButtonCallback(int button, KeyCallback callback);
  static void AddScrollCallback(ScrollCallback callback);
  static void AddCursorCallback(CursorCallback callback);

  static void BeginInput();
  static void EndInput();

  static void PollInputs();

  static glm::dvec2 GetCursorPos()                { return cursorPos; }
  static glm::dvec2 GetCursorDelta()              { return cursorDelta; }
  static bool       IsKeyHeld(int key)            { return heldKeys.count(key) > 0; }
  static bool       IsMouseButtonHeld(int button) { return heldMouseButtons.count(button) > 0; }

private:
  InputManager() = delete;

  static void GLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void GLFWMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
  static void GLFWScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
  static void GLFWCursorPosCallback(GLFWwindow* window, double xpos, double ypos);
  static void GLFWCharCallback(GLFWwindow* window, unsigned int codepoint);

  static glm::dvec2 GetMousePosScaling();

  static int PollModifier(int leftMod, int modFlag);
  static int PollModifiers();

  static inline GLFWwindow* window     = nullptr;
  static inline nk_context* nkContext   = nullptr;

  static inline std::map<int, std::vector<KeyCallback>> keyCallbacks;
  static inline std::map<int, std::vector<KeyCallback>> mouseButtonCallbacks;
  static inline std::vector<ScrollCallback>             scrollCallbacks;
  static inline std::vector<CursorCallback>             cursorCallbacks;

  static inline std::set<int> heldKeys;
  static inline std::set<int> heldMouseButtons;

  static inline glm::dvec2 cursorPos     = {0.0, 0.0};
  static inline glm::dvec2 lastCursorPos = {0.0, 0.0};
  static inline glm::dvec2 cursorDelta   = {0.0, 0.0};
};
