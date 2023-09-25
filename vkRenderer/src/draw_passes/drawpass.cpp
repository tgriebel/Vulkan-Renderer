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

void ShadowPass::Init()
{
	name = "Shadow Pass";
	passId = DRAWPASS_SHADOW;

	stateBits = GFX_STATE_NONE;
	stateBits |= GFX_STATE_DEPTH_TEST;
	stateBits |= GFX_STATE_DEPTH_WRITE;
	stateBits |= GFX_STATE_DEPTH_OP_0;

	sampleRate = imageSamples_t::IMAGE_SMP_1;
}


void DepthPass::Init()
{
	name = "Depth Pass";
	passId = DRAWPASS_DEPTH;

	stateBits = GFX_STATE_NONE;
	stateBits |= GFX_STATE_DEPTH_TEST;
	stateBits |= GFX_STATE_DEPTH_WRITE;
	stateBits |= GFX_STATE_COLOR_MASK;
	stateBits |= GFX_STATE_MSAA_ENABLE;
	stateBits |= GFX_STATE_CULL_MODE_BACK;
	stateBits |= GFX_STATE_STENCIL_ENABLE;

	sampleRate = imageSamples_t::IMAGE_SMP_1;
}


void OpaquePass::Init()
{
	name = "Opaque Pass";
	passId = DRAWPASS_OPAQUE;

	stateBits = GFX_STATE_NONE;
	stateBits |= GFX_STATE_DEPTH_TEST;
	stateBits |= GFX_STATE_DEPTH_WRITE;
	stateBits |= GFX_STATE_CULL_MODE_BACK;
	stateBits |= GFX_STATE_MSAA_ENABLE;

	sampleRate = imageSamples_t::IMAGE_SMP_1;
}


void TransPass::Init()
{
	name = "Trans Pass";
	passId = DRAWPASS_TRANS;

	stateBits = GFX_STATE_NONE;
	stateBits |= GFX_STATE_DEPTH_TEST;
	stateBits |= GFX_STATE_DEPTH_WRITE;
	stateBits |= GFX_STATE_COLOR_MASK;
	stateBits |= GFX_STATE_MSAA_ENABLE;
	stateBits |= GFX_STATE_CULL_MODE_BACK;
	stateBits |= GFX_STATE_STENCIL_ENABLE;

	sampleRate = imageSamples_t::IMAGE_SMP_1;
}


void EmissivePass::Init()
{
	name = "Emissive Pass";
	passId = DRAWPASS_EMISSIVE;

	stateBits = GFX_STATE_NONE;
	stateBits |= GFX_STATE_DEPTH_TEST;
	stateBits |= GFX_STATE_CULL_MODE_BACK;
	stateBits |= GFX_STATE_MSAA_ENABLE;
	stateBits |= GFX_STATE_BLEND_ENABLE;

	sampleRate = imageSamples_t::IMAGE_SMP_1;
}


void PostPass::Init()
{
	name = "Post Pass";
	passId = DRAWPASS_POST_2D;

	stateBits = GFX_STATE_NONE;
	stateBits |= GFX_STATE_BLEND_ENABLE;

	sampleRate = imageSamples_t::IMAGE_SMP_1;
}


void TerrainPass::Init()
{
	name = "Terrain Pass";
	passId = DRAWPASS_TERRAIN;

	stateBits = GFX_STATE_NONE;
	stateBits |= GFX_STATE_DEPTH_TEST;
	stateBits |= GFX_STATE_DEPTH_WRITE;
	stateBits |= GFX_STATE_CULL_MODE_BACK;
	stateBits |= GFX_STATE_MSAA_ENABLE;

	sampleRate = imageSamples_t::IMAGE_SMP_1;
}


void SkyboxPass::Init()
{
	name = "Skybox Pass";
	passId = DRAWPASS_SKYBOX;

	stateBits = GFX_STATE_NONE;
	stateBits |= GFX_STATE_DEPTH_TEST;
	stateBits |= GFX_STATE_DEPTH_WRITE;
	stateBits |= GFX_STATE_CULL_MODE_BACK;
	stateBits |= GFX_STATE_MSAA_ENABLE;

	sampleRate = imageSamples_t::IMAGE_SMP_1;
}


void WireframePass::Init()
{
	name = "Wireframe Pass";
	passId = DRAWPASS_DEBUG_WIREFRAME;

	stateBits = GFX_STATE_NONE;
	stateBits |= GFX_STATE_WIREFRAME_ENABLE;
	stateBits |= GFX_STATE_MSAA_ENABLE;

	sampleRate = imageSamples_t::IMAGE_SMP_1;
}


void Debug2dPass::Init()
{
	name = "Debug 2D Pass";
	passId = DRAWPASS_DEBUG_2D;

	stateBits = GFX_STATE_NONE;
	stateBits |= GFX_STATE_BLEND_ENABLE;

	sampleRate = imageSamples_t::IMAGE_SMP_1;
}


void Debug3dPass::Init()
{
	name = "Debug 3D Pass";
	passId = DRAWPASS_DEBUG_3D;

	stateBits = GFX_STATE_NONE;
	stateBits |= GFX_STATE_CULL_MODE_BACK;
	stateBits |= GFX_STATE_BLEND_ENABLE;
	stateBits |= GFX_STATE_MSAA_ENABLE;

	sampleRate = imageSamples_t::IMAGE_SMP_1;
}