#pragma once

#include "../render_binding/allocator.h"

struct imageInfo_t;
class AllocatorMemory;

class GpuImage
{
protected:
#ifdef USE_VULKAN
	VkImage				vk_image[ MaxFrameStates ];
	VkImageView			vk_view[ MaxFrameStates ];
	Allocation			m_allocation;
#endif
	resourceLifetime_t	m_lifetime;
	const char*			m_dbgName;
	int					m_id;

public:

	GpuImage()
	{
#ifdef USE_VULKAN
		vk_image[ 0 ]  = VK_NULL_HANDLE;
		vk_view[ 0 ] = VK_NULL_HANDLE;
#endif
		m_dbgName = "";
		m_id = -1;
	}


	virtual GpuImage::~GpuImage()
	{
		Destroy();
	}

	int GetId() const
	{
		return m_id;
	}

	void SetId( const int id )
	{
		m_id = id;
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