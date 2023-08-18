#include "postEffect.h"

void PostEffect::Init( const char* name, FrameBuffer& fb, const bool clear, const bool present )
{
	dbgName = name;

	DrawPass* pass = new DrawPass();

	pass->name = GetPassDebugName( DRAWPASS_POST_2D );
	pass->viewport.x = 0;
	pass->viewport.y = 0;
	pass->viewport.width = fb.GetWidth();
	pass->viewport.height = fb.GetHeight();
	pass->fb = &fb;

	pass->transitionState.bits = 0;
	pass->stateBits = GFX_STATE_NONE;
	pass->passId = drawPass_t( DRAWPASS_POST_2D );

	pass->transitionState.flags.clear = clear;
	pass->transitionState.flags.store = true;
	pass->transitionState.flags.presentAfter = present;
	pass->transitionState.flags.readAfter = !present;

	pass->clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );
	pass->clearDepth = 0.0f;
	pass->clearStencil = 0;

	pass->stateBits |= GFX_STATE_BLEND_ENABLE;
	pass->sampleRate = imageSamples_t::IMAGE_SMP_1;
}


void PostEffect::Shutdown()
{
	delete pass;
}


void PostEffect::Execute( ShaderBindParms* parms )
{
	
}