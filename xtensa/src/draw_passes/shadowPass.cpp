#include "shadowPass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void ShadowPass::Init( RenderContext* renderContext, FrameBuffer* frameBuffer )
{
	m_name = "Shadow Pass";
	m_passId = DRAWPASS_SHADOW;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_DEPTH_OP_0;
	// m_stateBits |= GFX_STATE_CULL_MODE_FRONT; // can be used to reduce peter-panning, but not used b/c it fails with planes

	codeImages.SetRenderContext( renderContext );
	codeCubeImages.SetRenderContext( renderContext );

	SetFrameBuffer( frameBuffer );
}


void ShadowPass::FrameBegin( const ResourceContext* resources )
{
	parms->Bind( BINDING_NAME( lightBuffer ),			&resources->lightParms );
	parms->Bind( BINDING_NAME( imageCodeArray ),		&codeImages );
	parms->Bind( BINDING_NAME( imageCodeCubeArray ),	&codeCubeImages );
	parms->Bind( BINDING_NAME( imageStencil ),			rc.whiteImage );
}


void ShadowPass::FrameEnd()
{

}
