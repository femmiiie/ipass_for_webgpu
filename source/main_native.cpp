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