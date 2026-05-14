#pragma once

#include "../globals/common.h"
#include "../render_core/renderResource.h"
#include "gpuBuffer.h"

class CommandList;


struct blasCreateInfo_t
{
	const char*			name;
	VkDeviceAddress		vertexBufferAddress;
	uint32_t			vertexCount;
	VkDeviceSize		vertexStride;
	VkFormat			vertexFormat;
	VkDeviceAddress		indexBufferAddress;
	VkIndexType			indexType;
	uint32_t			primitiveCount;
	resourceLifeTime_t	lifetime;
};


struct tlasCreateInfo_t
{
	const char*			name;
	uint32_t			instanceCount;
	resourceLifeTime_t	lifetime;
};


class GpuAccelerationStructure : public RenderResource
{
private:

	GpuBuffer					m_scratchBuffer;
	GpuBuffer					m_storageBuffer;
#ifdef USE_VULKAN_RTX
	VkAccelerationStructureKHR	m_accelerationStructure = VK_NULL_HANDLE;
#endif

public:
	void						AddGeometry( const blasCreateInfo_t& info, CommandList* cmdList, GpuBuffer& scratchBuffer );
	void						Create( CommandList* cmdList, const tlasCreateInfo_t& info, VkDeviceAddress instanceBufferAddress );
	void						Destroy() override;

#ifdef USE_VULKAN_RTX
	VkDeviceAddress				GetDeviceAddress() const;
	VkAccelerationStructureKHR	GetVkObject() const;
#endif
};
