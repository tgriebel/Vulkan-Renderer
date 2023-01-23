#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"
#include <scene/scene.h>
#include <scene/entity.h>
#include <sstream>

#include <io/io.h>
static RtView rtview;
static RtScene rtScene;

extern Scene* gScene;

static void BuildRayTraceScene( Scene* scene )
{
	rtScene.scene = scene;

	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	rtScene.lights.clear();
	rtScene.models.clear();
	rtScene.models.reserve( entCount );
	rtScene.aabb = AABB();

	for ( uint32_t i = 0; i < entCount; ++i )
	{
		RtModel rtModel;
		CreateRayTraceModel( gAssets, scene->entities[ i ], &rtModel );
		rtScene.models.push_back( rtModel );

		AABB& aabb = rtModel.octree.GetAABB();
		rtScene.aabb.Expand( aabb.GetMin() );
		rtScene.aabb.Expand( aabb.GetMax() );
	}

	for ( uint32_t i = 0; i < MaxLights; ++i )
	{
		rtScene.lights.push_back( scene->lights[ i ] );
	}
}


static void TraceScene()
{
	gImguiControls.raytraceScene = false;

	rtview.targetSize[ 0 ] = 320;
	rtview.targetSize[ 1 ] = 180;
	//window.GetWindowSize( rtview.targetSize[0], rtview.targetSize[1] );
	Image<Color> rtimage( rtview.targetSize[ 0 ], rtview.targetSize[ 1 ], Color::Black, "testRayTrace" );
	{
		rtview.camera = rtScene.scene->camera;
		rtview.viewTransform = rtview.camera.GetViewMatrix().Transpose();
		rtview.projTransform = rtview.camera.GetPerspectiveMatrix().Transpose();
		rtview.projView = rtview.projTransform * rtview.viewTransform;
	}

	// Need to turn cpu images back on
	//TraceScene( rtview, rtScene, rtimage );
	//RasterScene( rtimage, rtview, rtScene, false );

	{
		std::stringstream ss;
		const char* name = rtimage.GetName();
		ss << "output/";
		if ( ( name == nullptr ) || ( name == "" ) )
		{
			ss << reinterpret_cast<uint64_t>( &rtimage );
		}
		else
		{
			ss << name;
		}

		ss << ".bmp";

		Bitmap bitmap = Bitmap( rtimage.GetWidth(), rtimage.GetHeight() );
		ImageToBitmap( rtimage, bitmap );
		bitmap.Write( ss.str() );
	}
}

static bool CompareSortKey( drawSurf_t& surf0, drawSurf_t& surf1 )
{
	if( surf0.materialId == surf1.materialId ) {
		return ( surf0.objectId < surf1.objectId );
	} else {
		return ( surf0.materialId < surf1.materialId );
	}
}


