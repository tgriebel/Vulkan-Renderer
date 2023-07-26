#include "GpuSync.h"
#include "../render_state/deviceContext.h"

void GpuSemaphore::Create()
{
#ifdef USE_VULKAN
	VkSemaphoreCreateInfo semaphoreInfo{ };
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for( uint32_t i = 0; i < MaxFrameStates; ++i ) {
		if ( vkCreateSemaphore( context.device, &semaphoreInfo, nullptr, &semaphores[ i ] ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create semaphore!" );
		}
	}
#endif
}


void GpuSemaphore::Destroy()
{
#ifdef USE_VULKAN
	for ( uint32_t i = 0; i < MaxFrameStates; ++i ) {
		vkDestroySemaphore( context.device, semaphores[ i ], nullptr );
	}
#endif
}

#ifdef USE_VULKAN
VkSemaphore& GpuSemaphore::VkObject()
{
	return semaphores[ context.bufferId ];
}


VkSemaphore GpuSemaphore::GetVkObject() const
{
	return semaphores[ context.bufferId ];
}
#endif


void GpuFence::Create()
{
#ifdef USE_VULKAN
	VkFenceCreateInfo fenceInfo{ };
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for ( size_t i = 0; i < MaxFrameStates; ++i ) {
		if ( vkCreateFence( context.device, &fenceInfo, nullptr, &fences[ i ] ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create fence!" );
		}
	}
#endif
}


void GpuFence::Destroy()
{
#ifdef USE_VULKAN
	for ( uint32_t i = 0; i < MaxFrameStates; ++i ) {
		vkDestroyFence( context.device, fences[ i ], nullptr );
	}
#endif
}


void GpuFence::Wait()
{
	if ( fences[ context.bufferId ] != VK_NULL_HANDLE ) {
		vkWaitForFences( context.device, 1, &fences[ context.bufferId ], VK_TRUE, UINT64_MAX );
	}
}


void GpuFence::Reset()
{
	vkResetFences( context.device, 1, &fences[ context.bufferId ] );
}

#ifdef USE_VULKAN
VkFence& GpuFence::VkObject()
{
	return fences[ context.bufferId ];
}


VkFence GpuFence::GetVkObject() const
{
	return fences[ context.bufferId ];
}
#endif