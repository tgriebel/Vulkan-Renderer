#include "gpuAccelerationStructure.h"
#include "../render_state/cmdContext.h"
#include "../render_state/deviceContext.h"
#include "../render_core/renderUploader.h"

#include <SysCore/common.h>
using namespace SysCore;

extern DeviceContext context;


#ifdef USE_VULKAN_RTX


void GpuAccelerationStructure::Create( const char* name, resourceLifeTime_t lifetime )
{
	m_name = name;
	m_lifetime = lifetime;
	RenderResource::Create( resourceType_t::ACCELERATION_STRUCTURE, lifetime );
}


void GpuAccelerationStructure::AddGeometry( CommandList* cmdList, const blasCreateSurfaceInfo_t& surfaceInfo )
{
	// Temp sanity while RT is being stood up
	assert( surfaceInfo.vb->GetElementSize() == sizeof( vsInput_t ) );
	assert( surfaceInfo.ib->GetElementSize() == sizeof( uint32_t ) );

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = surfaceInfo.vb->GetDeviceAddress();
	triangles.vertexStride = surfaceInfo.vb->GetElementSizeAligned();
	triangles.maxVertex = surfaceInfo.surface->vertexOffset + surfaceInfo.surface->vertexCount - 1;
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = surfaceInfo.ib->GetDeviceAddress();

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.geometry.triangles = triangles;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	m_geometry.push_back( geometry );

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
	rangeInfo.primitiveCount = surfaceInfo.surface->indexCount / 3;
	rangeInfo.primitiveOffset = surfaceInfo.surface->firstIndex * sizeof( uint32_t );
	rangeInfo.firstVertex = surfaceInfo.surface->vertexOffset;
	rangeInfo.transformOffset = 0;

	m_rangeInfo.push_back( rangeInfo );
}


void GpuAccelerationStructure::Cleanup()
{
	for ( blasEntry_t& entry : m_blasEntries )
	{
		if ( entry.handle != VK_NULL_HANDLE )
		{
			context.vkDestroyAccelerationStructureKHR( context.device, entry.handle, nullptr );
			entry.handle = VK_NULL_HANDLE;
		}
	}
	m_blasEntries.clear();

	if ( m_blasStorage.GetMaxSize() > 0 ) {
		m_blasStorage.Destroy();
	}
	if ( m_blasScratch.GetMaxSize() > 0 ) {
		m_blasScratch.Destroy();
	}

	if ( m_tlas != VK_NULL_HANDLE )
	{
		context.vkDestroyAccelerationStructureKHR( context.device, m_tlas, nullptr );
		m_tlas = VK_NULL_HANDLE;
	}
	if( m_tlasStorage.GetMaxSize() > 0 ) {
		m_tlasStorage.Destroy();
	}
	if ( m_tlasScratch.GetMaxSize() > 0 ) {
		m_tlasScratch.Destroy();
	}
	if ( m_tlasInstances.GetMaxSize() > 0 ) {
		m_tlasInstances.Destroy();
	}
}


