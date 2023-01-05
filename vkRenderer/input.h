#pragma once

#include "common.h"

struct mouse_t
{
	mouse_t() : speed( 0.2f ),
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