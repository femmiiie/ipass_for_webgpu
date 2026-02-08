#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <nuklear.h>

#include <iostream>

#include "GLFWRenderContext.h"
#include "Renderer.h"

int main()
{
	GLFWRenderContext context = GLFWRenderContext({1920, 1080});
	Renderer renderer(context);

	while (context.isRunning())
	{
		glfwPollEvents();
		renderer.MainLoop();
	}

	return 0;
}