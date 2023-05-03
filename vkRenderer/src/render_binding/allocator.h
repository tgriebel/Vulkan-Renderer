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

template< class AllocatorType >
struct alloc_t;

template< class ResourceType >
class Allocator;

class AllocatorVkMemory;
using AllocationVk = alloc_t<AllocatorVkMemory>;

#pragma once
#include "../globals/common.h"

template< class ResourceType > class Allocator;

struct allocRecord_t
{
	uint64_t	offset;
	uint64_t	size;
	uint64_t	alignment;
	bool		isValid;
};

template< class AllocatorType >
struct alloc_t
{
public:
	alloc_t()
	{
		allocator = nullptr;
	}

	uint64_t GetOffset() const
	{
		if ( IsValid() )
		{
			const allocRecord_t* record = allocator->GetRecord( handle );
			if ( record != nullptr ) {
				return record->offset;
			}
		}
		return 0;
	}

	uint64_t GetSize() const
	{
		if ( IsValid() )
		{
			const allocRecord_t* record = allocator->GetRecord( handle );
			if ( record != nullptr ) {
				return record->size;
			}
		}
		return 0;
	}

	uint64_t GetAlignment() const
	{
		if ( IsValid() )
		{
			const allocRecord_t* record = allocator->GetRecord( handle );
			if ( record != nullptr ) {
				return record->alignment;
			}
		}
		return 0;
	}

	void* GetPtr() const
	{
		if ( IsValid() )
		{
			const allocRecord_t* record = allocator->GetRecord( handle );
			if ( record != nullptr ) {
				return allocator->GetMemoryMapPtr( *record );
			}
		}
		return nullptr;
	}

	void Free()
	{
		if ( IsValid() ) {
			allocator->Free( handle );
		}
	}

private:
	bool IsValid() const
	{
		return ( allocator != nullptr ) && ( handle.Get() >= 0 );
	}

	hdl_t			handle;
	AllocatorType* allocator;

	friend AllocatorType;
};

template< class ResourceType >
class Allocator
{
private:
	using allocator_t = Allocator< ResourceType >;
	using alloc_t = alloc_t< allocator_t >;
public:

	Allocator()
	{
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

	ResourceType& GetMemoryResource()
	{
		return resource;
	}

	bool IsMemoryCompatible( const uint32_t memoryType ) const
	{
		return ( type == memoryType );
	}

	void* GetMemoryMapPtr( const allocRecord_t& record ) const
	{
		if ( ptr == nullptr ) {
			return nullptr;
		}
		if ( ( record.offset + record.size ) > size ) { // starts from 0 offset
			return nullptr;
		}
		uint8_t* bytes = reinterpret_cast<uint8_t*>( ptr );
		return reinterpret_cast< void * >( bytes + record.offset );
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

private:
	uint32_t						type;
	uint64_t						size;
	uint64_t						offset;
	ResourceType					resource;
	void*							ptr;
	std::vector< allocRecord_t >	allocations;
	std::vector< hdl_t >			handles;

	friend alloc_t;
};

class AllocatorVkMemory : public Allocator<VkDeviceMemory>
{
public:
	uint32_t memoryTypeIndex;
};