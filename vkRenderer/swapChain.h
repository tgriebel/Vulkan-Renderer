#pragma once

#include "common.h"

class SwapChain
{
public:
	void CreateImageViews()
	{
	}

	void Create( VkPhysicalDevice physicalDevice, QueueFamilyIndices& queueIndices, uint32_t queueFamilyIndices[ QUEUE_COUNT ] )
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport( physicalDevice );

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat( swapChainSupport.formats );
		VkPresentModeKHR presentMode = ChooseSwapPresentMode( swapChainSupport.presentModes );
		VkExtent2D extent = ChooseSwapExtent( swapChainSupport.capabilities );

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if ( swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount )
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{ };
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = vk_surface;
		createInfo.oldSwapchain = vk_swapChain;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		if ( queueIndices.graphicsFamily.value() != queueIndices.presentFamily.value() )
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = QUEUE_COUNT;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
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

		if ( vkCreateSwapchainKHR( context.device, &createInfo, nullptr, &vk_swapChain ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create swap chain!" );
		}

		vkGetSwapchainImagesKHR( context.device, vk_swapChain, &imageCount, nullptr );
		vk_swapChainImages.resize( imageCount );
		vkGetSwapchainImagesKHR( context.device, vk_swapChain, &imageCount, vk_swapChainImages.data() );

		vk_swapChainImageFormat = surfaceFormat.format;
		vk_swapChainExtent = extent;

		vk_swapChainImageViews.resize( vk_swapChainImages.size() );

		for ( size_t i = 0; i < vk_swapChainImages.size(); i++ )
		{
			vk_swapChainImageViews[ i ] = CreateImageView( context.device, vk_swapChainImages[ i ], vk_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1 );
		}
	}

	void Destroy()
	{
		// Untested
		for ( size_t i = 0; i < vk_framebuffers.size(); i++ ) {
			vkDestroyFramebuffer( context.device, vk_framebuffers[ i ], nullptr );
		}

		for ( size_t i = 0; i < vk_swapChainImageViews.size(); i++ ) {
			vkDestroyImageView( context.device, vk_swapChainImageViews[ i ], nullptr );
		}

		vkDestroySwapchainKHR( context.device, vk_swapChain, nullptr );
	}


	void CreateSurface()
	{
	
	}

	void Recreate()
	{
	
	}

	SwapChainSupportDetails QuerySwapChainSupport( VkPhysicalDevice device )
	{
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, vk_surface, &details.capabilities );

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR( device, vk_surface, &formatCount, nullptr );

		if ( formatCount != 0 )
		{
			details.formats.resize( formatCount );
			vkGetPhysicalDeviceSurfaceFormatsKHR( device, vk_surface, &formatCount, details.formats.data() );
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR( device, vk_surface, &presentModeCount, nullptr );

		if ( presentModeCount != 0 )
		{
			details.presentModes.resize( presentModeCount );
			vkGetPhysicalDeviceSurfacePresentModesKHR( device, vk_surface, &presentModeCount, details.presentModes.data() );
		}

		return details;
	}

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats )
	{
		for ( const auto& availableFormat : availableFormats )
		{
			if ( availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR )
			{
				return availableFormat;
			}
		}

		return availableFormats[ 0 ];
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

	VkExtent2D ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& capabilities )
	{
		if ( capabilities.currentExtent.width != UINT32_MAX ) {
			return capabilities.currentExtent;
		} else {
			VkExtent2D actualExtent = {
				static_cast<uint32_t>( DISPLAY_WIDTH ),
				static_cast<uint32_t>( DISPLAY_HEIGHT )
			};

			actualExtent.width = std::max( capabilities.minImageExtent.width, std::min( capabilities.maxImageExtent.width, actualExtent.width ) );
			actualExtent.height = std::max( capabilities.minImageExtent.height, std::min( capabilities.maxImageExtent.height, actualExtent.height ) );

			return actualExtent;
		}
	}

	VkFormat GetBackBufferFormat() const {
		return vk_swapChainImageFormat;
	}

	VkSwapchainKHR GetApiObject() const {
		return vk_swapChain;
	}

	uint32_t GetBufferCount() const {
		return static_cast<uint32_t>( vk_swapChainImageViews.size() );
	}

	static const uint32_t MaxSwapChainBuffers = 4;

//private:
	uint32_t				currentImage;
	uint32_t				imageCount;

	VkSwapchainKHR				vk_swapChain;
	VkSurfaceKHR				vk_surface;
	std::vector<VkImage>		vk_swapChainImages;
	std::vector<VkImageView>	vk_swapChainImageViews;
	std::vector<VkFramebuffer>	vk_framebuffers;
	VkFormat					vk_swapChainImageFormat;
	VkExtent2D					vk_swapChainExtent;
};