void GpuAccelerationStructure::Build( CommandList* cmdList )
{
	const uint32_t blasCount = static_cast<uint32_t>( m_geometry.size() );

	if ( blasCount == 0 ) {
		return;
	}

	Cleanup();

	std::cout << "Building RT AS" << std::endl;

	// TODO: query from device properties
	// VkPhysicalDeviceAccelerationStructurePropertiesKHR::minAccelerationStructureScratchOffsetAlignment
	constexpr uint32_t scratchAlignment = 128;
	// VkAccelerationStructureCreateInfoKHR::offset must be a multiple of 256
	constexpr uint32_t storageAlignment = 256;

	m_blasEntries.resize( blasCount );

	std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos( blasCount );
	std::vector<VkAccelerationStructureBuildSizesInfoKHR> allSizes( blasCount );
	std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> pRangeInfos( blasCount );

	// 1: Query all sizes, accumulate total scratch and storage requirements
	uint32_t totalScratchSize = 0;
	uint32_t totalStorageSize = 0;
	for ( uint32_t i = 0; i < blasCount; ++i )
	{
		const uint32_t primCount = m_rangeInfo[ i ].primitiveCount;

		buildInfos[ i ] = {};
		buildInfos[ i ].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildInfos[ i ].type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildInfos[ i ].flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		buildInfos[ i ].geometryCount = 1;
		buildInfos[ i ].pGeometries = &m_geometry[ i ];

		allSizes[ i ].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		context.vkGetAccelerationStructureBuildSizesKHR( context.device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&buildInfos[ i ], &primCount, &allSizes[ i ] );

		totalScratchSize += Align( (uint64_t)allSizes[ i ].buildScratchSize, (uint64_t)scratchAlignment );
		totalStorageSize += Align( (uint64_t)allSizes[ i ].accelerationStructureSize, (uint64_t)storageAlignment );
	}
	m_blasScratch.Create( m_name, swapBuffering_t::SINGLE_FRAME, m_lifetime, totalScratchSize, 1, bufferType_t::STORAGE, bufferFlags_t::RT_VISIBLE );
	m_blasStorage.Create( m_name, swapBuffering_t::SINGLE_FRAME, m_lifetime, totalStorageSize, 1, bufferType_t::ACCELERATION_STRUCTURE );

	// 2: Create AS handles, assign scratch and storage views
	uint32_t scratchOffset = 0;
	uint32_t storageOffset = 0;
	for ( uint32_t i = 0; i < blasCount; ++i )
	{
		blasEntry_t& entry = m_blasEntries[ i ];
		const VkAccelerationStructureBuildSizesInfoKHR& sizes = allSizes[ i ];

		entry.storageView = m_blasStorage.GetBufferView( storageOffset, static_cast<uint32_t>( sizes.accelerationStructureSize ) );

		VkAccelerationStructureCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.buffer = m_blasStorage.GetVkObject();
		createInfo.offset = storageOffset;
		createInfo.size = sizes.accelerationStructureSize;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

		VK_CHECK_RESULT( context.vkCreateAccelerationStructureKHR( context.device, &createInfo, nullptr, &entry.handle ) );

		entry.scratchView = m_blasScratch.GetBufferView( scratchOffset, static_cast<uint32_t>( sizes.buildScratchSize ) );

		scratchOffset += Align( (uint64_t)sizes.buildScratchSize, (uint64_t)scratchAlignment );
		storageOffset += Align( (uint64_t)sizes.accelerationStructureSize, (uint64_t)storageAlignment );

		buildInfos[ i ].dstAccelerationStructure = entry.handle;
		buildInfos[ i ].scratchData.deviceAddress = entry.scratchView.GetDeviceAddress();
		pRangeInfos[ i ] = &m_rangeInfo[ i ];
	}

	// 3: Barrier: vertex/index data was written by vkCmdCopyBuffer (TRANSFER_WRITE).
	// The AS build reads geometry data as VK_ACCESS_SHADER_READ_BIT — ensure the copies are visible.
	VkMemoryBarrier geometryUploadBarrier{};
	geometryUploadBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	geometryUploadBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	geometryUploadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier( cmdList->CommandBuffer(),
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0, 1, &geometryUploadBarrier, 0, nullptr, 0, nullptr );

	// 4: Submit all BLAS builds in one call
	context.vkCmdBuildAccelerationStructuresKHR( cmdList->CommandBuffer(), blasCount, buildInfos.data(), pRangeInfos.data() );

	m_geometry.clear();
	m_rangeInfo.clear();
	m_blasScratch.Destroy();

	// 5: Construct TLAS
	{
		// Barrier: all BLAS writes must be visible before the TLAS build reads them
		VkMemoryBarrier blasBarrier{};
		blasBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		blasBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		blasBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
			| VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		vkCmdPipelineBarrier( cmdList->CommandBuffer(),
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &blasBarrier, 0, nullptr, 0, nullptr );

		// Build instance array per BLAS.
		// TODO: Build matrices
		std::vector<VkAccelerationStructureInstanceKHR> instances( blasCount );
		for( uint32_t i = 0; i < blasCount; ++i )
		{
			VkAccelerationStructureInstanceKHR& inst = instances[ i ];
			inst = {};
			inst.transform.matrix[ 0 ][ 0 ] = 1.0f;
			inst.transform.matrix[ 1 ][ 1 ] = 1.0f;
			inst.transform.matrix[ 2 ][ 2 ] = 1.0f;
			inst.instanceCustomIndex = i;
			inst.mask = 0xFF;
			inst.instanceShaderBindingTableRecordOffset = 0;
			inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			inst.accelerationStructureReference = GetBlasDeviceAddress( i );
		}

		m_tlasInstances.Create( m_name, swapBuffering_t::SINGLE_FRAME, m_lifetime,
			blasCount, sizeof( VkAccelerationStructureInstanceKHR ), bufferType_t::TLAS_INSTANCE_DATA );
		m_tlasInstances.CopyData( instances.data(), instances.size() * sizeof( VkAccelerationStructureInstanceKHR ) );

		// TLAS geometry points at the instance buffer
		VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
		instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		instancesData.arrayOfPointers = VK_FALSE;
		instancesData.data.deviceAddress = m_tlasInstances.GetDeviceAddress();

		VkAccelerationStructureGeometryKHR tlasGeometry{};
		tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		tlasGeometry.geometry.instances = instancesData;

		VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{};
		tlasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		tlasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		tlasBuildInfo.geometryCount = 1;
		tlasBuildInfo.pGeometries = &tlasGeometry;

		VkAccelerationStructureBuildSizesInfoKHR tlasSizes{};
		tlasSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		context.vkGetAccelerationStructureBuildSizesKHR( context.device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&tlasBuildInfo, &blasCount, &tlasSizes );

		m_tlasScratch.Create( m_name, swapBuffering_t::SINGLE_FRAME, m_lifetime,
			static_cast<uint32_t>( tlasSizes.buildScratchSize ), 1, bufferType_t::STORAGE, bufferFlags_t::RT_VISIBLE );
		m_tlasStorage.Create( m_name, swapBuffering_t::SINGLE_FRAME, m_lifetime,
			static_cast<uint32_t>( tlasSizes.accelerationStructureSize ), 1, bufferType_t::ACCELERATION_STRUCTURE );

		VkAccelerationStructureCreateInfoKHR tlasCreateInfo{};
		tlasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		tlasCreateInfo.buffer = m_tlasStorage.GetVkObject();
		tlasCreateInfo.offset = 0;
		tlasCreateInfo.size = tlasSizes.accelerationStructureSize;
		tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		VK_CHECK_RESULT( context.vkCreateAccelerationStructureKHR( context.device, &tlasCreateInfo, nullptr, &m_tlas ) );

		tlasBuildInfo.dstAccelerationStructure = m_tlas;
		tlasBuildInfo.scratchData.deviceAddress = m_tlasScratch.GetDeviceAddress();

		VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo{};
		tlasRangeInfo.primitiveCount = blasCount;
		const VkAccelerationStructureBuildRangeInfoKHR* pTlasRangeInfo = &tlasRangeInfo;

		context.vkCmdBuildAccelerationStructuresKHR( cmdList->CommandBuffer(), 1, &tlasBuildInfo, &pTlasRangeInfo );

		m_tlasScratch.Destroy();
		m_tlasInstances.Destroy();
	}
}


VkDeviceAddress GpuAccelerationStructure::GetBlasDeviceAddress( uint32_t index ) const
{
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = m_blasEntries[ index ].handle;
	return context.vkGetAccelerationStructureDeviceAddressKHR( context.device, &addressInfo );
}


VkDeviceAddress GpuAccelerationStructure::GetDeviceAddress() const
{
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = m_tlas;
	return context.vkGetAccelerationStructureDeviceAddressKHR( context.device, &addressInfo );
}


VkAccelerationStructureKHR GpuAccelerationStructure::GetVkObject() const
{
	return m_tlas;
}


void GpuAccelerationStructure::Destroy()
{
	Cleanup();
}

#endif
