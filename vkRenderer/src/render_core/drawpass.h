#pragma once

#include "../globals/common.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/shaderBinding.h"
#include "../render_state/FrameState.h"

class DrawPass
{
public:
	const char*					name;

	drawPass_t					passId;
	gfxStateBits_t				stateBits;
	imageSamples_t				sampleRate;
	viewport_t					viewport;

	Array<Image*, 100>			codeImages[ MaxFrameStates ];
	ShaderBindParms*			parms[ MaxFrameStates ];
	FrameBuffer*				fb;

	bool						updateDescriptorSets;
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
		case DRAWPASS_EMISSIVE:			return "Emissive Pass";
		case DRAWPASS_DEBUG_3D:			return "Debug 3D Pass";
		case DRAWPASS_DEBUG_WIREFRAME:	return "Wireframe Pass";
		case DRAWPASS_POST_2D:			return "2D Pass";
		case DRAWPASS_DEBUG_2D:			return "Debug Pass";
	};
	return "";
}