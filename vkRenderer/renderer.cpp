#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"

static bool CompareSortKey( drawSurf_t& surf0, drawSurf_t& surf1 )
{
	if( surf0.materialId == surf1.materialId ) {
		return ( surf0.objectId < surf1.objectId );
	} else {
		return ( surf0.materialId < surf1.materialId );
	}
}

void Renderer::Commit( const Scene& scene )
{
	renderView.committedModelCnt = 0;
	const uint32_t entCount = static_cast<uint32_t>( scene.entities.size() );
	for ( uint32_t i = 0; i < entCount; ++i ) {
		CommitModel( renderView, *scene.entities[i], 0 );
	}
	MergeSurfaces( renderView );

	shadowView.committedModelCnt = 0;
	for ( uint32_t i = 0; i < entCount; ++i )
	{
		if ( ( scene.entities[i] )->HasRenderFlag( NO_SHADOWS ) ) {
			continue;
		}
		CommitModel( shadowView, *scene.entities[i], MaxModels );
	}
	MergeSurfaces( shadowView );

	renderView.viewMatrix = scene.camera.GetViewMatrix();
	renderView.projMatrix = scene.camera.GetPerspectiveMatrix();

	for ( int i = 0; i < MaxLights; ++i ) {
		renderView.lights[ i ] = scene.lights[ i ];
	}

	UpdateView();
}

void Renderer::MergeSurfaces( RenderView& view )
{
	view.mergedModelCnt = 0;
	std::unordered_map< uint32_t, uint32_t > uniqueSurfs;
	uniqueSurfs.reserve( view.committedModelCnt );
	for ( uint32_t i = 0; i < view.committedModelCnt; ++i ) {
		drawSurfInstance_t& instance = view.instances[ i ];
		auto it = uniqueSurfs.find( view.surfaces[ i ].hash );
		if ( it == uniqueSurfs.end() ) {
			const uint32_t surfId = view.mergedModelCnt;
			uniqueSurfs[ view.surfaces[ i ].hash ] = surfId;

			view.instanceCounts[ surfId ] = 1;
			view.merged[ surfId ] = view.surfaces[ i ];

			instance.id = 0;
			instance.surfId = surfId;

			++view.mergedModelCnt;
		}
		else {
			instance.id = view.instanceCounts[ it->second ];
			instance.surfId = it->second;
			view.instanceCounts[ it->second ]++;
		}
	}
	uint32_t totalCount = 0;
	for ( uint32_t i = 0; i < view.mergedModelCnt; ++i ) {
		view.merged[ i ].objectId += totalCount;
		totalCount += view.instanceCounts[ i ];
	}
}

void Renderer::CommitModel( RenderView& view, const Entity& ent, const uint32_t objectOffset )
{
	if ( ent.HasRenderFlag( HIDDEN ) ) {
		return;
	}

	assert( DRAWPASS_COUNT <= Material::MaxMaterialShaders );

	Model* source = scene.modelLib.Find( ent.modelHdl );
	for ( uint32_t i = 0; i < source->surfCount; ++i ) {
		drawSurfInstance_t& instance = view.instances[ view.committedModelCnt ];
		drawSurf_t& surf = view.surfaces[ view.committedModelCnt ];
		surfaceUpload_t& upload = source->upload[ i ];

		const Material* material = scene.materialLib.Find( ent.materialHdl.IsValid() ? ent.materialHdl : source->surfs[ i ].materialHdl );
		assert( material->uploadId >= 0 );

		instance.modelMatrix = ent.GetMatrix();
		instance.surfId = 0;
		instance.id = 0;
		surf.vertexCount = upload.vertexCount;
		surf.vertexOffset = upload.vertexOffset;
		surf.firstIndex = upload.firstIndex;
		surf.indicesCnt = upload.indexCount;
		surf.materialId = material->uploadId;
		surf.objectId = objectOffset;
		surf.flags = ent.GetRenderFlags();
		surf.stencilBit = ent.outline ? 0x01 : 0;
		surf.hash = Hash( surf );

		for ( int pass = 0; pass < DRAWPASS_COUNT; ++pass ) {
			if ( material->shaders[ pass ].IsValid() ) {
				GpuProgram* prog = scene.gpuPrograms.Find( material->shaders[ pass ] );
				if ( prog == nullptr ) {
					continue;
				}
				surf.pipelineObject[ pass ] = prog->pipeline;
			}
			else {
				surf.pipelineObject[ pass ] = INVALID_HDL;
			}
		}

		++view.committedModelCnt;
	}
}

