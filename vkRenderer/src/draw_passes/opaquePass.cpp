#include "opaquePass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void OpaquePass::Init( RenderContext* renderContext, FrameBuffer* frameBuffer )
{
	m_name = "Opaque Pass";
	m_passId = DRAWPASS_OPAQUE;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;

	codeImages.SetRenderContext( renderContext );
	codeCubeImages.SetRenderContext( renderContext );

	codeImages.Resize( 3 );

	SetFrameBuffer( frameBuffer );
}


void OpaquePass::FrameBegin( const ResourceContext* resources )
{
	codeImages[ 0 ] = resources->shadowMapImage[ 0 ];
	codeImages[ 1 ] = resources->shadowMapImage[ 1 ];
	codeImages[ 2 ] = resources->shadowMapImage[ 2 ];

	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageCodeCubeArray, &codeCubeImages );
	parms->Bind( bind_imageStencil, rc.whiteImage );
}


void OpaquePass::FrameEnd()
{

}