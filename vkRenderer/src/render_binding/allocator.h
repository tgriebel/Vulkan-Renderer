#pragma once
#include "common.h"

template< class ResourceType >
class Allocator
{
private:
	using allocator_t = Allocator< ResourceType >;
	using alloc_t = alloc_t< allocator_t >;
public:

	Allocator() {
		Unbind();
	}

	Allocator( ResourceType& _memory, const uint64_t _size, const uint32_t _type )
	{
		offset = 0;
		resource = _memory;
		size = _size;
		type = _type;
		ptr = nullptr;
	}

	void Bind( ResourceType& _memory, void* memMap, const uint64_t _size, const uint32_t _type )
	{
		offset = 0;
		resource = _memory;
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

	ResourceType& GetMemoryResource() {
		return resource;
	}

	bool IsMemoryCompatible( const uint32_t memoryType ) const {
		return ( type == memoryType );
	}

	void* GetMemoryMapPtr( const allocRecord_t& record ) const {
		if ( ptr == nullptr ) {
			return nullptr;
		}
		if ( ( record.offset + record.size ) > size ) { // starts from 0 offset
			return nullptr;
		}
		uint8_t* bytes = reinterpret_cast<uint8_t*>( ptr );
		return reinterpret_cast< void * >( bytes + record.offset );
	}

	uint64_t GetSize() const {
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

	bool Allocate( uint64_t alignment, uint64_t allocSize, alloc_t& handle )
	{
		if ( !CanAllocate( alignment, allocSize ) ) {
			return false;
		}

		const uint64_t nextOffset = GetAlignedOffset( alignment );

		const int index = static_cast<int>( allocations.size() );

		allocRecord_t alloc;
		alloc.offset = nextOffset;
		alloc.size = allocSize;
		alloc.alignment = alignment;
		alloc.isValid = true;
		allocations.push_back( alloc );

		handle.handle = hdl_t( index );
		handle.allocator = this;
		handles.push_back( index );

		offset = nextOffset + allocSize;

		return true;
	}

	void Reset()
	{
		for ( int i = 0; i < static_cast<int>( handles.size() ); ++i ) {
			handles[ i ] = INVALID_HDL;
		}
		offset = 0;
		allocations.resize( 0 );
		handles.resize( 0 );
	}

	void Free( hdl_t& handle )
	{
		if ( IsValidIndex( handle.Get() ) ) {
			allocations[ handle.Get() ].isValid = false;
		}
		handle.Reset();
	}

	void Pack()
	{

	}

	void ReassignHandles() {
		for( int i = 0; i < static_cast< int >( handles.size() ); ++i ) {
			handles[ i ].Reassign( i );
		}
	}

private:
	[[nodiscard]]
	bool IsValidIndex( const uint64_t index ) const {
		return ( index >= 0 ) && ( index < allocations.size() );
	}

	[[nodiscard]]
	const allocRecord_t* GetRecord( const hdl_t& handle ) const
	{
		const uint64_t index = handle.Get();
		if ( IsValidIndex( index ) ) {
			return &allocations[ index ];
		}
		return nullptr;
	}

	uint32_t						type;
	uint64_t						size;
	uint64_t						offset;
	ResourceType					resource;
	void*							ptr;
	std::vector< allocRecord_t >	allocations;
	std::vector< hdl_t >			handles;

	friend alloc_t;
};