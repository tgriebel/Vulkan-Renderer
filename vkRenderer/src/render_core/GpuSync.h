#pragma once

#include "../globals/common.h"

class GpuSemaphore
{
private:
#ifdef USE_VULKAN
	VkSemaphore	semaphores[ MaxFrameStates ];
#endif
public:
	void			Create();
	void			Destroy();
#ifdef USE_VULKAN
	VkSemaphore&	VkObject();
	VkSemaphore		GetVkObject() const;
#endif
};


class GpuFence
{
private:
#ifdef USE_VULKAN
	VkFence	fences[ MaxFrameStates ];
#endif
public:
	void			Create();
	void			Destroy();
	void			Wait();
	void			Reset();
#ifdef USE_VULKAN
	VkFence&		VkObject();
	VkFence			GetVkObject() const;
#endif
};