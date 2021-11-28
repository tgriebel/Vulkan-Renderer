#include "window.h"
#include "deviceContext.h"

void FramebufferResizeCallback( GLFWwindow* window, int width, int height )
{
	Window* app = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );
	app->needsImageResize = true;
}


void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods )
{
	Window* app = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );

	if ( key == GLFW_KEY_D ) {
		app->input.keys[ 'D' ] = ( action != GLFW_RELEASE );
	} else if ( key == GLFW_KEY_A ) {
		app->input.keys[ 'A' ] = ( action != GLFW_RELEASE );
	} else if ( key == GLFW_KEY_W ) {
		app->input.keys[ 'W' ] = ( action != GLFW_RELEASE );
	} else if ( key == GLFW_KEY_S ) {
		app->input.keys[ 'S' ] = ( action != GLFW_RELEASE );
	} else if ( key == GLFW_KEY_KP_ADD ) {
		app->input.keys[ '+' ] = ( action != GLFW_RELEASE );
	} else if ( key == GLFW_KEY_KP_SUBTRACT ) {
		app->input.keys[ '-' ] = ( action != GLFW_RELEASE );
	} else if ( key == GLFW_KEY_LEFT_ALT ) {
		app->input.altDown = ( action != GLFW_RELEASE );
	}
}


void MousePressCallback( GLFWwindow* window, int button, int action, int mods )
{
	Window* app = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );
	mouse_t& mouse = app->input.mouse;

	if ( button == GLFW_MOUSE_BUTTON_LEFT ) {
		mouse.leftDown = true;
	} else {
		mouse.leftDown = false;
	}

	if ( button == GLFW_MOUSE_BUTTON_RIGHT ) {
		mouse.rightDown = true;
	} else {
		mouse.rightDown = false;
	}
}


void MouseMoveCallback( GLFWwindow* window, double xpos, double ypos )
{
	Window* app = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );
	mouse_t& mouse = app->input.mouse;

	mouse.x = static_cast<float>( xpos );
	mouse.y = static_cast<float>( ypos );

	if ( app->input.altDown ) {
		mouse.centered = false;
		return;
	}

	glfwSetCursorPos( window, 0.5f * DISPLAY_WIDTH, 0.5f * DISPLAY_HEIGHT );
	mouse.centered = true;
}

bool Window::IsOpen()
{
	return ( glfwWindowShouldClose( window ) == false );
}

void Window::PumpMessages()
{
	glfwPollEvents();
}

void Window::Init()
{
	glfwInit();

	glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
	glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );

	window = glfwCreateWindow( DISPLAY_WIDTH, DISPLAY_HEIGHT, "Vulkan", nullptr, nullptr );
	glfwSetWindowUserPointer( window, this );
	glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN );
	glfwSetFramebufferSizeCallback( window, FramebufferResizeCallback );
	glfwSetKeyCallback( window, KeyCallback );
	glfwSetMouseButtonCallback( window, MousePressCallback );
	glfwSetCursorPosCallback( window, MouseMoveCallback );
}

void Window::GetWindowPosition( int& x, int& y )
{
	glfwGetWindowPos( window, &x, &y );
}

void Window::GetWindowSize( int& width, int& height )
{
	glfwGetWindowSize( window, &width, &height );
}

void Window::GetWindowFrameBufferSize( int& width, int& height, const bool wait )
{
	glfwGetFramebufferSize( window, &width, &height );
	if( wait )
	{
		while ( width == 0 || height == 0 )
		{
			glfwGetFramebufferSize( window, &width, &height );
			glfwWaitEvents();
		}
	}
}

void Window::CreateSurface()
{
	if ( glfwCreateWindowSurface( context.instance, window, nullptr, &vk_surface ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create window surface!" );
	}
}

void Window::ClearKeyHistory()
{
	memset( input.keys, 0, 255 );
}
