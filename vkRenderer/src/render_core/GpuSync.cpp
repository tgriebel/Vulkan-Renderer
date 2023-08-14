#include "GpuSync.h"
#include "../render_state/deviceContext.h"

void GpuSemaphore::Create( const char* name, const bool isBinary )
{
#ifdef USE_VULKAN

	VkSemaphoreTypeCreateInfo timelineCreateInfo{};
	VkSemaphoreCreateInfo semaphoreInfo{ };
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if( isBinary == false )
	{	
		timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		timelineCreateInfo.pNext = NULL;
		timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		timelineCreateInfo.initialValue = 0;

		semaphoreInfo.flags = VK_SEMAPHORE_TYPE_TIMELINE;
		semaphoreInfo.pNext = &timelineCreateInfo;
	}

	for( uint32_t i = 0; i < MaxFrameStates; ++i ) {
		if ( vkCreateSemaphore( context.device, &semaphoreInfo, nullptr, &semaphores[ i ] ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create semaphore!" );
		}
		vk_MarkerSetObjectName( (uint64_t)semaphores[ i ], VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT, name );
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


void GpuFence::Create( const char* name )
{
#ifdef USE_VULKAN
	VkFenceCreateInfo fenceInfo{ };
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	
	if ( vkCreateFence( context.device, &fenceInfo, nullptr, &fence ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create fence!" );
	}
	vk_MarkerSetObjectName( (uint64_t)fence, VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT, name );
#endif
}


void GpuFence::Destroy()
{
#ifdef USE_VULKAN
	vkDestroyFence( context.device, fence, nullptr );
#endif
}


void GpuFence::Wait()
{
	if ( fence != VK_NULL_HANDLE ) {
		vkWaitForFences( context.device, 1, &fence, VK_TRUE, UINT64_MAX );
		vkResetFences( context.device, 1, &fence );
	}
}

#ifdef USE_VULKAN
VkFence& GpuFence::VkObject()
{
	return fence;
}


VkFence GpuFence::GetVkObject() const
{
	return fence;
}
#endif