gfxStateBits_t Renderer::GetStateBitsForDrawPass( const drawPass_t pass )
{
	uint64_t stateBits = 0;
	if ( pass == DRAWPASS_SKYBOX )
	{
		stateBits |= GFX_STATE_DEPTH_TEST;
		stateBits |= GFX_STATE_DEPTH_WRITE;
		stateBits |= GFX_STATE_CULL_MODE_BACK;
		stateBits |= GFX_STATE_MSAA_ENABLE;
	}
	else if ( pass == DRAWPASS_SHADOW )
	{
		stateBits |= GFX_STATE_DEPTH_TEST;
		stateBits |= GFX_STATE_DEPTH_WRITE;
		//	stateBits |= GFX_STATE_COLOR_MASK;
		stateBits |= GFX_STATE_DEPTH_OP_0; // FIXME: 3 bits, just set one for now
	//	stateBits |= GFX_STATE_CULL_MODE_FRONT;
	}
	else if ( pass == DRAWPASS_DEPTH )
	{
		stateBits |= GFX_STATE_DEPTH_TEST;
		stateBits |= GFX_STATE_DEPTH_WRITE;
		stateBits |= GFX_STATE_COLOR_MASK;
		stateBits |= GFX_STATE_MSAA_ENABLE;
		stateBits |= GFX_STATE_CULL_MODE_BACK;
		stateBits |= GFX_STATE_STENCIL_ENABLE;
	}
	else if ( pass == DRAWPASS_TERRAIN )
	{
		stateBits |= GFX_STATE_DEPTH_TEST;
		stateBits |= GFX_STATE_DEPTH_WRITE;
		stateBits |= GFX_STATE_CULL_MODE_BACK;
		stateBits |= GFX_STATE_MSAA_ENABLE;
	}
	else if ( pass == DRAWPASS_OPAQUE )
	{
		stateBits |= GFX_STATE_DEPTH_TEST;
		stateBits |= GFX_STATE_DEPTH_WRITE;
		stateBits |= GFX_STATE_MSAA_ENABLE;
		stateBits |= GFX_STATE_CULL_MODE_BACK;
	}
	else if ( pass == DRAWPASS_TRANS )
	{
		stateBits |= GFX_STATE_DEPTH_TEST;
		stateBits |= GFX_STATE_CULL_MODE_BACK;
		stateBits |= GFX_STATE_MSAA_ENABLE;
		stateBits |= GFX_STATE_BLEND_ENABLE;
	}
	else if ( pass == DRAWPASS_DEBUG_WIREFRAME )
	{
		stateBits |= GFX_STATE_WIREFRAME_ENABLE;
		stateBits |= GFX_STATE_MSAA_ENABLE;
	}
	else if ( pass == DRAWPASS_DEBUG_SOLID )
	{
		stateBits |= GFX_STATE_CULL_MODE_BACK;
		stateBits |= GFX_STATE_BLEND_ENABLE;
		stateBits |= GFX_STATE_MSAA_ENABLE;
	}
	else if ( pass == DRAWPASS_POST_2D )
	{
		stateBits |= GFX_STATE_BLEND_ENABLE;
	}
	else
	{
		stateBits |= GFX_STATE_DEPTH_TEST;
		stateBits |= GFX_STATE_DEPTH_WRITE;
		stateBits |= GFX_STATE_CULL_MODE_BACK;
		stateBits |= GFX_STATE_MSAA_ENABLE;
	}

	return static_cast<gfxStateBits_t>( stateBits );
}

