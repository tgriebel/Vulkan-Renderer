#pragma once

#include "../globals/common.h"
#include "../render_binding/shaderBinding.h"
#include "../render_state/FrameState.h"

class DrawPass
{
public:
	vec4f				clearColor;
	float				clearDepth;
	uint32_t			clearStencil;

	bool				readAfter;
	bool				presentAfter;

	int32_t				x;
	int32_t				y;
	uint32_t			width;
	uint32_t			height;

	Array<Texture*, 100> codeImages[ MAX_FRAMES_STATES ];
	ShaderBindParms*	parms[ MAX_FRAMES_STATES ];
	FrameBuffer*		fb[ MAX_FRAMES_STATES ];
};