#pragma once
#include "../globals/common.h"
#include "../render_core/renderResource.h"

class AllocatorMemory;

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

	const AllocatorMemory*	GetMemory() const;
	uint64_t				GetOffset() const;
	uint64_t				GetSize() const;
	uint64_t				GetAlignment() const;
	void*					GetPtr() const;
	void					Free();
};


class AllocatorMemory : public RenderResource
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

#ifdef USE_VULKAN
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
#endif

	void					Create( const uint32_t sizeBytes, const memoryRegion_t region, const resourceLifeTime_t lifetime );
	void					Destroy();

#ifdef USE_VULKAN
	void					Bind( VkDeviceMemory& _memory, void* memMap, const uint64_t _size, const uint32_t _type );
#endif
	void					Unbind();
	bool					IsMemoryCompatible( const uint32_t memoryType ) const;
	void*					GetMemoryMapPtr( const allocRecord_t& record ) const;
	uint64_t				GetSize() const;
	void					AdjustOffset( const uint64_t offset, const uint64_t alignment );
	uint64_t				GetAlignedOffset( const uint64_t alignment ) const;
	bool					CanAllocate( uint64_t alignment, uint64_t allocSize ) const;
	bool					Allocate( uint64_t alignment, uint64_t allocSize, Allocation& handle );
	void					Reset();
	void					Free( hdl_t& handle );
	memoryRegion_t			GetMemoryRegion() const;

#ifdef USE_VULKAN
	VkDeviceMemory			GetVkObject() const;
	uint32_t				GetVkMemoryType() const;
#endif

	[[nodiscard]]
	bool					IsValidIndex( const uint64_t index ) const;

	[[nodiscard]]
	const allocRecord_t*	GetRecord( const hdl_t& handle ) const;
};