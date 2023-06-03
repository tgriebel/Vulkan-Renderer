#pragma once

#include "../render_binding/allocator.h"

struct imageInfo_t;
class AllocatorMemory;

enum gpuImageStateFlags_t : uint8_t
{
	GPU_IMAGE_NONE			= 0,
	GPU_IMAGE_READ			= ( 1 << 0 ),
	GPU_IMAGE_TRANSFER_SRC	= ( 1 << 1 ),
	GPU_IMAGE_TRANSFER_DST	= ( 1 << 2 ),
	GPU_IMAGE_ALL			= 0xFF,
};
DEFINE_ENUM_OPERATORS( gpuImageStateFlags_t, uint8_t )

class GpuImage
{
protected:
#ifdef USE_VULKAN
	VkImage			vk_image;
	VkImageView		vk_view;
	Allocation		allocation;
#endif

public:

	GpuImage()
	{
#ifdef USE_VULKAN
		vk_image = VK_NULL_HANDLE;
		vk_view = VK_NULL_HANDLE;
#endif
	}


	virtual GpuImage::~GpuImage()
	{
		Destroy();
	}

#ifdef USE_VULKAN
	GpuImage( const VkImage image, const VkImageView view )
	{
		vk_image = image;
		vk_view = view;
	}


	inline VkImage GetVkImage() const
	{
		return vk_image;
	}

	inline VkImageView GetVkImageView() const
	{
		return vk_view;
	}


	inline void DetachVkImage()
	{
		vk_image = VK_NULL_HANDLE;
	}


	inline void DetachVkImageView()
	{
		vk_view = VK_NULL_HANDLE;
	}
#endif

	void Create( const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory );
	virtual void Destroy();
};