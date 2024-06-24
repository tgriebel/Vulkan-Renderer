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
#include "../render_state/cmdContext.h"
#include "../render_binding/bufferObjects.h"
#include <gfxcore/scene/assetManager.h>

extern AssetManager g_assets;

void Renderer::UpdateTextureData()
{
	const uint32_t textureCount = static_cast<uint32_t>( updateTextures.size() );
	if( textureCount == 0 ) {
		return;
	}

	for ( auto it = updateTextures.begin(); it != updateTextures.end(); ++it )
	{
		Asset<Image>* imageAsset = g_assets.textureLib.Find( *it );
		Image& image = imageAsset->Get();

		const uint64_t currentOffset = textureStagingBuffer.GetSize();
		textureStagingBuffer.CopyData( image.cpuImage->Ptr(), image.cpuImage->GetByteCount() );

		Transition( &uploadContext, image, GPU_IMAGE_NONE, GPU_IMAGE_TRANSFER_DST );

		CopyBufferToImage( &uploadContext, image, textureStagingBuffer, currentOffset );
	
		Transition( &uploadContext, image, GPU_IMAGE_TRANSFER_DST, GPU_IMAGE_READ );
	
		imageAsset->CompleteUpload();
	}

	updateTextures.clear();
}


void Renderer::UploadTextures()
{
	const uint32_t textureCount = static_cast<uint32_t>( uploadTextures.size() );
	if ( textureCount == 0 ) {
		return;
	}

	// 1. Upload Data
	for ( auto it = uploadTextures.begin(); it != uploadTextures.end(); ++it )
	{
		Asset<Image>* textureAsset = g_assets.textureLib.Find( *it );
		if( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Image& texture = textureAsset->Get();

		gpuImageStateFlags_t flags = ( GPU_IMAGE_READ | GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST );

		CreateImage( textureAsset->GetName().c_str(), texture.info, flags, renderContext.localMemory, texture );

		Transition( &uploadContext, texture, GPU_IMAGE_NONE, GPU_IMAGE_TRANSFER_DST );

		//const uint64_t alignmentOffset = textureStagingBuffer.GetAlignedSize( textureStagingBuffer.GetSize(), texture.gpuImage->GetAlignment() );
		//textureStagingBuffer.SetPos( alignmentOffset );

		const uint64_t currentOffset = textureStagingBuffer.GetSize();
		textureStagingBuffer.CopyData( texture.cpuImage->Ptr(), texture.cpuImage->GetByteCount() );

		CopyBufferToImage( &uploadContext, texture, textureStagingBuffer, currentOffset );
		
		assert( imageFreeSlot < MaxImageDescriptors );
		texture.gpuImage->SetId( imageFreeSlot );
		++imageFreeSlot;
	}

	// 2. Generate MIPS
	for ( auto it = uploadTextures.begin(); it != uploadTextures.end(); ++it )
	{
		Asset<Image>* textureAsset = g_assets.textureLib.Find( *it );
		if ( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Image& texture = textureAsset->Get();
		GenerateMipmaps( &uploadContext, texture );
	}

	// 3. Add to resource type lists
	{
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
			const int uploadId = texture.gpuImage->GetId();

			switch ( texture.info.type )
			{
				case IMAGE_TYPE_2D:
					resources.gpuImages2D[ uploadId ] = &texture;
					resources.gpuImagesCube[ uploadId ] = firstCube;
					break;
				case IMAGE_TYPE_CUBE:
					resources.gpuImages2D[ uploadId ] = &g_assets.textureLib.GetDefault()->Get();
					resources.gpuImagesCube[ uploadId ] = &texture;
					break;
			}
		}

		// Fill defaults
		for ( uint32_t i = imageFreeSlot; i < MaxImageDescriptors; ++i )
		{
			resources.gpuImages2D[ i ] = &g_assets.textureLib.GetDefault()->Get();
			resources.gpuImagesCube[ i ] = firstCube;
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
			m.uploadId = materialBuffer.Count();
			materialBuffer.Append( materialBufferObject_t() );
		}
		matAsset->CompleteUpload();

		assert( m.uploadId < MaxMaterials );
		materialBufferObject_t& materialObject = materialBuffer[ m.uploadId ];

		if( m.usage == MATERIAL_USAGE_CODE )
		{
			for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t ) {
				materialObject.textures[ t ] = (int)m.GetTexture( t ).Get();
			}
		}
		else
		{
			for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t )
			{
				const hdl_t handle = m.GetTexture( t );
				if ( handle.IsValid() )
				{
					const hdl_t imageId = m.GetTexture( t );
					const Asset<Image>* imageAsset = g_assets.textureLib.Find( imageId );
					const Image& image = imageAsset->Get();
					assert( image.gpuImage != nullptr );

					const int uploadId = image.gpuImage->GetId();
					assert( uploadId >= 0 );
					materialObject.textures[ t ] = uploadId;
				} else {
					materialObject.textures[ t ] = -1;
				}
			}
		}
		materialObject.Kd = vec3f( m.Kd().r, m.Kd().g, m.Kd().b );
		materialObject.Ks = vec3f( m.Ks().r, m.Ks().g, m.Ks().b );
		materialObject.Ka = vec3f( m.Ka().r, m.Ka().g, m.Ka().b );
		materialObject.Ke = vec3f( m.Ke().r, m.Ke().g, m.Ke().b );
		materialObject.Tf = vec3f( m.Tf().r, m.Tf().g, m.Tf().b );
		materialObject.Tr = m.Tr();
		materialObject.Ni = m.Ni();
		materialObject.Ns = m.Ns();
		materialObject.illum = m.Illum();
		materialObject.textured = m.IsTextured();
	}
	uploadMaterials.clear();
}

