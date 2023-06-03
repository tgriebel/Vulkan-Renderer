/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include <algorithm>
#include <iterator>
#include <map>
#include "../render_core/renderer.h"
#include "../render_state/rhi.h"
#include <gfxcore/scene/assetManager.h>

extern AssetManager g_assets;

void Renderer::CopyBufferToImage( VkCommandBuffer& commandBuffer, VkBuffer& buffer, const VkDeviceSize bufferOffset, Image& texture )
{
	const uint32_t layers = texture.info.layers;

	VkBufferImageCopy region{ };
	memset( &region, 0, sizeof( region ) );
	region.bufferOffset = bufferOffset;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = layers;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		texture.info.width,
		texture.info.height,
		1
	};

	vkCmdCopyBufferToImage(
		commandBuffer,
		buffer,
		texture.gpuImage->GetVkImage(),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);
}


void Renderer::UpdateTextures()
{
	const uint32_t textureCount = static_cast<uint32_t>( updateTextures.size() );
	if( textureCount == 0 ) {
		return;
	}
	stagingBuffer.SetPos();
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
	for ( auto it = updateTextures.begin(); it != updateTextures.end(); ++it )
	{
		Asset<Image>* textureAsset = g_assets.textureLib.Find( *it );
		Image& texture = textureAsset->Get();

		const VkDeviceSize currentOffset = stagingBuffer.GetSize();
		stagingBuffer.CopyData( texture.bytes, texture.sizeBytes );

		TransitionImageLayout( commandBuffer, texture.gpuImage->GetVkImage(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.info );

		CopyBufferToImage( commandBuffer, stagingBuffer.VkObject(), currentOffset, texture );
	
		TransitionImageLayout( commandBuffer, texture.gpuImage->GetVkImage(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.info );
	}
	EndSingleTimeCommands( commandBuffer );
}


void Renderer::UploadTextures()
{
	const uint32_t textureCount = static_cast<uint32_t>( uploadTextures.size() );
	if ( textureCount == 0 ) {
		return;
	}

	// 1. Upload Data
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
	for ( auto it = uploadTextures.begin(); it != uploadTextures.end(); ++it )
	{
		Asset<Image>* textureAsset = g_assets.textureLib.Find( *it );
		if( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Image& texture = textureAsset->Get();

		gpuImageStateFlags_t flags = ( GPU_IMAGE_READ | GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST );

		texture.gpuImage = CreateGpuImage( texture.info, flags, localMemory );

		TransitionImageLayout( commandBuffer, texture.gpuImage->GetVkImage(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.info );

		const VkDeviceSize currentOffset = stagingBuffer.GetSize();
		stagingBuffer.CopyData( texture.bytes, texture.sizeBytes );		

		CopyBufferToImage( commandBuffer, stagingBuffer.VkObject(), currentOffset, texture );
		
		assert( imageFreeSlot < MaxImageDescriptors );
		texture.uploadId = imageFreeSlot++;
	}

	// 2. Generate MIPS
	for ( auto it = uploadTextures.begin(); it != uploadTextures.end(); ++it )
	{
		Asset<Image>* textureAsset = g_assets.textureLib.Find( *it );
		if ( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Image& texture = textureAsset->Get();
		GenerateMipmaps( commandBuffer, texture.gpuImage->GetVkImage(), VK_FORMAT_R8G8B8A8_SRGB, texture.info );
	}
	EndSingleTimeCommands( commandBuffer );

	// 3. Create Views
	for ( auto it = uploadTextures.begin(); it != uploadTextures.end(); ++it )
	{
		Asset<Image>* textureAsset = g_assets.textureLib.Find( *it );
		if ( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Image& texture = textureAsset->Get();
		vk_CreateImageView( texture.gpuImage, texture.info );
	}

	// 4. Add to resource type lists
	{
		gpuImages2D.Resize( MaxImageDescriptors );
		gpuImagesCube.Resize( MaxImageDescriptors );

		// Find first cubemap. FIXME: Hacky, just done so there aren't nulls in the list
		Image* firstCube = nullptr;
		for ( auto it = uploadTextures.begin(); it != uploadTextures.end(); ++it )
		{
			Asset<Image>* textureAsset = g_assets.textureLib.Find( *it );
			if ( textureAsset->IsLoaded() == false ) {
				continue;
			}
			if ( textureAsset->Get().info.type == IMAGE_TYPE_CUBE )
			{
				firstCube = &textureAsset->Get();
				break;
			}
		}
		assert( firstCube != nullptr );

		// Fill assigned slots
		for ( uint32_t i = 0; i < imageFreeSlot; ++i )
		{
			Asset<Image>* textureAsset = g_assets.textureLib.Find( i );
			if ( textureAsset->IsLoaded() == false ) {
				continue;
			}
			Image& texture = textureAsset->Get();

			switch ( texture.info.type )
			{
				case IMAGE_TYPE_2D:
					gpuImages2D[ texture.uploadId ] = &texture;
					gpuImagesCube[ texture.uploadId ] = firstCube;
					break;
				case IMAGE_TYPE_CUBE:
					gpuImages2D[ texture.uploadId ] = &g_assets.textureLib.GetDefault()->Get();
					gpuImagesCube[ texture.uploadId ] = &texture;
					break;
			}
		}

		// Fill defaults
		for ( uint32_t i = imageFreeSlot; i < MaxImageDescriptors; ++i )
		{
			gpuImages2D[ i ] = &g_assets.textureLib.GetDefault()->Get();
			gpuImagesCube[ i ] = firstCube;
		}
	}

	uploadTextures.clear();
}

void Renderer::UpdateGpuMaterials()
{
	for ( auto it = uploadMaterials.begin(); it != uploadMaterials.end(); ++it )
	{
		Asset<Material>* matAsset = g_assets.materialLib.Find( *it );
		Material& m = matAsset->Get();
		if( m.uploadId < 0 ) {
			m.uploadId = materialFreeSlot++;
		}
		m.dirty = false;

		assert( m.uploadId < MaxMaterials );
		materialBufferObject_t& ubo = materialBuffer[ m.uploadId ];

		if( m.usage == MATERIAL_USAGE_CODE )
		{
			for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t )
			{
				ubo.textures[ t ] = (int)m.GetTexture( t ).Get();
			}
		}
		else
		{
			for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t )
			{
				const hdl_t handle = m.GetTexture( t );
				if ( handle.IsValid() ) {
					const int uploadId = g_assets.textureLib.Find( m.GetTexture( t ) )->Get().uploadId;
					assert( uploadId >= 0 );
					ubo.textures[ t ] = uploadId;
				}
				else {
					ubo.textures[ t ] = -1;
				}
			}
		}
		ubo.Kd = vec3f( m.Kd().r, m.Kd().g, m.Kd().b );
		ubo.Ks = vec3f( m.Ks().r, m.Ks().g, m.Ks().b );
		ubo.Ka = vec3f( m.Ka().r, m.Ka().g, m.Ka().b );
		ubo.Ke = vec3f( m.Ke().r, m.Ke().g, m.Ke().b );
		ubo.Tf = vec3f( m.Tf().r, m.Tf().g, m.Tf().b );
		ubo.Tr = m.Tr();
		ubo.Ni = m.Ni();
		ubo.Ns = m.Ns();
		ubo.illum = m.Illum();
		ubo.textured = m.IsTextured();
	}
	uploadMaterials.clear();
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
	const uint32_t modelCount = g_assets.modelLib.Count();

	for ( uint32_t m = 0; m < modelCount; ++m )
	{
		Model& model = g_assets.modelLib.Find( m )->Get();
		if ( model.uploaded ) {
			continue;
		}

		model.upload.resize( model.surfCount );
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
				stagingBuffer.SetPos();
				stagingBuffer.CopyData( vertexStream.data(), static_cast<size_t>( vbCopySize ) );

				VkBufferCopy vbCopyRegion{ };
				vbCopyRegion.size = vbCopySize;
				vbCopyRegion.srcOffset = 0;
				vbCopyRegion.dstOffset = vb.GetSize();
				CopyGpuBuffer( stagingBuffer, vb, vbCopyRegion );

				upload.vertexCount = vertexCount;
				vbBufElements += vertexCount;

				assert( vbBufElements < MaxVertices );
			}

			// Upload Index Buffer
			{
				// IB Copy
				VkDeviceSize ibCopySize = sizeof( surf.indices[ 0 ] ) * surf.indices.size();
				stagingBuffer.SetPos();
				stagingBuffer.CopyData( surf.indices.data(), static_cast<size_t>( ibCopySize ) );

				VkBufferCopy ibCopyRegion{ };
				ibCopyRegion.size = ibCopySize;
				ibCopyRegion.srcOffset = 0;
				ibCopyRegion.dstOffset = ib.GetSize();
				CopyGpuBuffer( stagingBuffer, ib, ibCopyRegion );

				const uint32_t indexCount = static_cast<uint32_t>( surf.indices.size() );
				upload.indexCount = indexCount;
				ibBufElements += indexCount;

				assert( ibBufElements < MaxIndices );
			}
		}
		model.uploaded = true;
	}
}