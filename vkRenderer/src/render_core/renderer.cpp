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
#include <numeric>
#include <map>
#include "renderer.h"
#include <scene/scene.h>
#include <scene/entity.h>
#include <sstream>
#include "debugMenu.h"
#include "gpuImage.h"
#include "../render_state/rhi.h"

#include <io/io.h>
static RtView rtview;
static RtScene rtScene;

extern Scene* gScene;

static VkDescriptorBufferInfo	vk_globalConstantsInfo;
static VkDescriptorBufferInfo	vk_viewUbo;
static VkDescriptorBufferInfo	vk_surfaceBuffer[ MaxViews ];
static VkDescriptorImageInfo	vk_image2DInfo[ MaxImageDescriptors ];
static VkDescriptorImageInfo	vk_shadowImageInfo[ MaxImageDescriptors ];
static VkDescriptorImageInfo	vk_codeImageInfo[ MaxCodeImages ];
static VkDescriptorImageInfo	vk_shadowCodeImageInfo[ MaxCodeImages ];
static VkDescriptorImageInfo	vk_imageCubeInfo[ MaxImageDescriptors ];
static VkDescriptorImageInfo	vk_postImageInfo[ MaxPostImageDescriptors ];
static VkDescriptorBufferInfo	vk_materialBufferInfo;
static VkDescriptorBufferInfo	vk_lightBufferInfo;

static void BuildRayTraceScene( Scene* scene )
{
	rtScene.scene = scene;
	rtScene.assets = &gAssets;

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

	const uint32_t texCount = gAssets.textureLib.Count();
	for ( uint32_t i = 0; i < texCount; ++i )
	{
		Asset<Texture>* texAsset = gAssets.textureLib.Find( i );
		Texture& texture = texAsset->Get();

		texture.cpuImage.Init( texture.info.width, texture.info.height );

		for ( uint32_t py = 0; py < texture.info.height; ++py ) {
			for ( uint32_t px = 0; px < texture.info.width; ++px ) {
				RGBA rgba;
				rgba.r = texture.bytes[ ( py * texture.info.width + px ) * 4 + 0 ];
				rgba.g = texture.bytes[ ( py * texture.info.width + px ) * 4 + 1 ];
				rgba.b = texture.bytes[ ( py * texture.info.width + px ) * 4 + 2 ];
				rgba.a = texture.bytes[ ( py * texture.info.width + px ) * 4 + 3 ];
				texture.cpuImage.SetPixel( px, py, rgba );
			}
		}
	}
}


static void TraceScene( const bool rasterize = false )
{
	gImguiControls.raytraceScene = false;

	rtview.targetSize[ 0 ] = 320;
	rtview.targetSize[ 1 ] = 180;
	//gWindow.GetWindowSize( rtview.targetSize[0], rtview.targetSize[1] );
	Image<Color> rtimage( rtview.targetSize[ 0 ], rtview.targetSize[ 1 ], Color::Black, "testRayTrace" );
	{
		rtview.camera = rtScene.scene->camera;
		rtview.viewTransform = rtview.camera.GetViewMatrix();
		rtview.projTransform = rtview.camera.GetPerspectiveMatrix();
		rtview.projView = rtview.projTransform * rtview.viewTransform;
	}

	if ( rasterize ) {
		RasterScene( rtimage, rtview, rtScene, true );		
	} else {
		TraceScene( rtview, rtScene, rtimage );
	}

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


void Renderer::Commit( const Scene* scene )
{
	// TODO: the scene needs to filter into views

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
		CommitModel( shadowView, *scene->entities[i], 0 );
	}
	MergeSurfaces( shadowView );

	view2D.committedModelCnt = 0;
	for ( uint32_t i = 0; i < entCount; ++i )
	{
		CommitModel( view2D, *scene->entities[ i ], 0 );
	}
	MergeSurfaces( view2D );

	UpdateViews( scene );
}


