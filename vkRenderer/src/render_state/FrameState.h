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

#pragma once

#include "../globals/common.h"
#include "../render_binding/gpuResources.h"
#include "../render_core/gpuImage.h"

enum vkRenderPassAttachmentMask_t : uint8_t
{
	RENDER_PASS_MASK_COLOR0 = ( 1 << 0 ),
	RENDER_PASS_MASK_COLOR1 = ( 1 << 1 ),
	RENDER_PASS_MASK_COLOR2 = ( 1 << 2 ),
	RENDER_PASS_MASK_DEPTH = ( 1 << 3 ),
	RENDER_PASS_MASK_STENCIL = ( 1 << 4 ),
};

struct vkRenderPassAttachmentBits_t
{
	textureSamples_t	samples			: 8;
	textureFmt_t		fmt				: 8;
	uint8_t				clear			: 1;
	uint8_t				store			: 1;
	uint8_t				readAfter		: 1;
};
static_assert( sizeof( textureSamples_t ) == 1, "Bits overflowed" );
static_assert( sizeof( textureFmt_t ) == 1, "Bits overflowed" );
static_assert( sizeof( vkRenderPassAttachmentBits_t ) == 3, "Bits overflowed" );

struct vkRenderPassBits_t
{
	union
	{
		struct vkRenderPassState_t
		{
			vkRenderPassAttachmentBits_t	colorAttach0;
			vkRenderPassAttachmentBits_t	colorAttach1;
			vkRenderPassAttachmentBits_t	colorAttach2;
			vkRenderPassAttachmentBits_t	depthAttach;
			vkRenderPassAttachmentBits_t	stencilAttach;
			vkRenderPassAttachmentMask_t	attachmentMask; // Mask for which attachments are used
		} semantic;
		uint8_t bytes[11];
	};
};
static_assert( sizeof( vkRenderPassBits_t ) == 16, "Bits overflowed" );

class FrameState
{
public:
	Texture			viewColorImage;
	Texture			shadowMapImage;
	Texture			depthImage;
	Texture			stencilImage;

	// FIXME: Lights and surfaces should be relative to a given render view.
	// Textures, materials, and view parms are all global

	GpuBuffer		globalConstants;
	GpuBuffer		viewParms;
	GpuBuffer		surfParms;
	GpuBuffer		materialBuffers;
	GpuBuffer		lightParms;
	GpuBuffer		particleBuffer;

	GpuBufferView	surfParmPartitions[ MaxViews ]; // "View" is used in two ways here: view of data, and view of scene
};


struct frameBufferCreateInfo_t
{
	uint32_t width;
	uint32_t height;
	Texture* color;
	Texture* color1;
	Texture* color2;
	Texture* depth;
	Texture* stencil;
};


class FrameBuffer
{
public:
	Texture*		color[ MAX_FRAMES_STATES ];
	Texture*		color1[ MAX_FRAMES_STATES ];
	Texture*		color2[ MAX_FRAMES_STATES ];
	Texture*		depth[ MAX_FRAMES_STATES ];
	Texture*		stencil[ MAX_FRAMES_STATES ];

#ifdef USE_VULKAN
	VkFramebuffer	buffer[ MAX_FRAMES_STATES ];
#endif

	uint32_t		width;
	uint32_t		height;

	FrameBuffer()
	{
		//color = nullptr;
		//color1 = nullptr;
		//color2 = nullptr;
		//depth = nullptr;
		//stencil = nullptr;

		width = 0;
		height = 0;
	}

	inline uint32_t GetWidth( const uint32_t layer )
	{
		return width;
	}

	inline uint32_t GetHeight( const uint32_t layer )
	{
		return height;
	}

	void Create( const frameBufferCreateInfo_t& createInfo )
	{
		// Can specify clear options, etc. Assigned to cached render pass that matches

		//VkImageView attachments[5];
		//
		//uint32_t attachmentCount = 1;
		//attachments[0] = createInfo.color->gpuImage->GetVkImageView();

		//VkFramebufferCreateInfo framebufferInfo{ };
		//framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		//framebufferInfo.renderPass = shadowPassState.pass;
		//framebufferInfo.attachmentCount = attachmentCount;
		//framebufferInfo.pAttachments = attachments;
		//framebufferInfo.width = ShadowMapWidth;
		//framebufferInfo.height = ShadowMapHeight;
		//framebufferInfo.layers = 1;

		//if ( vkCreateFramebuffer( context.device, &framebufferInfo, nullptr, &shadowMap.buffer[ i ] ) != VK_SUCCESS ) {
		//	throw std::runtime_error( "Failed to create framebuffer!" );
		//}
	}
};