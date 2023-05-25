#pragma once

#include "../globals/common.h"
#include "../render_binding/shaderBinding.h"
#include "../render_state/FrameState.h"

class DrawPass
{
public:
	const char*					name;

	vec4f						clearColor;
	float						clearDepth;
	uint32_t					clearStencil;

	gfxStateBits_t				stateBits;
	textureSamples_t			sampleRate;
	viewport_t					viewport;
	renderPassTransitionFlags_t transitionState;

	Array<Texture*, 100>		codeImages[ MAX_FRAMES_STATES ];
	ShaderBindParms*			parms[ MAX_FRAMES_STATES ];
	FrameBuffer*				fb[ MAX_FRAMES_STATES ];
};


static const char* GetPassDebugName( const drawPass_t pass )
{
	switch ( pass )
	{
		case DRAWPASS_SHADOW:			return "Shadow Pass";
		case DRAWPASS_DEPTH:			return "Depth Pass";
		case DRAWPASS_TERRAIN:			return "Terrain Pass";
		case DRAWPASS_OPAQUE:			return "Opaque Pass";
		case DRAWPASS_SKYBOX:			return "Skybox Pass";
		case DRAWPASS_TRANS:			return "Trans Pass";
		case DRAWPASS_DEBUG_SOLID:		return "Debug Solid Pass";
		case DRAWPASS_DEBUG_WIREFRAME:	return "Wireframe Pass";
		case DRAWPASS_POST_2D:			return "2D Pass";
	};
	return "";
}