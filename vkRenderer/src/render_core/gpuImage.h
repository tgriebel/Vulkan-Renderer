#pragma once

#include "../render_binding/allocator.h"

class GpuImage
{
public:
#ifdef USE_VULKAN
	VkImage			vk_image;
	VkImageView		vk_view;
	AllocationVk	allocation;
#endif
	GpuImage::~GpuImage()
	{
		Destroy();
	}
	void Destroy();
};

// TODO: for cases where only a different view is needed
//class GpuImageView;