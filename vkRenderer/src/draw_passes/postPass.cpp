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

	codeImages.Resize( 6 );

	SetFrameBuffer( frameBuffer );
}


void PostPass::FrameBegin( const ResourceContext* resources )
{
	codeImages.BindIndex( 0, resources->mainColorResolvedImage );
	codeImages.BindIndex( 1, resources->depthStencilResolvedImage );
	codeImages.BindIndex( 2, resources->dofBokeh );
	codeImages.BindIndex( 3, resources->currentLum );
	codeImages.BindIndex( 4, resources->bloom );
	codeImages.BindIndex( 5, resources->dofCocImage );

	parms->Bind( BINDING_NAME( lightBuffer ),			&resources->lightParms );
	parms->Bind( BINDING_NAME( imageCodeArray ),		&codeImages );
	parms->Bind( BINDING_NAME( imageCodeCubeArray ),	&codeCubeImages );
	parms->Bind( BINDING_NAME( imageStencil ),			&resources->stencilResolvedImageView );
}


void PostPass::FrameEnd()
{

}