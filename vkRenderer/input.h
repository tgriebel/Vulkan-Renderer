#pragma once

#include "src/globals/common.h"

#define KEY( k ) KEY_##k = (#k[0])

enum key_t
{
	KEY( 0 ),
	KEY( 1 ),
	KEY( 2 ),
	KEY( 3 ),
	KEY( 4 ),
	KEY( 5 ),
	KEY( 6 ),
	KEY( 7 ),
	KEY( 8 ),
	KEY( 9 ),
	KEY( A ),
	KEY( B ),
	KEY( C ),
	KEY( D ),
	KEY( E ),
	KEY( F ),
	KEY( G ),
	KEY( H ),
	KEY( I ),
	KEY( J ),
	KEY( K ),
	KEY( L ),
	KEY( M ),
	KEY( N ),
	KEY( O ),
	KEY( P ),
	KEY( Q ),
	KEY( R ),
	KEY( S ),
	KEY( T ),
	KEY( U ),
	KEY( V ),
	KEY( W ),
	KEY( X ),
	KEY( Y ),
	KEY( Z ),
	KEY_UP_ARROW,
	KEY_DOWN_ARROW,
	KEY_LEFT_ARROW,
	KEY_RIGHT_ARROW,
	KEY_LEFT_ALT,
	KEY_RIGHT_ALT,
	KEY_LEFT_CTRL,
	KEY_RIGHT_CTRL,
	KEY_LEFT_SHIFT,
	KEY_RIGHT_SHIFT,
	KEY_ENTER,
	KEY_SPACE,
	KEY_TAB,
	KEY_CAP,
	KEY_TILDA,
	KEY_F1,
	KEY_F2,
	KEY_F3,
	KEY_F4,
	KEY_F5,
	KEY_F6,
	KEY_F7,
	KEY_F8,
	KEY_F9,
	KEY_F10,
	KEY_F11,
	KEY_F12,
	KEY_BACKSPACE,
	KEY_DELETE,
	KEY_ADD,
	KEY_SUB,
	KEY_MUL,
	KEY_DIV,
	KEY_NUM_0,
	KEY_NUM_1,
	KEY_NUM_2,
	KEY_NUM_3,
	KEY_NUM_4,
	KEY_NUM_5,
	KEY_NUM_6,
	KEY_NUM_7,
	KEY_NUM_8,
	KEY_NUM_9,
};

#undef KEY

struct mouse_t
{
	mouse_t() : speed( 2.0f ),
		x( 0.0f ),
		y( 0.0f ),
		xPrev( 0.0f ),
		yPrev( 0.0f ),
		dx( 0.0f ),
		dy( 0.0f ),
		leftDown( false ),
		rightDown( false ),
		centered( false ) {}

	float	x;
	float	y;
	float	xPrev;
	float	yPrev;
	float	dx; // dx/dt
	float	dy; // dy/dt
	float	speed;
	bool	leftDown;
	bool	rightDown;
	bool	centered;
};


class Input
{
public:
	Input()
	{
		rBufferId = 0;
		wBufferId = 0;
		ClearKeyHistory();
	}

	bool IsKeyPressed( const char key ) {
		return keys[ rBufferId ][ key ];
	}

	const mouse_t& GetMouse() const {
		return mouse[ rBufferId ];
	}

	void NewFrame()
	{
		rBufferId = wBufferId;
		wBufferId = ( wBufferId + 1 ) % 2;
		mouse[ wBufferId ] = mouse[ rBufferId ];
		mouse[ wBufferId ].dx = 0.0f;
		mouse[ wBufferId ].dy = 0.0f;
		memcpy( keys[ wBufferId ], keys[ rBufferId ], 255 );
	}

private:
	mouse_t	mouse[ 2 ];
	int		rBufferId;
	int		wBufferId;
	bool	keys[ 2 ][ 256 ];

	void SetKey( const char key, const bool value ) {
		keys[ wBufferId ][ key ] = value;
	}

	mouse_t& GetMouseRef() {
		return mouse[ wBufferId ];
	}

	void ClearKeyHistory() {
		memset( keys[ 0 ], 0, 255 );
		memset( keys[ 1 ], 0, 255 );
	}

	friend void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods );
	friend void MousePressCallback( GLFWwindow* window, int button, int action, int mods );
	friend void MouseMoveCallback( GLFWwindow* window, double xpos, double ypos );
};


void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods );
void MousePressCallback( GLFWwindow* window, int button, int action, int mods );
void MouseMoveCallback( GLFWwindow* window, double xpos, double ypos );