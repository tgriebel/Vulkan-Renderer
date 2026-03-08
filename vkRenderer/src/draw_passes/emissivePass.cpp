#include "drawpass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void EmissivePass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Emissive Pass";
	m_passId = DRAWPASS_EMISSIVE;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	codeImages.Resize( 3 );

	SetFrameBuffer( frameBuffer );
}


void EmissivePass::FrameBegin( const ResourceContext* resources )
{
	codeImages[ 0 ] = &resources->shadowMapImage[ 0 ];
	codeImages[ 1 ] = &resources->shadowMapImage[ 1 ];
	codeImages[ 2 ] = &resources->shadowMapImage[ 2 ];

	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, rc.whiteImage );
}


void EmissivePass::FrameEnd()
{

}