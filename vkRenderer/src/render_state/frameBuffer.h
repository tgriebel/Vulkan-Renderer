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

struct frameBufferCreateInfo_t
{
	uint32_t			width;
	uint32_t			height;
	resourceLifetime_t	lifetime;
	const char*			name;
	Image*				color0[ MaxFrameStates ];
	Image*				color1[ MaxFrameStates ];
	Image*				color2[ MaxFrameStates ];
	Image*				depth[ MaxFrameStates ];
	Image*				stencil[ MaxFrameStates ];

	frameBufferCreateInfo_t() :
		width( 0 ),
		height( 0 ),
		lifetime( LIFETIME_TEMP )
	{
		name = "";

		memset( color0, 0, sizeof( Image* ) * MaxFrameStates );
		memset( color1, 0, sizeof( Image* ) * MaxFrameStates );
		memset( color2, 0, sizeof( Image* ) * MaxFrameStates );
		memset( depth, 0, sizeof( Image* ) * MaxFrameStates );
		memset( stencil, 0, sizeof( Image* ) * MaxFrameStates );
	}
};


class FrameBuffer
{
private:
	static const uint32_t MaxAttachmentCount = 5;

	Image*						color0[ MaxFrameStates ];
	Image*						color1[ MaxFrameStates ];
	Image*						color2[ MaxFrameStates ];
	Image*						depth[ MaxFrameStates ];
	Image*						stencil[ MaxFrameStates ];

	uint32_t					width;
	uint32_t					height;
	uint32_t					colorCount;
	uint32_t					dsCount;
	uint32_t					attachmentCount;
	uint32_t					bufferCount;

	resourceLifetime_t			lifetime;

#ifdef USE_VULKAN
	VkFramebuffer				buffers[ MaxFrameStates ][ PassPermCount ];
	VkRenderPass				renderPasses[ PassPermCount ];
#endif

	inline uint32_t GetBufferId( const uint32_t bufferId = 0 ) const
	{
		const uint32_t bufferCount = ( lifetime == LIFETIME_PERSISTENT ) ? MaxFrameStates : 1;
		return Min( bufferId, bufferCount - 1 );
	}

public:

	FrameBuffer() :
		attachmentCount( 0 ),
		bufferCount( 0 ),
		colorCount( 0 ),
		dsCount( 0 ),
		width( 0 ),
		height( 0 )
	{
		memset( color0, 0, sizeof( Image* ) * MaxFrameStates );
		memset( color1, 0, sizeof( Image* ) * MaxFrameStates );
		memset( color2, 0, sizeof( Image* ) * MaxFrameStates );
		memset( depth, 0, sizeof( Image* ) * MaxFrameStates );
		memset( stencil, 0, sizeof( Image* ) * MaxFrameStates );
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

	inline uint32_t ColorLayerCount() const
	{
		return colorCount;
	}

	inline uint32_t DepthLayerCount() const
	{
		return dsCount;
	}

	inline uint32_t LayerCount() const
	{
		return attachmentCount;
	}

	inline imageSamples_t SampleCount() const
	{
		return ( ColorLayerCount() > 0 ) ? GetColor()->info.subsamples : GetDepth()->info.subsamples;
	}

	inline const Image* GetColor( const uint32_t bufferId = 0 ) const
	{
		return ( colorCount > 0 ) ? color0[ GetBufferId( bufferId ) ] : nullptr;
	}

	inline const Image* GetColor1( const uint32_t bufferId = 0 ) const
	{
		return ( colorCount > 1 ) ? color1[ GetBufferId( bufferId ) ] : nullptr;
	}

	inline const Image* GetColor2( const uint32_t bufferId = 0 ) const
	{
		return ( colorCount > 2 ) ? color2[ GetBufferId( bufferId ) ] : nullptr;
	}

	inline const Image* GetDepth( const uint32_t bufferId = 0 ) const
	{
		return ( dsCount >= 1 ) ? depth[ GetBufferId( bufferId ) ] : nullptr;
	}

	inline const Image* GetStencil( const uint32_t bufferId = 0 ) const
	{
		return ( dsCount >= 1 ) ? stencil[ GetBufferId( bufferId ) ] : nullptr;
	}

#ifdef USE_VULKAN
	VkFramebuffer GetVkBuffer( const renderPassTransition_t& transitionState = {}, const uint32_t bufferId = 0 ) const
	{
		const uint32_t id = GetBufferId( bufferId );
		return buffers[ id ][ transitionState.bits ];
	}

	VkRenderPass GetVkRenderPass( const renderPassTransition_t& transitionState = {}, const uint32_t bufferId = 0 ) const
	{
		const uint32_t id = GetBufferId( bufferId );
		return renderPasses[ transitionState.bits ];
	}
#endif

	void Create( const frameBufferCreateInfo_t& createInfo );
	void Destroy();
};