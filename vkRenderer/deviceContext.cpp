#include "deviceContext.h"
#include "swapChain.h"

deviceContext_t context;

bool CheckDeviceExtensionSupport( VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions )
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, nullptr );

	std::vector<VkExtensionProperties> availableExtensions( extensionCount );
	vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, availableExtensions.data() );

	std::set<std::string> requiredExtensions( deviceExtensions.begin(), deviceExtensions.end() );

	for ( const auto& extension : availableExtensions ) {
		requiredExtensions.erase( extension.extensionName );
	}

	return requiredExtensions.empty();
}

bool IsDeviceSuitable( VkPhysicalDevice device, VkSurfaceKHR surface,  const std::vector<const char*>& deviceExtensions )
{
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceProperties( device, &deviceProperties );
	vkGetPhysicalDeviceFeatures( device, &deviceFeatures );

	QueueFamilyIndices indices = FindQueueFamilies( device, surface );

	bool extensionsSupported = CheckDeviceExtensionSupport( device, deviceExtensions );

	bool swapChainAdequate = false;
	if ( extensionsSupported )
	{
		SwapChainSupportDetails swapChainSupport = SwapChain::QuerySwapChainSupport( device, surface );
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures( device, &supportedFeatures );

	return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

QueueFamilyIndices FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface )
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, nullptr );

	std::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
	vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, queueFamilies.data() );

	int i = 0;
	for ( const auto& queueFamily : queueFamilies )
	{
		if ( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
			indices.graphicsFamily.set_value( i );
		}

		if ( queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT ) {
			indices.computeFamily.set_value( i );
		}

		VkBool32 presentSupport = false;

		vkGetPhysicalDeviceSurfaceSupportKHR( device, i, surface, &presentSupport );
		if ( presentSupport ) {
			indices.presentFamily.set_value( i );
		}

		if ( indices.IsComplete() ) {
			break;
		}

		i++;
	}

	return indices;
}

VkImageView CreateImageView( VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels )
{
	VkImageViewCreateInfo viewInfo{ };
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if ( vkCreateImageView( context.device, &viewInfo, nullptr, &imageView ) != VK_SUCCESS )
	{
		throw std::runtime_error( "Failed to create texture image view!" );
	}

	return imageView;
}

VkShaderModule CreateShaderModule( const std::vector<char>& code )
{
	VkShaderModuleCreateInfo createInfo{ };
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>( code.data() );

	VkShaderModule shaderModule;
	if ( vkCreateShaderModule( context.device, &createInfo, nullptr, &shaderModule ) != VK_SUCCESS )
	{
		throw std::runtime_error( "Failed to create shader module!" );
	}

	return shaderModule;
}