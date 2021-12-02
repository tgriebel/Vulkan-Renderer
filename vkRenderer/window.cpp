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
	} else if ( key == GLFW_KEY_KP_8 ) {
		app->input.SetKey( '8', ( action != GLFW_RELEASE ) );
	} else if ( key == GLFW_KEY_KP_2 ) {
		app->input.SetKey( '2', ( action != GLFW_RELEASE ) );
	} else if ( key == GLFW_KEY_KP_4 ) {
		app->input.SetKey( '4', ( action != GLFW_RELEASE ) );
	} else if ( key == GLFW_KEY_KP_6 ) {
		app->input.SetKey( '6', ( action != GLFW_RELEASE ) );
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

	static float lastTime = 0.0f;
	const float time = static_cast<float>( glfwGetTime() );
	const float deltaTime = ( time - lastTime );
	lastTime = time;
	const float x = static_cast<float>( xpos );
	const float y = static_cast<float>( ypos );
	if( fabs( mouse.x - x ) < 1.0f ) {
		mouse.dx = 0.0f;
	} else {
		mouse.dx = ( ( mouse.x - x ) < 0.0f ) ? -1.0f : 1.0f;
	}
	if ( fabs( mouse.y - y ) < 1.0f ) {
		mouse.dy = 0.0f;
	} else {
		mouse.dy = ( ( mouse.y - y ) < 0.0f ) ? -1.0f : 1.0f;
	}
	mouse.dx *= deltaTime;
	mouse.dy *= deltaTime;
	mouse.xPrev = mouse.x;
	mouse.yPrev = mouse.y;
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