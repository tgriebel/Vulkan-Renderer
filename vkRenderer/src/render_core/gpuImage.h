#pragma once

#include "../render_binding/allocator.h"
#include "../render_core/renderResource.h"

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

class GpuImage : public RenderResource
{
protected:
#ifdef USE_VULKAN
	VkImage				vk_image[ MaxFrameStates ];
	VkImageView			vk_view[ MaxFrameStates ];
	Allocation			m_allocation;
#endif
	swapBuffering_t		m_swapBuffering;
	const char*			m_dbgName;
	int					m_id;

	inline uint32_t GetBufferId( const uint32_t bufferId = 0 ) const
	{
		const uint32_t bufferCount = GetBufferCount();
		return Min( bufferId, bufferCount - 1 );
	}

public:
	GpuImage( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory, const resourceLifeTime_t lifetime )
	{
		Create( name, info, flags, memory, lifetime );
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

	inline uint32_t GetBufferCount() const
	{
		return ( m_swapBuffering == swapBuffering_t::MULTI_FRAME ) ? MaxFrameStates : 1;
	}

#ifdef USE_VULKAN
	GpuImage( const char* name, const VkImage image, const VkImageView view )
	{
		vk_image[ 0 ] = image;
		vk_view[ 0 ] = view;
		m_dbgName = name;
		m_swapBuffering = swapBuffering_t::SINGLE_FRAME;
	}

	// TODO: take in swapchain
	GpuImage( const char* name, const VkImage* image, const VkImageView* view, const gpuImageStateFlags_t flags )
	{
		m_dbgName = name;
		m_swapBuffering = ( flags & GPU_IMAGE_PERSISTENT ) != 0 ? swapBuffering_t::MULTI_FRAME : swapBuffering_t::SINGLE_FRAME;

		const uint32_t bufferCount = GetBufferCount();
		for ( uint32_t i = 0; i < bufferCount; ++i )
		{
			vk_image[ i ] = image[ i ];
			vk_view[ i ] = view[ i ];
		}
	}


	inline VkImage GetVkImage( const uint32_t bufferId ) const
	{
		return vk_image[ GetBufferId( bufferId ) ];
	}


	inline VkImageView GetVkImageView( const uint32_t bufferId ) const
	{
		return vk_view[ GetBufferId( bufferId ) ];
	}


	inline void DetachVkImage()
	{
		const uint32_t bufferCount = GetBufferCount();
		for ( uint32_t i = 0; i < bufferCount; ++i )
		{
			vk_image[ i ] = VK_NULL_HANDLE;
		}
	}


	inline void DetachVkImageView()
	{
		const uint32_t bufferCount = GetBufferCount();
		for ( uint32_t i = 0; i < bufferCount; ++i )
		{
			vk_view[ i ] = VK_NULL_HANDLE;
		}
	}
#endif
	inline const char* GetDebugName() const
	{
		return m_dbgName;
	}

	void Create( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory, const resourceLifeTime_t lifetime );
	virtual void Destroy();
};