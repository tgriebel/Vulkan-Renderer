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

#include "deviceContext.h"
#include "../render_core/swapChain.h"
#include "../render_core/drawpass.h"
#include "../render_core/gpuImage.h"

DeviceContext context;

bool vk_CheckDeviceExtensionSupport( VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions )
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


bool vk_IsDeviceSuitable( VkPhysicalDevice device, VkSurfaceKHR surface,  const std::vector<const char*>& deviceExtensions )
{
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties( device, &deviceProperties );

	QueueFamilyIndices indices = vk_FindQueueFamilies( device, surface );

	bool extensionsSupported = vk_CheckDeviceExtensionSupport( device, deviceExtensions );

	bool swapChainAdequate = false;
	if ( extensionsSupported )
	{
		swapChainInfo_t swapChainSupport = SwapChain::QuerySwapChainSupport( device, surface );
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures( device, &supportedFeatures );

	return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}


QueueFamilyIndices vk_FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface )
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


bool vk_ValidTextureFormat( const VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features )
{
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties( context.physicalDevice, format, &props );

	if ( tiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & features ) == features ) {
		return true;
	}
	else if ( tiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & features ) == features ) {
		return true;
	}
	return false;
}


uint32_t vk_FindMemoryType( uint32_t typeFilter, VkMemoryPropertyFlags properties )
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );

	for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ )
	{
		if ( typeFilter & ( 1 << i ) && ( memProperties.memoryTypes[ i ].propertyFlags & properties ) == properties )
		{
			return i;
		}
	}

	throw std::runtime_error( "Failed to find suitable memory type!" );
}


