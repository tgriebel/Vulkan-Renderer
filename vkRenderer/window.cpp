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
		app->input.SetKey( 'D', ( action != GLFW_RELEASE ) );
	} else if ( key == GLFW_KEY_A ) {
		app->input.SetKey( 'A', ( action != GLFW_RELEASE ) );
	} else if ( key == GLFW_KEY_W ) {
		app->input.SetKey( 'W', ( action != GLFW_RELEASE ) );
	} else if ( key == GLFW_KEY_S ) {
		app->input.SetKey( 'S', ( action != GLFW_RELEASE ) );
	} else if ( key == GLFW_KEY_KP_ADD ) {
		app->input.SetKey( '+', ( action != GLFW_RELEASE ) );
	} else if ( key == GLFW_KEY_KP_SUBTRACT ) {
		app->input.SetKey( '-', ( action != GLFW_RELEASE ) );
	} else if ( key == GLFW_KEY_LEFT_ALT ) {
		app->focused = ( action != GLFW_RELEASE );
	}
}


void MousePressCallback( GLFWwindow* window, int button, int action, int mods )
{
	Window* app = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );
	mouse_t& mouse = app->input.GetMouseRef();

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
	mouse_t& mouse = app->input.GetMouseRef();

	const float x = static_cast<float>( xpos );
	const float y = static_cast<float>( ypos );
	mouse.dx = ( mouse.x - x );
	mouse.dy = ( mouse.y - y );
	mouse.x = x;
	mouse.y = y;

	if ( app->IsFocused() ) {
		mouse.centered = false;
		glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
		return;
	}
	glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
	mouse.centered = true;
}

bool Window::IsOpen() const
{
	return ( glfwWindowShouldClose( window ) == false );
}

bool Window::IsFocused() const
{
	return focused;
}

void Window::PumpMessages()
{
	input.NewFrame();
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