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
#include "cmdContext.h"

class Window;
class ImageView;
class FrameBuffer;

enum downSampleMode_t : uint32_t
{
	DOWNSAMPLE_LINEAR,
	DOWNSAMPLE_GAUSSIAN,
};


struct swapChainInfo_t
{
	VkSurfaceCapabilitiesKHR		capabilities;
	std::vector<VkSurfaceFormatKHR>	formats;
	std::vector<VkPresentModeKHR>	presentModes;
};

struct copyImageSubViewParms_t
{
	uint32_t baseMip;
	uint32_t mipLevels;
	uint32_t baseArray;
	uint32_t arrayCount;
};


struct copyImageParms_t
{
	copyImageSubViewParms_t	subView;
	int32_t					x;
	int32_t					y;
	int32_t					z;
	int32_t					width;
	int32_t					height;
	int32_t					depth;
	int32_t					mipLevel;
};


class DeviceContext
{
public:
#ifdef USE_VULKAN
	const std::vector<const char*>		validationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*>		deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#endif
public:
#ifdef USE_VULKAN
	VkDevice							device;
	VkPhysicalDevice					physicalDevice;
	VkInstance							instance;
	VkPhysicalDeviceProperties			deviceProperties;
	VkPhysicalDeviceFeatures			deviceFeatures;
	VkDebugUtilsMessengerEXT			debugMessenger;
	VkDescriptorPool					descriptorPool;
	VkQueryPool							statQueryPool;
	VkQueryPool							timestampQueryPool;
	VkQueryPool							occlusionQueryPool;
	VkQueue								gfxContext;
	VkQueue								presentQueue;
	VkQueue								computeContext;
	VkSampler							bilinearSampler[ 3 ];
	VkSampler							depthShadowSampler;
	uint32_t							queueFamilyIndices[ QUEUE_COUNT ];

	bool								debugMarkersEnabled = false;
	PFN_vkDebugMarkerSetObjectTagEXT	fnDebugMarkerSetObjectTag = VK_NULL_HANDLE;
	PFN_vkDebugMarkerSetObjectNameEXT	fnDebugMarkerSetObjectName = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerBeginEXT		fnCmdDebugMarkerBegin = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerEndEXT			fnCmdDebugMarkerEnd = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerInsertEXT		fnCmdDebugMarkerInsert = VK_NULL_HANDLE;
#endif
	// "bufferId" flips between double/triple buffers - 0, 1, 2
	uint32_t							bufferId;

	void	Create( Window& window );
	void	Destroy( Window& window );
};

extern DeviceContext context;

#define VK_CHECK_RESULT( f )																			\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		throw std::runtime_error( #f );																	\
		assert(res == VK_SUCCESS);																		\
	}																									\
}

struct imageInfo_t;
struct imageSubResourceView_t;
union renderPassTransition_t;
class GpuImage;
class AllocatorMemory;
class DrawPass;

enum imageSamples_t : uint8_t;
enum downSampleMode_t : uint32_t;

bool				vk_CheckDeviceExtensionSupport( VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions );
bool				vk_IsDeviceSuitable( VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions );
QueueFamilyIndices	vk_FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface );
bool				vk_ValidTextureFormat( const VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features );
uint32_t			vk_FindMemoryType( uint32_t typeFilter, VkMemoryPropertyFlags properties );
VkImageView			vk_CreateImageView( const VkImage image, const imageInfo_t& info );
VkImageView			vk_CreateImageView( const VkImage image, const imageInfo_t& info, const imageSubResourceView_t& subResourceView );
void				vk_TransitionImageLayout( VkCommandBuffer cmdBuffer, const Image* image, const imageSubResourceView_t& subView, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next );
void				vk_GenerateMipmaps( VkCommandBuffer cmdBuffer, Image* image );
void				vk_GenerateDownsampleMips( CommandContext& cmdContext, std::vector<ImageView>& views, std::vector<DrawPass*>& passes, downSampleMode_t mode );
void				vk_RenderImageShader( CommandContext& cmdContext, const hdl_t pipeLineHandle, DrawPass* pass, const renderPassTransition_t& transitionState );
void				vk_CopyImage( VkCommandBuffer cmdBuffer, const Image& src, Image& dst );
void				vk_CopyImage( VkCommandBuffer cmdBuffer, const ImageView& src, ImageView& dst );
void				vk_CopyBufferToImage( VkCommandBuffer cmdBuffer, Image* texture, const copyImageParms_t& copyParms, GpuBuffer& buffer, const uint64_t bufferOffset );
imageSamples_t		vk_MaxImageSamples();
VkShaderModule		vk_CreateShaderModule( const std::vector<char>& code );
VkResult			vk_CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger );
void				vk_MarkerSetObjectTag( uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag );
void				vk_MarkerSetObjectName( uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name );