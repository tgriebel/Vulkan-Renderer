#include "debug2dPass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void Debug2dPass::Init( RenderContext* renderContext, FrameBuffer* frameBuffer )
{
	m_name = "Debug 2D Pass";
	m_passId = DRAWPASS_DEBUG_2D;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	codeImages.SetRenderContext( renderContext );
	codeCubeImages.SetRenderContext( renderContext );

	codeImages.Resize( 2 );

	SetFrameBuffer( frameBuffer );
}


void Debug2dPass::FrameBegin( const ResourceContext* resources )
{
	uint32_t uploadId = 0;
	const uint32_t codeImageCount = codeImages.Count();
	for( uint32_t i = 0; i < codeImageCount; ++i )
	{
		if( resources->gpuOutput2D[ i ].info.subsamples != imageSamples_t::IMAGE_SMP_1 ) {
		//	continue;
		}
		if ( resources->gpuOutput2D[ i ].info.type != imageType_t::IMAGE_TYPE_2D ) {
		//	continue;
		}
	//	codeImages[ uploadId++ ] = &resources->gpuOutput2D[ i ];
	}
	codeImages[ 0 ] = resources->mainColorResolvedImage;
	codeImages[ 1 ] = resources->depthStencilResolvedImage;

	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageCodeCubeArray, &codeCubeImages );
	parms->Bind( bind_imageStencil, &resources->stencilResolvedImageView );
}


void Debug2dPass::FrameEnd()
{

}