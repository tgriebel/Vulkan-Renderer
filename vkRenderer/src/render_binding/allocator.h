#pragma once
#include "../globals/common.h"
#include "../render_core/renderResource.h"

#ifdef USE_VULKAN
#include "../../external/vk_mem_alloc.h"
#endif

class AllocatorMemory;


class Allocation
{
private:
#ifdef USE_VULKAN
	VmaAllocation		m_allocation = VK_NULL_HANDLE;
	VmaAllocationInfo	m_info = {};
#endif
	uint64_t			m_alignment = 0;

	friend class AllocatorMemory;
	friend class GpuBuffer;
	friend class GpuImage;

public:
	Allocation() = default;

	uint64_t				GetOffset() const;
	uint64_t				GetSize() const;
	uint64_t				GetAlignment() const;
	void*					GetPtr() const;
	void					Free();

#ifdef USE_VULKAN
	VmaAllocation			GetVmaAllocation() const { return m_allocation; }
#endif
};


class AllocatorMemory : public RenderResource
{
private:
	memoryRegion_t			m_memoryRegion = memoryRegion_t::UNKNOWN;

#ifdef USE_VULKAN
	static VmaAllocator		s_allocator;
#endif

public:
	AllocatorMemory() = default;

	void					Create( const uint32_t sizeBytes, const memoryRegion_t region, const resourceLifeTime_t lifetime );
	void					Destroy();

	memoryRegion_t			GetMemoryRegion() const;

#ifdef USE_VULKAN
	static VmaAllocator		GetVmaAllocator();
	static void				CreateVmaAllocator();
	static void				DestroyVmaAllocator();
#endif
};
