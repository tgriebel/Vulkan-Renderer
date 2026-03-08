#include "drawpass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void WireframePass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Wireframe Pass";
	m_passId = DRAWPASS_DEBUG_WIREFRAME;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_WIREFRAME_ENABLE;

	SetFrameBuffer( frameBuffer );
}


void WireframePass::FrameBegin( const ResourceContext* resources )
{
	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, rc.whiteImage );
}


void WireframePass::FrameEnd()
{

}