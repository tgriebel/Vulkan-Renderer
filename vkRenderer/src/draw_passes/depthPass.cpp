#include "depthPass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void DepthPass::Init( RenderContext* renderContext, FrameBuffer* frameBuffer )
{
	m_name = "Depth Pass";
	m_passId = DRAWPASS_DEPTH;

	const bool velocityInDepth = true;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_COLOR0_MASK;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;
	m_stateBits |= GFX_STATE_STENCIL_ENABLE;

	if( velocityInDepth == false ) {
		m_stateBits |= GFX_STATE_COLOR1_MASK;
	}

	codeImages.SetRenderContext( renderContext );
	codeCubeImages.SetRenderContext( renderContext );

	SetFrameBuffer( frameBuffer );
}


void DepthPass::FrameBegin( const ResourceContext* resources )
{
	parms->Bind( BINDING_NAME( lightBuffer ),			&resources->lightParms );
	parms->Bind( BINDING_NAME( imageCodeArray ),		&codeImages );
	parms->Bind( BINDING_NAME( imageCodeCubeArray ),	&codeCubeImages );
	parms->Bind( BINDING_NAME( imageStencil ),			rc.whiteImage );
}


void DepthPass::FrameEnd()
{

}