void Renderer::Commit( const Scene* scene )
{
	renderView.committedModelCnt = 0;
	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	for ( uint32_t i = 0; i < entCount; ++i ) {
		CommitModel( renderView, *scene->entities[i], 0 );
	}
	MergeSurfaces( renderView );

	shadowView.committedModelCnt = 0;
	for ( uint32_t i = 0; i < entCount; ++i )
	{
		if ( ( scene->entities[i] )->HasFlag( ENT_FLAG_NO_SHADOWS ) ) {
			continue;
		}
		CommitModel( shadowView, *scene->entities[i], MaxModels );
	}
	MergeSurfaces( shadowView );

	renderView.viewMatrix = scene->camera.GetViewMatrix();
	renderView.projMatrix = scene->camera.GetPerspectiveMatrix();
	renderView.viewprojMatrix = renderView.projMatrix * renderView.viewMatrix;

	for ( int i = 0; i < MaxLights; ++i ) {
		renderView.lights[ i ] = scene->lights[ i ];
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
	if ( ent.HasFlag( ENT_FLAG_NO_DRAW ) ) {
		return;
	}

	assert( DRAWPASS_COUNT <= Material::MaxMaterialShaders );

	Model& source = gAssets.modelLib.Find( ent.modelHdl )->Get();
	for ( uint32_t i = 0; i < source.surfCount; ++i ) {
		drawSurfInstance_t& instance = view.instances[ view.committedModelCnt ];
		drawSurf_t& surf = view.surfaces[ view.committedModelCnt ];
		surfaceUpload_t& upload = source.upload[ i ];

		const Material& material = gAssets.materialLib.Find( ent.materialHdl.IsValid() ? ent.materialHdl : source.surfs[ i ].materialHdl )->Get();
		assert( material.uploadId >= 0 );

		renderFlags_t renderFlags = NONE;
		renderFlags = static_cast<renderFlags_t>( renderFlags | ( ent.HasFlag( ENT_FLAG_NO_DRAW ) ? HIDDEN : NONE ) );
		renderFlags = static_cast<renderFlags_t>( renderFlags | ( ent.HasFlag( ENT_FLAG_NO_SHADOWS ) ? NO_SHADOWS : NONE ) );
		renderFlags = static_cast<renderFlags_t>( renderFlags | ( ent.HasFlag( ENT_FLAG_WIREFRAME ) ? WIREFRAME | SKIP_OPAQUE : NONE ) );

		instance.modelMatrix = ent.GetMatrix();
		instance.surfId = 0;
		instance.id = 0;
		surf.vertexCount = upload.vertexCount;
		surf.vertexOffset = upload.vertexOffset;
		surf.firstIndex = upload.firstIndex;
		surf.indicesCnt = upload.indexCount;
		surf.materialId = material.uploadId;
		surf.objectId = objectOffset;
		surf.flags = renderFlags;
		surf.stencilBit = ent.outline ? 0x01 : 0;
		surf.hash = Hash( surf );

		for ( int pass = 0; pass < DRAWPASS_COUNT; ++pass ) {
			if ( material.GetShader( pass ).IsValid() ) {
				Asset<GpuProgram>* prog = gAssets.gpuPrograms.Find( material.GetShader( pass ) );
				if ( prog == nullptr ) {
					continue;
				}
				surf.pipelineObject[ pass ] = prog->Get().pipeline;
			}
			else {
				surf.pipelineObject[ pass ] = INVALID_HDL;
			}
		}

		++view.committedModelCnt;
	}
}


void Renderer::UploadAssets( AssetManager& assets )
{
	const uint32_t materialCount = assets.materialLib.Count();
	for ( uint32_t i = 0; i < materialCount; ++i )
	{
		Asset<Material>* materialAsset = assets.materialLib.Find( i );
		if ( materialAsset->IsLoaded() == false ) {
			continue;
		}
		Material& material = materialAsset->Get();
		if ( material.uploadId != -1 ) {
			continue;
		}
		pendingMaterials.push_back( i );
	}

	const uint32_t textureCount = assets.textureLib.Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		Asset<Texture>* textureAsset = assets.textureLib.Find( i );
		if ( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Texture& texture = textureAsset->Get();
		if ( texture.uploadId != -1 ) {
			continue;
		}
		pendingTextures.push_back( i );
	}

	UploadTextures();
	UploadModelsToGPU();
	UpdateDescriptorSets();
}


void Renderer::RenderScene( Scene* scene )
{
	UpdateGpuMaterials();
	Commit( scene );
	SubmitFrame();

	if( gImguiControls.rebuildRaytraceScene ) {
		BuildRayTraceScene( scene );
	}

	if ( gImguiControls.raytraceScene ) {
		TraceScene();
	}

	localMemory.Pack();
	sharedMemory.Pack();
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


void Renderer::UpdateDescriptorSets()
{
	const uint32_t frameBufferCount = static_cast<uint32_t>( swapChain.GetBufferCount() );
	for ( size_t i = 0; i < frameBufferCount; i++ )
	{
		UpdateFrameDescSet( static_cast<uint32_t>( i ) );
	}
}


void Renderer::SubmitFrame()
{
	WaitForEndFrame();

	VkResult result = vkAcquireNextImageKHR( context.device, swapChain.vk_swapChain, UINT64_MAX, graphicsQueue.imageAvailableSemaphores[ frameId ], VK_NULL_HANDLE, &bufferId );
	if ( result == VK_ERROR_OUT_OF_DATE_KHR )
	{
		RecreateSwapChain();
		return;
	}
	else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR )
	{
		throw std::runtime_error( "Failed to acquire swap chain image!" );
	}

	// Check if a previous frame is using this image (i.e. there is its fence to wait on)
	if ( graphicsQueue.imagesInFlight[ bufferId ] != VK_NULL_HANDLE ) {
		vkWaitForFences( context.device, 1, &graphicsQueue.imagesInFlight[ bufferId ], VK_TRUE, UINT64_MAX );
	}
	// Mark the image as now being in use by this frame
	graphicsQueue.imagesInFlight[ bufferId ] = graphicsQueue.inFlightFences[ frameId ];

	UpdateBufferContents( bufferId );
	UpdateFrameDescSet( bufferId );
	Render( renderView );

	VkSubmitInfo submitInfo{ };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { graphicsQueue.imageAvailableSemaphores[ frameId ] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &graphicsQueue.commandBuffers[ bufferId ];

	VkSemaphore signalSemaphores[] = { graphicsQueue.renderFinishedSemaphores[ frameId ] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences( context.device, 1, &graphicsQueue.inFlightFences[ frameId ] );

	if ( vkQueueSubmit( context.graphicsQueue, 1, &submitInfo, graphicsQueue.inFlightFences[ frameId ] ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to submit draw command buffer!" );
	}

	VkPresentInfoKHR presentInfo{ };
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { swapChain.GetApiObject() };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &bufferId;
	presentInfo.pResults = nullptr; // Optional

	result = vkQueuePresentKHR( context.presentQueue, &presentInfo );

	if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || gWindow.IsResizeRequested() )
	{
		RecreateSwapChain();
		gWindow.AcceptImageResize();
		return;
	}
	else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR )
	{
		throw std::runtime_error( "Failed to acquire swap chain image!" );
	}

	frameId = ( frameId + 1 ) % MAX_FRAMES_IN_FLIGHT;
	++frameNumber;
	static auto prevTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	renderTime = std::chrono::duration<float, std::chrono::milliseconds::period>( currentTime - prevTime ).count();
	prevTime = currentTime;
}


void Renderer::UpdateView()
{
	int width;
	int height;
	gWindow.GetWindowSize( width, height );
	renderView.viewport.width = static_cast<float>( width );
	renderView.viewport.height = static_cast<float>( height );

	vec3f shadowLightDir;
	shadowLightDir[ 0 ] = -renderView.lights[ 0 ].lightDir[ 0 ];
	shadowLightDir[ 1 ] = -renderView.lights[ 0 ].lightDir[ 1 ];
	shadowLightDir[ 2 ] = -renderView.lights[ 0 ].lightDir[ 2 ];

	// Temp shadow map set-up
	shadowView.viewport.x = 0.0f;
	shadowView.viewport.y = 0.0f;
	shadowView.viewport.near = 0.0f;
	shadowView.viewport.far = 1.0f;
	shadowView.viewport.width = ShadowMapWidth;
	shadowView.viewport.height = ShadowMapHeight;
	shadowView.viewMatrix = MatrixFromVector( shadowLightDir );
	shadowView.viewMatrix = shadowView.viewMatrix.Transpose();
	const vec4f shadowLightPos = shadowView.viewMatrix * renderView.lights[ 0 ].lightPos;
	shadowView.viewMatrix[ 3 ][ 0 ] = -shadowLightPos[ 0 ];
	shadowView.viewMatrix[ 3 ][ 1 ] = -shadowLightPos[ 1 ];
	shadowView.viewMatrix[ 3 ][ 2 ] = -shadowLightPos[ 2 ];

	Camera shadowCam;
	shadowCam = Camera( vec4f( 0.0f, 0.0f, 0.0f, 0.0f ) );
	shadowCam.far = nearPlane;
	shadowCam.near = farPlane;
	shadowCam.focalLength = shadowCam.far;
	shadowCam.SetFov( Radians( 90.0f ) );
	shadowCam.SetAspectRatio( ( ShadowMapWidth / (float)ShadowMapHeight ) );
	shadowView.projMatrix = shadowCam.GetPerspectiveMatrix();
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
	image2DInfo.resize( MaxImageDescriptors );
	imageCubeInfo.resize( MaxImageDescriptors );
	int firstCube = -1;
	const uint32_t textureCount = gAssets.textureLib.Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		Texture& texture = gAssets.textureLib.Find( i )->Get();
		if( texture.uploadId == -1 ) {
			continue;
		}

		VkImageView& imageView = texture.gpuImage.vk_view;
		VkDescriptorImageInfo info{ };
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = imageView;
		info.sampler = vk_bilinearSampler;

		if ( texture.info.type == TEXTURE_TYPE_CUBE ) {
			imageCubeInfo[ texture.uploadId ] = info;
			firstCube = texture.uploadId;

			VkDescriptorImageInfo info2d{ };
			info2d.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info2d.imageView = gAssets.textureLib.GetDefault()->gpuImage.vk_view;
			info2d.sampler = vk_bilinearSampler;
			image2DInfo[ texture.uploadId ] = info2d;
		}
		else 
		{
			image2DInfo[ texture.uploadId ] = info;
		}
	}
	// Defaults
	{
		const Texture* default2DTexture = gAssets.textureLib.GetDefault();
		for ( size_t j = textureCount; j < MaxImageDescriptors; ++j )
		{
			const VkImageView& imageView = default2DTexture->gpuImage.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			image2DInfo[j] = info;
		}
		assert( firstCube >= 0 ); // Hack: need a default
		for ( size_t i = 0; i < MaxImageDescriptors; ++i )
		{
			if( imageCubeInfo[i].imageView == nullptr ) {
				const VkImageView& imageView = imageCubeInfo[ firstCube ].imageView;
				VkDescriptorImageInfo info{ };
				info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				info.imageView = imageView;
				info.sampler = vk_bilinearSampler;
				imageCubeInfo[i] = info;
			}
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
		info.offset = j * sizeof( lightBufferObject_t );
		info.range = sizeof( lightBufferObject_t );
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
		Texture& texture = gAssets.textureLib.Find( i )->Get();
		VkImageView& imageView = texture.gpuImage.vk_view;
		VkDescriptorImageInfo info{ };
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = imageView;
		info.sampler = vk_bilinearSampler;
		shadowImageInfo.push_back( info );
	}
	// Defaults
	for ( size_t j = textureCount; j < MaxImageDescriptors; ++j )
	{
		const Texture* texture = gAssets.textureLib.GetDefault();
		const VkImageView& imageView = texture->gpuImage.vk_view;
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
		const Texture* texture = gAssets.textureLib.GetDefault();
		const VkImageView& imageView = texture->gpuImage.vk_view;
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
		globals.generic = vec4f( gImguiControls.heightMapHeight, gImguiControls.roughness, 0.0f, 0.0f );
		globals.dimensions = vec4f( viewWidth, viewHeight, 1.0f / viewWidth, 1.0f / viewHeight );
		globals.tonemap = vec4f( gImguiControls.toneMapColor[ 0 ], gImguiControls.toneMapColor[ 1 ], gImguiControls.toneMapColor[ 2 ], gImguiControls.toneMapColor[ 3 ] );
		globals.shadowParms = vec4f( ShadowObjectOffset, ShadowMapWidth, ShadowMapHeight, gImguiControls.shadowStrength );
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

	std::vector< lightBufferObject_t > lightBuffer;
	lightBuffer.reserve( MaxLights );
	for ( int i = 0; i < MaxLights; ++i )
	{
		lightBufferObject_t light;
		light.intensity = renderView.lights[ i ].intensity;
		light.lightDir = renderView.lights[ i ].lightDir;
		light.lightPos = renderView.lights[ i ].lightPos;
		lightBuffer.push_back( light );
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
	frameState[ currentImage ].lightParms.CopyData( lightBuffer.data(), sizeof( lightBufferObject_t ) * lightBuffer.size() );
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
		if ( IsDeviceSuitable( device, gWindow.vk_surface, deviceExtensions ) )
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


void Renderer::Render( RenderView& view )
{
	const uint32_t i = bufferId;
	vkResetCommandBuffer( graphicsQueue.commandBuffers[ i ], 0 );

	VkCommandBufferBeginInfo beginInfo{ };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	int width = 0;
	int height = 0;
	gWindow.GetWindowSize( width, height );

	if ( vkBeginCommandBuffer( graphicsQueue.commandBuffers[ i ], &beginInfo ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to begin recording command buffer!" );
	}

	VkBuffer vertexBuffers[] = { vb.GetVkObject() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers( graphicsQueue.commandBuffers[ i ], 0, 1, vertexBuffers, offsets );
	vkCmdBindIndexBuffer( graphicsQueue.commandBuffers[ i ], ib.GetVkObject(), 0, VK_INDEX_TYPE_UINT32 );

	// Shadow Passes
	{
		VkRenderPassBeginInfo shadowPassInfo{ };
		shadowPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		shadowPassInfo.renderPass = shadowPassState.pass;
		shadowPassInfo.framebuffer = shadowPassState.fb[ i ];
		shadowPassInfo.renderArea.offset = { 0, 0 };
		shadowPassInfo.renderArea.extent = { ShadowMapWidth, ShadowMapHeight };

		std::array<VkClearValue, 2> shadowClearValues{ };
		shadowClearValues[ 0 ].color = { 1.0f, 1.0f, 1.0f, 1.0f };
		shadowClearValues[ 1 ].depthStencil = { 1.0f, 0 };

		shadowPassInfo.clearValueCount = static_cast<uint32_t>( shadowClearValues.size() );
		shadowPassInfo.pClearValues = shadowClearValues.data();

		//VkDebugUtilsLabelEXT markerInfo = {};
		//markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		//markerInfo.pLabelName = "Begin Section";
		//vkCmdBeginDebugUtilsLabelEXT( graphicsQueue.commandBuffers[ i ], &markerInfo );

		//// contents of section here

		//vkCmdEndDebugUtilsLabelEXT( graphicsQueue.commandBuffers[ i ] );

		// TODO: how to handle views better?
		vkCmdBeginRenderPass( graphicsQueue.commandBuffers[ i ], &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE );

		for ( size_t surfIx = 0; surfIx < shadowView.mergedModelCnt; surfIx++ )
		{
			drawSurf_t& surface = shadowView.merged[ surfIx ];

			pipelineObject_t* pipelineObject = nullptr;
			if ( surface.pipelineObject[ DRAWPASS_SHADOW ] == INVALID_HDL ) {
				continue;
			}
			if ( ( surface.flags & SKIP_OPAQUE ) != 0 ) {
				continue;
			}
			GetPipelineObject( surface.pipelineObject[ DRAWPASS_SHADOW ], &pipelineObject );
			if ( pipelineObject == nullptr ) {
				continue;
			}

			// vkCmdSetDepthBias
			vkCmdBindPipeline( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
			vkCmdBindDescriptorSets( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &shadowPassState.descriptorSets[ i ], 0, nullptr );

			VkViewport viewport{ };
			viewport.x = shadowView.viewport.x;
			viewport.y = shadowView.viewport.y;
			viewport.width = shadowView.viewport.width;
			viewport.height = shadowView.viewport.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport( graphicsQueue.commandBuffers[ i ], 0, 1, &viewport );

			VkRect2D rect{ };
			rect.extent.width = static_cast<uint32_t>( shadowView.viewport.width );
			rect.extent.height = static_cast<uint32_t>( shadowView.viewport.height );
			vkCmdSetScissor( graphicsQueue.commandBuffers[ i ], 0, 1, &rect );

			pushConstants_t pushConstants = { surface.objectId, surface.materialId };
			vkCmdPushConstants( graphicsQueue.commandBuffers[ i ], pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

			vkCmdDrawIndexed( graphicsQueue.commandBuffers[ i ], surface.indicesCnt, shadowView.instanceCounts[ surfIx ], surface.firstIndex, surface.vertexOffset, 0 );
		}
		vkCmdEndRenderPass( graphicsQueue.commandBuffers[ i ] );
	}

	// Main Passes
	{
		VkRenderPassBeginInfo renderPassInfo{ };
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = mainPassState.pass;
		renderPassInfo.framebuffer = mainPassState.fb[ i ];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChain.vk_swapChainExtent;

		std::array<VkClearValue, 2> clearValues{ };
		clearValues[ 0 ].color = { 0.0f, 0.1f, 0.5f, 1.0f };
		clearValues[ 1 ].depthStencil = { 0.0f, 0x00 };

		renderPassInfo.clearValueCount = static_cast<uint32_t>( clearValues.size() );
		renderPassInfo.pClearValues = clearValues.data();
		renderPassInfo.clearValueCount = 2;

		vkCmdBeginRenderPass( graphicsQueue.commandBuffers[ i ], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

		VkViewport viewport{ };
		viewport.x = view.viewport.x;
		viewport.y = view.viewport.y;
		viewport.width = view.viewport.width;
		viewport.height = view.viewport.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport( graphicsQueue.commandBuffers[ i ], 0, 1, &viewport );

		VkRect2D rect{ };
		rect.extent.width = static_cast<uint32_t>( view.viewport.width );
		rect.extent.height = static_cast<uint32_t>( view.viewport.height );
		vkCmdSetScissor( graphicsQueue.commandBuffers[ i ], 0, 1, &rect );

		for ( uint32_t pass = DRAWPASS_DEPTH; pass < DRAWPASS_POST_2D; ++pass )
		{
			for ( size_t surfIx = 0; surfIx < view.mergedModelCnt; surfIx++ )
			{
				drawSurf_t& surface = view.merged[ surfIx ];

				pipelineObject_t* pipelineObject = nullptr;
				if ( surface.pipelineObject[ pass ] == INVALID_HDL ) {
					continue;
				}
				if ( ( pass == DRAWPASS_OPAQUE ) && ( ( surface.flags & SKIP_OPAQUE ) != 0 ) ) {
					continue;
				}
				if ( ( pass == DRAWPASS_DEBUG_WIREFRAME ) && ( ( surface.flags & WIREFRAME ) == 0 ) ) {
					continue;
				}
				if ( ( pass == DRAWPASS_DEBUG_SOLID ) && ( ( surface.flags & DEBUG_SOLID ) == 0 ) ) {
					continue;
				}
				GetPipelineObject( surface.pipelineObject[ pass ], &pipelineObject );
				if ( pipelineObject == nullptr ) {
					continue;
				}

				if ( pass == DRAWPASS_DEPTH ) {
					vkCmdSetStencilReference( graphicsQueue.commandBuffers[ i ], VK_STENCIL_FACE_FRONT_BIT, surface.stencilBit );
				}

				vkCmdBindPipeline( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
				vkCmdBindDescriptorSets( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &mainPassState.descriptorSets[ i ], 0, nullptr );

				pushConstants_t pushConstants = { surface.objectId, surface.materialId };
				vkCmdPushConstants( graphicsQueue.commandBuffers[ i ], pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

				vkCmdDrawIndexed( graphicsQueue.commandBuffers[ i ], surface.indicesCnt, view.instanceCounts[ surfIx ], surface.firstIndex, surface.vertexOffset, 0 );
			}
		}
		vkCmdEndRenderPass( graphicsQueue.commandBuffers[ i ] );
	}

	// Post Process Passes
	{
		VkRenderPassBeginInfo postProcessPassInfo{ };
		postProcessPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		postProcessPassInfo.renderPass = postPassState.pass;
		postProcessPassInfo.framebuffer = postPassState.fb[ i ];
		postProcessPassInfo.renderArea.offset = { 0, 0 };
		postProcessPassInfo.renderArea.extent = swapChain.vk_swapChainExtent;

		std::array<VkClearValue, 2> postProcessClearValues{ };
		postProcessClearValues[ 0 ].color = { 0.0f, 0.1f, 0.5f, 1.0f };
		postProcessClearValues[ 1 ].depthStencil = { 0.0f, 0 };

		postProcessPassInfo.clearValueCount = static_cast<uint32_t>( postProcessClearValues.size() );
		postProcessPassInfo.pClearValues = postProcessClearValues.data();

		vkCmdBeginRenderPass( graphicsQueue.commandBuffers[ i ], &postProcessPassInfo, VK_SUBPASS_CONTENTS_INLINE );
		for ( size_t surfIx = 0; surfIx < view.mergedModelCnt; surfIx++ )
		{
			drawSurf_t& surface = shadowView.merged[ surfIx ];
			if ( surface.pipelineObject[ DRAWPASS_POST_2D ] == INVALID_HDL ) {
				continue;
			}
			pipelineObject_t* pipelineObject = nullptr;
			GetPipelineObject( surface.pipelineObject[ DRAWPASS_POST_2D ], &pipelineObject );
			if ( pipelineObject == nullptr ) {
				continue;
			}
			vkCmdBindPipeline( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
			vkCmdBindDescriptorSets( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &postPassState.descriptorSets[ i ], 0, nullptr );

			pushConstants_t pushConstants = { surface.objectId, surface.materialId };
			vkCmdPushConstants( graphicsQueue.commandBuffers[ i ], pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

			vkCmdDrawIndexed( graphicsQueue.commandBuffers[ i ], surface.indicesCnt, view.instanceCounts[ surfIx ], surface.firstIndex, surface.vertexOffset, 0 );
		}

#ifdef USE_IMGUI 
		ImGui_ImplVulkan_NewFrame();
		ImGui::NewFrame();

		DrawDebugMenu();

		// Render dear imgui into screen
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), graphicsQueue.commandBuffers[ i ] );
#endif

		vkCmdEndRenderPass( graphicsQueue.commandBuffers[ i ] );
	}


	if ( vkEndCommandBuffer( graphicsQueue.commandBuffers[ i ] ) != VK_SUCCESS )
	{
		throw std::runtime_error( "Failed to record command buffer!" );
	}
}


void Renderer::DrawDebugMenu()
{
#if defined( USE_IMGUI )
	if ( ImGui::BeginMainMenuBar() )
	{
		if ( ImGui::BeginMenu( "File" ) )
		{
			ImGui::EndMenu();
		}
		if ( ImGui::BeginMenu( "Edit" ) )
		{
			if ( ImGui::MenuItem( "Undo", "CTRL+Z" ) ) {}
			if ( ImGui::MenuItem( "Redo", "CTRL+Y", false, false ) ) {}  // Disabled item
			ImGui::Separator();
			if ( ImGui::MenuItem( "Cut", "CTRL+X" ) ) {}
			if ( ImGui::MenuItem( "Copy", "CTRL+C" ) ) {}
			if ( ImGui::MenuItem( "Paste", "CTRL+V" ) ) {}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGui::Begin( "Control Panel" );
	if ( ImGui::BeginTabBar( "Tabs" ) )
	{
		if ( ImGui::BeginTabItem( "Loading" ) )
		{
			gImguiControls.rebuildShaders = ImGui::Button( "Reload Shaders" );
			ImGui::SameLine();
			gImguiControls.rebuildRaytraceScene = ImGui::Button( "Rebuild Raytrace Scene" );
			ImGui::SameLine();
			gImguiControls.raytraceScene = ImGui::Button( "Raytrace Scene" );
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Other" ) )
		{
			ImGui::InputFloat( "Heightmap Height", &gImguiControls.heightMapHeight, 0.1f, 1.0f );
			ImGui::SliderFloat( "Roughness", &gImguiControls.roughness, 0.1f, 1.0f );
			ImGui::SliderFloat( "Shadow Strength", &gImguiControls.shadowStrength, 0.0f, 1.0f );
			ImGui::InputFloat( "Tone Map R", &gImguiControls.toneMapColor[ 0 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map G", &gImguiControls.toneMapColor[ 1 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map B", &gImguiControls.toneMapColor[ 2 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map A", &gImguiControls.toneMapColor[ 3 ], 0.1f, 1.0f );
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Scene" ) )
		{
			if( ImGui::TreeNode( "Materials" ) )
			{
				const uint32_t matCount = gAssets.materialLib.Count();
				for ( uint32_t m = 0; m < matCount; ++m )
				{		
					Material& mat = gAssets.materialLib.Find(m)->Get();
					const char* matName = gAssets.materialLib.FindName(m);

					if ( ImGui::TreeNode( matName ) )
					{
						ImGui::Text( "Kd: (%1.2f, %1.2f, %1.2f)", mat.Kd.r, mat.Kd.g, mat.Kd.b );
						ImGui::Text( "Ks: (%1.2f, %1.2f, %1.2f)", mat.Ks.r, mat.Ks.g, mat.Ks.b );
						ImGui::Text( "Ke: (%1.2f, %1.2f, %1.2f)", mat.Ke.r, mat.Ke.g, mat.Ke.b );
						ImGui::Text( "Ka: (%1.2f, %1.2f, %1.2f)", mat.Ka.r, mat.Ka.g, mat.Ka.b );
						ImGui::Text( "Ni: %1.2f", mat.Ni );
						ImGui::Text( "Tf: %1.2f", mat.Tf );
						ImGui::Text( "Tr: %1.2f", mat.Tr );
						ImGui::Text( "d: %1.2f", mat.d );
						ImGui::Text( "illum: %1.2f", mat.illum );
						ImGui::Separator();
						for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t )
						{
							hdl_t texHdl = mat.GetTexture( t );
							if( texHdl.IsValid() == false ) {
								continue;
							}
							const char* texName = gAssets.textureLib.FindName( texHdl );
							ImGui::Text( texName );
						}
						ImGui::Separator();
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			if ( ImGui::TreeNode( "Models" ) )
			{
				const uint32_t modelCount = gAssets.modelLib.Count();
				for ( uint32_t m = 0; m < modelCount; ++m )
				{
					Model& model = gAssets.modelLib.Find( m )->Get();
					const char* modelName = gAssets.modelLib.FindName( m );
					if ( ImGui::TreeNode( modelName ) )
					{
						const vec3f& min = model.bounds.GetMin();
						const vec3f& max = model.bounds.GetMax();
						ImGui::Text( "Bounds: [(%4.3f, %4.3f, %4.3f), (%4.3f, %4.3f, %4.3f)]", min[0], min[1], min[2], max[ 0 ], max[ 1 ], max[ 2 ] );
						ImGui::Text( "%u", model.surfCount );
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			if ( ImGui::TreeNode( "Textures" ) )
			{
				const uint32_t texCount = gAssets.textureLib.Count();
				for ( uint32_t t = 0; t < texCount; ++t )
				{
					Texture& texture = gAssets.textureLib.Find( t )->Get();
					const char* texName = gAssets.textureLib.FindName( t );
					if ( ImGui::TreeNode( texName ) )
					{
						ImGui::Text("%u", texture.info.channels);
						ImGui::Text("%ux%u", texture.info.width, texture.info.height );
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			if ( ImGui::TreeNode( "Shaders" ) )
			{
				const uint32_t shaderCount = gAssets.gpuPrograms.Count();
				for ( uint32_t s = 0; s < shaderCount; ++s )
				{
					GpuProgram& shader = gAssets.gpuPrograms.Find( s )->Get();
					const char* shaderName = gAssets.gpuPrograms.FindName( s );
					ImGui::Text( shaderName );
				}
				ImGui::TreePop();
			}
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Outliner" ) )
		{
			static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

			if ( ImGui::BeginTable( "3ways", 3, flags ) )
			{
				// The first column will use the default _WidthStretch when ScrollX is Off and _WidthFixed when ScrollX is On
				ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthFixed, 50.0f );
				ImGui::TableSetupColumn( "Size", ImGuiTableColumnFlags_WidthFixed, 200.0f );
				ImGui::TableSetupColumn( "Type", ImGuiTableColumnFlags_WidthFixed, 200.0f );
				ImGui::TableHeadersRow();

				//const uint32_t entCount = scene.entities.size();
				//for ( uint32_t i = 0; i < entCount; ++i )
				//{
				//	Entity* ent = scene.entities[ i ];

				//	std::stringstream numberStream;
				//	numberStream << i;
				//	ImGui::Text( numberStream.str().c_str() );
				//	ImGui::NextColumn();

				//	ImGui::Text( ent->dbgName.c_str() );
				//	ImGui::NextColumn();
				//}

				struct EntityTreeNode
				{
					const char*	Name;
					const char*	Type;
					int			Size;
					int			ChildIdx;
					int			ChildCount;
					static void DisplayNode( const EntityTreeNode* node, const EntityTreeNode* all_nodes )
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						const bool is_folder = ( node->ChildCount > 0 );
						if ( is_folder )
						{
							bool open = ImGui::TreeNodeEx( node->Name, ImGuiTreeNodeFlags_SpanFullWidth );
							ImGui::TableNextColumn();
							ImGui::TextDisabled( "--" );
							ImGui::TableNextColumn();
							ImGui::TextUnformatted( node->Type );
							if ( open )
							{
								for ( int child_n = 0; child_n < node->ChildCount; child_n++ )
									DisplayNode( &all_nodes[ node->ChildIdx + child_n ], all_nodes );
								ImGui::TreePop();
							}
						}
						else
						{
							ImGui::TreeNodeEx( node->Name, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth );
							ImGui::TableNextColumn();
							ImGui::Text( "%d", node->Size );
							ImGui::TableNextColumn();
							ImGui::TextUnformatted( node->Type );
						}
					}
				};
				static const EntityTreeNode nodes[] =
				{
					{ "Root",                         "Folder",       -1,       1, 3    }, // 0
					{ "Music",                        "Folder",       -1,       4, 2    }, // 1
					{ "Textures",                     "Folder",       -1,       6, 3    }, // 2
					{ "desktop.ini",                  "System file",  1024,    -1,-1    }, // 3
					{ "File1_a.wav",                  "Audio file",   123000,  -1,-1    }, // 4
					{ "File1_b.wav",                  "Audio file",   456000,  -1,-1    }, // 5
					{ "Image001.png",                 "Image file",   203128,  -1,-1    }, // 6
					{ "Copy of Image001.png",         "Image file",   203256,  -1,-1    }, // 7
					{ "Copy of Image001 (Final2).png","Image file",   203512,  -1,-1    }, // 8
				};

				EntityTreeNode::DisplayNode( &nodes[ 0 ], nodes );

				ImGui::EndTable();
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::Separator();

	ImGui::InputInt( "Image Id", &gImguiControls.dbgImageId );
	ImGui::Text( "Mouse: (%f, %f)", (float)gWindow.input.GetMouse().x, (float)gWindow.input.GetMouse().y );
	ImGui::Text( "Mouse Dt: (%f, %f)", (float)gWindow.input.GetMouse().dx, (float)gWindow.input.GetMouse().dy );
	const vec4f cameraOrigin = gScene->camera.GetOrigin();
	ImGui::Text( "Camera: (%f, %f, %f)", cameraOrigin[ 0 ], cameraOrigin[ 1 ], cameraOrigin[ 2 ] );
	const vec2f ndc = gWindow.GetNdc( gWindow.input.GetMouse().x, gWindow.input.GetMouse().y );

	char entityName[ 256 ];
	if ( gImguiControls.selectedEntityId >= 0 ) {
		sprintf_s( entityName, "%i: %s", gImguiControls.selectedEntityId, gAssets.modelLib.FindName( gScene->entities[ gImguiControls.selectedEntityId ]->modelHdl ) );
	}
	else {
		memset( &entityName[ 0 ], 0, 256 );
	}
	static vec3f tempOrigin;
	ImGui::Text( "NDC: (%f, %f )", (float)ndc[ 0 ], (float)ndc[ 1 ] );

	if ( gImguiControls.selectedEntityId >= 0 ) {
		Entity* entity = gScene->FindEntity( (uint32_t)gImguiControls.selectedEntityId );
		entity->SetOrigin( vec3f( tempOrigin[ 0 ] + gImguiControls.selectedModelOrigin[ 0 ],
			tempOrigin[ 1 ] + gImguiControls.selectedModelOrigin[ 1 ],
			tempOrigin[ 2 ] + gImguiControls.selectedModelOrigin[ 2 ] ) );
	}

	ImGui::Text( "Frame Number: %d", frameNumber );
	ImGui::SameLine();
	ImGui::Text( "FPS: %f", 1000.0f / renderTime );
	//ImGui::Text( "Model %i: %s", 0, models[ 0 ].name.c_str() );
	ImGui::End();
#endif
}