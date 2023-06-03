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

struct deviceContext_t
{
	VkDevice					device;
	VkPhysicalDevice			physicalDevice;
	VkInstance					instance;
	VkPhysicalDeviceProperties	deviceProperties;
	VkQueue						graphicsQueue;
	VkQueue						presentQueue;
	VkQueue						computeQueue;
	uint32_t					queueFamilyIndices[ QUEUE_COUNT ];
};

extern deviceContext_t context;

struct imageInfo_t;
class GpuImage;

bool				CheckDeviceExtensionSupport( VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions );
bool				IsDeviceSuitable( VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions );
QueueFamilyIndices	FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface );
bool				vk_ValidTextureFormat( const VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features );
uint32_t			FindMemoryType( uint32_t typeFilter, VkMemoryPropertyFlags properties );
void				vk_CreateImageView( GpuImage* image, const imageInfo_t& info );
VkShaderModule		vk_CreateShaderModule( const std::vector<char>& code );