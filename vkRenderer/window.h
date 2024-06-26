#pragma once
#include "src/globals/common.h"
#include "input.h"

#define WINDOWS

static const char *const ApplicationName = "Extensa";

class Window {
public:

	Window() : needsImageResize( false ) { }

	~Window() {
		glfwDestroyWindow( window );
		glfwTerminate();
	}

	GLFWwindow*				window;
	VkSurfaceKHR			vk_surface;
	Input					input;
	void					Init();
	void					BeginFrame();
	void					EndFrame();
	bool					IsOpen() const;
	bool					IsFocused() const;
	std::string				OpenFileDialog( const std::string& title, const std::vector<const char*>& filters, const std::string& filterDesc );
	void					PumpMessages();
	vec2f					GetNdc( const float x, const float y );
	void					GetWindowPosition( int& x, int& y );
	void					GetWindowSize( int& width, int& height );
	void					GetWindowFrameBufferSize( int & width, int & height, const bool wait = false );
	float					GetWindowFrameBufferAspect( const bool wait = false );
	bool					IsResizeRequested() const { return needsImageResize; }
	void					CompleteImageResize() { needsImageResize = false; }
	void					RequestImageResize() { needsImageResize = true; }

#ifdef USE_VULKAN
	void					CreateGlfwSurface( const VkInstance instance );
	void					DestroyGlfwSurface( const VkInstance instance );
#endif

private:
	bool					needsImageResize;
	bool					focused;

	friend void FramebufferResizeCallback( GLFWwindow* window, int width, int height );
	friend void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods );
	friend void MousePressCallback( GLFWwindow* window, int button, int action, int mods );
	friend void MouseMoveCallback( GLFWwindow* window, double xpos, double ypos );
};