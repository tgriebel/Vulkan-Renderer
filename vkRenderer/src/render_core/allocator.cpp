#include <iostream>

// Verbose debug prints
#if 0
#define VMA_DEBUG_LOG_LEVEL 4
#define VMA_DEBUG_LOG_FORMAT( format, ... ) \
    do { \
        char _vmaBuf[ 512 ]; \
        snprintf( _vmaBuf, sizeof( _vmaBuf ), (format), __VA_ARGS__ ); \
        std::cout << "[VMA] " << _vmaBuf << "\n"; \
    } while( false )
#endif

#define VMA_IMPLEMENTATION
#include "allocator.h"
#include "../render_state/deviceContext.h"

extern DeviceContext context;


// ---- Allocation ----

uint64_t Allocation::GetOffset() const
{
#ifdef USE_VULKAN
	return m_info.offset;
#else
	return 0;
#endif
}


uint64_t Allocation::GetSize() const
{
#ifdef USE_VULKAN
	return m_info.size;
#else
	return 0;
#endif
}


uint64_t Allocation::GetAlignment() const
{
	return m_alignment;
}


void* Allocation::GetPtr() const
{
#ifdef USE_VULKAN
	return m_info.pMappedData;
#else
	return nullptr;
#endif
}


void Allocation::Free()
{
#ifdef USE_VULKAN
	if ( m_allocation != VK_NULL_HANDLE )
	{
		vmaFreeMemory( AllocatorMemory::GetVmaAllocator(), m_allocation );
		m_allocation = VK_NULL_HANDLE;
		m_info = {};
		m_alignment = 0;
	}
#endif
}


// ---- AllocatorMemory ----

#ifdef USE_VULKAN
VmaAllocator AllocatorMemory::s_allocator = VK_NULL_HANDLE;


void AllocatorMemory::CreateVmaAllocator()
{
	if ( s_allocator != VK_NULL_HANDLE ) {
		return;
	}

	VmaAllocatorCreateInfo createInfo = {};
	createInfo.physicalDevice = context.physicalDevice;
	createInfo.device = context.device;
	createInfo.instance = context.instance;
	createInfo.vulkanApiVersion = VK_API_VERSION_1_2;

	createInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	VK_CHECK_RESULT( vmaCreateAllocator( &createInfo, &s_allocator ) );
}


void AllocatorMemory::DestroyVmaAllocator()
{
	if ( s_allocator != VK_NULL_HANDLE )
	{
		vmaDestroyAllocator( s_allocator );
		s_allocator = VK_NULL_HANDLE;
	}
}


VmaAllocator AllocatorMemory::GetVmaAllocator()
{
	return s_allocator;
}
#endif


void AllocatorMemory::Create( const uint32_t sizeBytes, const memoryRegion_t region, const resourceLifeTime_t lifetime )
{
	RenderResource::Create( resourceType_t::MEMORY, lifetime );
	m_memoryRegion = region;

#ifdef USE_VULKAN
	CreateVmaAllocator();
#endif
}


void AllocatorMemory::Destroy()
{
	// VMA manages memory internally — individual pools are not backed by dedicated VkDeviceMemory.
	// Call AllocatorMemory::DestroyVmaAllocator() at shutdown after all resources are freed.
}


memoryRegion_t AllocatorMemory::GetMemoryRegion() const
{
	return m_memoryRegion;
}
