#include "drawpass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void DrawPass::InsertResourceBarriers( CommandContext& cmdContext )
{
	const uint32_t codeImageCount = codeImages.Count();
	for ( uint32_t i = 0; i < codeImageCount; ++i ) {
		// HACK: depth-stencil transitions are creating validation issues
		if ( ( codeImages[ i ]->info.aspect & IMAGE_ASPECT_DEPTH_FLAG ) != 0 ) {
			continue;
		}
		if ( ( codeImages[ i ]->info.aspect & IMAGE_ASPECT_STENCIL_FLAG ) != 0 ) {
			continue;
		}
		Transition( &cmdContext, *codeImages[ i ], GPU_IMAGE_READ, GPU_IMAGE_READ );
	}

	const uint32_t codeCubeImageCount = codeCubeImages.Count();
	for ( uint32_t i = 0; i < codeCubeImageCount; ++i ) {
		// HACK: depth-stencil transitions are creating validation issues
		if ( ( codeCubeImages[ i ]->info.aspect & IMAGE_ASPECT_DEPTH_FLAG ) != 0 ) {
			continue;
		}
		if ( ( codeCubeImages[ i ]->info.aspect & IMAGE_ASPECT_STENCIL_FLAG ) != 0 ) {
			continue;
		}
		Transition( &cmdContext, *codeCubeImages[ i ], GPU_IMAGE_READ, GPU_IMAGE_READ );
	}
}