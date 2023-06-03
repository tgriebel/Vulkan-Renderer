#include "gpuImage.h"

#include <GfxCore/asset_types/texture.h>
#include "../render_state/deviceContext.h"
#include "../render_state/rhi.h"

void GpuImage::Create( const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory )
{
	VkImageCreateInfo imageInfo{ };
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>( info.width );
	imageInfo.extent.height = static_cast<uint32_t>( info.height );
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = info.mipLevels;
	imageInfo.arrayLayers = info.layers;
	imageInfo.format = vk_GetTextureFormat( info.fmt );
	imageInfo.tiling = ( info.tiling == IMAGE_TILING_LINEAR ) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = vk_GetSampleCount( info.subsamples );

	if ( ( info.aspect & IMAGE_ASPECT_COLOR_FLAG ) != 0 ) {
		imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	if ( ( info.aspect & IMAGE_ASPECT_DEPTH_FLAG ) != 0 ) {
		imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}

	if ( ( flags & GPU_IMAGE_READ ) != 0 ) {
		imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}

	if ( ( flags & GPU_IMAGE_TRANSFER_SRC ) != 0 ) {
		imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}

	if ( ( flags & GPU_IMAGE_TRANSFER_DST ) != 0 ) {
		imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	VkImageStencilUsageCreateInfo stencilUsage{};
	if ( ( info.aspect & ( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG ) ) != 0 )
	{
		stencilUsage.sType = VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO;
		stencilUsage.stencilUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
		imageInfo.pNext = &stencilUsage;
	}
	else
	{
		imageInfo.flags = ( info.type == IMAGE_TYPE_CUBE ) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	}

	if ( vkCreateImage( context.device, &imageInfo, nullptr, &vk_image ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create image!" );
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements( context.device, vk_image, &memRequirements );

	VkMemoryAllocateInfo allocInfo{ };
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = memory.memoryTypeIndex;

	alloc_t<Allocator<VkDeviceMemory>> alloc;
	if ( memory.Allocate( memRequirements.alignment, memRequirements.size, alloc ) ) {
		vkBindImageMemory( context.device, vk_image, memory.GetMemoryResource(), alloc.GetOffset() );
	} else {
		throw std::runtime_error( "Buffer could not be allocated!" );
	}

	vk_view = vk_CreateImageView( vk_image, info );
}


void GpuImage::Destroy()
{
	assert( context.device != VK_NULL_HANDLE );
	if ( context.device != VK_NULL_HANDLE )
	{
		if( vk_view != VK_NULL_HANDLE ) {
			vkDestroyImageView( context.device, vk_view, nullptr );
			vk_view = VK_NULL_HANDLE;
		}
		if ( vk_image != VK_NULL_HANDLE ) {
			vkDestroyImage( context.device, vk_image, nullptr );
			vk_image = VK_NULL_HANDLE;
		}
		allocation.Free();
	}
}