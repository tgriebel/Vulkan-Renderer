#pragma once

#include "../render_binding/allocator.h"

struct imageInfo_t;
class AllocatorMemory;

enum gpuImageStateFlags_t : uint8_t
{
	GPU_IMAGE_NONE			= 0,
	GPU_IMAGE_READ			= ( 1 << 0 ),
	GPU_IMAGE_WRITE			= ( 1 << 1 ),
	GPU_IMAGE_TRANSFER_SRC	= ( 1 << 2 ),
	GPU_IMAGE_TRANSFER_DST	= ( 1 << 3 ),
	GPU_IMAGE_PERSISTENT	= ( 1 << 4 ),
	GPU_IMAGE_PRESENT		= ( 1 << 5 ),
	GPU_IMAGE_TRANSFER		= ( GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST ),
	GPU_IMAGE_RW			= ( GPU_IMAGE_READ | GPU_IMAGE_WRITE ),
	GPU_IMAGE_ALL			= 0xFF,
};
DEFINE_ENUM_OPERATORS( gpuImageStateFlags_t, uint8_t )

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

	inline uint32_t GetBufferId( const uint32_t bufferId = 0 ) const
	{
		const uint32_t bufferCount = GetBufferCount();
		return Min( bufferId, bufferCount - 1 );
	}

	inline uint32_t GetBufferCount() const
	{
		return ( m_lifetime == LIFETIME_PERSISTENT ) ? MaxFrameStates : 1;
	}

public:

	GpuImage()
	{
#ifdef USE_VULKAN
		for( uint32_t i = 0; i < MaxFrameStates; ++i )
		{
			vk_image[ i ]  = VK_NULL_HANDLE;
			vk_view[ i ] = VK_NULL_HANDLE;
		}
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

	inline uint64_t GetAlignment()
	{
		return m_allocation.GetAlignment();
	}

#ifdef USE_VULKAN
	GpuImage( const char* name, const VkImage image, const VkImageView view )
	{
		vk_image[ 0 ] = image;
		vk_view[ 0 ] = view;
		m_dbgName = name;
	}


	inline VkImage GetVkImage( const uint32_t bufferId = 0 ) const
	{
		return vk_image[ GetBufferId( bufferId ) ];
	}


	inline VkImageView GetVkImageView( const uint32_t bufferId = 0 ) const
	{
		return vk_view[ GetBufferId( bufferId ) ];
	}


	inline void DetachVkImage( const uint32_t bufferId = 0 )
	{
		vk_image[ GetBufferId( bufferId ) ] = VK_NULL_HANDLE;
	}


	inline void DetachVkImageView( const uint32_t bufferId = 0 )
	{
		vk_view[ GetBufferId( bufferId ) ] = VK_NULL_HANDLE;
	}
#endif
	inline const char* GetDebugName() const
	{
		return m_dbgName;
	}

	void Create( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory );
	virtual void Destroy();
};