/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

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