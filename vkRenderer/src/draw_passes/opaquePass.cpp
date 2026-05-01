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

	codeImages.Resize( 4 );

	SetFrameBuffer( frameBuffer );
}


void OpaquePass::FrameBegin( const ResourceContext* resources )
{
	codeImages.BindIndex( 0, resources->shadowMapImage[ 0 ] );
	codeImages.BindIndex( 1, resources->shadowMapImage[ 1 ] );
	codeImages.BindIndex( 2, resources->shadowMapImage[ 2 ] );
	codeImages.BindIndex( 3, resources->ssaoImage );

	parms->Bind( BINDING_NAME( lightBuffer ),			&resources->lightParms );
	parms->Bind( BINDING_NAME( imageCodeArray ),		&codeImages );
	parms->Bind( BINDING_NAME( imageCodeCubeArray ),	&codeCubeImages );
	parms->Bind( BINDING_NAME( imageStencil ),			rc.whiteImage );
}


void OpaquePass::FrameEnd()
{

}