viewport_t Renderer::GetDrawPassViewport( const drawPass_t pass )
{
	if ( pass == DRAWPASS_SHADOW ) {
		return shadowView.viewport;
	}
	else {
		return renderView.viewport;
	}
}

void Renderer::UpdateFrameDescSet( const int currentImage )
{
	const int i = currentImage;

	//////////////////////////////////////////////////////
	//													//
	// Scene Descriptor Sets							//
	//													//
	//////////////////////////////////////////////////////

	std::vector<VkDescriptorBufferInfo> globalConstantsInfo;
	globalConstantsInfo.reserve( 1 );
	{
		VkDescriptorBufferInfo info{ };
		info.buffer = frameState[ i ].globalConstants.GetVkObject();
		info.offset = 0;
		info.range = sizeof( globalUboConstants_t );
		globalConstantsInfo.push_back( info );
	}

	std::vector<VkDescriptorBufferInfo> bufferInfo;
	bufferInfo.reserve( MaxSurfaces );
	for ( int j = 0; j < MaxSurfaces; ++j )
	{
		VkDescriptorBufferInfo info{ };
		info.buffer = frameState[ i ].surfParms.GetVkObject();
		info.offset = j * sizeof( uniformBufferObject_t );
		info.range = sizeof( uniformBufferObject_t );
		bufferInfo.push_back( info );
	}

	std::vector<VkDescriptorImageInfo> image2DInfo;
	std::vector<VkDescriptorImageInfo> imageCubeInfo;
	image2DInfo.reserve( MaxImageDescriptors );
	imageCubeInfo.reserve( MaxImageDescriptors );
	const uint32_t textureCount = scene.textureLib.Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		texture_t* texture = scene.textureLib.Find( i );
		VkImageView& imageView = texture->image.vk_view;
		VkDescriptorImageInfo info{ };
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = imageView;
		info.sampler = vk_bilinearSampler;

		if ( texture->info.type == TEXTURE_TYPE_CUBE ) {
			imageCubeInfo.push_back( info );

			VkDescriptorImageInfo info2d{ };
			info2d.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info2d.imageView = scene.textureLib.GetDefault()->image.vk_view;
			info2d.sampler = vk_bilinearSampler;
			image2DInfo.push_back( info2d );
		}
		else {
			image2DInfo.push_back( info );
		}
	}
	// Defaults
	{
		const texture_t* default2DTexture = scene.textureLib.GetDefault();
		for ( size_t j = image2DInfo.size(); j < MaxImageDescriptors; ++j )
		{
			const VkImageView& imageView = default2DTexture->image.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			image2DInfo.push_back( info );
		}
		for ( size_t j = imageCubeInfo.size(); j < MaxImageDescriptors; ++j )
		{
			const VkImageView& imageView = imageCubeInfo[ 0 ].imageView;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			imageCubeInfo.push_back( info );
		}
	}

	std::vector<VkDescriptorBufferInfo> materialBufferInfo;
	materialBufferInfo.reserve( MaxMaterialDescriptors );
	for ( int j = 0; j < MaxMaterialDescriptors; ++j )
	{
		VkDescriptorBufferInfo info{ };
		info.buffer = frameState[ i ].materialBuffers.GetVkObject();
		info.offset = j * sizeof( materialBufferObject_t );
		info.range = sizeof( materialBufferObject_t );
		materialBufferInfo.push_back( info );
	}

	std::vector<VkDescriptorBufferInfo> lightBufferInfo;
	lightBufferInfo.reserve( MaxLights );
	for ( int j = 0; j < MaxLights; ++j )
	{
		VkDescriptorBufferInfo info{ };
		info.buffer = frameState[ i ].lightParms.GetVkObject();
		info.offset = j * sizeof( light_t );
		info.range = sizeof( light_t );
		lightBufferInfo.push_back( info );
	}

	std::vector<VkDescriptorImageInfo> codeImageInfo;
	codeImageInfo.reserve( MaxCodeImages );
	// Shadow Map
	for ( int j = 0; j < MaxCodeImages; ++j )
	{
		VkImageView& imageView = frameState[ currentImage ].shadowMapImage.vk_view;
		VkDescriptorImageInfo info{ };
		info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		info.imageView = imageView;
		info.sampler = vk_depthShadowSampler;
		codeImageInfo.push_back( info );
	}

	const uint32_t descriptorSetCnt = 8;
	std::array<VkWriteDescriptorSet, descriptorSetCnt> descriptorWrites{ };

	uint32_t descriptorId = 0;
	descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
	descriptorWrites[ descriptorId ].dstBinding = 0;
	descriptorWrites[ descriptorId ].dstArrayElement = 0;
	descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[ descriptorId ].descriptorCount = 1;
	descriptorWrites[ descriptorId ].pBufferInfo = &globalConstantsInfo[ 0 ];
	++descriptorId;

	descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
	descriptorWrites[ descriptorId ].dstBinding = 1;
	descriptorWrites[ descriptorId ].dstArrayElement = 0;
	descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[ descriptorId ].descriptorCount = MaxSurfaces;
	descriptorWrites[ descriptorId ].pBufferInfo = &bufferInfo[ 0 ];
	++descriptorId;

	descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
	descriptorWrites[ descriptorId ].dstBinding = 2;
	descriptorWrites[ descriptorId ].dstArrayElement = 0;
	descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
	descriptorWrites[ descriptorId ].pImageInfo = &image2DInfo[ 0 ];
	++descriptorId;

	descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
	descriptorWrites[ descriptorId ].dstBinding = 3;
	descriptorWrites[ descriptorId ].dstArrayElement = 0;
	descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
	descriptorWrites[ descriptorId ].pImageInfo = &imageCubeInfo[ 0 ];
	++descriptorId;

	descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
	descriptorWrites[ descriptorId ].dstBinding = 4;
	descriptorWrites[ descriptorId ].dstArrayElement = 0;
	descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[ descriptorId ].descriptorCount = MaxMaterialDescriptors;
	descriptorWrites[ descriptorId ].pBufferInfo = &materialBufferInfo[ 0 ]; // TODO: replace
	++descriptorId;

	descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
	descriptorWrites[ descriptorId ].dstBinding = 5;
	descriptorWrites[ descriptorId ].dstArrayElement = 0;
	descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[ descriptorId ].descriptorCount = MaxLights;
	descriptorWrites[ descriptorId ].pBufferInfo = &lightBufferInfo[ 0 ];
	++descriptorId;

	descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
	descriptorWrites[ descriptorId ].dstBinding = 6;
	descriptorWrites[ descriptorId ].dstArrayElement = 0;
	descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[ descriptorId ].descriptorCount = MaxCodeImages;
	descriptorWrites[ descriptorId ].pImageInfo = &codeImageInfo[ 0 ];
	++descriptorId;

	descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
	descriptorWrites[ descriptorId ].dstBinding = 7;
	descriptorWrites[ descriptorId ].dstArrayElement = 0;
	descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[ descriptorId ].descriptorCount = 1;
	descriptorWrites[ descriptorId ].pImageInfo = &codeImageInfo[ 0 ];
	++descriptorId;

	assert( descriptorId == descriptorSetCnt );

	vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( descriptorWrites.size() ), descriptorWrites.data(), 0, nullptr );

	//////////////////////////////////////////////////////
	//													//
	// Shadow Descriptor Sets							//
	//													//
	//////////////////////////////////////////////////////

	std::vector<VkDescriptorImageInfo> shadowImageInfo;
	shadowImageInfo.reserve( MaxImageDescriptors );
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		texture_t* texture = scene.textureLib.Find( i );
		VkImageView& imageView = texture->image.vk_view;
		VkDescriptorImageInfo info{ };
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = imageView;
		info.sampler = vk_bilinearSampler;
		shadowImageInfo.push_back( info );
	}
	// Defaults
	for ( size_t j = textureCount; j < MaxImageDescriptors; ++j )
	{
		const texture_t* texture = scene.textureLib.GetDefault();
		const VkImageView& imageView = texture->image.vk_view;
		VkDescriptorImageInfo info{ };
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = imageView;
		info.sampler = vk_bilinearSampler;
		shadowImageInfo.push_back( info );
	}

	std::vector<VkDescriptorImageInfo> shadowCodeImageInfo;
	shadowCodeImageInfo.reserve( MaxCodeImages );
	for ( size_t j = 0; j < MaxCodeImages; ++j )
	{
		const texture_t* texture = scene.textureLib.GetDefault();
		const VkImageView& imageView = texture->image.vk_view;
		VkDescriptorImageInfo info{ };
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = imageView;
		info.sampler = vk_bilinearSampler;
		shadowCodeImageInfo.push_back( info );
	}

	std::array<VkWriteDescriptorSet, descriptorSetCnt> shadowDescriptorWrites{ };

	descriptorId = 0;
	shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
	shadowDescriptorWrites[ descriptorId ].dstBinding = 0;
	shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	shadowDescriptorWrites[ descriptorId ].descriptorCount = 1;
	shadowDescriptorWrites[ descriptorId ].pBufferInfo = &globalConstantsInfo[ 0 ];
	++descriptorId;

	shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
	shadowDescriptorWrites[ descriptorId ].dstBinding = 1;
	shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxSurfaces;
	shadowDescriptorWrites[ descriptorId ].pBufferInfo = &bufferInfo[ 0 ];
	++descriptorId;

	shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
	shadowDescriptorWrites[ descriptorId ].dstBinding = 2;
	shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
	shadowDescriptorWrites[ descriptorId ].pImageInfo = &shadowImageInfo[ 0 ];
	++descriptorId;

	shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
	shadowDescriptorWrites[ descriptorId ].dstBinding = 3;
	shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
	shadowDescriptorWrites[ descriptorId ].pImageInfo = &shadowImageInfo[ 0 ];
	++descriptorId;

	shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
	shadowDescriptorWrites[ descriptorId ].dstBinding = 4;
	shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxMaterialDescriptors;
	shadowDescriptorWrites[ descriptorId ].pBufferInfo = &materialBufferInfo[ 0 ];
	++descriptorId;

	shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
	shadowDescriptorWrites[ descriptorId ].dstBinding = 5;
	shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxLights;
	shadowDescriptorWrites[ descriptorId ].pBufferInfo = &lightBufferInfo[ 0 ];
	++descriptorId;

	shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
	shadowDescriptorWrites[ descriptorId ].dstBinding = 6;
	shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxCodeImages;
	shadowDescriptorWrites[ descriptorId ].pImageInfo = &shadowCodeImageInfo[ 0 ];
	++descriptorId;

	shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
	shadowDescriptorWrites[ descriptorId ].dstBinding = 7;
	shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowDescriptorWrites[ descriptorId ].descriptorCount = 1;
	shadowDescriptorWrites[ descriptorId ].pImageInfo = &shadowCodeImageInfo[ 0 ];
	++descriptorId;

	assert( descriptorId == descriptorSetCnt );
	vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( shadowDescriptorWrites.size() ), shadowDescriptorWrites.data(), 0, nullptr );

	//////////////////////////////////////////////////////
	//													//
	// Post Descriptor Sets								//
	//													//
	//////////////////////////////////////////////////////
	std::vector<VkDescriptorImageInfo> postImageInfo;
	postImageInfo.reserve( 3 );
	// View Color Map
	{
		VkImageView& imageView = frameState[ currentImage ].viewColorImage.vk_view;
		VkDescriptorImageInfo info{ };
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = imageView;
		info.sampler = vk_bilinearSampler;
		postImageInfo.push_back( info );
	}
	// View Depth Map
	{
		VkImageView& imageView = frameState[ currentImage ].depthImage.vk_view;
		VkDescriptorImageInfo info{ };
		info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		info.imageView = imageView;
		info.sampler = vk_bilinearSampler;
		postImageInfo.push_back( info );
	}
	// View Stencil Map
	{
		VkImageView& imageView = frameState[ currentImage ].stencilImage.vk_view;
		VkDescriptorImageInfo info{ };
		info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		info.imageView = imageView;
		info.sampler = vk_bilinearSampler;
		postImageInfo.push_back( info );
	}

	std::array<VkWriteDescriptorSet, 8> postDescriptorWrites{ };

	descriptorId = 0;
	postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
	postDescriptorWrites[ descriptorId ].dstBinding = 0;
	postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	postDescriptorWrites[ descriptorId ].descriptorCount = 1;
	postDescriptorWrites[ descriptorId ].pBufferInfo = &globalConstantsInfo[ 0 ];
	++descriptorId;

	postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
	postDescriptorWrites[ descriptorId ].dstBinding = 1;
	postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	postDescriptorWrites[ descriptorId ].descriptorCount = MaxSurfaces;
	postDescriptorWrites[ descriptorId ].pBufferInfo = &bufferInfo[ 0 ];
	++descriptorId;

	postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
	postDescriptorWrites[ descriptorId ].dstBinding = 2;
	postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	postDescriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
	postDescriptorWrites[ descriptorId ].pImageInfo = &image2DInfo[ 0 ];
	++descriptorId;

	postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
	postDescriptorWrites[ descriptorId ].dstBinding = 3;
	postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	postDescriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
	postDescriptorWrites[ descriptorId ].pImageInfo = &imageCubeInfo[ 0 ];
	++descriptorId;

	postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
	postDescriptorWrites[ descriptorId ].dstBinding = 4;
	postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	postDescriptorWrites[ descriptorId ].descriptorCount = MaxMaterialDescriptors;
	postDescriptorWrites[ descriptorId ].pBufferInfo = &materialBufferInfo[ 0 ];
	++descriptorId;

	postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
	postDescriptorWrites[ descriptorId ].dstBinding = 5;
	postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	postDescriptorWrites[ descriptorId ].descriptorCount = MaxLights;
	postDescriptorWrites[ descriptorId ].pBufferInfo = &lightBufferInfo[ 0 ];
	++descriptorId;

	postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
	postDescriptorWrites[ descriptorId ].dstBinding = 6;
	postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	postDescriptorWrites[ descriptorId ].descriptorCount = MaxCodeImages;
	postDescriptorWrites[ descriptorId ].pImageInfo = &postImageInfo[ 0 ];
	++descriptorId;

	postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
	postDescriptorWrites[ descriptorId ].dstBinding = 7;
	postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
	postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	postDescriptorWrites[ descriptorId ].descriptorCount = 1;
	postDescriptorWrites[ descriptorId ].pImageInfo = &postImageInfo[ 2 ];
	++descriptorId;

	vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( postDescriptorWrites.size() ), postDescriptorWrites.data(), 0, nullptr );
}

