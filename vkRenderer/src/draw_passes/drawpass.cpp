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


void DepthPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Depth Pass";
	m_passId = DRAWPASS_DEPTH;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_COLOR_MASK;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;
	m_stateBits |= GFX_STATE_STENCIL_ENABLE;

	SetFrameBuffer( frameBuffer );
}


void OpaquePass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Opaque Pass";
	m_passId = DRAWPASS_OPAQUE;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;

	SetFrameBuffer( frameBuffer );
}


void TransPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Trans Pass";
	m_passId = DRAWPASS_TRANS;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_COLOR_MASK;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;
	m_stateBits |= GFX_STATE_STENCIL_ENABLE;

	SetFrameBuffer( frameBuffer );
}


void EmissivePass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Emissive Pass";
	m_passId = DRAWPASS_EMISSIVE;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	SetFrameBuffer( frameBuffer );
}


void PostPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Post Pass";
	m_passId = DRAWPASS_POST_2D;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	SetFrameBuffer( frameBuffer );
}


void TerrainPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Terrain Pass";
	m_passId = DRAWPASS_TERRAIN;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;

	SetFrameBuffer( frameBuffer );
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


void WireframePass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Wireframe Pass";
	m_passId = DRAWPASS_DEBUG_WIREFRAME;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_WIREFRAME_ENABLE;

	SetFrameBuffer( frameBuffer );
}


void Debug2dPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Debug 2D Pass";
	m_passId = DRAWPASS_DEBUG_2D;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;

	SetFrameBuffer( frameBuffer );
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