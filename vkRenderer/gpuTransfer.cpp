#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"
#include <scene/assetManager.h>

extern AssetManager gAssets;

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
	const uint32_t textureCount = static_cast<uint32_t>( pendingTextures.size() );
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
	for ( auto it = pendingTextures.begin(); it != pendingTextures.end(); ++it )
	{
		Asset<Texture>* textureAsset = gAssets.textureLib.Find( *it );
		if( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Texture& texture = textureAsset->Get();

		// VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
		VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT;
		CreateImage( texture.info, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_SAMPLE_COUNT_1_BIT, flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.gpuImage, localMemory );

		TransitionImageLayout( texture.gpuImage.vk_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.info );

		const VkDeviceSize currentOffset = stagingBuffer.GetSize();
		stagingBuffer.CopyData( texture.bytes, texture.sizeBytes );

		const uint32_t layers = texture.info.layers;
		CopyBufferToImage( commandBuffer, stagingBuffer.GetVkObject(), currentOffset, texture.gpuImage.vk_image, static_cast<uint32_t>( texture.info.width ), static_cast<uint32_t>( texture.info.height ), layers );
		texture.uploadId = imageFreeSlot++;
		gpuImages[texture.uploadId];
	}
	EndSingleTimeCommands( commandBuffer );

	for ( auto it = pendingTextures.begin(); it != pendingTextures.end(); ++it )
	{
		Asset<Texture>* textureAsset = gAssets.textureLib.Find( *it );
		if ( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Texture& texture = textureAsset->Get();
		GenerateMipmaps( texture.gpuImage.vk_image, VK_FORMAT_R8G8B8A8_SRGB, texture.info );
	}

	for ( auto it = pendingTextures.begin(); it != pendingTextures.end(); ++it )
	{
		Asset<Texture>* textureAsset = gAssets.textureLib.Find( *it );
		if ( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Texture& texture = textureAsset->Get();

		VkImageViewType type;
		switch ( texture.info.type ) {
		default:
		case TEXTURE_TYPE_2D:	type = VK_IMAGE_VIEW_TYPE_2D;		break;
		case TEXTURE_TYPE_CUBE:	type = VK_IMAGE_VIEW_TYPE_CUBE;		break;
		}
		texture.gpuImage.vk_view = CreateImageView( texture.gpuImage.vk_image, VK_FORMAT_R8G8B8A8_SRGB, type, VK_IMAGE_ASPECT_COLOR_BIT, texture.info.mipLevels );
	}

	pendingTextures.clear();
}

void Renderer::UpdateGpuMaterials()
{
	for ( auto it = pendingMaterials.begin(); it != pendingMaterials.end(); ++it )
	{
		Material& m = gAssets.materialLib.Find( *it )->Get();
		if( m.uploadId < 0 ) {
			m.uploadId = materialFreeSlot++;
		}

		materialBufferObject_t& ubo = materialBuffer[ m.uploadId ];
		for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t ) {
			const hdl_t handle = m.GetTexture( t );
			if ( handle.IsValid() ) {
				const int uploadId = gAssets.textureLib.Find( m.GetTexture( t ) )->Get().uploadId;
				assert( uploadId >= 0 );
				ubo.textures[ t ] = uploadId;
			}
			else {
				ubo.textures[ t ] = 0;
			}
		}
		ubo.Kd = vec4f( m.Kd.r, m.Kd.g, m.Kd.b, 1.0f );
		ubo.Ks = vec4f( m.Ks.r, m.Ks.g, m.Ks.b, 1.0f );
		ubo.Ka = vec4f( m.Ka.r, m.Ka.g, m.Ka.b, 1.0f );
		ubo.Ke = vec4f( m.Ke.r, m.Ke.g, m.Ke.b, 1.0f );
		ubo.Tf = vec4f( m.Tf.r, m.Tf.g, m.Tf.b, 1.0f );
		ubo.Tr = m.Tr;
		ubo.Ni = m.Ni;
		ubo.Ns = m.Ns;
		ubo.illum = m.illum;
		ubo.d = m.d;
	}
	pendingMaterials.clear();
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
	const uint32_t modelCount = gAssets.modelLib.Count();

	for ( uint32_t m = 0; m < modelCount; ++m )
	{
		Model& model = gAssets.modelLib.Find( m )->Get();
		if ( model.uploaded ) {
			continue;
		}

		for ( uint32_t s = 0; s < model.surfCount; ++s )
		{
			Surface& surf = model.surfs[ s ];
			surfaceUpload_t& upload = model.upload[ s ];

			upload.vertexOffset = vbBufElements;
			upload.firstIndex = ibBufElements;

			// Upload Vertex Buffer
			{
				// Create vertex stream data
				std::vector<vsInput_t> vertexStream;
				const uint32_t vertexCount = static_cast<uint32_t>( surf.vertices.size() );
				vertexStream.resize( vertexCount );
				for ( uint32_t vIx = 0; vIx < vertexCount; ++vIx )
				{
					vertexStream[vIx].pos = Trunc<4,1>( surf.vertices[vIx].pos );
					vertexStream[vIx].color = ColorToVector( surf.vertices[vIx].color );
					vertexStream[vIx].normal = surf.vertices[vIx].normal;
					vertexStream[vIx].tangent = surf.vertices[vIx].tangent;
					vertexStream[vIx].bitangent = surf.vertices[vIx].bitangent;
					vertexStream[vIx].texCoord[0] = surf.vertices[vIx].uv[0];
					vertexStream[vIx].texCoord[1] = surf.vertices[vIx].uv[1];
					vertexStream[vIx].texCoord[2] = surf.vertices[vIx].uv2[0];
					vertexStream[vIx].texCoord[3] = surf.vertices[vIx].uv2[1];
				}

				// Copy stream to staging buffer
				VkDeviceSize vbCopySize = sizeof( vertexStream[0] ) * vertexCount;
				stagingBuffer.Reset();
				stagingBuffer.CopyData( vertexStream.data(), static_cast<size_t>( vbCopySize ) );

				VkBufferCopy vbCopyRegion{ };
				vbCopyRegion.size = vbCopySize;
				vbCopyRegion.srcOffset = 0;
				vbCopyRegion.dstOffset = vb.GetSize();
				CopyGpuBuffer( stagingBuffer, vb, vbCopyRegion );

				upload.vertexCount = vertexCount;
				vbBufElements += vertexCount;
			}

			// Upload Index Buffer
			{
				// IB Copy
				VkDeviceSize ibCopySize = sizeof( surf.indices[ 0 ] ) * surf.indices.size();
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
		model.uploaded = true;
	}
}