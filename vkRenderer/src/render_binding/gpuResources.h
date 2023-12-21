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

enum class bufferType_t
{
	UNIFORM,
	STORAGE,
	VERTEX,
	INDEX,
	STAGING
};


struct bufferCreateInfo_t
{
	const char*			name;
	resourceLifetime_t	lifetime;
	uint32_t			elements;
	uint32_t			elementSizeBytes;
	bufferType_t		type;
	AllocatorMemory*	bufferMemory;
};


class GpuBufferView;

class GpuBuffer
{
private:
	uint32_t		ClampId( const uint32_t bufferId ) const;
public:
	static uint64_t	GetAlignedSize( const uint64_t size, const uint64_t alignment );

	virtual void	SetPos( const uint64_t pos );
	virtual uint64_t GetMaxSize() const;

	uint64_t		GetSize() const;
	uint64_t		GetBaseOffset() const;
	uint64_t		GetElementSize() const;
	uint64_t		GetElementSizeAligned() const;
	VkBuffer&		VkObject();
	VkBuffer		GetVkObject() const;
	void			Create( const bufferCreateInfo_t info );
	void			Create( const char* name, const resourceLifetime_t lifetime, const uint32_t elements, const uint32_t elementSizeBytes, bufferType_t type, AllocatorMemory& bufferMemory );
	void			Destroy();
	bool			VisibleToCpu() const;
	void			Allocate( const uint64_t size );
	void			CopyData( const void* data, const size_t sizeInBytes );
	void			CopyFrom( void* data, const size_t sizeInBytes ) const;

	const char*		GetName() const;
	GpuBufferView	GetView( const uint64_t baseElementIx, const uint64_t elementCount );

protected:
	struct buffer_t
	{
		Allocation			alloc;
		VkBuffer			buffer;
		uint64_t			baseOffset;
		uint64_t			offset;
	};

	buffer_t			m_buffer[ MaxFrameStates ];
	uint32_t			m_bufferCount;
	uint64_t			m_end;
	uint64_t			m_elementSize;
	uint64_t			m_elementPadding;
	bufferType_t		m_type;
	resourceLifetime_t	m_lifetime;
	const char*			m_name;

	friend class GpuBufferView;
};

class GpuBufferView : public GpuBuffer
{
private:
	using GpuBuffer::Create;
	using GpuBuffer::Destroy;
};