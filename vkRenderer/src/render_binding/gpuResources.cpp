#include "gpuResources.h"
#include "../render_state/deviceContext.h"

extern deviceContext_t context;

void GpuBuffer::SetPos( const uint32_t pos )
{
	offset = pos;
}


uint64_t GpuBuffer::GetSize() const
{
	return offset;
}


uint64_t GpuBuffer::GetMaxSize() const
{
	return alloc.GetSize();
}


void GpuBuffer::Allocate( const uint64_t size )
{
	offset += size;
}


VkBuffer& GpuBuffer::GetVkObject()
{
	return buffer;
}


void GpuBuffer::Create( VkDeviceSize size, VkBufferUsageFlags usage, AllocatorVkMemory& bufferMemory )
{
	VkBufferCreateInfo bufferInfo{ };
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if ( vkCreateBuffer( context.device, &bufferInfo, nullptr, &GetVkObject() ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create buffer!" );
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements( context.device, GetVkObject(), &memRequirements );

	VkMemoryAllocateInfo allocInfo{ };
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = bufferMemory.memoryTypeIndex;

	if ( bufferMemory.Allocate( memRequirements.alignment, memRequirements.size, alloc ) ) {
		vkBindBufferMemory( context.device, GetVkObject(), bufferMemory.GetMemoryResource(), alloc.GetOffset() );
	}
	else {
		throw std::runtime_error( "Buffer could not allocate!" );
	}
}


void GpuBuffer::Destroy()
{
	if ( context.device == VK_NULL_HANDLE ) {
		throw std::runtime_error( "GPU Buffer: Destroy: No device context!" );
	}
	VkBuffer vkBuffer = GetVkObject();
	if ( vkBuffer == VK_NULL_HANDLE ) {
		vkDestroyBuffer( context.device, vkBuffer, nullptr );
	}
	SetPos();
}


void GpuBuffer::CopyData( void* data, const size_t sizeInBytes )
{
	assert( ( GetSize() + sizeInBytes ) <= GetMaxSize() );
	void* mappedData = alloc.GetPtr();
	if ( mappedData != nullptr )
	{
		memcpy( (uint8_t*)mappedData + offset, data, sizeInBytes );
		offset += static_cast<uint32_t>( ( sizeInBytes + ( alloc.GetAlignment() - 1 ) ) & ~( alloc.GetAlignment() - 1 ) );
	}
}