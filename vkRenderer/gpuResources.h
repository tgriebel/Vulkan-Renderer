#pragma once
#include "common.h"
#include "allocator.h"

struct GpuBuffer
{
	allocRecordVk_t allocation;
	allocVk_t		alloc;

	void Reset() {
		offset = 0;
	}

	uint64_t GetSize() const {
		return offset;
	}

	uint64_t GetMaxSize() const {
		return allocation.size;
	}

	void Allocate( const uint64_t size ) {
		offset += size;
	}

	VkBuffer& GetVkObject() {
		return buffer;
	}

	void CopyData( void* data, const size_t sizeInBytes )
	{
		void* mappedData = allocation.memory->GetMemoryMapPtr( allocation );
		if ( mappedData != nullptr )
		{
			memcpy( (uint8_t*)mappedData + offset, data, sizeInBytes );
			offset += static_cast<uint32_t>( ( sizeInBytes + ( allocation.alignment - 1 ) ) & ~( allocation.alignment - 1 ) );
		}
	}

private:
	uint64_t	offset;
	VkBuffer	buffer;
};

struct GpuImage
{
	VkImage			image;
	VkImageView		view;
	allocRecordVk_t	allocation;
};