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

	codeImages.SetRenderContext( renderContext );
	codeCubeImages.SetRenderContext( renderContext );

	SetFrameBuffer( frameBuffer );
}


void ShadowPass::FrameBegin( const ResourceContext* resources )
{
	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageCodeCubeArray, &codeCubeImages );
	parms->Bind( bind_imageStencil, rc.whiteImage );
}


void ShadowPass::FrameEnd()
{

}