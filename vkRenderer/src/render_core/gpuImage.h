#pragma once

#include "../render_binding/allocator.h"

struct imageInfo_t;
class AllocatorMemory;

enum gpuImageStateFlags_t : uint8_t
{
	GPU_IMAGE_NONE			= 0,
	GPU_IMAGE_READ			= ( 1 << 0 ),
	GPU_IMAGE_TRANSFER_SRC	= ( 1 << 1 ),
	GPU_IMAGE_TRANSFER_DST	= ( 1 << 2 ),
	GPU_IMAGE_PERSISTENT	= ( 1 << 3 ),
	GPU_IMAGE_ALL			= 0xFF,
};
DEFINE_ENUM_OPERATORS( gpuImageStateFlags_t, uint8_t )

class GpuImage
{
protected:
#ifdef USE_VULKAN
	VkImage				vk_image[ MAX_FRAMES_IN_FLIGHT ];
	VkImageView			vk_view[ MAX_FRAMES_IN_FLIGHT ];
	Allocation			m_allocation;
#endif
	resourceLifetime_t	m_lifetime;
	const char*			m_dbgName;

public:

	GpuImage()
	{
#ifdef USE_VULKAN
		vk_image[ 0 ]  = VK_NULL_HANDLE;
		vk_view[ 0 ] = VK_NULL_HANDLE;
#endif
		m_dbgName = "";
	}


	virtual GpuImage::~GpuImage()
	{
		Destroy();
	}

#ifdef USE_VULKAN
	GpuImage( const char* name, const VkImage image, const VkImageView view )
	{
		vk_image[ 0 ] = image;
		vk_view[ 0 ] = view;
		m_dbgName = name;
	}


	inline VkImage GetVkImage() const
	{
		return vk_image[ 0 ];
	}

	inline VkImageView GetVkImageView() const
	{
		return vk_view[ 0 ];
	}


	inline void DetachVkImage()
	{
		vk_image[ 0 ] = VK_NULL_HANDLE;
	}


	inline void DetachVkImageView()
	{
		vk_view[ 0 ] = VK_NULL_HANDLE;
	}
#endif

	inline const char* GetDebugName() const
	{
		return m_dbgName;
	}

	void Create( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory );
	virtual void Destroy();
};