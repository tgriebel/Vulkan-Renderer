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

#include <gfxcore/asset_types/texture.h>
#include "../globals/common.h"
#include "../../window.h"
#include "../render_state/deviceContext.h"
#include "../render_state/rhi.h"
#include "../render_core/gpuImage.h"
#include "../render_state/FrameState.h"

QueueFamilyIndices FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface );

class SwapChain
{
	static const uint32_t MaxSwapChainBuffers = MaxFrameStates;

private:
	const Window*				m_window;
	uint32_t					m_imageCount;
	imageFmt_t					m_swapChainImageFormat;
	Image						m_swapChainImages[ MaxSwapChainBuffers ];
#ifdef USE_VULKAN
	VkSwapchainKHR				vk_swapChain;
#endif
public:
	FrameBuffer					framebuffers;

public:

	static swapChainInfo_t SwapChain::QuerySwapChainSupport( VkPhysicalDevice device, const VkSurfaceKHR surface );

	inline imageFmt_t GetBackBufferFormat() const
	{
		return m_swapChainImageFormat;
	}

#ifdef USE_VULKAN
	inline VkSwapchainKHR GetVkObject() const
	{
		return vk_swapChain;
	}
#endif

	inline uint32_t GetBufferCount() const
	{
		return m_imageCount;
	}
	

	inline const FrameBuffer* GetFrameBuffer() const
	{
		return &framebuffers;
	}


	inline uint32_t GetWidth() const
	{
		return m_swapChainImages[ 0 ].info.width;
	}


	inline uint32_t GetHeight() const
	{
		return m_swapChainImages[ 0 ].info.height;
	}

	void Create( const Window* _window, const int displayWidth, const int displayHeight );
	void Destroy();
};