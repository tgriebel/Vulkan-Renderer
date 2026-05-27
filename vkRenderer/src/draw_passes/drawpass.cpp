#include "drawpass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void DrawPass::InsertResourceBarriers( CommandList& cmdContext )
{
	const uint32_t codeImageCount = codeImages.Count();
	for ( uint32_t i = 0; i < codeImageCount; ++i )
	{
		if( codeImages[ i ] == nullptr ) {
			continue;
		}

		// HACK: depth-stencil transitions are creating validation issues
		if ( IsDepthStencilCompatible( codeImages[ i ]->info.fmt ) ) {
			continue;
		}
		Transition( &cmdContext, *codeImages[ i ], GPU_IMAGE_READ, GPU_IMAGE_READ );
	}

	const uint32_t codeCubeImageCount = codeCubeImages.Count();
	for ( uint32_t i = 0; i < codeCubeImageCount; ++i )
	{
		if( codeCubeImages[ i ] == nullptr ) {
			continue;
		}

		// HACK: depth-stencil transitions are creating validation issues
		if( IsDepthStencilCompatible( codeImages[ i ]->info.fmt ) ) {
			continue;
		}
		Transition( &cmdContext, *codeCubeImages[ i ], GPU_IMAGE_READ, GPU_IMAGE_READ );
	}
}