#include "gpuAccelerationStructure.h"
#include "../render_state/cmdContext.h"
#include "../render_state/deviceContext.h"

extern DeviceContext context;


#ifdef USE_VULKAN_RTX

void GpuAccelerationStructure::Allocate( const char* name, const VkAccelerationStructureTypeKHR type, const VkAccelerationStructureBuildSizesInfoKHR& sizes, const resourceLifeTime_t lifetime )
{
	const uint32_t storageSize = static_cast<uint32_t>( sizes.accelerationStructureSize );
	m_storageBuffer.Create( name, swapBuffering_t::SINGLE_FRAME, lifetime, 1, storageSize, bufferType_t::ACCELERATION_STRUCTURE );

	VkAccelerationStructureCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.buffer = m_storageBuffer.GetVkObject();
	createInfo.size = sizes.accelerationStructureSize;
	createInfo.type = type;

	VK_CHECK_RESULT( context.vkCreateAccelerationStructureKHR( context.device, &createInfo, nullptr, &m_accelerationStructure ) );
}


void GpuAccelerationStructure::CreateBlas( const blasCreateInfo_t& info, CommandList* cmdList, GpuBuffer& scratchBuffer )
{
	RenderResource::Create( resourceType_t::ACCELERATION_STRUCTURE, info.lifetime );

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = info.vertexFormat;
	triangles.vertexData.deviceAddress = info.vertexBufferAddress;
	triangles.vertexStride = info.vertexStride;
	triangles.maxVertex = info.vertexCount - 1;
	triangles.indexType = info.indexType;
	triangles.indexData.deviceAddress = info.indexBufferAddress;

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.geometry.triangles = triangles;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;

	VkAccelerationStructureBuildSizesInfoKHR sizes{};
	sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	context.vkGetAccelerationStructureBuildSizesKHR( context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &info.primitiveCount, &sizes );

	assert( scratchBuffer.GetMaxSize() >= sizes.buildScratchSize );

	Allocate( info.name, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, sizes, info.lifetime );

	buildInfo.dstAccelerationStructure = m_accelerationStructure;
	buildInfo.scratchData.deviceAddress = scratchBuffer.GetDeviceAddress();

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
	rangeInfo.primitiveCount = info.primitiveCount;

	const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
	context.vkCmdBuildAccelerationStructuresKHR( cmdList->CommandBuffer(), 1, &buildInfo, &pRangeInfo );
}


void GpuAccelerationStructure::CreateTlas( const tlasCreateInfo_t& info, VkDeviceAddress instanceBufferAddress, CommandList* cmdList, GpuBuffer& scratchBuffer )
{
	RenderResource::Create( resourceType_t::ACCELERATION_STRUCTURE, info.lifetime );

	VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
	instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instancesData.data.deviceAddress = instanceBufferAddress;

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances = instancesData;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;

	VkAccelerationStructureBuildSizesInfoKHR sizes{};
	sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	context.vkGetAccelerationStructureBuildSizesKHR( context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &info.instanceCount, &sizes );

	assert( scratchBuffer.GetMaxSize() >= sizes.buildScratchSize );

	Allocate( info.name, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, sizes, info.lifetime );

	buildInfo.dstAccelerationStructure = m_accelerationStructure;
	buildInfo.scratchData.deviceAddress = scratchBuffer.GetDeviceAddress();

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
	rangeInfo.primitiveCount = info.instanceCount;

	const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
	context.vkCmdBuildAccelerationStructuresKHR( cmdList->CommandBuffer(), 1, &buildInfo, &pRangeInfo );
}


VkDeviceAddress GpuAccelerationStructure::GetDeviceAddress() const
{
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = m_accelerationStructure;
	return context.vkGetAccelerationStructureDeviceAddressKHR( context.device, &addressInfo );
}


VkAccelerationStructureKHR GpuAccelerationStructure::GetVkObject() const
{
	return m_accelerationStructure;
}


void GpuAccelerationStructure::Destroy()
{
	if ( m_accelerationStructure != VK_NULL_HANDLE )
	{
		context.vkDestroyAccelerationStructureKHR( context.device, m_accelerationStructure, nullptr );
		m_accelerationStructure = VK_NULL_HANDLE;
	}
	m_storageBuffer.Destroy();
}

#endif