void Renderer::CopyGpuBuffer( GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion )
{
	VkCommandBuffer commandBuffer = uploadContext.CommandBuffer();
	vkCmdCopyBuffer( commandBuffer, srcBuffer.GetVkObject(), dstBuffer.GetVkObject(), 1, &copyRegion );

	dstBuffer.Allocate( copyRegion.size );
}

void Renderer::UploadModelsToGPU()
{
	const uint32_t modelCount = g_assets.modelLib.Count();

	for ( uint32_t m = 0; m < modelCount; ++m )
	{
		Asset<Model>* modelAsset = g_assets.modelLib.Find( m );
		Model& model = modelAsset->Get();
		if ( modelAsset->IsUploaded() ) {
			continue;
		}
		if( model.uploadId == -1 ) {
			continue;
		}

		for ( uint32_t s = 0; s < model.surfCount; ++s )
		{
			Surface& surf = model.surfs[ s ];	
			surfaceUpload_t& upload = geometry.surfUploads[ model.uploadId + s ];

			upload.vertexOffset = geometry.vbBufElements;
			upload.firstIndex = geometry.ibBufElements;

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
				VkDeviceSize vbCopySize = sizeof( vertexStream[ 0 ] ) * vertexCount;

				VkBufferCopy vbCopyRegion{ };
				vbCopyRegion.size = vbCopySize;
				vbCopyRegion.srcOffset = geometry.stagingBuffer.GetSize();
				vbCopyRegion.dstOffset = geometry.vb.GetSize();
			
				geometry.stagingBuffer.CopyData( vertexStream.data(), static_cast<size_t>( vbCopySize ) );

				CopyGpuBuffer( geometry.stagingBuffer, geometry.vb, vbCopyRegion );

				upload.vertexCount = vertexCount;
				geometry.vbBufElements += vertexCount;

				assert( geometry.vbBufElements < MaxVertices );
			}

			// Upload Index Buffer
			{
				// IB Copy
				VkDeviceSize ibCopySize = sizeof( surf.indices[ 0 ] ) * surf.indices.size();

				VkBufferCopy ibCopyRegion{ };
				ibCopyRegion.size = ibCopySize;
				ibCopyRegion.srcOffset = geometry.stagingBuffer.GetSize();
				ibCopyRegion.dstOffset = geometry.ib.GetSize();
				CopyGpuBuffer( geometry.stagingBuffer, geometry.ib, ibCopyRegion );

				geometry.stagingBuffer.CopyData( surf.indices.data(), static_cast<size_t>( ibCopySize ) );

				const uint32_t indexCount = static_cast<uint32_t>( surf.indices.size() );
				upload.indexCount = indexCount;
				geometry.ibBufElements += indexCount;

				assert( geometry.ibBufElements < MaxIndices );
			}
		}
		modelAsset->CompleteUpload();
	}
}