#include <algorithm>
#include <iterator>
#include <map>
#include "../render_core/renderer.h"
#include "../render_state/rhi.h"
#include "../render_state/cmdContext.h"
#include "../render_binding/bufferObjects.h"
#include "../globals/assetDefs.h"

void Renderer::UpdateTextureData( CommandContext* cmdCommand )
{
	const uint32_t textureCount = static_cast<uint32_t>( updateTextures.size() );
	if( textureCount == 0 ) {
		return;
	}

	for ( auto it = updateTextures.begin(); it != updateTextures.end(); ++it )
	{
		Asset<Image>* imageAsset = TextureLib().Find( *it );
		Image& image = imageAsset->Get();

		Transition( cmdCommand, image, GPU_IMAGE_NONE, GPU_IMAGE_TRANSFER_DST );

		imageSubResourceView_t subView {};
		subView.baseMip = 0;
		subView.mipLevels = 1;
		subView.baseArray = 0;
		subView.arrayCount = 1;

		UploadImageData( cmdCommand, image, subView, textureStagingBuffer );
	
		Transition( cmdCommand, image, GPU_IMAGE_TRANSFER_DST, GPU_IMAGE_READ );
	
		imageAsset->CompleteUpload();
	}

	updateTextures.clear();
}


void Renderer::UploadTextures( CommandContext* cmdCommand )
{
	const uint32_t textureCount = static_cast<uint32_t>( uploadTextures.size() );
	if ( textureCount == 0 ) {
		return;
	}

	// 1. Upload Data
	for ( auto it = uploadTextures.begin(); it != uploadTextures.end(); ++it )
	{
		Asset<Image>* textureAsset = TextureLib().Find( *it );
		if( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Image& texture = textureAsset->Get();

		// TODO: Allow for recreation for read-only textures
		if ( texture.gpuImage != nullptr )
		{
			if ( HasFlags( texture.gpuImage->GetFlags(), GPU_IMAGE_WRITE ) )
			{		
				continue;
			}
			else
			{
				std::cout << "GPU Image already exists!" << std::endl;
				continue;
			}
		}

		if ( texture.cpuImage == nullptr )
		{
			continue;
		}

		gpuImageStateFlags_t flags = ( GPU_IMAGE_READ | GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST );

		texture.gpuImage = 
			new GpuImage( textureAsset->GetName().c_str(), texture.info, flags, renderContext.localMemory, resourceLifeTime_t::REBOOT );

		Transition( cmdCommand, texture, GPU_IMAGE_NONE, GPU_IMAGE_TRANSFER_DST );

		imageSubResourceView_t subView{};
		subView.baseMip = 0;
		subView.mipLevels = texture.generateMips ? 1 : texture.info.mipLevels;
		subView.baseArray = 0;
		subView.arrayCount = texture.info.layers;

		UploadImageData( cmdCommand, texture, subView, textureStagingBuffer );
		
		assert( imageFreeSlot < MaxImageDescriptors );
		texture.gpuImage->SetId( imageFreeSlot );
		++imageFreeSlot;
	}

	// 2. Generate MIPS
	for ( auto it = uploadTextures.begin(); it != uploadTextures.end(); ++it )
	{
		Asset<Image>* textureAsset = TextureLib().Find( *it );
		if ( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Image& texture = textureAsset->Get();

		if ( texture.gpuImage == nullptr )
		{
			continue;
		}
		if ( HasFlags( texture.gpuImage->GetFlags(), GPU_IMAGE_WRITE ) )
		{
			continue;
		}
		if( texture.generateMips == false )
		{
			Transition( cmdCommand, texture, GPU_IMAGE_TRANSFER_DST, GPU_IMAGE_READ );
			continue;
		}
		GenerateMipmaps( cmdCommand, texture );
	}

	// 3. Add to resource type lists
	{
		// Find first cubemap. FIXME: Hacky, just done so there aren't nulls in the list
		Image* firstCube = nullptr;
		for ( auto it = uploadTextures.begin(); it != uploadTextures.end(); ++it )
		{
			Asset<Image>* textureAsset = TextureLib().Find( *it );
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
			Asset<Image>* textureAsset = TextureLib().Find( i );
			if ( textureAsset->IsLoaded() == false ) {
				continue;
			}

			Image& texture = textureAsset->Get();

			if ( texture.gpuImage == nullptr )
			{
				continue;
			}

			const int uploadId = texture.gpuImage->GetId();

			switch ( texture.info.type )
			{
				case IMAGE_TYPE_2D:
					resources.gpuImages2D.BindIndex( uploadId, &texture );
					resources.gpuImagesCube.BindIndex( uploadId, firstCube );
					break;
				case IMAGE_TYPE_CUBE:
					resources.gpuImages2D.BindIndex( uploadId, &TextureLib().GetDefault()->Get() );
					resources.gpuImagesCube.BindIndex( uploadId, &texture );
					break;
			}
		}

		// Fill defaults
		for ( uint32_t i = imageFreeSlot; i < MaxImageDescriptors; ++i )
		{
			resources.gpuImages2D.BindIndex( i, &TextureLib().GetDefault()->Get() );
			resources.gpuImagesCube.BindIndex( i, firstCube );
		}
	}

	uploadTextures.clear();
}

void Renderer::UpdateGpuMaterials()
{
	for ( auto it = uploadMaterials.begin(); it != uploadMaterials.end(); ++it )
	{
		Asset<Material>* matAsset = MaterialLib().Find( *it );
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
					const Asset<Image>* imageAsset = TextureLib().Find( imageId );
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

		const materialParms_t& parms = m.GetParms();

		materialObject.Kd = vec3f( parms.Kd.r, parms.Kd.g, parms.Kd.b );
		materialObject.Ks = vec3f( parms.Ks.r, parms.Ks.g, parms.Ks.b );
		materialObject.Ka = vec3f( parms.Ka.r, parms.Ka.g, parms.Ka.b );
		materialObject.Ke = vec3f( parms.Ke.r, parms.Ke.g, parms.Ke.b );
		materialObject.Tf = vec3f( parms.Tf.r, parms.Tf.g, parms.Tf.b );
		materialObject.Tr = parms.Tr;
		materialObject.Ni = parms.Ni;
		materialObject.Ns = parms.Ns;
		materialObject.illum = parms.illum;
		materialObject.roughness = parms.roughness;
		materialObject.metalness = parms.metalness;
		materialObject.sheen = parms.sheen;
		materialObject.clearcoatThickness = parms.clearcoatThickness;
		materialObject.clearcoatRoughness = parms.clearcoatRoughness;
		materialObject.anisotropy = parms.anisotropy;
		materialObject.anisotropyRotation = parms.anisotropyRotation;

		materialObject.textured = m.IsTextured();

		m.CopyExtraData( materialObject.extra, m.GetExtraDataByteCount() );
	}
	uploadMaterials.clear();
}

void Renderer::CopyGpuBuffer( CommandContext* cmdCommand, GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion )
{
	VkCommandBuffer commandBuffer = cmdCommand->CommandBuffer();
	vkCmdCopyBuffer( commandBuffer, srcBuffer.GetVkObject(), dstBuffer.GetVkObject(), 1, &copyRegion );

	dstBuffer.Allocate( copyRegion.size );
}

void Renderer::UploadModelsToGPU( CommandContext* cmdCommand )
{
	const uint32_t modelCount = ModelLib().Count();

	for ( uint32_t m = 0; m < modelCount; ++m )
	{
		Asset<Model>* modelAsset = ModelLib().Find( m );
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

				CopyGpuBuffer( cmdCommand, geometry.stagingBuffer, geometry.vb, vbCopyRegion );

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
				CopyGpuBuffer( cmdCommand, geometry.stagingBuffer, geometry.ib, ibCopyRegion );

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
