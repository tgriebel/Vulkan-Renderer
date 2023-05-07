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

void GpuBuffer::SetPos( const uint64_t pos )
{
	m_offset = Clamp( pos, m_baseOffset, GetMaxSize() );
}


uint64_t GpuBuffer::GetSize() const
{
	return m_offset;
}


uint64_t GpuBuffer::GetBaseOffset() const
{
	return m_baseOffset;
}


uint64_t GpuBuffer::GetElementSize() const
{
	return m_elementSize;
}


uint64_t GpuBuffer::GetElementSizeAligned() const
{
	return m_elementPadding;
}


uint64_t GpuBuffer::GetMaxSize() const
{
	return m_end;
}


void GpuBuffer::Allocate( const uint64_t size )
{
	SetPos( m_offset + size );
}


VkBuffer GpuBuffer::GetVkObject() const
{
	return m_buffer;
}


VkBuffer& GpuBuffer::VkObject()
{
	return m_buffer;
}


void GpuBuffer::Create( const char* name, const uint32_t elements, const uint32_t elementSizeBytes, bufferType_t type, AllocatorVkMemory& bufferMemory )
{
	VkBufferUsageFlags usage = 0;
	VkDeviceSize bufferSize = VkDeviceSize( elements ) * elementSizeBytes;
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

	m_elementSize = elementSizeBytes;
	m_elementPadding = GpuBuffer::GetAlignedSize( elementSizeBytes, alignment );

	VkDeviceSize stride = elementSizeBytes + m_elementPadding;
	bufferSize = stride * elements;

	VkBufferCreateInfo bufferInfo{ };
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
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

	if ( bufferMemory.Allocate( memRequirements.alignment, memRequirements.size, m_alloc ) ) {
		vkBindBufferMemory( context.device, GetVkObject(), bufferMemory.GetMemoryResource(), m_alloc.GetOffset() );
	}
	else {
		throw std::runtime_error( "Buffer could not allocate!" );
	}

	m_name = name;

	m_end = m_alloc.GetSize();
	m_baseOffset = 0;
	SetPos( m_baseOffset );
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


uint64_t GpuBuffer::GetAlignedSize( const uint64_t size, const uint64_t alignment )
{
	return static_cast<uint32_t>( ( size + ( alignment - 1 ) ) & ~( alignment - 1 ) );
}


bool GpuBuffer::VisibleToCpu() const
{
	const void* mappedData = m_alloc.GetPtr();
	return ( mappedData != nullptr );
}


void GpuBuffer::CopyData( void* data, const size_t sizeInBytes )
{
	assert( ( GetSize() + sizeInBytes ) <= GetMaxSize() );
	void* mappedData = m_alloc.GetPtr();
	if ( mappedData != nullptr )
	{
		memcpy( (uint8_t*)mappedData + m_offset, data, sizeInBytes );
		m_offset += GetAlignedSize( sizeInBytes, m_alloc.GetAlignment() );
	}
}


GpuBufferView GpuBuffer::GetView( const uint64_t baseElementIx, const uint64_t elementCount )
{
	GpuBufferView view;

	const uint64_t maxSize = GetMaxSize();

	view.m_baseOffset = baseElementIx * m_elementPadding;
	view.m_baseOffset = Clamp( view.m_baseOffset, m_baseOffset, maxSize );
	view.m_alloc = m_alloc;
	view.m_buffer = m_buffer;
	view.m_offset = view.m_baseOffset;
	view.m_end = Min( view.m_baseOffset + elementCount * m_elementPadding, maxSize );
	view.m_elementSize = m_elementSize;
	view.m_elementPadding = m_elementPadding;

	return view;
}