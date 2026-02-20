#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <nuklear.h>

#include <iostream>

#include "Renderer.h"
#include "InputManager.h"
#include "Camera.h"

int main()
{
	Renderer renderer({1920, 1080});

	InputManager::Initialize(renderer.getWindow(), renderer.getUIContext());
	
	Camera camera;
	camera.setScrollScaling(0.5f);
	camera.getAspect_M() = 1920.0f / 1080.0f;
	camera.deferUpdate();

	InputManager::AddScrollCallback(
		[&camera](double x, double y) { camera.OnScroll(x, y); }
	);
	InputManager::AddMouseButtonCallback(
		GLFW_MOUSE_BUTTON_MIDDLE,
		[&camera](int action, int mods) { camera.OnMouseButton(action, mods); }
	);

	renderer.SetCamera(&camera);

	while (renderer.isRunning())
	{
		InputManager::BeginInput();
		glfwPollEvents();
		InputManager::EndInput();
		InputManager::PollInputs();

		renderer.MainLoop();
	}

	return 0;
}