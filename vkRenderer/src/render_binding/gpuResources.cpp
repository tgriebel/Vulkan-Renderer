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

extern DeviceContext context;


uint32_t GpuBuffer::ClampId( const uint32_t bufferId ) const
{
	if( m_swapBuffering == swapBuffering_t::SINGLE_FRAME ) {
		return 0;
	}
	if ( bufferId >= m_bufferCount )
	{
		assert( 0 );
		return 0;
	}
	return bufferId;
}


void GpuBuffer::SetPos( const uint64_t pos )
{
	const uint32_t id = ClampId( context.bufferId );
	m_buffer[ id ].offset = Clamp( m_buffer[ id ].baseOffset + pos, m_buffer[ id ].baseOffset, GetMaxSize() );
}


uint64_t GpuBuffer::GetSize() const
{
	const uint32_t id = ClampId( context.bufferId );
	return ( m_buffer[ id ].offset - m_buffer[ id ].baseOffset );
}


uint64_t GpuBuffer::GetBaseOffset() const
{
	const uint32_t id = ClampId( context.bufferId );
	return m_buffer[ id ].baseOffset;
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
	const uint32_t id = ClampId( context.bufferId );
	SetPos( m_buffer[ id ].offset + size );
}


VkBuffer GpuBuffer::GetVkObject() const
{
	const uint32_t id = ClampId( context.bufferId );
	return m_buffer[ id ].buffer;
}


VkBuffer& GpuBuffer::VkObject()
{
	const uint32_t id = ClampId( context.bufferId );
	return m_buffer[ id ].buffer;
}


void GpuBuffer::Create( const bufferCreateInfo_t info )
{
	Create( info.name, info.swapBuffering, info.lifetime, info.elements, info.elementSizeBytes, info.type, *info.bufferMemory );
}


void GpuBuffer::Create( const char* name, const swapBuffering_t swapBuffering, const resourceLifeTime_t lifetime, const uint32_t elements, const uint32_t elementSizeBytes, bufferType_t type, AllocatorMemory& bufferMemory )
{
	// Resource Management
	{
		RenderResource::Create( lifetime );
	}

#ifdef USE_VULKAN
	{
		VkBufferUsageFlags usage = 0;
		VkDeviceSize bufferSize = VkDeviceSize( elements ) * elementSizeBytes;
		VkDeviceSize alignment = elementSizeBytes;

		m_swapBuffering = swapBuffering;
		if( m_swapBuffering == swapBuffering_t::MULTI_FRAME ) {
			m_bufferCount = MaxFrameStates;
		} else {
			m_bufferCount = 1;
		}

		if( type == bufferType_t::STORAGE ) {
			alignment = context.deviceProperties.limits.minStorageBufferOffsetAlignment;
			usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
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

		VkDeviceSize stride = m_elementPadding;
		bufferSize = stride * elements;

		VkBufferCreateInfo bufferInfo{ };
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = bufferSize;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		for( uint32_t bufferId = 0; bufferId < m_bufferCount; ++bufferId )
		{
			VK_CHECK_RESULT( vkCreateBuffer( context.device, &bufferInfo, nullptr, &m_buffer[ bufferId ].buffer ) );

			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements( context.device, m_buffer[ bufferId ].buffer, &memRequirements );

			VkMemoryAllocateInfo allocInfo{ };
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = bufferMemory.GetVkMemoryType();

			if ( bufferMemory.Allocate( memRequirements.alignment, memRequirements.size, m_buffer[ bufferId ].alloc ) ) {
				vkBindBufferMemory( context.device, m_buffer[ bufferId ].buffer, bufferMemory.GetVkObject(), m_buffer[ bufferId ].alloc.GetOffset() );
			} else {
				throw std::runtime_error( "Buffer could not allocate!" );
			}

			vk_MarkerSetObjectName( (uint64_t)m_buffer[ bufferId ].buffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, name );

			m_name = name;

			m_end = m_buffer[ bufferId ].alloc.GetSize();
			m_buffer[ bufferId ].baseOffset = 0;
			m_buffer[ bufferId ].offset = 0;
		}
	}
#endif
}


void GpuBuffer::Destroy()
{
	if ( context.device == VK_NULL_HANDLE ) {
		throw std::runtime_error( "GPU Buffer: Destroy: No device context!" );
	}
	for ( uint32_t bufferId = 0; bufferId < m_bufferCount; ++bufferId )
	{
		if ( m_buffer[ bufferId ].buffer != VK_NULL_HANDLE ) {
			vkDestroyBuffer( context.device, m_buffer[ bufferId ].buffer, nullptr );
		}
		m_buffer[ bufferId ].offset = 0;
	}
}


uint64_t GpuBuffer::GetAlignedSize( const uint64_t size, const uint64_t alignment )
{
	return static_cast<uint32_t>( ( size + ( alignment - 1 ) ) & ~( alignment - 1 ) );
}


bool GpuBuffer::VisibleToCpu() const
{
	const void* mappedData = m_buffer[ 0 ].alloc.GetPtr();
	return ( mappedData != nullptr );
}


void GpuBuffer::CopyData( const void* data, const size_t sizeInBytes )
{
	const uint32_t id = ClampId( context.bufferId );

	assert( ( GetSize() + sizeInBytes ) <= GetMaxSize() );
	void* mappedData = m_buffer[ id ].alloc.GetPtr();
	if ( mappedData != nullptr )
	{
		memcpy( (uint8_t*)mappedData + m_buffer[ id ].offset, data, sizeInBytes );
		m_buffer[ id ].offset += GetAlignedSize( sizeInBytes, m_buffer[ id ].alloc.GetAlignment() );
	} else {
		assert( 0 );
	}
}


void GpuBuffer::CopyFrom( void* data, const size_t sizeInBytes ) const
{
	const uint32_t id = ClampId( context.bufferId );

	assert( ( GetSize() + sizeInBytes ) <= GetMaxSize() );
	void* mappedData = m_buffer[ id ].alloc.GetPtr();

	if ( mappedData != nullptr )
	{
		memcpy( data, reinterpret_cast<uint8_t*>( mappedData ), sizeInBytes );
	}
	else {
		assert( 0 );
	}
}


void* GpuBuffer::Get() const
{
	const uint32_t id = ClampId( context.bufferId );

	void* mappedData = m_buffer[ id ].alloc.GetPtr();
	assert( mappedData != nullptr );
	return mappedData;
}


const char* GpuBuffer::GetName() const
{
	return m_name;
}


GpuBufferView GpuBuffer::GetView( const uint64_t baseElementIx, const uint64_t elementCount )
{
	GpuBufferView view;

	const uint64_t maxSize = GetMaxSize();

	for ( uint32_t bufferId = 0; bufferId < m_bufferCount; ++bufferId )
	{
		view.m_buffer[ bufferId ] = m_buffer[ bufferId ];
		view.m_buffer[ bufferId ].baseOffset = baseElementIx * m_elementPadding;
		assert( view.m_buffer[ bufferId ].baseOffset <= maxSize );
		view.m_buffer[ bufferId ].baseOffset = Clamp( view.m_buffer[ bufferId ].baseOffset, m_buffer[ bufferId ].baseOffset, maxSize );
	}
	view.m_end = Min( view.m_buffer[ 0 ].baseOffset + elementCount * m_elementPadding, maxSize );
	view.m_elementSize = m_elementSize;
	view.m_elementPadding = m_elementPadding;
	view.m_bufferCount = m_bufferCount;
	view.m_type = m_type;
	view.m_swapBuffering = m_swapBuffering;

	return view;
}