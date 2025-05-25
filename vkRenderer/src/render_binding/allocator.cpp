#include "allocator.h"
#include "../render_state/deviceContext.h"

const AllocatorMemory* Allocation::GetMemory() const
{
	return allocator;
}


uint64_t Allocation::GetOffset() const
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


uint64_t Allocation::GetSize() const
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


uint64_t Allocation::GetAlignment() const
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


void* Allocation::GetPtr() const
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


void Allocation::Free()
{
	if ( IsValid() ) {
		allocator->Free( handle );
	}
}


void AllocatorMemory::Create( const uint32_t sizeBytes, const memoryRegion_t region, const renderResourceLifeTime_t lifetime )
{
	// Resource Management
	{
		RenderResource::Create( lifetime );
	}

#ifdef USE_VULKAN
	{
		VkMemoryPropertyFlags flags = 0;
		if ( region == SHARED ) {
			flags = VkMemoryPropertyFlagBits( VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		} else if ( region == LOCAL ) {
			flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		} else {
			assert(0);
		}
		m_memoryRegion = region;

		uint32_t typeIndex = vk_FindMemoryType( ~0x00, flags );

		VkMemoryAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = sizeBytes;
		allocInfo.memoryTypeIndex = typeIndex;

		VkDeviceMemory memory;
		VK_CHECK_RESULT( vkAllocateMemory( context.device, &allocInfo, nullptr, &memory ) );

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );
		VkMemoryType type = memProperties.memoryTypes[ typeIndex ];

		void* memPtr = nullptr;
		if ( ( type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) == 0 ) {
			VK_CHECK_RESULT( vkMapMemory( context.device, memory, 0, VK_WHOLE_SIZE, 0, &memPtr ) );
		}

		Bind( memory, memPtr, sizeBytes, typeIndex );
		vk_memoryTypeIndex = typeIndex;
	}
#endif

	Reset();
}


void AllocatorMemory::Destroy()
{
	vkFreeMemory( context.device, vk_deviceMemory, nullptr );
	Unbind();
	Reset();
}


void AllocatorMemory::Bind( VkDeviceMemory& _memory, void* memMap, const uint64_t _size, const uint32_t _type )
{
	m_offset = 0;
	vk_deviceMemory = _memory;
	m_size = _size;
	m_type = _type;
	ptr = memMap;
}


void AllocatorMemory::Unbind()
{
	m_offset = 0;
	m_size = 0;
	m_type = 0;
	ptr = nullptr;
}

#ifdef USE_VULKAN
VkDeviceMemory AllocatorMemory::GetVkObject() const
{
	return vk_deviceMemory;
}


uint32_t AllocatorMemory::GetVkMemoryType() const
{
	return vk_memoryTypeIndex;
}
#endif

bool AllocatorMemory::IsMemoryCompatible( const uint32_t memoryType ) const
{
	return ( m_type == memoryType );
}


void* AllocatorMemory::GetMemoryMapPtr( const allocRecord_t& record ) const
{
	if ( ptr == nullptr ) {
		return nullptr;
	}
	if ( ( record.offset + record.size ) > m_size ) { // starts from 0 m_offset
		return nullptr;
	}
	uint8_t* bytes = reinterpret_cast<uint8_t*>( ptr );
	return reinterpret_cast<void*>( bytes + record.offset );
}


uint64_t AllocatorMemory::GetSize() const
{
	return m_offset;
}


void AllocatorMemory::AdjustOffset( const uint64_t offset, const uint64_t alignment )
{
	if( alignment == 0 ) {
		m_offset = offset;
	} else {
		m_offset = GetAlignedOffset( alignment );
	}
}


uint64_t AllocatorMemory::GetAlignedOffset( const uint64_t alignment ) const
{
	const uint64_t boundary = ( m_offset % alignment );
	const uint64_t nextOffset = ( boundary == 0 ) ? boundary : ( alignment - boundary );

	return ( m_offset + nextOffset );
}


bool AllocatorMemory::CanAllocate( uint64_t alignment, uint64_t allocSize ) const
{
	const uint64_t nextOffset = GetAlignedOffset( alignment );
	return ( ( nextOffset + allocSize ) < m_size );
}


bool AllocatorMemory::Allocate( uint64_t alignment, uint64_t allocSize, Allocation& handle )
{
	if ( !CanAllocate( alignment, allocSize ) ) {
		return false;
	}

	const uint64_t nextOffset = GetAlignedOffset( alignment );

	const int index = static_cast<int>( m_allocations.size() );

	allocRecord_t alloc;
	alloc.offset = nextOffset;
	alloc.size = allocSize;
	alloc.alignment = alignment;
	alloc.isValid = true;
	m_allocations.push_back( alloc );

	handle.handle = hdl_t( index );
	handle.allocator = this;
	m_handles.push_back( index );

	m_offset = nextOffset + allocSize;

	return true;
}


void AllocatorMemory::Reset()
{
	for ( int i = 0; i < static_cast<int>( m_handles.size() ); ++i ) {
		m_handles[ i ] = INVALID_HDL;
	}
	m_offset = 0;
	m_allocations.resize( 0 );
	m_handles.resize( 0 );
}


void AllocatorMemory::Free( hdl_t& handle )
{
	if ( IsValidIndex( handle.Get() ) ) {
		m_allocations[ handle.Get() ].isValid = false;
	}
	handle.Reset();
}


memoryRegion_t AllocatorMemory::GetMemoryRegion() const
{
	return m_memoryRegion;
}


bool AllocatorMemory::IsValidIndex( const uint64_t index ) const
{
	return ( index >= 0 ) && ( index < m_allocations.size() );
}


const allocRecord_t* AllocatorMemory::GetRecord( const hdl_t& handle ) const
{
	const uint64_t index = handle.Get();
	if ( IsValidIndex( index ) ) {
		return &m_allocations[ index ];
	}
	return nullptr;
}