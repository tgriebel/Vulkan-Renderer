#pragma once

#include "../globals/common.h"
#include "../render_core/renderResource.h"
#include "gpuBuffer.h"

class CommandList;

struct surfaceUpload_t;

struct blasCreateSurfaceInfo_t
{
	const char*				name;
	const GpuBuffer*		vb;
	const GpuBuffer*		ib;
	const surfaceUpload_t*	surface;
};


class GpuAccelerationStructure : public RenderResource
{
private:

#ifdef USE_VULKAN_RTX
	struct blasEntry_t
	{
		GpuBufferView				scratchView;
		GpuBufferView				storageView;
		VkAccelerationStructureKHR	handle = VK_NULL_HANDLE;
	};

	// Pending geometry accumulated by AddGeometry(), consumed by Build()
	std::vector<VkAccelerationStructureGeometryKHR>			m_geometry;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR>	m_rangeInfo;

	// One entry per surface BLAS, populated by Build()
	std::vector<blasEntry_t>	m_blasEntries;
	GpuBuffer					m_blasScratch;		// Shared scratch for all BLAS builds
	GpuBuffer					m_blasStorage;		// Shared storage for all BLAS handles

	// TLAS
	GpuBuffer					m_tlasInstances;	// One per BLAS
	GpuBuffer					m_tlasStorage;
	GpuBuffer					m_tlasScratch;
	VkAccelerationStructureKHR	m_tlas = VK_NULL_HANDLE;

	const char*			m_name     = nullptr;
	resourceLifeTime_t	m_lifetime = {};

	void				Cleanup();
#endif

public:
	void						Create( const char* name, resourceLifeTime_t lifetime );
	void						AddGeometry( CommandList* cmdList, const blasCreateSurfaceInfo_t& surfaceInfo );
	void						Build( CommandList* cmdList );
	void						Destroy() override;

#ifdef USE_VULKAN_RTX
	VkDeviceAddress				GetBlasDeviceAddress( uint32_t index ) const;
	uint32_t					GetBlasCount() const { return static_cast<uint32_t>( m_blasEntries.size() ); }

	VkDeviceAddress				GetDeviceAddress() const;
	VkAccelerationStructureKHR	GetVkObject() const;
#endif
};
