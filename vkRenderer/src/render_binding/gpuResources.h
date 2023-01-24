#pragma once
#include "../globals/common.h"
#include "allocator.h"

struct GpuBuffer
{
	AllocationVk alloc;

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
	uint64_t	offset;
	VkBuffer	buffer;
};