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
		Transition( &cmdContext, *codeImages[ i ], GPU_IMAGE_WRITE, GPU_IMAGE_READ );
	}
}

void ShadowPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Shadow Pass";
	m_passId = DRAWPASS_SHADOW;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_DEPTH_OP_0;

	SetFrameBuffer( frameBuffer );
}


void ShadowPass::FrameBegin( const ResourceContext* resources )
{
	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, &rc.whiteImage );
}


void ShadowPass::FrameEnd()
{

}


void DepthPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Depth Pass";
	m_passId = DRAWPASS_DEPTH;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_COLOR0_MASK;
	m_stateBits |= GFX_STATE_COLOR1_MASK;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;
	m_stateBits |= GFX_STATE_STENCIL_ENABLE;

	SetFrameBuffer( frameBuffer );
}


void DepthPass::FrameBegin( const ResourceContext* resources )
{
	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, &rc.whiteImage );
}


void DepthPass::FrameEnd()
{

}


void OpaquePass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Opaque Pass";
	m_passId = DRAWPASS_OPAQUE;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;

	codeImages.Resize( 3 );

	SetFrameBuffer( frameBuffer );
}


void OpaquePass::FrameBegin( const ResourceContext* resources )
{
	codeImages[ 0 ] = &resources->shadowMapImage[ 0 ];
	codeImages[ 1 ] = &resources->shadowMapImage[ 1 ];
	codeImages[ 2 ] = &resources->shadowMapImage[ 2 ];

	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, &rc.whiteImage );
}


void OpaquePass::FrameEnd()
{

}


void TransPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Trans Pass";
	m_passId = DRAWPASS_TRANS;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	codeImages.Resize( 3 );

	SetFrameBuffer( frameBuffer );
}

void TransPass::FrameBegin( const ResourceContext* resources )
{
	codeImages[ 0 ] = &resources->shadowMapImage[ 0 ];
	codeImages[ 1 ] = &resources->shadowMapImage[ 1 ];
	codeImages[ 2 ] = &resources->shadowMapImage[ 2 ];

	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, &rc.whiteImage );
}


void TransPass::FrameEnd()
{

}


void EmissivePass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Emissive Pass";
	m_passId = DRAWPASS_EMISSIVE;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	codeImages.Resize( 3 );

	SetFrameBuffer( frameBuffer );
}


void EmissivePass::FrameBegin( const ResourceContext* resources )
{
	codeImages[ 0 ] = &resources->shadowMapImage[ 0 ];
	codeImages[ 1 ] = &resources->shadowMapImage[ 1 ];
	codeImages[ 2 ] = &resources->shadowMapImage[ 2 ];

	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, &rc.whiteImage );
}


void EmissivePass::FrameEnd()
{

}


void PostPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Post Pass";
	m_passId = DRAWPASS_POST_2D;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	codeImages.Resize( 2 );

	SetFrameBuffer( frameBuffer );
}


void PostPass::FrameBegin( const ResourceContext* resources )
{
	codeImages[ 0 ] = &resources->mainColorResolvedImage;
	codeImages[ 1 ] = &resources->depthStencilResolvedImage;

	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, &resources->stencilResolvedImageView );
}


void PostPass::FrameEnd()
{

}


void TerrainPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Terrain Pass";
	m_passId = DRAWPASS_TERRAIN;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;

	codeImages.Resize( 3 );

	SetFrameBuffer( frameBuffer );
}


void TerrainPass::FrameBegin( const ResourceContext* resources )
{
	codeImages[ 0 ] = &resources->shadowMapImage[ 0 ];
	codeImages[ 1 ] = &resources->shadowMapImage[ 1 ];
	codeImages[ 2 ] = &resources->shadowMapImage[ 2 ];

	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, &rc.whiteImage );
}


void TerrainPass::FrameEnd()
{

}


void SkyboxPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Skybox Pass";
	m_passId = DRAWPASS_SKYBOX;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;

	SetFrameBuffer( frameBuffer );
}


void SkyboxPass::FrameBegin( const ResourceContext* resources )
{
	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, &rc.whiteImage );
}


void SkyboxPass::FrameEnd()
{

}


void WireframePass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Wireframe Pass";
	m_passId = DRAWPASS_DEBUG_WIREFRAME;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_WIREFRAME_ENABLE;

	SetFrameBuffer( frameBuffer );
}


void WireframePass::FrameBegin( const ResourceContext* resources )
{
	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, &rc.whiteImage );
}


void WireframePass::FrameEnd()
{

}


void Debug2dPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Debug 2D Pass";
	m_passId = DRAWPASS_DEBUG_2D;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	codeImages.Resize( 2 );

	SetFrameBuffer( frameBuffer );
}


void Debug2dPass::FrameBegin( const ResourceContext* resources )
{
	codeImages[ 0 ] = &resources->mainColorResolvedImage;
	codeImages[ 1 ] = &resources->depthStencilResolvedImage;

	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, &resources->stencilResolvedImageView );
}


void Debug2dPass::FrameEnd()
{

}


void Debug3dPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Debug 3D Pass";
	m_passId = DRAWPASS_DEBUG_3D;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	SetFrameBuffer( frameBuffer );
}


void Debug3dPass::FrameBegin( const ResourceContext* resources )
{
	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageStencil, &rc.whiteImage );
}


void Debug3dPass::FrameEnd()
{

}