void Renderer::MergeSurfaces( RenderView& view )
{
	// Sort Surfaces
	{
		class Comparator
		{
		private:
			RenderView* view;
		public:
			Comparator( RenderView* view ) : view( view ) {}
			bool operator()( const uint32_t ix0, const uint32_t ix1 )
			{
				const auto lhs = view->surfaces[ ix0 ].sortKey.key;
				const auto rhs = view->surfaces[ ix1 ].sortKey.key;
				return ( lhs < rhs );
			}
		};

		std::vector<uint32_t> sortIndices( view.committedModelCnt );
		std::iota( sortIndices.begin(), sortIndices.end(), 0 );

		std::sort( sortIndices.begin(), sortIndices.end(), Comparator( &view ) );

		for( uint32_t srcIndex = 0; srcIndex < view.committedModelCnt; ++srcIndex )
		{
			const uint32_t dstIndex = sortIndices[ srcIndex ];
			view.sortedSurfaces[ dstIndex ] = view.surfaces[ srcIndex ];
			view.sortedInstances[ dstIndex ] = view.instances[ srcIndex ];
		}
	}

	// Merge Surfaces
	{
		view.mergedModelCnt = 0;
		std::unordered_map< uint32_t, uint32_t > uniqueSurfs;
		uniqueSurfs.reserve( view.committedModelCnt );
		for ( uint32_t i = 0; i < view.committedModelCnt; ++i ) {
			drawSurfInstance_t& instance = view.sortedInstances[ i ];
			auto it = uniqueSurfs.find( view.sortedSurfaces[ i ].hash );
			if ( it == uniqueSurfs.end() ) {
				const uint32_t surfId = view.mergedModelCnt;
				uniqueSurfs[ view.sortedSurfaces[ i ].hash ] = surfId;

				view.instanceCounts[ surfId ] = 1;
				view.merged[ surfId ] = view.sortedSurfaces[ i ];

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

		hdl_t materialHdl = ent.materialHdl.IsValid() ? ent.materialHdl : source.surfs[ i ].materialHdl;
		const Asset<Material>* materialAsset = gAssets.materialLib.Find( materialHdl );
		const Material& material = materialAsset->Get();

		renderFlags_t renderFlags = NONE;
		renderFlags = static_cast<renderFlags_t>( renderFlags | ( ent.HasFlag( ENT_FLAG_NO_DRAW ) ? HIDDEN : NONE ) );
		renderFlags = static_cast<renderFlags_t>( renderFlags | ( ent.HasFlag( ENT_FLAG_NO_SHADOWS ) ? NO_SHADOWS : NONE ) );
		renderFlags = static_cast<renderFlags_t>( renderFlags | ( ent.HasFlag( ENT_FLAG_WIREFRAME ) ? WIREFRAME | SKIP_OPAQUE : NONE ) );
		renderFlags = static_cast<renderFlags_t>( renderFlags | ( ent.HasFlag( ENT_FLAG_DEBUG ) ? DEBUG_SOLID | SKIP_OPAQUE : NONE ) );

		// TODO: Make InRegion() function
		if( view.region == renderViewRegion_t::SHADOW )
		{
			if( material.GetShader( DRAWPASS_SHADOW ) == INVALID_HDL ) {
				continue;
			}
			if ( ( renderFlags & SKIP_OPAQUE ) != 0 ) {
				continue;
			}
		}
		else if ( view.region == renderViewRegion_t::POST )
		{
			if ( material.GetShader( DRAWPASS_POST_2D ) == INVALID_HDL ) {
				continue;
			}
		}
		else if ( view.region == renderViewRegion_t::MAIN )
		{
			const drawPass_t mainPasses[] = {	DRAWPASS_DEPTH,
												DRAWPASS_TERRAIN,
												DRAWPASS_OPAQUE,
												DRAWPASS_SKYBOX,
												DRAWPASS_TRANS,
												DRAWPASS_DEBUG_SOLID,
												DRAWPASS_DEBUG_WIREFRAME
											};
			const uint32_t passCount = COUNTARRAY( mainPasses );

			uint32_t passId = 0;
			for( passId = 0; passId < passCount; ++passId ) {
				if ( material.GetShader( mainPasses[ passId ] ) != INVALID_HDL ) {
					break;
				}
			}
			if( passId >= passCount ) {
				continue;
			}
		}

		instance.modelMatrix = ent.GetMatrix();
		instance.surfId = 0;
		instance.id = 0;
		surf.vertexCount = upload.vertexCount;
		surf.vertexOffset = upload.vertexOffset;
		surf.firstIndex = upload.firstIndex;
		surf.indicesCnt = upload.indexCount;
		surf.sortKey.materialId = material.uploadId;
		surf.objectId = objectOffset;
		surf.flags = renderFlags;
		surf.stencilBit = ent.outline ? 0x01 : 0;
		surf.hash = Hash( surf );
		surf.dbgName = materialAsset->GetName().c_str();

		if( material.dirty ) {
			uploadMaterials.insert( materialHdl );
		}

		for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t ) {
			const hdl_t texHandle = material.GetTexture( t );
			if ( texHandle.IsValid() ) {
				Texture& texture = gAssets.textureLib.Find( texHandle )->Get();
				if( texture.uploadId < 0 ) {
					uploadTextures.insert( texHandle );
				}
				if ( texture.dirty ) {
					updateTextures.insert( texHandle );
				}
			}
		}

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


void Renderer::InitGPU()
{
	InitShaderResources();
	RecreateSwapChain();
}


void Renderer::ShutdownGPU()
{
	FlushGPU();
	ShutdownShaderResources();

	memset( &vk_globalConstantsInfo, 0, sizeof( VkDescriptorBufferInfo ) );
	memset( &vk_viewUbo, 0, sizeof( VkDescriptorBufferInfo ) );
	memset( &vk_surfaceBuffer, 0, sizeof( VkDescriptorBufferInfo ) );
	memset( &vk_image2DInfo[0], 0, sizeof( VkDescriptorBufferInfo ) * MaxImageDescriptors );
	memset( &vk_shadowImageInfo[0], 0, sizeof( VkDescriptorBufferInfo ) * MaxImageDescriptors );
	memset( &vk_codeImageInfo[0], 0, sizeof( VkDescriptorBufferInfo ) * MaxCodeImages );
	memset( &vk_shadowCodeImageInfo[0], 0, sizeof( VkDescriptorBufferInfo ) * MaxCodeImages );
	memset( &vk_imageCubeInfo[0], 0, sizeof( VkDescriptorBufferInfo ) * MaxImageDescriptors );
	memset( &vk_postImageInfo[0], 0, sizeof( VkDescriptorBufferInfo ) * MaxPostImageDescriptors );
	memset( &vk_materialBufferInfo, 0, sizeof( VkDescriptorBufferInfo ) );
	memset( &vk_lightBufferInfo, 0, sizeof( VkDescriptorBufferInfo ) );

	imageFreeSlot = 0;
	materialFreeSlot = 0;
	vbBufElements = 0;
	ibBufElements = 0;
}


void Renderer::UploadAssets()
{
	const uint32_t materialCount = gAssets.materialLib.Count();
	for ( uint32_t i = 0; i < materialCount; ++i )
	{
		Asset<Material>* materialAsset = gAssets.materialLib.Find( i );
		if ( materialAsset->IsLoaded() == false ) {
			continue;
		}
		Material& material = materialAsset->Get();
		if ( material.uploadId != -1 ) {
			continue;
		}
		uploadMaterials.insert( materialAsset->Handle() );
	}

	const uint32_t textureCount = gAssets.textureLib.Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		Asset<Texture>* textureAsset = gAssets.textureLib.Find( i );
		if ( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Texture& texture = textureAsset->Get();
		if ( texture.uploadId != -1 ) {
			continue;
		}
		uploadTextures.insert( textureAsset->Handle() );
	}

	UploadTextures();
	UploadModelsToGPU();
	UpdateGpuMaterials();
	UpdateDescriptorSets();
}


void Renderer::RenderScene( Scene* scene )
{
	frameTimer.Start();

	UploadModelsToGPU(); // TODO: Can this be moved after commit?

	Commit( scene );

	UploadTextures();
	UpdateTextures();
	UpdateGpuMaterials();

	SubmitFrame();

	frameTimer.Stop();
	renderTime = static_cast<float>( frameTimer.GetElapsed() );

	if( gImguiControls.rebuildRaytraceScene ) {
		BuildRayTraceScene( scene );
	}

	if ( gImguiControls.raytraceScene ) {
		TraceScene( false );
	}

	if ( gImguiControls.rasterizeScene ) {
		TraceScene( true );
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


textureSamples_t Renderer::GetSampleCountForDrawPass( const drawPass_t pass )
{
	uint64_t stateBits = 0;
	if ( ( pass == DRAWPASS_SHADOW ) || ( pass == DRAWPASS_POST_2D ) ) {
		return textureSamples_t::TEXTURE_SMP_1;
	} else {
		return config.mainColorSubSamples;
	}
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


void Renderer::WaitForEndFrame()
{
	vkWaitForFences( context.device, 1, &graphicsQueue.inFlightFences[ frameId ], VK_TRUE, UINT64_MAX );
}


void Renderer::FlushGPU()
{
	for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
		vkWaitForFences( context.device, 1, &graphicsQueue.inFlightFences[ i ], VK_TRUE, UINT64_MAX );
	}
}


void Renderer::Dispatch( VkCommandBuffer commandBuffer, GpuProgram& prog, ShaderParmSet& parms, VkDescriptorSet descSet, const uint32_t x, const uint32_t y, const uint32_t z )
{
	pipelineObject_t* pipelineObject = nullptr;
	GetPipelineObject( prog.pipeline, &pipelineObject );
	if ( pipelineObject != nullptr )
	{
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineObject->pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineObject->pipelineLayout, 0, 1, &descSet, 0, 0 );

		vkCmdDispatch( commandBuffer, x, y, z );
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

	// Compute
	{
		vkResetCommandBuffer( computeQueue.commandBuffers[ bufferId ], 0 );

		VkCommandBufferBeginInfo beginInfo{ };
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if ( vkBeginCommandBuffer( computeQueue.commandBuffers[ bufferId ], &beginInfo ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to begin recording command buffer!" );
		}

		GpuProgram& prog = gAssets.gpuPrograms.Find( "ClearParticles" )->Get();

		Dispatch( computeQueue.commandBuffers[ bufferId ], prog, particleShaderParms, particleState.descriptorSets[ bufferId ], MaxParticles / 256 );

		if ( vkEndCommandBuffer( computeQueue.commandBuffers[ bufferId ] ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to record command buffer!" );
		}
	}

	RenderViews();

	// Compute queue submit
	{
		VkSubmitInfo submitInfo{ };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &computeQueue.commandBuffers[ bufferId ];

		if ( vkQueueSubmit( context.computeQueue, 1, &submitInfo, VK_NULL_HANDLE ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to submit compute command buffers!" );
		}
	}

	// Graphics queue submit
	{
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
			throw std::runtime_error( "Failed to submit draw command buffers!" );
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
	}

	frameId = ( frameId + 1 ) % MAX_FRAMES_IN_FLIGHT;
	++frameNumber;
}


void Renderer::UpdateViews( const Scene* scene )
{
	int width;
	int height;
	gWindow.GetWindowSize( width, height );

	// Main view
	{
		renderView.viewport.width = static_cast<float>( width );
		renderView.viewport.height = static_cast<float>( height );
		renderView.viewMatrix = scene->camera.GetViewMatrix();
		renderView.projMatrix = scene->camera.GetPerspectiveMatrix();
		renderView.viewprojMatrix = renderView.projMatrix * renderView.viewMatrix;

		renderView.numLights = static_cast<uint32_t>( scene->lights.size() );
		for ( uint32_t i = 0; i < renderView.numLights; ++i ) {
			renderView.lights[ i ] = scene->lights[ i ];
		}
	}

	// Shadow views
	{
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
		shadowView.viewMatrix = shadowView.viewMatrix;
		const vec4f shadowLightPos = shadowView.viewMatrix * renderView.lights[ 0 ].lightPos;
		shadowView.viewMatrix[ 3 ][ 0 ] = -shadowLightPos[ 0 ];
		shadowView.viewMatrix[ 3 ][ 1 ] = -shadowLightPos[ 1 ];
		shadowView.viewMatrix[ 3 ][ 2 ] = -shadowLightPos[ 2 ];

		Camera shadowCam;
		shadowCam = Camera( vec4f( 0.0f, 0.0f, 0.0f, 0.0f ) );
		shadowCam.near = shadowNearPlane;
		shadowCam.far = shadowFarPlane;
		shadowCam.focalLength = shadowCam.far;
		shadowCam.SetFov( Radians( 90.0f ) );
		shadowCam.SetAspectRatio( ( ShadowMapWidth / (float)ShadowMapHeight ) );
		shadowView.projMatrix = shadowCam.GetPerspectiveMatrix( false );
		shadowView.viewprojMatrix = shadowView.projMatrix * shadowView.viewMatrix;
	}

	// Post view
	{
		view2D.viewport.x = 0.0f;
		view2D.viewport.y = 0.0f;
		view2D.viewport.near = 0.0f;
		view2D.viewport.far = 1.0f;
		view2D.viewport.width = static_cast<float>( width );
		view2D.viewport.height = static_cast<float>( height );
		view2D.viewMatrix = mat4x4f( 1.0f );
		view2D.projMatrix = mat4x4f( 1.0f );
		shadowView.viewprojMatrix = mat4x4f( 1.0f );
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

	{
		vk_globalConstantsInfo.buffer = frameState[ i ].globalConstants.GetVkObject();
		vk_globalConstantsInfo.offset = 0;
		vk_globalConstantsInfo.range = sizeof( globalUboConstants_t );
	}

	vk_viewUbo.buffer = frameState[ i ].viewParms.GetVkObject();
	vk_viewUbo.offset = 0;
	vk_viewUbo.range = MaxViews * sizeof( viewBufferObject_t );

	for ( uint32_t v = 0; v < MaxViews; ++v )
	{
		const VkDeviceSize size = MaxSurfaces * sizeof( uniformBufferObject_t );
		vk_surfaceBuffer[v].buffer = frameState[ i ].surfParms.GetVkObject();
		vk_surfaceBuffer[v].offset = v * size;
		vk_surfaceBuffer[v].range = size;
	}

	int firstCube = -1;
	const uint32_t textureCount = gAssets.textureLib.Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		Texture& texture = gAssets.textureLib.Find( i )->Get();
		if( texture.uploadId == -1 ) {
			continue;
		}

		VkImageView& imageView = texture.gpuImage->vk_view;
		VkDescriptorImageInfo info{ };
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = imageView;
		info.sampler = vk_bilinearSampler;

		if ( texture.info.type == TEXTURE_TYPE_CUBE ) {
			vk_imageCubeInfo[ texture.uploadId ] = info;
			firstCube = firstCube == -1 ? texture.uploadId : firstCube;

			VkDescriptorImageInfo info2d{ };
			info2d.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info2d.imageView = gAssets.textureLib.GetDefault()->Get().gpuImage->vk_view;
			info2d.sampler = vk_bilinearSampler;
			vk_image2DInfo[ texture.uploadId ] = info2d;
		}
		else 
		{
			vk_image2DInfo[ texture.uploadId ] = info;
		}
	}
	// Defaults
	{
		const Texture& default2DTexture = gAssets.textureLib.GetDefault()->Get();
		for ( size_t j = textureCount; j < MaxImageDescriptors; ++j )
		{
			const VkImageView& imageView = default2DTexture.gpuImage->vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			vk_image2DInfo[j] = info;
		}
		assert( firstCube >= 0 ); // Hack: need a default
		for ( size_t i = 0; i < MaxImageDescriptors; ++i )
		{
			if( vk_imageCubeInfo[i].imageView == nullptr ) {
				const VkImageView& imageView = vk_imageCubeInfo[ firstCube ].imageView;
				VkDescriptorImageInfo info{ };
				info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				info.imageView = imageView;
				info.sampler = vk_bilinearSampler;
				vk_imageCubeInfo[i] = info;
			}
		}
	}

	{
		VkDescriptorBufferInfo info{ };
		info.buffer = frameState[ i ].materialBuffers.GetVkObject();
		info.offset = 0;
		info.range = VK_WHOLE_SIZE;
		vk_materialBufferInfo = info;
	}

	{
		VkDescriptorBufferInfo info{ };
		info.buffer = frameState[ i ].lightParms.GetVkObject();
		info.offset = 0;
		info.range = VK_WHOLE_SIZE;
		vk_lightBufferInfo = info;
	}

	const uint32_t descriptorSetCnt = 9;

	{
		// Shadow Map
		for ( int j = 0; j < MaxCodeImages; ++j )
		{
			VkImageView& imageView = frameState[ currentImage ].shadowMapImage.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_depthShadowSampler;
			vk_codeImageInfo[j] = info;
		}

		std::array<VkWriteDescriptorSet, descriptorSetCnt> descriptorWrites{ };

		uint32_t descriptorId = 0;
		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 0;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = 1;
		descriptorWrites[ descriptorId ].pBufferInfo = &vk_globalConstantsInfo;
		++descriptorId;

		if( vk_viewUbo.range > 0 )
		{
			descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
			descriptorWrites[ descriptorId ].dstBinding = 1;
			descriptorWrites[ descriptorId ].dstArrayElement = 0;
			descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[ descriptorId ].descriptorCount = 1;
			descriptorWrites[ descriptorId ].pBufferInfo = &vk_viewUbo;
			++descriptorId;
		}

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 2;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = 1;
		descriptorWrites[ descriptorId ].pBufferInfo = &vk_surfaceBuffer[0];
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 3;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
		descriptorWrites[ descriptorId ].pImageInfo = &vk_image2DInfo[ 0 ];
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 4;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
		descriptorWrites[ descriptorId ].pImageInfo = &vk_imageCubeInfo[ 0 ];
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 5;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = 1;
		descriptorWrites[ descriptorId ].pBufferInfo = &vk_materialBufferInfo;
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 6;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = 1;
		descriptorWrites[ descriptorId ].pBufferInfo = &vk_lightBufferInfo;
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 7;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxCodeImages;
		descriptorWrites[ descriptorId ].pImageInfo = &vk_codeImageInfo[ 0 ];
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 8;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[ descriptorId ].descriptorCount = 1;
		descriptorWrites[ descriptorId ].pImageInfo = &vk_codeImageInfo[ 0 ];
		++descriptorId;

		assert( descriptorId <= descriptorSetCnt );

		vkUpdateDescriptorSets( context.device, descriptorId, descriptorWrites.data(), 0, nullptr );
	}

	//////////////////////////////////////////////////////
	//													//
	// Shadow Descriptor Sets							//
	//													//
	//////////////////////////////////////////////////////
	{
		for ( uint32_t i = 0; i < textureCount; ++i )
		{
			Texture& texture = gAssets.textureLib.Find( i )->Get();
			VkImageView& imageView = texture.gpuImage->vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			vk_shadowImageInfo[i] = info;
		}
		// Defaults
		for ( size_t j = textureCount; j < MaxImageDescriptors; ++j )
		{
			const Texture& texture = gAssets.textureLib.GetDefault()->Get();
			const VkImageView& imageView = texture.gpuImage->vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			vk_shadowImageInfo[j] = info;
		}

		for ( size_t j = 0; j < MaxCodeImages; ++j )
		{
			const Texture& texture = gAssets.textureLib.GetDefault()->Get();
			const VkImageView& imageView = texture.gpuImage->vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			vk_shadowCodeImageInfo[j] = info;
		}

		std::array<VkWriteDescriptorSet, descriptorSetCnt> shadowDescriptorWrites{ };

		uint32_t descriptorId = 0;
		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 0;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = 1;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &vk_globalConstantsInfo;
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 1;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = 1;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &vk_viewUbo;
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 2;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = 1;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &vk_surfaceBuffer[1];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 3;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
		shadowDescriptorWrites[ descriptorId ].pImageInfo = &vk_shadowImageInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 4;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
		shadowDescriptorWrites[ descriptorId ].pImageInfo = &vk_shadowImageInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 5;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = 1;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &vk_materialBufferInfo;
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 6;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = 1;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &vk_lightBufferInfo;
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 7;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxCodeImages;
		shadowDescriptorWrites[ descriptorId ].pImageInfo = &vk_shadowCodeImageInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 8;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = 1;
		shadowDescriptorWrites[ descriptorId ].pImageInfo = &vk_shadowCodeImageInfo[ 0 ];
		++descriptorId;

		assert( descriptorId == descriptorSetCnt );
		vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( shadowDescriptorWrites.size() ), shadowDescriptorWrites.data(), 0, nullptr );
	}

	//////////////////////////////////////////////////////
	//													//
	// Post Descriptor Sets								//
	//													//
	//////////////////////////////////////////////////////
	{
		// View Color Map
		{
			VkImageView& imageView = frameState[ currentImage ].viewColorImage.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			vk_postImageInfo[0] = info;
		}

		// View Depth Map
		{
			VkImageView& imageView = frameState[ currentImage ].depthImage.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			vk_postImageInfo[ 1 ] = info;
		}

		// View Stencil Map
		{
			VkImageView& imageView = frameState[ currentImage ].stencilImage.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			vk_postImageInfo[ 2 ] = info;
		}

		std::array<VkWriteDescriptorSet, 9> postDescriptorWrites{ };

		uint32_t descriptorId = 0;
		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 0;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = 1;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &vk_globalConstantsInfo;
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 1;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = 1;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &vk_viewUbo;
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 1;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = 1;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &vk_surfaceBuffer[0];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 3;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
		postDescriptorWrites[ descriptorId ].pImageInfo = &vk_image2DInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 4;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
		postDescriptorWrites[ descriptorId ].pImageInfo = &vk_imageCubeInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 5;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = 1;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &vk_materialBufferInfo;
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 6;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = 1;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &vk_lightBufferInfo;
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 7;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxCodeImages;
		postDescriptorWrites[ descriptorId ].pImageInfo = &vk_postImageInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 8;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		postDescriptorWrites[ descriptorId ].descriptorCount = 1;
		postDescriptorWrites[ descriptorId ].pImageInfo = &vk_postImageInfo[ 2 ];
		++descriptorId;

		vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( postDescriptorWrites.size() ), postDescriptorWrites.data(), 0, nullptr );
	}

	//////////////////////////////////////////////////////
	//													//
	// Compute											//
	//													//
	//////////////////////////////////////////////////////
	{
		//particleShader.Bind( globalsBuffer, frameState[ i ].globalConstants );

		//vk_globalConstantsInfo.buffer = frameState[ i ].globalConstants.GetVkObject();
		//vk_globalConstantsInfo.offset = 0;
		//vk_globalConstantsInfo.range = sizeof( globalUboConstants_t );

		std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

		VkDescriptorBufferInfo storageBufferInfoCurrentFrame{};
		{		
			storageBufferInfoCurrentFrame.buffer = frameState[ currentImage ].particleBuffer.GetVkObject();
			storageBufferInfoCurrentFrame.offset = 0;
			storageBufferInfoCurrentFrame.range = frameState[ currentImage ].particleBuffer.GetMaxSize();
		}

		uint32_t descriptorId = 0;
		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = particleState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 0;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = 1;
		descriptorWrites[ descriptorId ].pBufferInfo = &vk_globalConstantsInfo;
		++descriptorId;

		if( storageBufferInfoCurrentFrame.range > 0 )
		{
			descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[ descriptorId ].dstSet = particleState.descriptorSets[ i ];
			descriptorWrites[ descriptorId ].dstBinding = 1;
			descriptorWrites[ descriptorId ].dstArrayElement = 0;
			descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[ descriptorId ].descriptorCount = 1;
			descriptorWrites[ descriptorId ].pBufferInfo = &storageBufferInfoCurrentFrame;
			++descriptorId;
		}

		vkUpdateDescriptorSets( context.device, descriptorId, descriptorWrites.data(), 0, nullptr );
	}
}


void Renderer::UpdateBufferContents( uint32_t currentImage )
{
	static globalUboConstants_t globalsBuffer;
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
		globals.numSamples = vk_GetSampleCount( config.mainColorSubSamples );
		globals.numLights = renderView.numLights;
		globalsBuffer = globals;
	}

	static viewBufferObject_t viewBuffer[MaxViews];
	{
		viewBuffer[ int(renderView.region) ].view = renderView.viewMatrix;
		viewBuffer[ int( renderView.region ) ].proj = renderView.projMatrix;

		viewBuffer[ int( shadowView.region ) ].view = shadowView.viewMatrix;
		viewBuffer[ int( shadowView.region ) ].proj = shadowView.projMatrix;

		viewBuffer[ int( view2D.region ) ].view = view2D.viewMatrix;
		viewBuffer[ int( view2D.region ) ].proj = view2D.projMatrix;

		for( uint32_t i = 3; i < MaxViews; ++i )
		{
			viewBuffer[i].view = mat4x4f( 1.0f );
			viewBuffer[i].proj = mat4x4f( 1.0f );
		}
	}

	static uniformBufferObject_t uboBuffer[ MaxSurfaces ];
	assert( renderView.committedModelCnt < MaxSurfaces );
	for ( uint32_t i = 0; i < renderView.committedModelCnt; ++i )
	{
		uniformBufferObject_t ubo;
		ubo.model = renderView.sortedInstances[ i ].modelMatrix;
		const drawSurf_t& surf = renderView.merged[ renderView.sortedInstances[ i ].surfId ];
		const uint32_t objectId = ( renderView.sortedInstances[ i ].id + surf.objectId );
		uboBuffer[ objectId ] = ubo;
	}

	static uniformBufferObject_t shadowUboBuffer[ MaxSurfaces ];
	assert( shadowView.committedModelCnt < MaxSurfaces );
	for ( uint32_t i = 0; i < shadowView.committedModelCnt; ++i )
	{
		uniformBufferObject_t ubo;
		ubo.model = shadowView.sortedInstances[ i ].modelMatrix;
		const drawSurf_t& surf = shadowView.merged[ shadowView.sortedInstances[ i ].surfId ];
		const uint32_t objectId = ( shadowView.sortedInstances[ i ].id + surf.objectId );
		shadowUboBuffer[ objectId ] = ubo;
	}

	static uniformBufferObject_t postUboBuffer[ MaxSurfaces ];
	assert( view2D.committedModelCnt < MaxSurfaces );
	for ( uint32_t i = 0; i < shadowView.committedModelCnt; ++i )
	{
		uniformBufferObject_t ubo;
		ubo.model = view2D.sortedInstances[ i ].modelMatrix;
		const drawSurf_t& surf = view2D.merged[ view2D.sortedInstances[ i ].surfId ];
		const uint32_t objectId = ( view2D.sortedInstances[ i ].id + surf.objectId );
		postUboBuffer[ objectId ] = ubo;
	}

	static lightBufferObject_t lightBuffer[ MaxLights ];
	for ( int i = 0; i < MaxLights; ++i )
	{
		// TODO: this should be all committed lights for all views eventually
		lightBufferObject_t light;
		light.intensity = renderView.lights[ i ].intensity;
		light.lightDir = renderView.lights[ i ].lightDir;
		light.lightPos = renderView.lights[ i ].lightPos;
		light.shadowViewId = uint32_t(shadowView.region);
		lightBuffer[i] = light;
	}

	frameState[ currentImage ].globalConstants.SetPos();
	frameState[ currentImage ].globalConstants.CopyData( &globalsBuffer, sizeof( globalUboConstants_t ) );

	frameState[ currentImage ].viewParms.SetPos();
	frameState[ currentImage ].viewParms.CopyData( &viewBuffer, sizeof( viewBufferObject_t ) * MaxViews );

	frameState[ currentImage ].surfParms.SetPos();
	frameState[ currentImage ].surfParms.CopyData( uboBuffer, sizeof( uniformBufferObject_t ) * MaxSurfaces );
	frameState[ currentImage ].surfParms.CopyData( shadowUboBuffer, sizeof( uniformBufferObject_t ) * MaxSurfaces );
	frameState[ currentImage ].surfParms.CopyData( postUboBuffer, sizeof( uniformBufferObject_t ) * MaxSurfaces );

	frameState[ currentImage ].materialBuffers.SetPos();
	frameState[ currentImage ].materialBuffers.CopyData( materialBuffer, sizeof( materialBufferObject_t ) * materialFreeSlot );

	frameState[ currentImage ].lightParms.SetPos();
	frameState[ currentImage ].lightParms.CopyData( lightBuffer, sizeof( lightBufferObject_t ) * MaxLights );
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
			vkGetPhysicalDeviceProperties( device, &context.deviceProperties );
			context.physicalDevice = device;
			break;
		}
	}

	if ( context.physicalDevice == VK_NULL_HANDLE ) {
		throw std::runtime_error( "Failed to find a suitable GPU!" );
	}

	config.mainColorSubSamples = GetMaxUsableSampleCount();
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


void Renderer::SetupMarkers()
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties( context.physicalDevice, nullptr, &extensionCount, nullptr );

	std::vector<VkExtensionProperties> availableExtensions( extensionCount );
	vkEnumerateDeviceExtensionProperties( context.physicalDevice, nullptr, &extensionCount, availableExtensions.data() );

	bool found = false;
	for ( const auto& extension : availableExtensions ) {
		if ( strcmp( extension.extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME ) == 0 ) {
			found = true;
			break;
		}
	}

	if( found )
	{
		std::cout << "Enabling debug markers." << std::endl;

		vk_fnDebugMarkerSetObjectTag = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr( context.device, "vkDebugMarkerSetObjectTagEXT" );
		vk_fnDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr( context.device, "vkDebugMarkerSetObjectNameEXT" );
		vk_fnCmdDebugMarkerBegin = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr( context.device, "vkCmdDebugMarkerBeginEXT" );
		vk_fnCmdDebugMarkerEnd = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr( context.device, "vkCmdDebugMarkerEndEXT" );
		vk_fnCmdDebugMarkerInsert = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr( context.device, "vkCmdDebugMarkerInsertEXT" );

		debugMarkersEnabled = ( vk_fnDebugMarkerSetObjectName != VK_NULL_HANDLE );
	} else {
		std::cout << "Debug markers \"" << VK_EXT_DEBUG_MARKER_EXTENSION_NAME << "\" disabled.";
	}
}


void Renderer::MarkerSetObjectName( uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name )
{
	if ( debugMarkersEnabled )
	{
		VkDebugMarkerObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = objectType;
		nameInfo.object = object;
		nameInfo.pObjectName = name;
		vk_fnDebugMarkerSetObjectName( context.device, &nameInfo );
	}
}


void Renderer::MarkerSetObjectTag( uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag )
{
	if ( debugMarkersEnabled )
	{
		VkDebugMarkerObjectTagInfoEXT tagInfo = {};
		tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
		tagInfo.objectType = objectType;
		tagInfo.object = object;
		tagInfo.tagName = name;
		tagInfo.tagSize = tagSize;
		tagInfo.pTag = tag;
		vk_fnDebugMarkerSetObjectTag( context.device, &tagInfo );
	}
}


void Renderer::MarkerBeginRegion( VkCommandBuffer cmdbuffer, const char* pMarkerName, const vec4f color )
{
	if ( debugMarkersEnabled )
	{
		VkDebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		markerInfo.color[0] = color[0];
		markerInfo.color[1] = color[1];
		markerInfo.color[2] = color[2];
		markerInfo.color[3] = color[3];
		markerInfo.pMarkerName = pMarkerName;
		vk_fnCmdDebugMarkerBegin( cmdbuffer, &markerInfo );	
	}
}


void Renderer::MarkerEndRegion( VkCommandBuffer cmdBuffer )
{
	if ( debugMarkersEnabled )
	{
		vk_fnCmdDebugMarkerEnd( cmdBuffer );
	}
}


void Renderer::MarkerInsert( VkCommandBuffer cmdbuffer, std::string markerName, const vec4f color )
{
	if ( debugMarkersEnabled )
	{
		VkDebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		memcpy( markerInfo.color, &color[ 0 ], sizeof( float ) * 4 );
		markerInfo.pMarkerName = markerName.c_str();
		vk_fnCmdDebugMarkerInsert( cmdbuffer, &markerInfo );
	}
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


static const char* GetPassDebugName( const drawPass_t pass )
{
	switch( pass )
	{
		case DRAWPASS_SHADOW:			return "Shadow Pass";
		case DRAWPASS_DEPTH:			return "Depth Pass";
		case DRAWPASS_TERRAIN:			return "Terrain Pass";
		case DRAWPASS_OPAQUE:			return "Opaque Pass";
		case DRAWPASS_SKYBOX:			return "Skybox Pass";
		case DRAWPASS_TRANS:			return "Trans Pass";
		case DRAWPASS_DEBUG_SOLID:		return "Debug Solid Pass";
		case DRAWPASS_DEBUG_WIREFRAME:	return "Wireframe Pass";
		case DRAWPASS_POST_2D:			return "2D Pass";
	};
	return "";
}


const DrawPassState* Renderer::GetPassState( const drawPass_t pass )
{
	switch( pass )
	{
		case DRAWPASS_DEPTH:
		case DRAWPASS_TERRAIN:
		case DRAWPASS_OPAQUE:
		case DRAWPASS_TRANS:
		case DRAWPASS_SKYBOX:
		case DRAWPASS_DEBUG_SOLID:
		case DRAWPASS_DEBUG_WIREFRAME:	return &mainPassState;
		case DRAWPASS_SHADOW:			return &shadowPassState;
		case DRAWPASS_POST_2D:			return &postPassState;
	}
	return nullptr;
}


bool Renderer::SkipPass( const drawSurf_t& surf, const drawPass_t pass )
{
	if ( surf.pipelineObject[ pass ] == INVALID_HDL ) {
		return true;
	}

	if ( ( surf.flags & SKIP_OPAQUE ) != 0 )
	{
		if( ( pass == DRAWPASS_SHADOW ) ||
			( pass == DRAWPASS_DEPTH ) ||
			( pass == DRAWPASS_TERRAIN ) ||
			( pass == DRAWPASS_OPAQUE ) ||
			( pass == DRAWPASS_SKYBOX ) ||
			( pass == DRAWPASS_DEBUG_SOLID )
		) {
			return true;
		}
	}

	if ( ( pass == DRAWPASS_DEBUG_SOLID ) && ( ( surf.flags & DEBUG_SOLID ) == 0 ) ) {
		return true;
	}

	if ( ( pass == DRAWPASS_DEBUG_WIREFRAME ) && ( ( surf.flags & WIREFRAME ) == 0 ) ) {
		return true;
	}

	return false;
}


drawPass_t Renderer::ViewRegionPassBegin( const renderViewRegion_t region )
{
	switch( region )
	{
		case renderViewRegion_t::SHADOW:	return DRAWPASS_SHADOW_BEGIN;
		case renderViewRegion_t::MAIN:		return DRAWPASS_MAIN_BEGIN;
		case renderViewRegion_t::POST:		return DRAWPASS_POST_BEGIN;
	}
	return DRAWPASS_COUNT;
}


drawPass_t Renderer::ViewRegionPassEnd( const renderViewRegion_t region )
{
	switch ( region )
	{
		case renderViewRegion_t::SHADOW:	return DRAWPASS_SHADOW_END;
		case renderViewRegion_t::MAIN:		return DRAWPASS_MAIN_END;
		case renderViewRegion_t::POST:		return DRAWPASS_POST_END;
	}
	return DRAWPASS_COUNT;
}


void Renderer::RenderViewSurfaces( RenderView& view, VkCommandBuffer commandBuffer )
{
	const drawPass_t passBegin = ViewRegionPassBegin( view.region );
	const drawPass_t passEnd = ViewRegionPassEnd( view.region );

	// For now the pass state is the same for the entire view region
	const DrawPassState* passState = GetPassState( passBegin );
	if ( passState == nullptr ) {
		throw std::runtime_error( "Missing pass state!" );
	}

	VkRenderPassBeginInfo passInfo{ };
	passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passInfo.renderPass = passState->pass;
	passInfo.framebuffer = passState->fb->buffer[ bufferId ];
	passInfo.renderArea.offset = { passState->x, passState->y };
	passInfo.renderArea.extent = { passState->width, passState->height };

	std::array<VkClearValue, 2> clearValues{ };
	clearValues[ 0 ].color = { passState->clearColor[ 0 ], passState->clearColor[ 1 ], passState->clearColor[ 2 ], passState->clearColor[ 3 ] };
	clearValues[ 1 ].depthStencil = { passState->clearDepth, passState->clearStencil };

	passInfo.clearValueCount = static_cast<uint32_t>( clearValues.size() );
	passInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass( commandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE );

	// Clear
	{
		std::array<VkClearAttachment, 1> attachment{ };
		attachment[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		attachment[0].clearValue.color = { passState->clearColor[ 0 ], passState->clearColor[ 1 ], passState->clearColor[ 2 ], passState->clearColor[ 3 ] };

		//attachment[1].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		//attachment[1].clearValue.depthStencil = { passState->clearDepth, passState->clearStencil };

		std::array<VkClearRect, 1> rects = {};
		rects[0].layerCount = 1;
		rects[0].rect.offset = { passState->x, passState->y };
		rects[0].rect.extent = { passState->width, passState->height };
		//rects[1].layerCount = 1;
		//rects[1].rect.offset = { passState->x, passState->y };
		//rects[1].rect.extent = { passState->width, passState->height };

		vkCmdClearAttachments( commandBuffer, static_cast<uint32_t>( attachment.size() ), attachment.data(), static_cast<uint32_t>( rects.size() ), rects.data() );
	}

	VkViewport viewport{ };
	viewport.x = view.viewport.x;
	viewport.y = view.viewport.y;
	viewport.width = view.viewport.width;
	viewport.height = view.viewport.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport( commandBuffer, 0, 1, &viewport );

	VkRect2D rect{ };
	rect.extent.width = static_cast<uint32_t>( view.viewport.width );
	rect.extent.height = static_cast<uint32_t>( view.viewport.height );
	vkCmdSetScissor( commandBuffer, 0, 1, &rect );

	for ( uint32_t pass = passBegin; pass <= passEnd; ++pass )
	{
		sortKey_t lastKey = {};
		lastKey.materialId = INVALID_HDL.Get();

		MarkerBeginRegion( commandBuffer, GetPassDebugName( drawPass_t( pass ) ), ColorToVector( Color::White ) );
		for ( size_t surfIx = 0; surfIx < view.mergedModelCnt; surfIx++ )
		{
			drawSurf_t& surface = view.merged[ surfIx ];	

			if ( SkipPass( surface, drawPass_t( pass ) ) ) {
				continue;
			}

			pipelineObject_t* pipelineObject = nullptr;
			GetPipelineObject( surface.pipelineObject[ pass ], &pipelineObject );
			if ( pipelineObject == nullptr ) {
				continue;
			}

			if ( pass == DRAWPASS_DEPTH ) {
				// vkCmdSetDepthBias
				vkCmdSetStencilReference( commandBuffer, VK_STENCIL_FACE_FRONT_BIT, surface.stencilBit );
			}

			vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
			vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &passState->descriptorSets[bufferId], 0, nullptr );
			lastKey = surface.sortKey;

			pushConstants_t pushConstants = { surface.objectId, surface.sortKey.materialId, uint32_t( view.region ) };
			vkCmdPushConstants( commandBuffer, pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

			MarkerInsert( commandBuffer, surface.dbgName, ColorToVector( Color::LGrey ) );
			vkCmdDrawIndexed( commandBuffer, surface.indicesCnt, view.instanceCounts[ surfIx ], surface.firstIndex, surface.vertexOffset, 0 );
		}
		MarkerEndRegion( commandBuffer );
	}

	if( view.region == renderViewRegion_t::POST )
	{
#ifdef USE_IMGUI
		MarkerBeginRegion( commandBuffer, "Debug Menus", ColorToVector( Color::White ) );
		ImGui_ImplVulkan_NewFrame();
		ImGui::NewFrame();

		DrawDebugMenu();

		// Render dear imgui into screen
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), commandBuffer );
		MarkerEndRegion( commandBuffer );
#endif
	}

	vkCmdEndRenderPass( commandBuffer );
}


void Renderer::RenderViews()
{
	const uint32_t i = bufferId;
	vkResetCommandBuffer( graphicsQueue.commandBuffers[ i ], 0 );

	VkCommandBufferBeginInfo beginInfo{ };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	if ( vkBeginCommandBuffer( graphicsQueue.commandBuffers[ i ], &beginInfo ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to begin recording command buffer!" );
	}

	VkBuffer vertexBuffers[] = { vb.GetVkObject() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers( graphicsQueue.commandBuffers[ i ], 0, 1, vertexBuffers, offsets );
	vkCmdBindIndexBuffer( graphicsQueue.commandBuffers[ i ], ib.GetVkObject(), 0, VK_INDEX_TYPE_UINT32 );

	// Shadow View
	{
		MarkerBeginRegion( graphicsQueue.commandBuffers[ i ], shadowView.name, ColorToVector( Color::White ) );

		RenderViewSurfaces( shadowView, graphicsQueue.commandBuffers[ i ] );

		MarkerEndRegion( graphicsQueue.commandBuffers[ i ] );
	}

	// Main View
	{
		MarkerBeginRegion( graphicsQueue.commandBuffers[ i ], renderView.name, ColorToVector( Color::White ) );

		RenderViewSurfaces( renderView, graphicsQueue.commandBuffers[ i ] );
		
		MarkerEndRegion( graphicsQueue.commandBuffers[ i ] );
	}

	// 2D View
	{
		MarkerBeginRegion( graphicsQueue.commandBuffers[ i ], view2D.name, ColorToVector( Color::White ) );

		RenderViewSurfaces( view2D, graphicsQueue.commandBuffers[ i ] );
		
		MarkerEndRegion( graphicsQueue.commandBuffers[ i ] );
	}


	if ( vkEndCommandBuffer( graphicsQueue.commandBuffers[ i ] ) != VK_SUCCESS )
	{
		throw std::runtime_error( "Failed to record command buffer!" );
	}
}


void Renderer::DrawDebugMenu()
{
#if defined( USE_IMGUI )
	static ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

	if ( ImGui::BeginMainMenuBar() )
	{
		if ( ImGui::BeginMenu( "File" ) )
		{
			if ( ImGui::MenuItem( "Open Scene", "CTRL+O" ) ) {
				gImguiControls.openSceneFileDialog = true;
			}
			if ( ImGui::MenuItem( "Reload", "CTRL+R" ) ) {
				gImguiControls.reloadScene = true;
			}
			if ( ImGui::MenuItem( "Import Obj", "CTRL+I" ) ) {
				gImguiControls.openModelImportFileDialog = true;
			}
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
		if ( ImGui::BeginTabItem( "Debug" ) )
		{
			gImguiControls.rebuildShaders = ImGui::Button( "Reload Shaders" );
			gImguiControls.rebuildRaytraceScene = ImGui::Button( "Rebuild Raytrace Scene" );
			ImGui::SameLine();
			gImguiControls.raytraceScene = ImGui::Button( "Raytrace Scene" );
			ImGui::SameLine();
			gImguiControls.rasterizeScene = ImGui::Button( "Rasterize Scene" );

			ImGui::InputFloat( "Heightmap Height", &gImguiControls.heightMapHeight, 0.1f, 1.0f );
			ImGui::SliderFloat( "Roughness", &gImguiControls.roughness, 0.1f, 1.0f );
			ImGui::SliderFloat( "Shadow Strength", &gImguiControls.shadowStrength, 0.0f, 1.0f );
			ImGui::InputFloat( "Tone Map R", &gImguiControls.toneMapColor[ 0 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map G", &gImguiControls.toneMapColor[ 1 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map B", &gImguiControls.toneMapColor[ 2 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map A", &gImguiControls.toneMapColor[ 3 ], 0.1f, 1.0f );
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Device" ) )
		{
			DebugMenuDeviceProperties( context.deviceProperties );
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Assets" ) )
		{
			const uint32_t matCount = gAssets.materialLib.Count();
			if( ImGui::TreeNode( "Materials", "Materials (%i)", matCount ) )
			{
				for ( uint32_t m = 0; m < matCount; ++m )
				{
					Asset<Material>* matAsset = gAssets.materialLib.Find( m );
					Material& mat = matAsset->Get();
					const char* matName = gAssets.materialLib.FindName(m);

					if ( ImGui::TreeNode( matAsset->GetName().c_str() ) )
					{
						DebugMenuMaterialEdit( matAsset );
						ImGui::Separator();
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			const uint32_t modelCount = gAssets.modelLib.Count();
			if ( ImGui::TreeNode( "Models", "Models (%i)", modelCount ) )
			{
				for ( uint32_t m = 0; m < modelCount; ++m )
				{
					Asset<Model>* modelAsset = gAssets.modelLib.Find( m );
					DebugMenuModelTreeNode( modelAsset );
				}
				ImGui::TreePop();
			}
			const uint32_t texCount = gAssets.textureLib.Count();
			if ( ImGui::TreeNode( "Textures", "Textures (%i)", texCount ) )
			{
				for ( uint32_t t = 0; t < texCount; ++t )
				{
					Asset<Texture>* texAsset = gAssets.textureLib.Find( t );
					DebugMenuTextureTreeNode( texAsset );
				}
				ImGui::TreePop();
			}
			const uint32_t shaderCount = gAssets.gpuPrograms.Count();
			if ( ImGui::TreeNode( "Shaders", "Shaders (%i)", shaderCount ) )
			{
				for ( uint32_t s = 0; s < shaderCount; ++s )
				{
					Asset<GpuProgram>* shaderAsset = gAssets.gpuPrograms.Find( s );
					GpuProgram& shader = shaderAsset->Get();
					const char* shaderName = shaderAsset->GetName().c_str();
					ImGui::Text( shaderName );
				}
				ImGui::TreePop();
			}
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Manip" ) )
		{
			static uint32_t currentIdx = 0;
			Entity* ent = gScene->FindEntity( currentIdx );
			const char* previewValue = ent->name.c_str();
			if ( ImGui::BeginCombo( "Entity", previewValue ) )
			{
				const uint32_t modelCount = gAssets.modelLib.Count();
				for ( uint32_t e = 0; e < gScene->EntityCount(); ++e )
				{
					Entity* comboEnt = gScene->FindEntity( e );

					const bool selected = ( currentIdx == e );
					if ( ImGui::Selectable( comboEnt->name.c_str(), selected ) ) {
						currentIdx = e;
					}

					if ( selected ) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			if ( ent != nullptr )
			{
				const vec3f o = ent->GetOrigin();
				const vec3f s = ent->GetScale();
				const mat4x4f r = ent->GetRotation();
				float origin[3] = { o[0], o[1], o[2] };
				ImGui::PushItemWidth( 100 );
				ImGui::Text( "Origin" );
				ImGui::SameLine();
				ImGui::InputFloat( "##OriginX", &origin[0], 0.1f, 1.0f );
				ImGui::SameLine();
				ImGui::InputFloat( "##OriginY", &origin[1], 0.1f, 1.0f );
				ImGui::SameLine();
				ImGui::InputFloat( "##OriginZ", &origin[2], 0.1f, 1.0f );

				float scale[ 3 ] = { s[0], s[1], s[2] };
				ImGui::Text( "Scale" );
				ImGui::SameLine();
				ImGui::InputFloat( "##ScaleX", &scale[ 0 ], 0.1f, 1.0f );
				ImGui::SameLine();
				ImGui::InputFloat( "##ScaleY", &scale[ 1 ], 0.1f, 1.0f );
				ImGui::SameLine();
				ImGui::InputFloat( "##ScaleZ", &scale[ 2 ], 0.1f, 1.0f );

				float rotation[ 3 ] = { 0.0f, 0.0f, 0.0f };
				MatrixToEulerZYX( r, rotation[0], rotation[1], rotation[2] );

				ImGui::Text( "Rotation" );
				ImGui::SameLine();
				ImGui::InputFloat( "##RotationX", &rotation[ 0 ], 1.0f, 10.0f );
				ImGui::SameLine();
				ImGui::InputFloat( "##RotationY", &rotation[ 1 ], 1.0f, 10.0f );
				ImGui::SameLine();
				ImGui::InputFloat( "##RotationZ", &rotation[ 2 ], 1.0f, 10.0f );
				ImGui::PopItemWidth();

				ent->SetOrigin( origin );
				ent->SetScale( scale );
				ent->SetRotation( rotation );

				if( ImGui::Button( "Add Bounds" ) )
				{
					AABB bounds = ent->GetBounds();
					const vec3f X = bounds.GetMax()[0] - bounds.GetMin()[0];
					const vec3f Y = bounds.GetMax()[1] - bounds.GetMin()[1];
					const vec3f Z = bounds.GetMax()[2] - bounds.GetMin()[2];

					//mat4x4f m = CreateMatrix4x4( )

					Entity* boundEnt = new Entity( *ent );
					boundEnt->name = ent->name + "_bounds";
					boundEnt->SetFlag( ENT_FLAG_WIREFRAME );
					boundEnt->materialHdl = gAssets.materialLib.RetrieveHdl( "DEBUG_WIRE" );

					gScene->entities.push_back( boundEnt );
					gScene->CreateEntityBounds( gAssets.modelLib.RetrieveHdl( "cube" ), *boundEnt );
				}

				ImGui::SameLine();
				if( ImGui::Button( "Export Model" ) )
				{
					Asset<Model>* asset = gAssets.modelLib.Find( ent->modelHdl );
					WriteModel( asset, BakePath + asset->GetName() + BakedModelExtension );
				}
				ImGui::SameLine();
				bool hidden = ent->HasFlag( ENT_FLAG_NO_DRAW );
				if( ImGui::Checkbox( "Hide", &hidden ) )
				{
					if( hidden ) {
						ent->SetFlag( ENT_FLAG_NO_DRAW );
					} else {
						ent->ClearFlag( ENT_FLAG_NO_DRAW );
					}
				}
				ImGui::SameLine();
				bool wireframe = ent->HasFlag( ENT_FLAG_WIREFRAME );
				if ( ImGui::Checkbox( "Wireframe", &wireframe ) )
				{
					if ( wireframe ) {
						ent->SetFlag( ENT_FLAG_WIREFRAME );
					} else {
						ent->ClearFlag( ENT_FLAG_WIREFRAME );
					}
				}
			}
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Create Entity" ) )
		{
			static char name[ 128 ] = {};
			static uint32_t currentIdx = 0;
			const char* previewValue = gAssets.modelLib.FindName( currentIdx );
			if ( ImGui::BeginCombo( "Model", previewValue ) )
			{
				const uint32_t modelCount = gAssets.modelLib.Count();
				for ( uint32_t m = 0; m < modelCount; ++m )
				{
					Asset<Model>* modelAsset = gAssets.modelLib.Find( m );

					const bool selected = ( currentIdx == m );
					if ( ImGui::Selectable( modelAsset->GetName().c_str(), selected ) ) {
						currentIdx = m;
					}

					if ( selected ) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::InputText( "Name", name, 128 );

			if( ImGui::Button("Create") )
			{
				Entity* ent = new Entity();
				ent->name = name;
				ent->SetFlag( ENT_FLAG_DEBUG );
				gScene->entities.push_back( ent );
				gScene->CreateEntityBounds( gAssets.modelLib.RetrieveHdl( gAssets.modelLib.FindName( currentIdx ) ), *ent );
			}

			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Outliner" ) )
		{
			static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

			DebugMenuLightEdit( gScene );

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
	gImguiControls.dbgImageId = Clamp( gImguiControls.dbgImageId, -1, int(gAssets.textureLib.Count() - 1) );

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

	ImGui::Text( "NDC: (%f, %f )", (float)ndc[ 0 ], (float)ndc[ 1 ] );
	ImGui::Text( "Frame Number: %d", frameNumber );
	ImGui::SameLine();
	ImGui::Text( "FPS: %f", 1000.0f / renderTime );
	//ImGui::Text( "Model %i: %s", 0, models[ 0 ].name.c_str() );
	ImGui::End();
#endif
}