VkImageView vk_CreateImageView( const VkImage image, const imageInfo_t& info )
{
	VkImageAspectFlags aspectFlags = 0;
	aspectFlags |= ( info.aspect & IMAGE_ASPECT_COLOR_FLAG ) != 0 ? VK_IMAGE_ASPECT_COLOR_BIT : 0;
	aspectFlags |= ( info.aspect & IMAGE_ASPECT_DEPTH_FLAG ) != 0 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
	aspectFlags |= ( info.aspect & IMAGE_ASPECT_STENCIL_FLAG ) != 0 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;

	VkImageViewCreateInfo viewInfo{ };
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = vk_GetTextureType( info.type );
	viewInfo.format = vk_GetTextureFormat( info.fmt );
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = info.mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = ( info.type == IMAGE_TYPE_CUBE ) ? 6 : 1;

	VkImageView imageView;
	if ( vkCreateImageView( context.device, &viewInfo, nullptr, &imageView ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create texture image view!" );
	}
	return imageView;
}


void vk_TransitionImageLayout( VkCommandBuffer cmdBuffer, Image& image, gpuImageStateFlags_t current, gpuImageStateFlags_t next )
{
	VkImageMemoryBarrier barrier{ };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image.gpuImage->GetVkImage();
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = image.info.mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = image.info.layers;

	const bool hasColorAspect = ( image.info.aspect & IMAGE_ASPECT_COLOR_FLAG ) != 0;
	const bool hasDepthAspect = ( image.info.aspect & IMAGE_ASPECT_DEPTH_FLAG ) != 0;
	const bool hasStencilAspect = ( image.info.aspect & IMAGE_ASPECT_STENCIL_FLAG ) != 0;

	VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( ( current & GPU_IMAGE_READ ) != 0 ) {
		oldLayout = hasColorAspect ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	}
	else if ( ( current & GPU_IMAGE_PRESENT ) != 0 ) {
		oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	else if ( ( current & GPU_IMAGE_TRANSFER_SRC ) != 0 ) {
		oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
	else if ( ( current & GPU_IMAGE_TRANSFER_DST ) != 0 ) {
		oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}

	if ( ( next & GPU_IMAGE_READ ) != 0 ) {
		newLayout = hasColorAspect ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	}
	else if ( ( next & GPU_IMAGE_PRESENT ) != 0 ) {
		newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	else if ( ( next & GPU_IMAGE_TRANSFER_SRC ) != 0 ) {
		newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
	else if ( ( next & GPU_IMAGE_TRANSFER_DST ) != 0 ) {
		newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}

	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;

	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_NONE;
	barrier.subresourceRange.aspectMask |= hasColorAspect ? VK_IMAGE_ASPECT_COLOR_BIT : 0;
	barrier.subresourceRange.aspectMask |= hasDepthAspect ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
	barrier.subresourceRange.aspectMask |= hasStencilAspect ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if ( oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL )
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if ( oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL )
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else if ( oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL )
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if ( oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL )
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if ( oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR )
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else if ( oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL )
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else
	{
		throw std::invalid_argument( "unsupported layout transition!" );
	}

	vkCmdPipelineBarrier(
		cmdBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}


void vk_GenerateMipmaps( VkCommandBuffer cmdBuffer, Image& image )
{
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties( context.physicalDevice, vk_GetTextureFormat( image.info.fmt ), &formatProperties );

	if ( !( formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT ) )
	{
		throw std::runtime_error( "texture image format does not support linear blitting!" );
	}

	VkImageMemoryBarrier barrier{ };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image.gpuImage->GetVkImage();
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = image.info.layers;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = image.info.width;
	int32_t mipHeight = image.info.height;

	for ( uint32_t i = 1; i < image.info.mipLevels; i++ )
	{
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(	cmdBuffer,
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								0,
								0, nullptr,
								0, nullptr,
								1, &barrier );

		VkImageBlit blit{ };
		blit.srcOffsets[ 0 ] = { 0, 0, 0 };
		blit.srcOffsets[ 1 ] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = image.info.layers;
		blit.dstOffsets[ 0 ] = { 0, 0, 0 };
		blit.dstOffsets[ 1 ] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = image.info.layers;

		vkCmdBlitImage( cmdBuffer,
						image.gpuImage->GetVkImage(),
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						image.gpuImage->GetVkImage(),
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1,
						&blit,
						VK_FILTER_LINEAR );

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(	cmdBuffer,
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								0,
								0, nullptr,
								0, nullptr,
								1, &barrier );

		if ( mipWidth > 1 ) mipWidth /= 2;
		if ( mipHeight > 1 ) mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = image.info.mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(	cmdBuffer,
							VK_PIPELINE_STAGE_TRANSFER_BIT,
							VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							0,
							0, nullptr,
							0, nullptr,
							1, &barrier );
}


void vk_CopyBufferToImage( VkCommandBuffer cmdBuffer, Image& texture, GpuBuffer& buffer, const uint64_t bufferOffset )
{
	const uint32_t layers = texture.info.layers;

	VkBufferImageCopy region{ };
	memset( &region, 0, sizeof( region ) );
	region.bufferOffset = bufferOffset;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = layers;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		texture.info.width,
		texture.info.height,
		1
	};

	vkCmdCopyBufferToImage( cmdBuffer,
							buffer.GetVkObject(),
							texture.gpuImage->GetVkImage(),
							VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							1,
							&region
	);
}


VkShaderModule vk_CreateShaderModule( const std::vector<char>& code )
{
	VkShaderModuleCreateInfo createInfo{ };
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>( code.data() );

	VkShaderModule shaderModule;
	if ( vkCreateShaderModule( context.device, &createInfo, nullptr, &shaderModule ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create shader module!" );
	}

	return shaderModule;
}


VkResult vk_CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger )
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
	if ( func != nullptr ) {
		return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}


void vk_MarkerSetObjectTag( uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag )
{
	if ( context.debugMarkersEnabled )
	{
		VkDebugMarkerObjectTagInfoEXT tagInfo = {};
		tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
		tagInfo.objectType = objectType;
		tagInfo.object = object;
		tagInfo.tagName = name;
		tagInfo.tagSize = tagSize;
		tagInfo.pTag = tag;
		context.fnDebugMarkerSetObjectTag( context.device, &tagInfo );
	}
}


void vk_MarkerSetObjectName( uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name )
{
	if ( context.debugMarkersEnabled )
	{
		VkDebugMarkerObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = objectType;
		nameInfo.object = object;
		nameInfo.pObjectName = name;
		context.fnDebugMarkerSetObjectName( context.device, &nameInfo );
	}
}


static VKAPI_ATTR VkBool32 VKAPI_CALL vk_DebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData )
{

	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}


void vk_PopulateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo )
{
	createInfo = { };
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	createInfo.messageSeverity = 0;
	if ( ValidateVerbose ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	}

	if ( ValidateWarnings ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	}

	if ( ValidateErrors ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	}

	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = vk_DebugCallback;
}


bool vk_CheckValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

	std::vector<VkLayerProperties> availableLayers( layerCount );
	vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

	for ( const char* layerName : context.validationLayers ) {
		bool layerFound = false;
		for ( const auto& layerProperties : availableLayers ) {
			if ( strcmp( layerName, layerProperties.layerName ) == 0 ) {
				layerFound = true;
				break;
			}
		}
		if ( !layerFound ) {
			return false;
		}
	}
	return true;
}


std::vector<const char*> vk_GetRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

	std::vector<const char*> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );

	if ( EnableValidationLayers ) {
		extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
	}

	return extensions;
}


void DeviceContext::Create( Window& window )
{
	// Create Instance
	{
		if ( EnableValidationLayers && !vk_CheckValidationLayerSupport() )
		{
			throw std::runtime_error( "validation layers requested, but not available!" );
		}

		VkApplicationInfo appInfo{ };
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Extensia";
		appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo createInfo{ };
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );
		std::vector<VkExtensionProperties> extensionProperties( extensionCount );
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensionProperties.data() );

		std::cout << "available extensions:\n";

		for ( const auto& extension : extensionProperties )
		{
			std::cout << '\t' << extension.extensionName << '\n';
		}

		uint32_t glfwExtensionCount = 0;

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if ( EnableValidationLayers )
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>( validationLayers.size() );
			createInfo.ppEnabledLayerNames = validationLayers.data();

			vk_PopulateDebugMessengerCreateInfo( debugCreateInfo );
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		auto extensions = vk_GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>( extensions.size() );
		createInfo.ppEnabledExtensionNames = extensions.data();

		if ( vkCreateInstance( &createInfo, nullptr, &instance ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create instance!" );
		}
	}

	// Debug Messenger
	{
		if ( !EnableValidationLayers ) {
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		vk_PopulateDebugMessengerCreateInfo( createInfo );

		if ( vk_CreateDebugUtilsMessengerEXT( instance, &createInfo, nullptr, &debugMessenger ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to set up debug messenger!" );
		}
	}

	// Window Surface
	{
		window.CreateGlfwSurface();
	}

	// Pick physical device
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices( instance, &deviceCount, nullptr );

		if ( deviceCount == 0 ) {
			throw std::runtime_error( "Failed to find GPUs with Vulkan support!" );
		}

		std::vector<VkPhysicalDevice> devices( deviceCount );
		vkEnumeratePhysicalDevices( instance, &deviceCount, devices.data() );

		for ( const auto& device : devices )
		{
			if ( vk_IsDeviceSuitable( device, window.vk_surface, deviceExtensions ) )
			{
				vkGetPhysicalDeviceProperties( device, &deviceProperties );
				vkGetPhysicalDeviceFeatures( device, &deviceFeatures );
				physicalDevice = device;
				break;
			}
		}

		if ( physicalDevice == VK_NULL_HANDLE ) {
			throw std::runtime_error( "Failed to find a suitable GPU!" );
		}
	}

	// Create logical device
	{
		QueueFamilyIndices indices = vk_FindQueueFamilies( physicalDevice, window.vk_surface );
		queueFamilyIndices[ QUEUE_GRAPHICS ] = indices.graphicsFamily.value();
		queueFamilyIndices[ QUEUE_PRESENT ] = indices.presentFamily.value();
		queueFamilyIndices[ QUEUE_COMPUTE ] = indices.computeFamily.value();

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value(), indices.computeFamily.value() };

		float queuePriority = 1.0f;
		for ( uint32_t queueFamily : uniqueQueueFamilies )
		{
			VkDeviceQueueCreateInfo queueCreateInfo{ };
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back( queueCreateInfo );
		}

		VkDeviceCreateInfo createInfo{ };
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>( queueCreateInfos.size() );
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		VkPhysicalDeviceFeatures deviceFeatures{ };
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.fillModeNonSolid = VK_TRUE;
		deviceFeatures.sampleRateShading = VK_TRUE;
		deviceFeatures.pipelineStatisticsQuery = VK_TRUE;
		createInfo.pEnabledFeatures = &deviceFeatures;

		std::vector<const char*> enabledExtensions;
		for ( auto ext : deviceExtensions )
		{
			enabledExtensions.push_back( ext );
		}
		std::vector<const char*> captureExtensions = { VK_EXT_DEBUG_MARKER_EXTENSION_NAME };
		if ( vk_IsDeviceSuitable( physicalDevice, window.vk_surface, captureExtensions ) ) {
			for ( const char* ext : captureExtensions ) {
				enabledExtensions.push_back( ext );
			}
		}

		createInfo.enabledExtensionCount = static_cast<uint32_t>( enabledExtensions.size() );
		createInfo.ppEnabledExtensionNames = enabledExtensions.data();
		if ( EnableValidationLayers )
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>( validationLayers.size() );
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VkPhysicalDeviceDescriptorIndexingFeatures descIndexing;
		memset( &descIndexing, 0, sizeof( VkPhysicalDeviceDescriptorIndexingFeatures ) );
		descIndexing.runtimeDescriptorArray = true;
		descIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		descIndexing.pNext = NULL;
		createInfo.pNext = &descIndexing;

		if ( vkCreateDevice( physicalDevice, &createInfo, nullptr, &device ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create logical context.device!" );
		}

		vkGetDeviceQueue( device, indices.graphicsFamily.value(), 0, &gfxContext );
		vkGetDeviceQueue( device, indices.presentFamily.value(), 0, &presentQueue );
		vkGetDeviceQueue( device, indices.computeFamily.value(), 0, &computeContext );
	}

	// Debug Markers
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, &extensionCount, nullptr );

		std::vector<VkExtensionProperties> availableExtensions( extensionCount );
		vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, &extensionCount, availableExtensions.data() );

		bool found = false;
		for ( const auto& extension : availableExtensions ) {
			if ( strcmp( extension.extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME ) == 0 ) {
				found = true;
				break;
			}
		}

		if ( found )
		{
			std::cout << "Enabling debug markers." << std::endl;

			fnDebugMarkerSetObjectTag = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr( device, "vkDebugMarkerSetObjectTagEXT" );
			fnDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr( device, "vkDebugMarkerSetObjectNameEXT" );
			fnCmdDebugMarkerBegin = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr( device, "vkCmdDebugMarkerBeginEXT" );
			fnCmdDebugMarkerEnd = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr( device, "vkCmdDebugMarkerEndEXT" );
			fnCmdDebugMarkerInsert = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr( device, "vkCmdDebugMarkerInsertEXT" );

			debugMarkersEnabled = true;
			debugMarkersEnabled = debugMarkersEnabled && ( fnDebugMarkerSetObjectTag != VK_NULL_HANDLE );
			debugMarkersEnabled = debugMarkersEnabled && ( fnDebugMarkerSetObjectName != VK_NULL_HANDLE );
			debugMarkersEnabled = debugMarkersEnabled && ( fnCmdDebugMarkerBegin != VK_NULL_HANDLE );
			debugMarkersEnabled = debugMarkersEnabled && ( fnCmdDebugMarkerEnd != VK_NULL_HANDLE );
			debugMarkersEnabled = debugMarkersEnabled && ( fnCmdDebugMarkerInsert != VK_NULL_HANDLE );
		}
		else {
			std::cout << "Debug markers \"" << VK_EXT_DEBUG_MARKER_EXTENSION_NAME << "\" disabled." << std::endl;
		}
	}

	// Default Bilinear Sampler
	{
		VkSamplerCreateInfo samplerInfo{ };
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = 16.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 16.0f;
		samplerInfo.mipLodBias = 0.0f;

		if ( vkCreateSampler( device, &samplerInfo, nullptr, &bilinearSampler ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create texture sampler!" );
		}
	}

	// Depth sampler
	{
		VkSamplerCreateInfo samplerInfo{ };
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 0.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 16.0f;
		samplerInfo.mipLodBias = 0.0f;

		if ( vkCreateSampler( device, &samplerInfo, nullptr, &depthShadowSampler ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create depth sampler!" );
		}
	}

	// Descriptor Pool
	{
		std::array<VkDescriptorPoolSize, 3> poolSizes{ };
		poolSizes[ 0 ].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[ 0 ].descriptorCount = DescriptorPoolMaxUniformBuffers;
		poolSizes[ 1 ].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[ 1 ].descriptorCount = DescriptorPoolMaxComboImages;
		poolSizes[ 2 ].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		poolSizes[ 2 ].descriptorCount = DescriptorPoolMaxImages;

		VkDescriptorPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>( poolSizes.size() );
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = DescriptorPoolMaxSets;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

		if ( vkCreateDescriptorPool( device, &poolInfo, nullptr, &descriptorPool ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create descriptor pool!" );
		}
	}

	// Query Pool (Statistics)
	if ( deviceFeatures.pipelineStatisticsQuery )
	{
		VkQueryPoolCreateInfo queryPoolInfo = {};
		queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		// This query pool will store pipeline statistics
		queryPoolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
		// Pipeline counters to be returned for this pool
		queryPoolInfo.pipelineStatistics =
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;
		queryPoolInfo.queryCount = 6;

		VK_CHECK_RESULT( vkCreateQueryPool( device, &queryPoolInfo, NULL, &statQueryPool ) );
	}

	// Query Pool (Timestamp)
	if ( deviceProperties.limits.timestampComputeAndGraphics )
	{
		VkQueryPoolCreateInfo queryPoolInfo = {};
		queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryPoolInfo.queryCount = MaxTimeStampQueries;

		VK_CHECK_RESULT( vkCreateQueryPool( device, &queryPoolInfo, NULL, &timestampQueryPool ) );
	}

	// Query Pool (Occlusion)
	{
		VkQueryPoolCreateInfo queryPoolInfo = {};
		queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
		queryPoolInfo.queryCount = MaxOcclusionQueries;

		VK_CHECK_RESULT( vkCreateQueryPool( device, &queryPoolInfo, NULL, &occlusionQueryPool ) );
	}

	bufferId = 0;
}


void DeviceContext::Destroy( Window& window )
{
	vkDestroySampler( device, bilinearSampler, nullptr );
	vkDestroySampler( device, depthShadowSampler, nullptr );
	vkDestroyQueryPool( device, statQueryPool, nullptr );
	vkDestroyQueryPool( device, timestampQueryPool, nullptr );
	vkDestroyQueryPool( device, occlusionQueryPool, nullptr );

	vkDestroyDescriptorPool( device, descriptorPool, nullptr );

	vkDestroyDevice( device, nullptr );

	if ( EnableValidationLayers )
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
		if ( func != nullptr ) {
			func( instance, debugMessenger, nullptr );
		}
	}

	window.DestroyGlfwSurface();
	
	vkDestroyInstance( instance, nullptr );
}