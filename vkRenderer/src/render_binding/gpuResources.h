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

class GpuBufferView;

class GpuBuffer
{
public:
	static uint64_t	GetAlignedSize( const uint64_t size, const uint64_t alignment );

	virtual void	SetPos( const uint64_t pos = 0 );
	virtual uint64_t GetMaxSize() const;

	uint64_t		GetSize() const;	
	uint64_t		GetBaseOffset() const;
	uint64_t		GetElementSize() const;
	uint64_t		GetElementSizeAligned() const;
	VkBuffer&		VkObject();
	VkBuffer		GetVkObject() const;
	void			Create( const uint32_t elements, const uint32_t elementSizeBytes, bufferType_t type, AllocatorVkMemory& bufferMemory );
	void			Destroy();
	bool			VisibleToCpu() const;
	void			Allocate( const uint64_t size );
	void			CopyData( void* data, const size_t sizeInBytes );

	GpuBufferView	GetView( const uint64_t baseElementIx, const uint64_t elementCount );

protected:
	alloc_t<Allocator<VkDeviceMemory>> m_alloc;
	VkBuffer	m_buffer;
	uint64_t	m_baseOffset;
	uint64_t	m_offset;
	uint64_t	m_end;
	uint64_t	m_elementSize;
	uint64_t	m_elementPadding;

	friend class GpuBufferView;
};

class GpuBufferView : public GpuBuffer
{
private:
	using GpuBuffer::Create;
	using GpuBuffer::Destroy;
};