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
	resourceLifetime_t	lifetime;
	const char*			name;
	Image*				color0[ MaxFrameStates ];
	Image*				color1[ MaxFrameStates ];
	Image*				color2[ MaxFrameStates ];
	Image*				depth[ MaxFrameStates ];
	Image*				stencil[ MaxFrameStates ];

	frameBufferCreateInfo_t() :
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

	Image*						m_color0[ MaxFrameStates ];
	Image*						m_color1[ MaxFrameStates ];
	Image*						m_color2[ MaxFrameStates ];
	Image*						m_depth[ MaxFrameStates ];
	Image*						m_stencil[ MaxFrameStates ];

	uint32_t					m_width;
	uint32_t					m_height;
	uint32_t					m_colorCount;
	uint32_t					m_dsCount;
	uint32_t					m_attachmentCount;
	uint32_t					m_bufferCount;

	resourceLifetime_t			m_lifetime;
	renderPassAttachmentMask_t	m_attachmentMask;
	renderAttachmentBits_t		m_attachmentBits;

#ifdef USE_VULKAN
	VkFramebuffer				vk_buffers[ MaxFrameStates ][ PassPermCount ];
	VkRenderPass				vk_renderPasses[ PassPermCount ];
#endif

	inline uint32_t GetBufferId( const uint32_t bufferId = 0 ) const
	{
		const uint32_t bufferCount = ( m_lifetime == LIFETIME_PERSISTENT ) ? MaxFrameStates : 1;
		return Min( bufferId, bufferCount - 1 );
	}

public:

	FrameBuffer() :
		m_attachmentCount( 0 ),
		m_bufferCount( 0 ),
		m_colorCount( 0 ),
		m_dsCount( 0 ),
		m_width( 0 ),
		m_height( 0 )
	{
		for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx )
		{
			m_color0[ frameIx ] = nullptr;
			m_color1[ frameIx ] = nullptr;
			m_color2[ frameIx ] = nullptr;
			m_depth[ frameIx ] = nullptr;
			m_stencil[ frameIx ] = nullptr;
		}
	}

	inline bool IsValid() const
	{
		return ( vk_buffers != VK_NULL_HANDLE );
	}

	inline uint32_t GetWidth() const
	{
		return m_width;
	}

	inline uint32_t GetHeight() const
	{
		return m_height;
	}

	inline uint32_t ColorLayerCount() const
	{
		return m_colorCount;
	}

	inline uint32_t DepthLayerCount() const
	{
		return m_dsCount;
	}

	inline uint32_t LayerCount() const
	{
		return m_attachmentCount;
	}

	inline imageSamples_t SampleCount() const
	{
		return ( ColorLayerCount() > 0 ) ? GetColor()->info.subsamples : GetDepth()->info.subsamples;
	}

	inline const Image* GetColor( const uint32_t bufferId = 0 ) const
	{
		return ( m_colorCount > 0 ) ? m_color0[ GetBufferId( bufferId ) ] : nullptr;
	}

	inline const Image* GetColor1( const uint32_t bufferId = 0 ) const
	{
		return ( m_colorCount > 1 ) ? m_color1[ GetBufferId( bufferId ) ] : nullptr;
	}

	inline const Image* GetColor2( const uint32_t bufferId = 0 ) const
	{
		return ( m_colorCount > 2 ) ? m_color2[ GetBufferId( bufferId ) ] : nullptr;
	}

	inline const Image* GetDepth( const uint32_t bufferId = 0 ) const
	{
		return ( m_dsCount >= 1 ) ? m_depth[ GetBufferId( bufferId ) ] : nullptr;
	}

	inline const Image* GetStencil( const uint32_t bufferId = 0 ) const
	{
		return ( m_dsCount >= 1 ) ? m_stencil[ GetBufferId( bufferId ) ] : nullptr;
	}

	inline renderAttachmentBits_t GetAttachmentBits() const
	{
		return m_attachmentBits;
	}

	inline renderPassAttachmentMask_t GetAttachmentMask() const
	{
		return m_attachmentMask;
	}

#ifdef USE_VULKAN
	VkFramebuffer GetVkBuffer( const renderPassTransition_t& transitionState = {}, const uint32_t bufferId = 0 ) const
	{
		const uint32_t id = GetBufferId( bufferId );
		assert( id < MaxFrameStates );
		return vk_buffers[ id ][ transitionState.bits ];
	}

	VkRenderPass GetVkRenderPass( const renderPassTransition_t& transitionState = {}, const uint32_t bufferId = 0 ) const
	{
		assert( transitionState.bits < PassPermCount );
		return vk_renderPasses[ transitionState.bits ];
	}
#endif

	void Create( const frameBufferCreateInfo_t& createInfo );
	void Destroy();
};