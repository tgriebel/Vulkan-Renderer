#include "gpuImage.h"

#include <GfxCore/asset_types/texture.h>
#include "../render_state/deviceContext.h"
#include "../render_state/rhi.h"

// TODO: move
#ifdef USE_VULKAN	
static VkImageCreateInfo vk_GetImageCreateInfo( const imageInfo_t& info, const gpuImageStateFlags_t flags )
{
	VkImageCreateInfo imageInfo{ };
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>( info.width );
	imageInfo.extent.height = static_cast<uint32_t>( info.height );
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = info.mipLevels;
	imageInfo.arrayLayers = ( info.type == IMAGE_TYPE_CUBE ) ? 6 : info.layers;
	imageInfo.format = vk_GetTextureFormat( info.fmt );
	imageInfo.tiling = ( info.tiling == IMAGE_TILING_LINEAR ) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.samples = vk_GetSampleCount( info.subsamples );

	imageInfo.usage = 0;
	if( ( flags & GPU_IMAGE_WRITE ) != 0 )
	{
		imageInfo.usage |= ( info.aspect & IMAGE_ASPECT_COLOR_FLAG ) != 0 ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
		imageInfo.usage |= ( info.aspect & IMAGE_ASPECT_DEPTH_FLAG ) != 0 ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
		imageInfo.usage |= ( info.aspect & IMAGE_ASPECT_STENCIL_FLAG ) != 0 ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
	}
	imageInfo.usage |= ( flags & GPU_IMAGE_READ ) != 0 ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;
	imageInfo.usage |= ( flags & GPU_IMAGE_TRANSFER_SRC ) != 0 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0;
	imageInfo.usage |= ( flags & GPU_IMAGE_TRANSFER_DST ) != 0 ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;

	imageInfo.flags = 0;
	imageInfo.flags |= ( info.type == IMAGE_TYPE_CUBE ) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

	return imageInfo;
}
#endif

void GpuImage::Create( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory )
{
	VkImageCreateInfo imageInfo = vk_GetImageCreateInfo( info, flags );

	VkImageStencilUsageCreateInfo stencilUsage{};
	if ( ( info.aspect & ( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG ) ) != 0 )
	{
		stencilUsage.sType = VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO;
		stencilUsage.stencilUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
		imageInfo.pNext = &stencilUsage;
	}

	m_dbgName = name;
	m_swapBuffering = ( flags & GPU_IMAGE_PERSISTENT ) != 0 ? swapBuffering_t::MULTI_FRAME : swapBuffering_t::SINGLE_FRAME;

	const uint32_t bufferCount = GetBufferCount();
	for ( uint32_t i = 0; i < bufferCount; ++i )
	{
		VK_CHECK_RESULT( vkCreateImage( context.device, &imageInfo, nullptr, &vk_image[ i ] ) );

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements( context.device, vk_image[ i ], &memRequirements );

		VkMemoryAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = memory.GetVkMemoryType();

		if ( memory.Allocate( memRequirements.alignment, memRequirements.size, m_allocation ) ) {
			vkBindImageMemory( context.device, vk_image[ i ], memory.GetVkObject(), m_allocation.GetOffset() );
		} else {
			throw std::runtime_error( "Buffer could not be allocated!" );
		}

		vk_MarkerSetObjectName( (uint64_t)vk_image[ i ], VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, m_dbgName );

		vk_view[ i ] = vk_CreateImageView( vk_image[ i ], info );
	}
}


void GpuImage::Destroy()
{
	assert( context.device != VK_NULL_HANDLE );
	if ( context.device != VK_NULL_HANDLE )
	{
		const uint32_t bufferCount = GetBufferCount();
		for ( uint32_t i = 0; i < bufferCount; ++i )
		{
			if( vk_view[ i ] != VK_NULL_HANDLE ) {
				vkDestroyImageView( context.device, vk_view[ i ], nullptr );
				vk_view[ i ] = VK_NULL_HANDLE;
			}
			if ( vk_image[ i ] != VK_NULL_HANDLE ) {
				vkDestroyImage( context.device, vk_image[ i ], nullptr );
				vk_image[ i ] = VK_NULL_HANDLE;
			}
		}
		m_allocation.Free();
	}
}