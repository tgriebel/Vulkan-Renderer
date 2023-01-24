#pragma once
#include "../globals/common.h"

struct deviceContext_t
{
	VkDevice					device;
	VkPhysicalDevice			physicalDevice;
	VkInstance					instance;
	VkPhysicalDeviceLimits		limits;
	VkQueue						graphicsQueue;
	VkQueue						presentQueue;
	VkQueue						computeQueue;
	uint32_t					queueFamilyIndices[ QUEUE_COUNT ];
};

extern deviceContext_t context;

bool				CheckDeviceExtensionSupport( VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions );
bool				IsDeviceSuitable( VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions );
QueueFamilyIndices	FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface );
VkFormat			FindSupportedFormat( const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features );
uint32_t			FindMemoryType( uint32_t typeFilter, VkMemoryPropertyFlags properties );
VkImageView			CreateImageView( VkImage image, VkFormat format, VkImageViewType type, VkImageAspectFlags aspectFlags, uint32_t mipLevels );
VkShaderModule		CreateShaderModule( const std::vector<char>& code );