void Renderer::UpdateBufferContents( uint32_t currentImage )
{
	std::vector< globalUboConstants_t > globalsBuffer;
	globalsBuffer.reserve( 1 );
	{
		globalUboConstants_t globals;
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

		float intPart = 0;
		const float fracPart = modf( time, &intPart );

		const float viewWidth = renderView.viewport.width;
		const float viewHeight = renderView.viewport.height;

		globals.time = vec4f( time, intPart, fracPart, 1.0f );
		globals.generic = vec4f( imguiControls.heightMapHeight, imguiControls.roughness, 0.0f, 0.0f );
		globals.dimensions = vec4f( viewWidth, viewHeight, 1.0f / viewWidth, 1.0f / viewHeight );
		globals.tonemap = vec4f( imguiControls.toneMapColor[ 0 ], imguiControls.toneMapColor[ 1 ], imguiControls.toneMapColor[ 2 ], imguiControls.toneMapColor[ 3 ] );
		globals.shadowParms = vec4f( ShadowObjectOffset, ShadowMapWidth, ShadowMapHeight, imguiControls.shadowStrength );
		globalsBuffer.push_back( globals );
	}

	std::vector< uniformBufferObject_t > uboBuffer;
	uboBuffer.resize( MaxSurfacesDescriptors );
	assert( renderView.committedModelCnt < MaxModels );
	for ( uint32_t i = 0; i < renderView.committedModelCnt; ++i )
	{
		uniformBufferObject_t ubo;
		ubo.model = renderView.instances[ i ].modelMatrix;
		ubo.view = renderView.viewMatrix;
		ubo.proj = renderView.projMatrix;
		const drawSurf_t& surf = renderView.merged[ renderView.instances[ i ].surfId ];
		const uint32_t objectId = ( renderView.instances[ i ].id + surf.objectId );
		uboBuffer[ objectId ] = ubo;
	}
	assert( shadowView.committedModelCnt < MaxModels );
	for ( uint32_t i = 0; i < shadowView.committedModelCnt; ++i )
	{
		uniformBufferObject_t ubo;
		ubo.model = shadowView.instances[ i ].modelMatrix;
		ubo.view = shadowView.viewMatrix;
		ubo.proj = shadowView.projMatrix;
		const drawSurf_t& surf = shadowView.merged[ shadowView.instances[ i ].surfId ];
		const uint32_t objectId = ( shadowView.instances[ i ].id + surf.objectId );
		uboBuffer[ objectId ] = ubo;
	}

	std::vector< light_t > lightBuffer;
	lightBuffer.reserve( MaxLights );
	for ( int i = 0; i < MaxLights; ++i )
	{
		lightBuffer.push_back( renderView.lights[ i ] );
	}

	frameState[ currentImage ].globalConstants.Reset();
	frameState[ currentImage ].globalConstants.CopyData( globalsBuffer.data(), sizeof( globalUboConstants_t ) );

	assert( uboBuffer.size() <= MaxSurfaces );
	frameState[ currentImage ].surfParms.Reset();
	frameState[ currentImage ].surfParms.CopyData( uboBuffer.data(), sizeof( uniformBufferObject_t ) * uboBuffer.size() );

	assert( materialBuffer.size() <= MaxMaterialDescriptors );
	frameState[ currentImage ].materialBuffers.Reset();
	frameState[ currentImage ].materialBuffers.CopyData( materialBuffer.data(), sizeof( materialBufferObject_t ) * materialBuffer.size() );

	assert( lightBuffer.size() <= MaxLights );
	frameState[ currentImage ].lightParms.Reset();
	frameState[ currentImage ].lightParms.CopyData( lightBuffer.data(), sizeof( light_t ) * lightBuffer.size() );
}

