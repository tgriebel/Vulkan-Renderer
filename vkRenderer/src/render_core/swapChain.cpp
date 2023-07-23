#include "swapChain.h"


VkPresentModeKHR vk_ChooseSwapPresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes )
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


static VkSurfaceFormatKHR vk_ChooseSwapSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats )
{
	for ( const auto& availableFormat : availableFormats ) {
		if ( availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
			return availableFormat;
		}
	}
	return availableFormats[ 0 ];
}


static VkExtent2D vk_ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& capabilities, const int displayWidth, const int displayHeight )
{
	if ( capabilities.currentExtent.width != UINT32_MAX ) {
		return capabilities.currentExtent;
	}
	else
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


swapChainInfo_t SwapChain::QuerySwapChainSupport( VkPhysicalDevice device, const VkSurfaceKHR surface )
{
	swapChainInfo_t info;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, surface, &info.capabilities );

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, &formatCount, nullptr );

	if ( formatCount != 0 )
	{
		info.formats.resize( formatCount );
		vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, &formatCount, info.formats.data() );
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, &presentModeCount, nullptr );

	if ( presentModeCount != 0 )
	{
		info.presentModes.resize( presentModeCount );
		vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, &presentModeCount, info.presentModes.data() );
	}

	return info;
}


void SwapChain::Create( const Window* _window, const int displayWidth, const int displayHeight )
{
	m_window = _window;
	swapChainInfo_t swapChainSupport = QuerySwapChainSupport( context.physicalDevice, m_window->vk_surface );

	VkSurfaceFormatKHR surfaceFormat = vk_ChooseSwapSurfaceFormat( swapChainSupport.formats );
	VkPresentModeKHR presentMode = vk_ChooseSwapPresentMode( swapChainSupport.presentModes );
	VkExtent2D extent = vk_ChooseSwapExtent( swapChainSupport.capabilities, displayWidth, displayHeight );

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

	for ( uint32_t i = 0; i < m_imageCount; ++i )
	{
		imageInfo_t& info = m_swapChainImages[ i ].info;

		info.fmt = m_swapChainImageFormat;
		info.width = extent.width;
		info.height = extent.height;
		info.layers = 1;
		info.mipLevels = 1;
		info.channels = 4;
		info.subsamples = IMAGE_SMP_1;
		info.tiling = IMAGE_TILING_LINEAR;
		info.type = IMAGE_TYPE_2D;

		m_swapChainImages[ i ].gpuImage = new GpuImage( "_backbuffer", vk_swapChainImages[ i ], vk_CreateImageView( vk_swapChainImages[ i ], info ) );
	}

	frameBufferCreateInfo_t fbInfo = {};
	for ( uint32_t i = 0; i < m_imageCount; ++i ) {
		fbInfo.color0[ i ] = &m_swapChainImages[ i ];
	}
	fbInfo.width = m_swapChainImages[ 0 ].info.width;
	fbInfo.height = m_swapChainImages[ 0 ].info.height;
	fbInfo.lifetime = LIFETIME_PERSISTENT;

	framebuffers.Create( fbInfo );
}


void SwapChain::Destroy()
{
	framebuffers.Destroy();

	for ( size_t i = 0; i < m_imageCount; i++ )
	{
		// Vulkan swapchain images are a bit special since they need to be destroyed with the swapchain
		m_swapChainImages[ i ].gpuImage->DetachVkImage();
		delete m_swapChainImages[ i ].gpuImage;
	}
	vkDestroySwapchainKHR( context.device, vk_swapChain, nullptr );
}