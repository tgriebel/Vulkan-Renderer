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
#include "rhi.h"
#include <gfxcore/asset_types/texture.h>
#include "../render_binding/imageView.h"

class FrameState
{
public:
	Image			viewColorImage;
	Image			shadowMapImage[ MaxShadowViews ];
	Image			depthStencilImage;
	ImageView		depthImageView;
	ImageView		stencilImageView;

	// FIXME: Lights and surfaces should be relative to a given render view.
	// Textures, materials, and view parms are all global

	GpuBuffer		globalConstants;
	GpuBuffer		viewParms;
	GpuBuffer		surfParms;
	GpuBuffer		materialBuffers;
	GpuBuffer		lightParms;
	GpuBuffer		particleBuffer;

	GpuBufferView	surfParmPartitions[ MaxViews ]; // "View" is used in two ways here: view of data, and view of scene

	void Create();
	void Destroy();
};


struct frameBufferCreateInfo_t
{
	uint32_t			width;
	uint32_t			height;
	uint32_t			bufferCount;
	resourceLifetime_t	lifetime;
	Image*				color0[ MAX_FRAMES_STATES ];
	Image*				color1[ MAX_FRAMES_STATES ];
	Image*				color2[ MAX_FRAMES_STATES ];
	Image*				depth[ MAX_FRAMES_STATES ];
	Image*				stencil[ MAX_FRAMES_STATES ];

	frameBufferCreateInfo_t() :
		width( 0 ),
		height( 0 ),
		bufferCount( 0 ),
		lifetime( LIFETIME_TEMP )
	{
		memset( color0, 0, sizeof( Image* ) * MAX_FRAMES_STATES );
		memset( color1, 0, sizeof( Image* ) * MAX_FRAMES_STATES );
		memset( color2, 0, sizeof( Image* ) * MAX_FRAMES_STATES );
		memset( depth, 0, sizeof( Image* ) * MAX_FRAMES_STATES );
		memset( stencil, 0, sizeof( Image* ) * MAX_FRAMES_STATES );
	}
};


class FrameBuffer
{
private:
	Image*				color0[ MAX_FRAMES_STATES ];
	Image*				color1[ MAX_FRAMES_STATES ];
	Image*				color2[ MAX_FRAMES_STATES ];
	Image*				depth[ MAX_FRAMES_STATES ];
	Image*				stencil[ MAX_FRAMES_STATES ];

	uint32_t			width;
	uint32_t			height;
	uint32_t			colorCount;
	uint32_t			dsCount;
	uint32_t			attachmentCount;

	resourceLifetime_t	lifetime;

#ifdef USE_VULKAN
	VkFramebuffer		buffers[ MAX_FRAMES_STATES ][ PassPermCount ];
	VkRenderPass		renderPasses[ PassPermCount ];
#endif

public:

	FrameBuffer()
	{
		attachmentCount = 0;
		colorCount = 0;
		dsCount = 0;
		width = 0;
		height = 0;
	}

	inline bool IsValid() const
	{
		return ( buffers != VK_NULL_HANDLE );
	}

	inline uint32_t GetWidth() const
	{
		return width;
	}

	inline uint32_t GetHeight() const
	{
		return height;
	}

	inline uint32_t GetColorLayers() const
	{
		return colorCount;
	}

	inline uint32_t GetDepthLayers() const
	{
		return colorCount;
	}

	inline uint32_t GetLayers() const
	{
		return attachmentCount;
	}

	inline const Image* GetColor( const uint32_t bufferId = 0 ) const
	{
		return ( colorCount > 0 ) ? color0[ bufferId ] : nullptr;
	}

	inline const Image* GetColor1( const uint32_t bufferId = 0 ) const
	{
		return ( colorCount > 1 ) ? color1[ bufferId ] : nullptr;
	}

	inline const Image* GetColor2( const uint32_t bufferId = 0 ) const
	{
		return ( colorCount > 2 ) ? color2[ bufferId ] : nullptr;
	}

	inline const Image* GetDepth( const uint32_t bufferId = 0 ) const
	{
		return ( dsCount >= 1 ) ? depth[ bufferId ] : nullptr;
	}

	inline const Image* GetStencil( const uint32_t bufferId = 0 ) const
	{
		return ( dsCount >= 1 ) ? stencil[ bufferId ] : nullptr;
	}

#ifdef USE_VULKAN
	VkFramebuffer GetVkBuffer( renderPassTransitionFlags_t transitionState = {}, const uint32_t bufferId = 0 ) const
	{
		return buffers[ bufferId ][ transitionState.bits ];
	}

	VkRenderPass GetVkRenderPass( renderPassTransitionFlags_t transitionState = {} )
	{
		return renderPasses[ transitionState.bits ];
	}
#endif

	void Create( const frameBufferCreateInfo_t& createInfo );
	void Destroy();
};