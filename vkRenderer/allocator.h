#pragma once
#include "common.h"

template< class ResourceType >
class Allocator
{
private:
	using allocator_t = Allocator< ResourceType >;
	using allocRecord_t = allocRecord_t< allocator_t >;
public:

	Allocator() {
		Unbind();
	}

	Allocator( ResourceType& _memory, const uint64_t _size, const uint32_t _type )
	{
		offset = 0;
		memory = _memory;
		size = _size;
		type = _type;
		ptr = NULL;
	}

	void Bind( ResourceType& _memory, void* memMap, const uint64_t _size, const uint32_t _type )
	{
		offset = 0;
		memory = _memory;
		size = _size;
		type = _type;
		ptr = memMap;
	}

	void Unbind()
	{
		offset = 0;
		size = 0;
		type = 0;
		ptr = nullptr;
	}

	ResourceType& GetDeviceMemory() {
		return memory;
	}

	bool IsMemoryCompatible( const uint32_t memoryType ) const {
		return ( type == memoryType );
	}

	void* GetMemoryMapPtr( allocRecord_t& record ) {
		if ( ptr == nullptr ) {
			return nullptr;
		}
		if ( ( record.offset + record.size ) > size ) { // starts from 0 offset
			return nullptr;
		}

		return static_cast<void*>( (uint8_t*)ptr + record.offset );
	}

	uint64_t GetSize() const
	{
		return offset;
	}

	uint64_t GetAlignedOffset( const uint64_t alignment ) const
	{
		const uint64_t boundary = ( offset % alignment );
		const uint64_t nextOffset = ( boundary == 0 ) ? boundary : ( alignment - boundary );

		return ( offset + nextOffset );
	}

	bool CanAllocate( uint64_t alignment, uint64_t allocSize ) const
	{
		const uint64_t nextOffset = GetAlignedOffset( alignment );
		return ( ( nextOffset + allocSize ) < size );
	}

	bool CreateAllocation( uint64_t alignment, uint64_t allocSize, allocRecord_t& subAlloc )
	{
		if ( !CanAllocate( alignment, allocSize ) ) {
			return false;
		}

		const uint64_t nextOffset = GetAlignedOffset( alignment );

		subAlloc.memory = this;
		subAlloc.offset = nextOffset;
		subAlloc.size = allocSize;
		subAlloc.alignment = alignment;
		subAlloc.index = static_cast<int>( allocations.size() );
		allocations.push_back( subAlloc );

		//handle.hdl = hdl_t( subAlloc.index );
		//handle.allocator = this;
		//handles.push_back( allocHandle_t );

		offset = nextOffset + allocSize;

		return true;
	}

	bool DestroyAllocation( uint64_t alignment, uint64_t allocSize, allocRecord_t& subAlloc )
	{
		if ( IsValidIndex( subAlloc.index ) && ( subAlloc.memory == this ) ) {
			freeList.push_back( subAlloc.index );
		}
	}

	void Pack()
	{
		const int numPendingFreeRecords = static_cast<int>( freeList.size() );
		for ( int i = 0; i < numPendingFreeRecords; ++i )
		{
			//	freeList[ i ].index;
		}
	}

private:
	[[nodiscard]]
	bool IsValidIndex( const int index ) const {
		return ( index >= 0 ) && ( index < allocations.size() );
	}

	[[nodiscard]]
	const allocRecord_t* GetRecord( const hdl_t& handle ) const
	{
		const int index = handle.Get();
		if ( IsValidIndex( index ) ) {
			return &allocations[ index ];
		}
		return NULL;
	}

	uint32_t						type;
	uint64_t						size;
	uint64_t						offset;
	ResourceType					memory;
	void* ptr;
	std::vector< allocRecord_t >	allocations;
	//std::vector< allocHdl_t >		handles;
	std::vector< hdl_t >			freeList;

	friend alloc_t< Allocator< ResourceType > >;
};