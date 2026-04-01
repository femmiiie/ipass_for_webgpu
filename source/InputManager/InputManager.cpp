#include "InputManager.h"
#include "KeyTable.h"

void InputManager::Initialize(GLFWwindow* win, nk_context* nkCtx)
{
  window    = win;
  nkContext = nkCtx;

  glfwSetKeyCallback(window, GLFWKeyCallback);
  glfwSetMouseButtonCallback(window, GLFWMouseButtonCallback);
  glfwSetScrollCallback(window, GLFWScrollCallback);
  glfwSetCursorPosCallback(window, GLFWCursorPosCallback);
  glfwSetCharCallback(window, GLFWCharCallback);

  glfwGetCursorPos(window, &cursorPos.x, &cursorPos.y);
  lastCursorPos = cursorPos;
}


void InputManager::AddKeyCallback(int key, KeyCallback callback)
{
  keyCallbacks[key].emplace_back(std::move(callback));
}

void InputManager::AddMouseButtonCallback(int button, KeyCallback callback)
{
  mouseButtonCallbacks[button].emplace_back(std::move(callback));
}

void InputManager::AddScrollCallback(ScrollCallback callback)
{
  scrollCallbacks.emplace_back(std::move(callback));
}

void InputManager::AddCursorCallback(CursorCallback callback)
{
  cursorCallbacks.emplace_back(std::move(callback));
}


void InputManager::BeginInput()
{
  if (nkContext) { nk_input_begin(nkContext); }
}

void InputManager::EndInput()
{
  if (nkContext) { nk_input_end(nkContext); }
}

void InputManager::PollInputs()
{
  cursorDelta   = cursorPos - lastCursorPos;
  lastCursorPos = cursorPos;

  int mods = PollModifiers();

  for (int key : heldKeys)
  {
    if (glfwGetKey(window, key) == GLFW_PRESS && keyCallbacks.count(key))
    {
      for (KeyCallback& callback : keyCallbacks[key])
      {
        callback(GLFW_REPEAT, mods);
      }
    }
  }

  for (int button : heldMouseButtons)
  {
    if (glfwGetMouseButton(window, button) == GLFW_PRESS &&
        mouseButtonCallbacks.count(button))
    {
      for (KeyCallback& callback : mouseButtonCallbacks[button])
      {
        callback(GLFW_REPEAT, mods);
      }
    }
  }
}


void InputManager::GLFWKeyCallback(GLFWwindow* /*window*/, int key, int /*scancode*/,
                                   int action, int mods)
{
  if (nkContext)
  {
    nk_bool down = (action != GLFW_RELEASE);

    if (keyTable.contains(key))
    {
      nk_input_key(nkContext, keyTable[key], down);
    }

    if (mods & GLFW_MOD_CONTROL && modKeyTable.contains(key))
    {
      nk_input_key(nkContext, modKeyTable[key], down);
    }
  }

  if (nkContext && nk_item_is_any_active(nkContext)) { return; }

  if (action == GLFW_PRESS)        { heldKeys.insert(key); }
  else if (action == GLFW_RELEASE) { heldKeys.erase(key);  }

  if (keyCallbacks.count(key))
  {
    for (auto& cb : keyCallbacks[key]) { cb(action, mods); }
  }
}

void InputManager::GLFWMouseButtonCallback(GLFWwindow* /*window*/, int button,
                                           int action, int mods)
{
  if (nkContext)
  {
    int x = static_cast<int>(cursorPos.x);
    int y = static_cast<int>(cursorPos.y);
    nk_bool down = (action == GLFW_PRESS);

    if (mouseTable.contains(button))
    {
      nk_input_button(nkContext, mouseTable[button], x, y, down);
    }

    switch (button)
    {
      case GLFW_MOUSE_BUTTON_LEFT:   nk_input_button(nkContext, NK_BUTTON_LEFT,   x, y, down); break;
      case GLFW_MOUSE_BUTTON_MIDDLE: nk_input_button(nkContext, NK_BUTTON_MIDDLE, x, y, down); break;
      case GLFW_MOUSE_BUTTON_RIGHT:  nk_input_button(nkContext, NK_BUTTON_RIGHT,  x, y, down); break;
      default: break;
    }
  }

  if (nkContext && nk_window_is_any_hovered(nkContext)) { return; }

  if (action == GLFW_PRESS)        { heldMouseButtons.insert(button); }
  else if (action == GLFW_RELEASE) { heldMouseButtons.erase(button);  }

  if (mouseButtonCallbacks.count(button))
  {
    for (auto& cb : mouseButtonCallbacks[button]) { cb(action, mods); }
  }
}

void InputManager::GLFWScrollCallback(GLFWwindow* /*window*/, double xoffset, double yoffset)
{
  if (nkContext && nk_window_is_any_hovered(nkContext))
  {
    nk_input_scroll(nkContext, nk_vec2(static_cast<float>(xoffset),
                                       static_cast<float>(yoffset)));
    return;
  }

  for (auto& cb : scrollCallbacks) { cb(xoffset, yoffset); }
}

void InputManager::GLFWCursorPosCallback(GLFWwindow* /*window*/, double xpos, double ypos)
{
  if (nkContext)
  {
    nk_input_motion(nkContext, static_cast<int>(xpos), static_cast<int>(ypos));
  }

  cursorPos = {xpos, ypos};

  for (auto& cb : cursorCallbacks) { cb(xpos, ypos); }
}

void InputManager::GLFWCharCallback(GLFWwindow* /*window*/, unsigned int codepoint)
{
  if (nkContext)
  {
    nk_input_unicode(nkContext, codepoint);
  }
}


int InputManager::PollModifier(int leftMod, int modFlag)
{
  if (glfwGetKey(window, leftMod) == GLFW_PRESS ||
      glfwGetKey(window, leftMod + 4) == GLFW_PRESS)
  {
    return modFlag;
  }
  return 0;
}

int InputManager::PollModifiers()
{
  int mods = 0;
  mods |= PollModifier(GLFW_KEY_LEFT_SHIFT,   GLFW_MOD_SHIFT);
  mods |= PollModifier(GLFW_KEY_LEFT_CONTROL, GLFW_MOD_CONTROL);
  mods |= PollModifier(GLFW_KEY_LEFT_ALT,     GLFW_MOD_ALT);
  mods |= PollModifier(GLFW_KEY_LEFT_SUPER,   GLFW_MOD_SUPER);
  return mods;
}