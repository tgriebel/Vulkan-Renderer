#pragma once

#include "../render_binding/allocator.h"

class GpuImage
{
private:
#ifdef USE_VULKAN
	VkImage			vk_image;
	VkImageView		vk_view;
	Allocation		allocation;
#endif

public:
	GpuImage::~GpuImage()
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

	void Destroy();
};


// TODO: for cases where only a different view is needed
//class GpuImageView;