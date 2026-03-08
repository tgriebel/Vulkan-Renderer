#include "drawpass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void Debug2dPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Debug 2D Pass";
	m_passId = DRAWPASS_DEBUG_2D;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	codeImages.Resize( 2 );

	SetFrameBuffer( frameBuffer );
}


void Debug2dPass::FrameBegin( const ResourceContext* resources )
{
	codeImages[ 0 ] = &resources->mainColorResolvedImage;
	codeImages[ 1 ] = &resources->depthStencilResolvedImage;

	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, &resources->stencilResolvedImageView );
}


void Debug2dPass::FrameEnd()
{

}