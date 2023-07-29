#pragma once

#include "../globals/common.h"

class GpuSemaphore
{
#ifdef USE_VULKAN
private:
	VkSemaphore				semaphores[ MaxFrameStates ];
public:
	VkPipelineStageFlagBits	waitStage;
#endif
public:
	void					Create();
	void					Destroy();
#ifdef USE_VULKAN
	VkSemaphore&			VkObject();
	VkSemaphore				GetVkObject() const;
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
	void			Wait( const uint32_t waitIndex );
	void			Reset();
#ifdef USE_VULKAN
	VkFence&		VkObject();
	VkFence			GetVkObject() const;
#endif
};