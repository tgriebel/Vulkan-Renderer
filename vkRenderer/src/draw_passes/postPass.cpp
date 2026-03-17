#include "postPass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void PostPass::Init( RenderContext* renderContext, FrameBuffer* frameBuffer )
{
	m_name = "Post Pass";
	m_passId = DRAWPASS_2D;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	codeImages.SetRenderContext( renderContext );
	codeCubeImages.SetRenderContext( renderContext );

	codeImages.Resize( 4 );

	SetFrameBuffer( frameBuffer );
}


void PostPass::FrameBegin( const ResourceContext* resources )
{
	codeImages.BindIndex( 0, resources->mainColorResolvedImage );
	codeImages.BindIndex( 1, resources->depthStencilResolvedImage );
	codeImages.BindIndex( 2, resources->blurredImage );
	codeImages.BindIndex( 3, resources->currentLum );

	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageCodeCubeArray, &codeCubeImages );
	parms->Bind( bind_imageStencil, &resources->stencilResolvedImageView );
}


void PostPass::FrameEnd()
{

}