void Renderer::CreateTextureSamplers()
{
	{
		// Default Bilinear Sampler
		VkSamplerCreateInfo samplerInfo{ };
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = 16.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 16.0f;
		samplerInfo.mipLodBias = 0.0f;

		if ( vkCreateSampler( context.device, &samplerInfo, nullptr, &vk_bilinearSampler ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create texture sampler!" );
		}
	}

	{
		// Depth sampler
		VkSamplerCreateInfo samplerInfo{ };
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 0.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 16.0f;
		samplerInfo.mipLodBias = 0.0f;

		if ( vkCreateSampler( context.device, &samplerInfo, nullptr, &vk_depthShadowSampler ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create depth sampler!" );
		}
	}
}

void Renderer::CreateLogicalDevice()
{
	QueueFamilyIndices indices = FindQueueFamilies( context.physicalDevice, window.vk_surface );
	context.queueFamilyIndices[ QUEUE_GRAPHICS ] = indices.graphicsFamily.value();
	context.queueFamilyIndices[ QUEUE_PRESENT ] = indices.presentFamily.value();
	context.queueFamilyIndices[ QUEUE_COMPUTE ] = indices.computeFamily.value();

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value(), indices.computeFamily.value() };

	float queuePriority = 1.0f;
	for ( uint32_t queueFamily : uniqueQueueFamilies )
	{
		VkDeviceQueueCreateInfo queueCreateInfo{ };
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back( queueCreateInfo );
	}

	VkDeviceCreateInfo createInfo{ };
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>( queueCreateInfos.size() );
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	VkPhysicalDeviceFeatures deviceFeatures{ };
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.fillModeNonSolid = VK_TRUE;
	deviceFeatures.sampleRateShading = VK_TRUE;
	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = static_cast<uint32_t>( deviceExtensions.size() );
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
	if ( enableValidationLayers )
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>( validationLayers.size() );
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	VkPhysicalDeviceDescriptorIndexingFeatures descIndexing;
	memset( &descIndexing, 0, sizeof( VkPhysicalDeviceDescriptorIndexingFeatures ) );
	descIndexing.runtimeDescriptorArray = true;
	descIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	descIndexing.pNext = NULL;
	createInfo.pNext = &descIndexing;

	if ( vkCreateDevice( context.physicalDevice, &createInfo, nullptr, &context.device ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create logical context.device!" );
	}

	vkGetDeviceQueue( context.device, indices.graphicsFamily.value(), 0, &context.graphicsQueue );
	vkGetDeviceQueue( context.device, indices.presentFamily.value(), 0, &context.presentQueue );
	vkGetDeviceQueue( context.device, indices.computeFamily.value(), 0, &context.computeQueue );
}

void Renderer::PickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices( context.instance, &deviceCount, nullptr );

	if ( deviceCount == 0 ) {
		throw std::runtime_error( "Failed to find GPUs with Vulkan support!" );
	}

	std::vector<VkPhysicalDevice> devices( deviceCount );
	vkEnumeratePhysicalDevices( context.instance, &deviceCount, devices.data() );

	for ( const auto& device : devices )
	{
		if ( IsDeviceSuitable( device, window.vk_surface, deviceExtensions ) )
		{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties( device, &deviceProperties );
			context.physicalDevice = device;
			context.limits = deviceProperties.limits;
			msaaSamples = GetMaxUsableSampleCount();
			break;
		}
	}

	if ( context.physicalDevice == VK_NULL_HANDLE ) {
		throw std::runtime_error( "Failed to find a suitable GPU!" );
	}
}

std::vector<const char*> Renderer::GetRequiredExtensions() const
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

	std::vector<const char*> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );

	if ( enableValidationLayers ) {
		extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
	}

	return extensions;
}

