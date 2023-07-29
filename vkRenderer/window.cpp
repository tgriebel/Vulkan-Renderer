#include "window.h"
#include "src/render_state/deviceContext.h"

#include "external/tinyfiledialogs/tinyfiledialogs.h"

#if defined( USE_IMGUI )
#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#endif

#define KEY_MAP( k ) { GLFW_KEY_##k, KEY_##k }

struct KeyPair
{
	int		glfwKey;
	key_t	key;
};

static KeyPair GlfwKeyMap[] =
{
	KEY_MAP( 0 ),
	KEY_MAP( 1 ),
	KEY_MAP( 2 ),
	KEY_MAP( 3 ),
	KEY_MAP( 4 ),
	KEY_MAP( 5 ),
	KEY_MAP( 6 ),
	KEY_MAP( 7 ),
	KEY_MAP( 8 ),
	KEY_MAP( 9 ),
	KEY_MAP( A ),
	KEY_MAP( B ),
	KEY_MAP( C ),
	KEY_MAP( D ),
	KEY_MAP( E ),
	KEY_MAP( F ),
	KEY_MAP( G ),
	KEY_MAP( H ),
	KEY_MAP( I ),
	KEY_MAP( J ),
	KEY_MAP( K ),
	KEY_MAP( L ),
	KEY_MAP( M ),
	KEY_MAP( N ),
	KEY_MAP( O ),
	KEY_MAP( P ),
	KEY_MAP( Q ),
	KEY_MAP( R ),
	KEY_MAP( S ),
	KEY_MAP( T ),
	KEY_MAP( U ),
	KEY_MAP( V ),
	KEY_MAP( W ),
	KEY_MAP( X ),
	KEY_MAP( Y ),
	KEY_MAP( Z ),

	{ GLFW_KEY_KP_0,		KEY_NUM_0 },
	{ GLFW_KEY_KP_1,		KEY_NUM_1 },
	{ GLFW_KEY_KP_2,		KEY_NUM_2 },
	{ GLFW_KEY_KP_3,		KEY_NUM_3 },
	{ GLFW_KEY_KP_4,		KEY_NUM_4 },
	{ GLFW_KEY_KP_5,		KEY_NUM_5 },
	{ GLFW_KEY_KP_6,		KEY_NUM_6 },
	{ GLFW_KEY_KP_7,		KEY_NUM_7 },
	{ GLFW_KEY_KP_8,		KEY_NUM_8 },
	{ GLFW_KEY_KP_9,		KEY_NUM_9 },
	{ GLFW_KEY_KP_ADD,		KEY_ADD },
	{ GLFW_KEY_KP_SUBTRACT,	KEY_SUB},
	{ GLFW_KEY_KP_MULTIPLY,	KEY_MUL },
	{ GLFW_KEY_KP_DIVIDE,	KEY_DIV },

	{ GLFW_KEY_LEFT_ALT,	KEY_LEFT_ALT },
	{ GLFW_KEY_RIGHT_ALT,	KEY_RIGHT_ALT },
	{ GLFW_KEY_LEFT,		KEY_LEFT_ARROW },
	{ GLFW_KEY_RIGHT,		KEY_RIGHT_ARROW },
	{ GLFW_KEY_UP,			KEY_UP_ARROW },
	{ GLFW_KEY_DOWN,		KEY_DOWN_ARROW },
};

#undef KEY_MAP

void FramebufferResizeCallback( GLFWwindow* window, int width, int height )
{
	Window* app = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );
	app->RequestImageResize();
}


void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods )
{
	Window* app = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );

	const uint32_t mappedKeys = COUNTARRAY( GlfwKeyMap );

	for( uint32_t k = 0; k < mappedKeys; ++k )
	{
		if( key == GlfwKeyMap[k].glfwKey ) {
			app->input.SetKey( GlfwKeyMap[ k ].key, ( action != GLFW_RELEASE ) );
			break;
		}
	}

	if ( key == GLFW_KEY_LEFT_ALT ) {
		if ( action != GLFW_RELEASE ) {
			app->focused = !app->focused;
		}
	}
}


void MousePressCallback( GLFWwindow* window, int button, int action, int mods )
{
	Window* app = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );
	mouse_t& mouse = app->input.GetMouseRef();

	if ( button == GLFW_MOUSE_BUTTON_LEFT ) {
		mouse.leftDown = ( action == GLFW_RELEASE ) ? false : true;
	}

	if ( button == GLFW_MOUSE_BUTTON_RIGHT ) {
		mouse.rightDown = ( action == GLFW_RELEASE ) ? false : true;;
	}
}


void MouseMoveCallback( GLFWwindow* window, double xpos, double ypos )
{
	Window* app = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );
	mouse_t& mouse = app->input.GetMouseRef();

	int w, h;
	app->GetWindowSize( w, h );
	const vec2f prevNdc = app->GetNdc( mouse.x, mouse.y );
	const vec2f ndc = app->GetNdc( static_cast<float>( xpos ), static_cast<float>( ypos ) );

	static float lastTime = 0.0f;
	const float time = static_cast<float>( glfwGetTime() );
	const float deltaTime = ( time - lastTime );
	lastTime = time;
	mouse.dx = ( mouse.x - ndc[0] );
	mouse.dy = ( mouse.y - ndc[1] );
	mouse.xPrev = mouse.x;
	mouse.yPrev = mouse.y;
	mouse.x = ndc[0];
	mouse.y = ndc[1];

	if ( app->IsFocused() ) {
		mouse.centered = false;
		glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
		return;
	}
	glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
	mouse.centered = true;
}


void Window::BeginFrame()
{
	input.NewFrame();

#if defined( USE_IMGUI )
	ImGui::NewFrame();
	ImGui_ImplGlfw_NewFrame();
#endif
}


void Window::EndFrame()
{

}


bool Window::IsOpen() const
{
	return ( glfwWindowShouldClose( window ) == false );
}


bool Window::IsFocused() const
{
	return focused;
}


std::string Window::OpenFileDialog( const std::string& title, const std::vector<const char*>& filters, const std::string& filterDesc )
{
	tinyfd_assumeGraphicDisplay = 1;

	char const* fileFilter[1] = { "*.obj" };

	char* fileName = tinyfd_openFileDialog(
		title.c_str(),
		"",
		static_cast<int>(filters.size()),
		filters.data(),
		filterDesc.c_str(),
		0 );

	return ( fileName == nullptr ) ? "" : fileName;
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

	window = glfwCreateWindow( DefaultDisplayWidth, DefaultDisplayHeight, ApplicationName, nullptr, nullptr );
	glfwSetWindowUserPointer( window, this );
	glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN );
	glfwSetFramebufferSizeCallback( window, FramebufferResizeCallback );
	glfwSetKeyCallback( window, KeyCallback );
	glfwSetMouseButtonCallback( window, MousePressCallback );
	glfwSetCursorPosCallback( window, MouseMoveCallback );
}

vec2f Window::GetNdc( const float x, const float y )
{
	const vec2f screenPoint = vec2f( x, y );
	int w, h;
	GetWindowSize( w, h );
	const vec2f ndc = 2.0f * Multiply( screenPoint, vec2f( 1.0f / w, 1.0f / h ) ) - vec2f( 1.0f );
	return ndc;
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

float Window::GetWindowFrameBufferAspect( const bool wait )
{
	int width, height;
	GetWindowFrameBufferSize( width, height, wait );
	return width / static_cast<float>( height );
}

void Window::CreateSurface()
{
	if ( glfwCreateWindowSurface( context.instance, window, nullptr, &vk_surface ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create window surface!" );
	}
}