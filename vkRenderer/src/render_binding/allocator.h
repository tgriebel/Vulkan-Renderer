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

class AllocatorMemory;

enum memoryRegion_t
{
	UNKNOWN,
	LOCAL,
	SHARED,
};


struct allocRecord_t
{
	uint64_t	offset;
	uint64_t	size;
	uint64_t	alignment;
	bool		isValid;
};


class Allocation
{
private:
	inline bool IsValid() const
	{
		return ( allocator != nullptr ) && ( handle.Get() >= 0 );
	}

	hdl_t				handle;
	AllocatorMemory*	allocator;

	friend class AllocatorMemory;

public:
	Allocation()
	{
		allocator = nullptr;
	}

	uint64_t	GetOffset() const;
	uint64_t	GetSize() const;
	uint64_t	GetAlignment() const;
	void*		GetPtr() const;
	void		Free();
};


class AllocatorMemory
{
private:
	uint32_t						m_type;
	uint64_t						m_size;
	uint64_t						m_offset;
	void* ptr;
	std::vector< allocRecord_t >	m_allocations;
	std::vector< hdl_t >			m_handles;

	memoryRegion_t					m_memoryRegion;

#ifdef USE_VULKAN
	uint32_t						vk_memoryTypeIndex;
	VkDeviceMemory					vk_deviceMemory;
#endif

	friend class Allocation;

public:
	AllocatorMemory()
	{
		Unbind();
	}

	AllocatorMemory( VkDeviceMemory& _memory, const uint64_t _size, const uint32_t _type )
	{
		m_offset = 0;
		vk_deviceMemory = _memory;
		m_size = _size;
		m_type = _type;
		ptr = nullptr;

		vk_memoryTypeIndex = 0;
		m_memoryRegion = memoryRegion_t::UNKNOWN;
	}

	void					Create( const uint32_t sizeBytes, const memoryRegion_t region );
	void					Destroy();

	void					Bind( VkDeviceMemory& _memory, void* memMap, const uint64_t _size, const uint32_t _type );
	void					Unbind();
	bool					IsMemoryCompatible( const uint32_t memoryType ) const;
	void*					GetMemoryMapPtr( const allocRecord_t& record ) const;
	uint64_t				GetSize() const;
	uint64_t				GetAlignedOffset( const uint64_t alignment ) const;
	bool					CanAllocate( uint64_t alignment, uint64_t allocSize ) const;
	bool					Allocate( uint64_t alignment, uint64_t allocSize, Allocation& handle );
	void					Reset();
	void					Free( hdl_t& handle );
	memoryRegion_t			GetMemoryRegion() const;

#ifdef USE_VULKAN
	VkDeviceMemory&			GetVkObject();
	uint32_t				GetVkMemoryType() const;
#endif

	[[nodiscard]]
	bool					IsValidIndex( const uint64_t index ) const;

	[[nodiscard]]
	const allocRecord_t*	GetRecord( const hdl_t& handle ) const;
};