bool Renderer::CheckValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

	std::vector<VkLayerProperties> availableLayers( layerCount );
	vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

	for ( const char* layerName : validationLayers ) {
		bool layerFound = false;
		for ( const auto& layerProperties : availableLayers ) {
			if ( strcmp( layerName, layerProperties.layerName ) == 0 ) {
				layerFound = true;
				break;
			}
		}
		if ( !layerFound ) {
			return false;
		}
	}
	return true;
}

void Renderer::PopulateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo )
{
	createInfo = { };
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	createInfo.messageSeverity = 0;
	if ( ValidateVerbose ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	}

	if ( ValidateWarnings ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	}

	if ( ValidateErrors ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	}

	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
}

void Renderer::SetupDebugMessenger()
{
	if ( !enableValidationLayers ) {
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	PopulateDebugMessengerCreateInfo( createInfo );

	if ( CreateDebugUtilsMessengerEXT( context.instance, &createInfo, nullptr, &debugMessenger ) != VK_SUCCESS )
	{
		throw std::runtime_error( "Failed to set up debug messenger!" );
	}
}

VkResult Renderer::CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger )
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
	if ( func != nullptr ) {
		return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void Renderer::DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator )
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
	if ( func != nullptr ) {
		func( instance, debugMessenger, pAllocator );
	}
}