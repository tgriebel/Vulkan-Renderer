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

#include <resource_types/texture.h>
#include "../globals/common.h"
#include "../../window.h"
#include "../render_state/deviceContext.h"
#include "../render_state/rhi.h"
#include "../render_core/gpuImage.h"
#include "../render_state/FrameState.h"

QueueFamilyIndices FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface );

class SwapChain
{
	static const uint32_t MaxSwapChainBuffers = MAX_FRAMES_STATES;

private:
	const Window*				m_window;
	uint32_t					m_imageCount;
	textureFmt_t				m_swapChainImageFormat;
	Image						m_swapChainImages[ MaxSwapChainBuffers ];
#ifdef USE_VULKAN
	VkSwapchainKHR				vk_swapChain;
#endif
public:
	FrameBuffer					framebuffers[ MaxSwapChainBuffers ];

public:
	inline textureFmt_t GetBackBufferFormat() const
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
	

	inline const FrameBuffer* GetFrameBuffer( const uint32_t i ) const
	{
		return &framebuffers[ i ];
	}


	inline uint32_t GetWidth() const
	{
		return m_swapChainImages[ 0 ].info.width;
	}


	inline uint32_t GetHeight() const
	{
		return m_swapChainImages[ 0 ].info.height;
	}

	void Create( const Window* _window, const int displayWidth, const int displayHeight )
	{
		m_window = _window;
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport( context.physicalDevice, m_window->vk_surface );

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat( swapChainSupport.formats );
		VkPresentModeKHR presentMode = ChooseSwapPresentMode( swapChainSupport.presentModes );
		VkExtent2D extent = ChooseSwapExtent( swapChainSupport.capabilities, displayWidth, displayHeight );

		m_imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if ( swapChainSupport.capabilities.maxImageCount > 0 && m_imageCount > swapChainSupport.capabilities.maxImageCount ) {
			m_imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{ };
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_window->vk_surface;
		createInfo.oldSwapchain = vk_swapChain;
		createInfo.minImageCount = m_imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		if ( context.queueFamilyIndices[ QUEUE_GRAPHICS ] != context.queueFamilyIndices[ QUEUE_PRESENT ] )
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = QUEUE_COUNT;
			createInfo.pQueueFamilyIndices = context.queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if ( vkCreateSwapchainKHR( context.device, &createInfo, nullptr, &vk_swapChain ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create swap chain!" );
		}

		VkImage vk_swapChainImages[ MaxSwapChainBuffers ];
		vkGetSwapchainImagesKHR( context.device, vk_swapChain, &m_imageCount, nullptr );
		assert( m_imageCount <= MaxSwapChainBuffers );
		vkGetSwapchainImagesKHR( context.device, vk_swapChain, &m_imageCount, vk_swapChainImages );

		m_swapChainImageFormat = vk_GetTextureFormat( surfaceFormat.format );

		for( uint32_t i = 0; i < m_imageCount; ++i )
		{
			m_swapChainImages[ i ].info.fmt = m_swapChainImageFormat;
			m_swapChainImages[ i ].info.width = extent.width;
			m_swapChainImages[ i ].info.height = extent.height;
			m_swapChainImages[ i ].info.layers = 1;
			m_swapChainImages[ i ].info.mipLevels = 1;
			m_swapChainImages[ i ].info.channels = 4;
			m_swapChainImages[ i ].info.subsamples = TEXTURE_SMP_1;
			m_swapChainImages[ i ].info.tiling = TEXTURE_TILING_LINEAR;
			m_swapChainImages[ i ].info.type = TEXTURE_TYPE_2D;
			
			m_swapChainImages[ i ].gpuImage = new GpuImage();
			m_swapChainImages[ i ].gpuImage->VkImage() = vk_swapChainImages[ i ];
			m_swapChainImages[ i ].gpuImage->VkImageView() = CreateImageView( m_swapChainImages[ i ] );

			frameBufferCreateInfo_t fbInfo = {};
			fbInfo.color0 = &m_swapChainImages[ i ];
			fbInfo.width = m_swapChainImages[ i ].info.width;
			fbInfo.height = m_swapChainImages[ i ].info.height;

			framebuffers[ i ].Create( fbInfo );
		}
	}

	void Destroy()
	{
		for ( size_t i = 0; i < m_imageCount; i++ ) {
			framebuffers[ i ].Destroy();
		}

		for ( size_t i = 0; i < m_imageCount; i++ )
		{
			// Vulkan swapchain images are a bit special since they need to be destroyed with the swapchain
			m_swapChainImages[ i ].gpuImage->VkImage() = nullptr;
			delete m_swapChainImages[ i ].gpuImage;
		}

		vkDestroySwapchainKHR( context.device, vk_swapChain, nullptr );
	}

	void Recreate()
	{
	
	}

	static SwapChainSupportDetails QuerySwapChainSupport( VkPhysicalDevice device, const VkSurfaceKHR surface )
	{
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, surface, &details.capabilities );

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, &formatCount, nullptr );

		if ( formatCount != 0 )
		{
			details.formats.resize( formatCount );
			vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, &formatCount, details.formats.data() );
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, &presentModeCount, nullptr );

		if ( presentModeCount != 0 )
		{
			details.presentModes.resize( presentModeCount );
			vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, &presentModeCount, details.presentModes.data() );
		}

		return details;
	}


	VkPresentModeKHR ChooseSwapPresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes )
	{
		for ( const auto& availablePresentMode : availablePresentModes )
		{
			if ( availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR )
			{
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

private:
	static VkSurfaceFormatKHR ChooseSwapSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats )
	{
		for ( const auto& availableFormat : availableFormats ) {
			if ( availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR )	{
				return availableFormat;
			}
		}
		return availableFormats[ 0 ];
	}

	static VkExtent2D ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& capabilities, const int displayWidth, const int displayHeight )
	{
		if ( capabilities.currentExtent.width != UINT32_MAX ) {
			return capabilities.currentExtent;
		} else 
		{
			VkExtent2D actualExtent = {
				static_cast<uint32_t>( displayWidth ),
				static_cast<uint32_t>( displayHeight )
			};

			actualExtent.width = std::max( capabilities.minImageExtent.width, std::min( capabilities.maxImageExtent.width, actualExtent.width ) );
			actualExtent.height = std::max( capabilities.minImageExtent.height, std::min( capabilities.maxImageExtent.height, actualExtent.height ) );

			return actualExtent;
		}
	}
};