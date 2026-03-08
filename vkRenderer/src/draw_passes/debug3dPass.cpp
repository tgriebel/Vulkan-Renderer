#include "drawpass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void Debug3dPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Debug 3D Pass";
	m_passId = DRAWPASS_DEBUG_3D;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	SetFrameBuffer( frameBuffer );
}


void Debug3dPass::FrameBegin( const ResourceContext* resources )
{
	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, rc.whiteImage );
}


void Debug3dPass::FrameEnd()
{

}