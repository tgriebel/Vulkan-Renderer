#include "allocator.h"
#include "../render_state/deviceContext.h"

void AllocatorMemory::Create( const uint32_t sizeBytes, const memoryRegion_t region )
{
	VkMemoryPropertyFlags flags = 0;
	if ( region == SHARED ) {
		flags = VkMemoryPropertyFlagBits( VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	} else {
		flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}
	memoryRegion = region;

	uint32_t typeIndex = vk_FindMemoryType( ~0x00, flags );

	VkMemoryAllocateInfo allocInfo{ };
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = sizeBytes;
	allocInfo.memoryTypeIndex = typeIndex;

	VkDeviceMemory memory;
	if ( vkAllocateMemory( context.device, &allocInfo, nullptr, &memory ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to allocate buffer memory!" );
	}

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );
	VkMemoryType type = memProperties.memoryTypes[ typeIndex ];

	void* memPtr = nullptr;
	if ( ( type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) == 0 ) {
		if ( vkMapMemory( context.device, memory, 0, VK_WHOLE_SIZE, 0, &memPtr ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to map memory to context.device memory!" );
		}
	}

	Bind( memory, memPtr, sizeBytes, typeIndex );
	vk_memoryTypeIndex = typeIndex;

	Reset();
}


void AllocatorMemory::Destroy()
{
	vkFreeMemory( context.device, GetMemoryResource(), nullptr );
	Unbind();
	Reset();
}