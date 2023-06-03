#pragma once

#include "../render_binding/allocator.h"

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
	virtual GpuImage::~GpuImage()
	{
		Destroy();
	}

#ifdef USE_VULKAN
	inline VkImage GetVkImage() const
	{
		return vk_image;
	}


	inline VkImage& VkImage()
	{
		return vk_image;
	}


	inline VkImageView GetVkImageView() const
	{
		return vk_view;
	}


	inline VkImageView& VkImageView()
	{
		return vk_view;
	}
#endif

	virtual void Destroy();
};