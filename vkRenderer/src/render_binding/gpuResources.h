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

#pragma once
#include "../globals/common.h"
#include "allocator.h"

class GpuBuffer
{
public:

	void Reset() {
		offset = 0;
	}


	uint64_t GetSize() const {
		return offset;
	}


	uint64_t GetMaxSize() const {
		return alloc.GetSize();
	}


	void Allocate( const uint64_t size ) {
		offset += size;
	}


	VkBuffer& GetVkObject() {
		return buffer;
	}


	void Create( VkDeviceSize size, VkBufferUsageFlags usage, AllocatorVkMemory& bufferMemory )
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

	void Destroy()
	{
		if( context.device == VK_NULL_HANDLE ) {
			throw std::runtime_error( "GPU Buffer: Destroy: No device context!" );
		}
		VkBuffer vkBuffer = GetVkObject();
		if( vkBuffer == VK_NULL_HANDLE ) {
			vkDestroyBuffer( context.device, vkBuffer, nullptr );
		}
		Reset();
	}


	void CopyData( void* data, const size_t sizeInBytes )
	{
		assert( ( GetSize() + sizeInBytes ) <= GetMaxSize() );
		void* mappedData = alloc.GetPtr();
		if ( mappedData != nullptr )
		{
			memcpy( (uint8_t*)mappedData + offset, data, sizeInBytes );
			offset += static_cast<uint32_t>( ( sizeInBytes + ( alloc.GetAlignment() - 1 ) ) & ~( alloc.GetAlignment() - 1 ) );
		}
	}

private:
	alloc_t<Allocator<VkDeviceMemory>> alloc;
	uint64_t	offset;
	VkBuffer	buffer;
};