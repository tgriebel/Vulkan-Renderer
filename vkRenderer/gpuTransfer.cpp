#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"

void Renderer::CopyBufferToImage( VkCommandBuffer& commandBuffer, VkBuffer& buffer, const VkDeviceSize bufferOffset, VkImage& image, const uint32_t width, const uint32_t height, const uint32_t layers )
{
	VkBufferImageCopy region{ };
	memset( &region, 0, sizeof( region ) );
	region.bufferOffset = bufferOffset;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = layers;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(
		commandBuffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);
}

void Renderer::UploadTextures()
{
	const uint32_t textureCount = textureLib.Count();
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		texture_t* texture = textureLib.Find( i );
		// VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
		VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT;
		CreateImage( texture->info, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_SAMPLE_COUNT_1_BIT, flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image, localMemory );

		TransitionImageLayout( texture->image.vk_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->info );

		const VkDeviceSize currentOffset = stagingBuffer.GetSize();
		stagingBuffer.CopyData( texture->bytes, texture->sizeBytes );

		const uint32_t layers = texture->info.layers;
		CopyBufferToImage( commandBuffer, stagingBuffer.GetVkObject(), currentOffset, texture->image.vk_image, static_cast<uint32_t>( texture->info.width ), static_cast<uint32_t>( texture->info.height ), layers );
		texture->uploaded = true;
	}
	EndSingleTimeCommands( commandBuffer );

	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		texture_t* texture = textureLib.Find( i );
		GenerateMipmaps( texture->image.vk_image, VK_FORMAT_R8G8B8A8_SRGB, texture->info );
	}

	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		texture_t* texture = textureLib.Find( i );
		VkImageViewType type;
		switch ( texture->info.type ) {
		default:
		case TEXTURE_TYPE_2D:	type = VK_IMAGE_VIEW_TYPE_2D;		break;
		case TEXTURE_TYPE_CUBE:	type = VK_IMAGE_VIEW_TYPE_CUBE;		break;
		}
		texture->image.vk_view = CreateImageView( texture->image.vk_image, VK_FORMAT_R8G8B8A8_SRGB, type, VK_IMAGE_ASPECT_COLOR_BIT, texture->info.mipLevels );
	}
}

void Renderer::CopyGpuBuffer( GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion )
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
	vkCmdCopyBuffer( commandBuffer, srcBuffer.GetVkObject(), dstBuffer.GetVkObject(), 1, &copyRegion );
	EndSingleTimeCommands( commandBuffer );

	dstBuffer.Allocate( copyRegion.size );
}

void Renderer::UploadModelsToGPU()
{
	const VkDeviceSize vbSize = sizeof( VertexInput ) * MaxVertices;
	const VkDeviceSize ibSize = sizeof( uint32_t ) * MaxIndices;
	const uint32_t modelCount = modelLib.Count();
	CreateBuffer( vbSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vb, localMemory );
	CreateBuffer( ibSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ib, localMemory );

	static uint32_t vbBufElements = 0;
	static uint32_t ibBufElements = 0;
	for ( uint32_t m = 0; m < modelCount; ++m )
	{
		modelSource_t* model = modelLib.Find( m );
		for ( uint32_t s = 0; s < model->surfCount; ++s )
		{
			surface_t& surf = model->surfs[ s ];
			surfUpload_t& upload = model->upload[ s ];
			VkDeviceSize vbCopySize = sizeof( surf.vertices[ 0 ] ) * surf.vertices.size();
			VkDeviceSize ibCopySize = sizeof( surf.indices[ 0 ] ) * surf.indices.size();

			upload.vertexOffset = vbBufElements;
			upload.firstIndex = ibBufElements;

			// VB Copy
			stagingBuffer.Reset();
			stagingBuffer.CopyData( surf.vertices.data(), static_cast<size_t>( vbCopySize ) );

			VkBufferCopy vbCopyRegion{ };
			vbCopyRegion.size = vbCopySize;
			vbCopyRegion.srcOffset = 0;
			vbCopyRegion.dstOffset = vb.GetSize();
			CopyGpuBuffer( stagingBuffer, vb, vbCopyRegion );

			const uint32_t vertexCount = static_cast<uint32_t>( surf.vertices.size() );
			upload.vertexCount = vertexCount;
			vbBufElements += vertexCount;

			// IB Copy
			stagingBuffer.Reset();
			stagingBuffer.CopyData( surf.indices.data(), static_cast<size_t>( ibCopySize ) );

			VkBufferCopy ibCopyRegion{ };
			ibCopyRegion.size = ibCopySize;
			ibCopyRegion.srcOffset = 0;
			ibCopyRegion.dstOffset = ib.GetSize();
			CopyGpuBuffer( stagingBuffer, ib, ibCopyRegion );

			const uint32_t indexCount = static_cast<uint32_t>( surf.indices.size() );
			upload.indexCount = indexCount;
			ibBufElements += indexCount;
		}
	}
}