#pragma once
#include "common.h"

class Window {
public:
	Window() : needsImageResize( false )
	{
		ClearKeyHistory();
	}

	~Window() {
		ClearKeyHistory();
		glfwDestroyWindow( window );
		glfwTerminate();
	}

	GLFWwindow*				window;
	VkSurfaceKHR			vk_surface;
	input_t					input;
	void					Init();
	bool					IsOpen();
	void					CreateSurface();
	void					GetWindowPosition( int& x, int& y );
	void					GetWindowSize( int& width, int& height );
	void					GetWindowFrameBufferSize( int & width, int & height, const bool wait = false );
	void					ClearKeyHistory();
	bool					IsResizeRequested() const { return needsImageResize; }
	void					AcceptImageResize() { needsImageResize = false; }

private:
	bool					needsImageResize;

	friend void FramebufferResizeCallback( GLFWwindow* window, int width, int height );
	friend void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods );
	friend void MousePressCallback( GLFWwindow* window, int button, int action, int mods );
	friend void MouseMoveCallback( GLFWwindow* window, double xpos, double ypos );
};