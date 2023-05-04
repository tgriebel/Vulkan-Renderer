/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

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


VkBuffer GpuBuffer::GetVkObject() const
{
	return buffer;
}


VkBuffer& GpuBuffer::VkObject()
{
	return buffer;
}


void GpuBuffer::Create( const uint32_t elements, const uint32_t elementSizeBytes, bufferType_t type, AllocatorVkMemory& bufferMemory )
{
	VkBufferUsageFlags usage = 0;
	VkDeviceSize size = VkDeviceSize( elements ) * elementSizeBytes;
	VkDeviceSize alignment = elementSizeBytes;

	if( type == bufferType_t::STORAGE ) {
		alignment = context.deviceProperties.limits.minStorageBufferOffsetAlignment;
		usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	} else if ( type == bufferType_t::UNIFORM ) {
		alignment = context.deviceProperties.limits.minUniformBufferOffsetAlignment;
		usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	} else if ( type == bufferType_t::VERTEX ) {
		usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	} else if ( type == bufferType_t::INDEX ) {
		usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	} else if ( type == bufferType_t::STAGING ) {
		usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	} else {
		assert(0);
	}

	VkDeviceSize stride = GpuBuffer::GetPadding( elementSizeBytes, alignment );
	size = stride * elements;

	VkBufferCreateInfo bufferInfo{ };
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if ( vkCreateBuffer( context.device, &bufferInfo, nullptr, &VkObject() ) != VK_SUCCESS ) {
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

	SetPos( 0 );
}


void GpuBuffer::Destroy()
{
	if ( context.device == VK_NULL_HANDLE ) {
		throw std::runtime_error( "GPU Buffer: Destroy: No device context!" );
	}
	VkBuffer vkBuffer = GetVkObject();
	if ( vkBuffer != VK_NULL_HANDLE ) {
		vkDestroyBuffer( context.device, vkBuffer, nullptr );
	}
	SetPos();
}


uint64_t GpuBuffer::GetPadding( const uint64_t size, const uint64_t alignment )
{
	return static_cast<uint32_t>( ( size + ( alignment - 1 ) ) & ~( alignment - 1 ) );
}


bool GpuBuffer::VisibleToCpu() const
{
	const void* mappedData = alloc.GetPtr();
	return ( mappedData != nullptr );
}


void GpuBuffer::CopyData( void* data, const size_t sizeInBytes )
{
	assert( ( GetSize() + sizeInBytes ) <= GetMaxSize() );
	void* mappedData = alloc.GetPtr();
	if ( mappedData != nullptr )
	{
		memcpy( (uint8_t*)mappedData + offset, data, sizeInBytes );
		offset += GetPadding( sizeInBytes, alloc.GetAlignment() );
	}
}