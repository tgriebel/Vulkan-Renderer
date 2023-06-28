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
#include "renderer.h"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <map>
#include <sstream>

#include <gfxcore/scene/scene.h>
#include <gfxcore/scene/entity.h>
#include <gfxcore/core/assetLib.h>
#include <gfxcore/primitives/geom.h>
#include <gfxcore/asset_types/texture.h>
#include <gfxcore/asset_types/gpuProgram.h>
#include <gfxcore/scene/scene.h>
#include <gfxcore/scene/entity.h>

#include "debugMenu.h"
#include "gpuImage.h"
#include "swapChain.h"

#include "../render_state/rhi.h"
#include "../render_state/deviceContext.h"
#include "../render_state/FrameState.h"
#include "../render_binding/bindings.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/shaderBinding.h"
#include "../globals/render_util.h"

#include "../../GeoBuilder.h"
#include "../../window.h"
#include "../../input.h"
#include "../io/io.h"

#include <gfxcore/io/io.h>

#if defined( USE_IMGUI )
#include "../../external/imgui/backends/imgui_impl_vulkan.h"
#endif

extern Scene* g_scene;

SwapChain g_swapChain;

#if defined( USE_IMGUI )
static ImGui_ImplVulkanH_Window imguiMainWindowData;
#endif

void Renderer::Commit( const Scene* scene )
{
	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	for ( uint32_t viewIx = 0; viewIx < MaxViews; ++viewIx )
	{
		RenderView& view = views[ viewIx ];
		if( view.IsCommitted() == false ) {
			continue;
		}
		view.committedModelCnt = 0;
		for ( uint32_t entIx = 0; entIx < entCount; ++entIx ) {
			CommitModel( view, *scene->entities[ entIx ] );
		}
		MergeSurfaces( view );
	}
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


void Renderer::CommitModel( RenderView& view, const Entity& ent )
{
	if ( ent.HasFlag( ENT_FLAG_NO_DRAW ) ) {
		return;
	}

	assert( DRAWPASS_COUNT <= Material::MaxMaterialShaders );

	Asset<Model>* modelAsset = g_assets.modelLib.Find( ent.modelHdl );
	Model& model = modelAsset->Get();

	for ( uint32_t i = 0; i < model.surfCount; ++i )
	{
		drawSurfInstance_t& instance = view.instances[ view.committedModelCnt ];
		drawSurf_t& surf = view.surfaces[ view.committedModelCnt ];

		if( model.uploadId == -1 )
		{
			model.uploadId = surfUploads.Count();
			surfUploads.Grow( model.surfCount );
		}

		hdl_t materialHdl = ent.materialHdl.IsValid() ? ent.materialHdl : model.surfs[ i ].materialHdl;
		const Asset<Material>* materialAsset = g_assets.materialLib.Find( materialHdl );
		const Material& material = materialAsset->Get();

		renderFlags_t renderFlags = NONE;
		renderFlags = static_cast<renderFlags_t>( renderFlags | ( ent.HasFlag( ENT_FLAG_NO_DRAW ) ? HIDDEN : NONE ) );
		renderFlags = static_cast<renderFlags_t>( renderFlags | ( ent.HasFlag( ENT_FLAG_NO_SHADOWS ) ? NO_SHADOWS : NONE ) );
		renderFlags = static_cast<renderFlags_t>( renderFlags | ( ent.HasFlag( ENT_FLAG_WIREFRAME ) ? WIREFRAME | SKIP_OPAQUE : NONE ) );
		renderFlags = static_cast<renderFlags_t>( renderFlags | ( ent.HasFlag( ENT_FLAG_DEBUG ) ? DEBUG_SOLID | SKIP_OPAQUE : NONE ) );

		// TODO: Make InRegion() function
		if( view.GetRegion() == renderViewRegion_t::SHADOW )
		{
			if( material.GetShader( DRAWPASS_SHADOW ) == INVALID_HDL ) {
				continue;
			}
			if ( ent.HasFlag( ENT_FLAG_NO_SHADOWS ) ) {
				continue;
			}
			if ( ( renderFlags & SKIP_OPAQUE ) != 0 ) {
				continue;
			}
		}
		else if ( view.GetRegion() == renderViewRegion_t::POST )
		{
			if ( material.GetShader( DRAWPASS_POST_2D ) == INVALID_HDL ) {
				continue;
			}
		}
		else if ( view.GetRegion() == renderViewRegion_t::STANDARD_RASTER )
		{
			const drawPass_t mainPasses[] = {	DRAWPASS_DEPTH,
												DRAWPASS_TERRAIN,
												DRAWPASS_OPAQUE,
												DRAWPASS_SKYBOX,
												DRAWPASS_TRANS,
												DRAWPASS_EMISSIVE,
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
		surf.uploadId = ( model.uploadId + i );
		surf.sortKey.materialId = material.uploadId;
		surf.objectId = 0;
		surf.flags = renderFlags;
		surf.stencilBit = ent.outline ? 0x01 : 0;
		surf.hash = Hash( surf );
		surf.dbgName = materialAsset->GetName().c_str();

		if( materialAsset->IsUploaded() == false ) {
			uploadMaterials.insert( materialHdl );
		}

		for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t ) {
			const hdl_t texHandle = material.GetTexture( t );
			if ( texHandle.IsValid() ) {
				Asset<Image>* imageAsset = g_assets.textureLib.Find( texHandle );
				Image& image = imageAsset->Get();
				if( image.gpuImage->GetId() < 0 ) {
					uploadTextures.insert( texHandle );
				}
				if ( imageAsset->IsUploaded() == false ) {
					updateTextures.insert( texHandle );
				}
			}
		}

		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			surf.pipelineObject[ passIx ] = INVALID_HDL;
			if ( material.GetShader( drawPass_t( passIx ) ).IsValid() == false ) {
				continue;
			}

			Asset<GpuProgram>* prog = g_assets.gpuPrograms.Find( material.GetShader( drawPass_t( passIx ) ) );
			if ( prog == nullptr ) {
				continue;
			}

			const DrawPass* pass = view.passes[ passIx ];
			if( pass == nullptr ){
				continue;
			}

			surf.pipelineObject[ passIx ] = FindPipelineObject( pass, *prog );
			assert( surf.pipelineObject[ passIx ] != INVALID_HDL );
		}

		++view.committedModelCnt;
	}
}


void Renderer::InitGPU()
{
	{
		// Memory Allocations
		sharedMemory.Create( MaxSharedMemory, memoryRegion_t::SHARED );
		localMemory.Create( MaxLocalMemory, memoryRegion_t::LOCAL );
	}

	InitShaderResources();
	RecreateSwapChain();
	UploadAssets();
}


void Renderer::ShutdownGPU()
{
	FlushGPU();
	ShutdownShaderResources();

	imageFreeSlot = 0;
	vbBufElements = 0;
	ibBufElements = 0;
}


void Renderer::RecreateSwapChain()
{
	int width = 0, height = 0;
	g_window.GetWindowFrameBufferSize( width, height, true );

	vkDeviceWaitIdle( context.device );

	DestroyFramebuffers();
	g_swapChain.Destroy();

	frameBufferMemory.Create( MaxSharedMemory, memoryRegion_t::LOCAL );

	g_swapChain.Create( &g_window, width, height );
	CreateFramebuffers();

	for ( uint32_t viewIx = 0; viewIx < viewCount; ++viewIx )
	{
		views[ viewIx ].Resize();
		views[ viewIx ].SetViewRect( 0, 0, width, height );
	}
}


void Renderer::Resize()
{
	RecreateSwapChain();
	UpdateDescriptorSets();
}


void Renderer::UploadAssets()
{
	const uint32_t materialCount = g_assets.materialLib.Count();
	for ( uint32_t i = 0; i < materialCount; ++i )
	{
		AssetInterface* materialAsset = g_assets.materialLib.Find( i );
		if ( materialAsset->IsLoaded() == false ) {
			continue;
		}
		if ( materialAsset->IsUploaded() ) {
			continue;
		}
		uploadMaterials.insert( materialAsset->Handle() );
	}

	const uint32_t textureCount = g_assets.textureLib.Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		AssetInterface* textureAsset = g_assets.textureLib.Find( i );
		if ( textureAsset->IsLoaded() == false ) {
			continue;
		}
		if ( textureAsset->IsUploaded() ) {
			continue;
		}
		uploadTextures.insert( textureAsset->Handle() );
	}

	ClearPipelineCache();
	BuildPipelines();
	UploadTextures();
	UploadModelsToGPU();
	UpdateGpuMaterials();
	UpdateDescriptorSets();
}


void Renderer::Render()
{
	frameTimer.Start();

	UploadModelsToGPU();
	UploadTextures();
	UpdateTextures();
	UpdateGpuMaterials();
	BuildPipelines();

	//UpdateDescriptorSets(); // need to allow dynamic views

	SubmitFrame();

	frameTimer.Stop();
	renderTime = static_cast<float>( frameTimer.GetElapsed() );
}


void Renderer::UpdateDescriptorSets()
{
	const uint32_t frameBufferCount = static_cast<uint32_t>( g_swapChain.GetBufferCount() );
	for ( uint32_t i = 0; i < frameBufferCount; i++ )
	{
		UpdateBuffers( i );
		UpdateBindSets( i );
		UpdateFrameDescSet(i );
	}
}


void Renderer::WaitForEndFrame()
{
	vkWaitForFences( context.device, 1, &gfxContext.inFlightFences[ m_frameId ], VK_TRUE, UINT64_MAX );

	VkResult result = vkAcquireNextImageKHR( context.device, g_swapChain.GetVkObject(), UINT64_MAX, gfxContext.imageAvailableSemaphores[ m_frameId ], VK_NULL_HANDLE, &m_bufferId );
	if ( result != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to acquire swap chain image!" );
	}

	// Check if a previous frame is using this image (i.e. there is its fence to wait on)
	if ( gfxContext.imagesInFlight[ m_bufferId ] != VK_NULL_HANDLE ) {
		vkWaitForFences( context.device, 1, &gfxContext.imagesInFlight[ m_bufferId ], VK_TRUE, UINT64_MAX );
	}
	// Mark the image as now being in use by this frame
	gfxContext.imagesInFlight[ m_bufferId ] = gfxContext.inFlightFences[ m_frameId ];
}


void Renderer::FlushGPU()
{
	for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
		vkWaitForFences( context.device, 1, &gfxContext.inFlightFences[ i ], VK_TRUE, UINT64_MAX );
	}
}


void Renderer::Dispatch( ComputeContext& computeContext, hdl_t progHdl, ShaderBindSet& bindSet, VkDescriptorSet descSet, const uint32_t x, const uint32_t y, const uint32_t z )
{
	pipelineState_t state = {};
	state.progHdl = progHdl;

	const hdl_t pipelineHdl = Hash( reinterpret_cast<const uint8_t*>( &state ), sizeof( state ) );

	pipelineObject_t* pipelineObject = nullptr;
	GetPipelineObject( pipelineHdl, &pipelineObject );

	VkCommandBuffer cmdBuffer = computeContext.commandBuffers[ m_bufferId ];

	if ( pipelineObject != nullptr )
	{
		vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineObject->pipeline );
		vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineObject->pipelineLayout, 0, 1, &descSet, 0, 0 );

		vkCmdDispatch( cmdBuffer, x, y, z );
	}
}


void Renderer::SubmitFrame()
{
	WaitForEndFrame();

	UpdateBuffers( m_bufferId );
	//UpdateFrameDescSet( m_bufferId );

	// Compute
	{
		vkResetCommandBuffer( computeContext.commandBuffers[ m_bufferId ], 0 );

		VkCommandBufferBeginInfo beginInfo{ };
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if ( vkBeginCommandBuffer( computeContext.commandBuffers[ m_bufferId ], &beginInfo ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to begin recording command buffer!" );
		}

		const hdl_t progHdl = g_assets.gpuPrograms.RetrieveHdl( "ClearParticles" );

		Dispatch( computeContext, progHdl, particleShaderBinds, particleState.parms[ m_bufferId ]->GetVkObject(), MaxParticles / 256 );

		if ( vkEndCommandBuffer( computeContext.commandBuffers[ m_bufferId ] ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to record command buffer!" );
		}
	}

	RenderViews();

	// Compute queue submit
	{
		VkSubmitInfo submitInfo{ };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &computeContext.commandBuffers[ m_bufferId ];

		if ( vkQueueSubmit( context.computeContext, 1, &submitInfo, VK_NULL_HANDLE ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to submit compute command buffers!" );
		}
	}

	// Graphics queue submit
	{
		VkSubmitInfo submitInfo{ };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { gfxContext.imageAvailableSemaphores[ m_frameId ] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &gfxContext.commandBuffers[ m_bufferId ];

		VkSemaphore signalSemaphores[] = { gfxContext.renderFinishedSemaphores[ m_frameId ] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences( context.device, 1, &gfxContext.inFlightFences[ m_frameId ] );

		if ( vkQueueSubmit( context.gfxContext, 1, &submitInfo, gfxContext.inFlightFences[ m_frameId ] ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to submit draw command buffers!" );
		}

		VkPresentInfoKHR presentInfo{ };
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { g_swapChain.GetVkObject() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &m_bufferId;
		presentInfo.pResults = nullptr; // Optional

		VkResult result = vkQueuePresentKHR( context.presentQueue, &presentInfo );
		if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR )
		{
			g_window.RequestImageResize();
			return;
		}
		else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR )
		{
			throw std::runtime_error( "Failed to acquire swap chain image!" );
		}
	}

	m_frameId = ( m_frameId + 1 ) % MAX_FRAMES_IN_FLIGHT;
	++m_frameNumber;
}


void Renderer::UpdateViews( const Scene* scene )
{
	int width;
	int height;
	g_window.GetWindowSize( width, height );

	shadowCount = 0;
	const uint32_t lightCount = static_cast<uint32_t>( scene->lights.size() );
	assert( lightCount <= MaxLights );

	lightsBuffer.Reset();

	for( uint32_t i = 0; i < lightCount; ++i )
	{
		const light_t& light = scene->lights[ i ];
		if ( ( light.flags & LIGHT_FLAGS_HIDDEN ) != 0 ) {
			continue;
		}

		lightBufferObject_t lightObject = {};
		lightObject.intensity = light.intensity * ColorToVector( light.color );
		lightObject.lightDir = light.dir;
		lightObject.lightPos = light.pos;

		if ( ( light.flags & LIGHT_FLAGS_SHADOW ) == 0 ) {
			lightObject.shadowViewId = 0xFF;
		} else
		{
			lightObject.shadowViewId = shadowCount;

			Camera shadowCam;
			shadowCam = Camera( light.pos, MatrixFromVector( light.dir.Reverse() ) );
			shadowCam.SetClip( 0.1f, 1000.0f );
			shadowCam.SetFov( Radians( 90.0f ) );
			shadowCam.SetAspectRatio( ( ShadowMapWidth / (float)ShadowMapHeight ) );

			shadowViews[ shadowCount ]->SetViewRect( 0, 0, ShadowMapWidth, ShadowMapHeight );
			shadowViews[ shadowCount ]->SetCamera( shadowCam, false );
		}	
		lightsBuffer.Append( lightObject );

		++shadowCount;
		assert( shadowCount < MaxShadowMaps );
	}


	// Main view
	{
		renderViews[ 0 ]->SetViewRect( 0, 0, width, height );
		renderViews[ 0 ]->SetCamera( scene->camera );

		renderViews[ 0 ]->numLights = lightCount;
		for ( uint32_t i = 0; i < lightCount; ++i ) {
			renderViews[ 0 ]->lights[ i ] = i;
		}
	}

	// Post view
	{
		view2Ds[ 0 ]->SetViewRect( 0, 0, width, height );
	}

	// FIXME: TEMP HACK
	activeViewCount = 0;
	for( uint32_t i = 0; i < shadowCount; ++i )
	{
		activeViews[ i ] = shadowViews[ i ];
		++activeViewCount;
	}
	activeViews[ activeViewCount++ ] = renderViews[ 0 ];
	activeViews[ activeViewCount++ ] = view2Ds[ 0 ];
}


void Renderer::UpdateBindSets( const uint32_t currentImage )
{
	const uint32_t i = currentImage;

	for ( uint32_t viewIx = 0; viewIx < MaxViews; ++viewIx )
	{
		if( views[ viewIx ].IsCommitted() == false ) {
			continue;
		}
		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			DrawPass* pass = views[ viewIx ].passes[ passIx ];
			if( pass == nullptr ) {
				continue;
			}

			pass->codeImages[ currentImage ].Resize( 3 );
			if ( passIx == DRAWPASS_SHADOW )
			{				
				pass->codeImages[ currentImage ][ 0 ] = gpuImages2D[ 0 ];
				pass->codeImages[ currentImage ][ 1 ] = gpuImages2D[ 0 ];
				pass->codeImages[ currentImage ][ 2 ] = gpuImages2D[ 0 ];
			}
			else if ( passIx == DRAWPASS_POST_2D )
			{
				pass->codeImages[ currentImage ][ 0 ] = &mainColorImage;
				pass->codeImages[ currentImage ][ 1 ] = &frameState.depthImageView;
				pass->codeImages[ currentImage ][ 2 ] = &frameState.stencilImageView;
			}
			else
			{
				pass->codeImages[ currentImage ][ 0 ] = &shadowMapImage[ 0 ];
				pass->codeImages[ currentImage ][ 1 ] = &shadowMapImage[ 1 ];
				pass->codeImages[ currentImage ][ 2 ] = &shadowMapImage[ 2 ];
			}

			pass->parms[ i ]->Bind( bind_globalsBuffer, &frameState.globalConstants );
			pass->parms[ i ]->Bind( bind_viewBuffer, &frameState.viewParms );
			pass->parms[ i ]->Bind( bind_modelBuffer, &frameState.surfParmPartitions[ views[ viewIx ].GetViewId() ] );
			pass->parms[ i ]->Bind( bind_image2DArray, &gpuImages2D );
			pass->parms[ i ]->Bind( bind_imageCubeArray, &gpuImagesCube );
			pass->parms[ i ]->Bind( bind_materialBuffer, &frameState.materialBuffers );
			pass->parms[ i ]->Bind( bind_lightBuffer, &frameState.lightParms );
			pass->parms[ i ]->Bind( bind_imageCodeArray, &pass->codeImages[ i ] );
			pass->parms[ i ]->Bind( bind_imageStencil, ( passIx == DRAWPASS_POST_2D ) ? &frameState.stencilImageView : pass->codeImages[ i ][ 0 ] );
		}
	}

	{
		particleState.parms[ i ]->Bind( bind_globalsBuffer, &frameState.globalConstants );
		particleState.parms[ i ]->Bind( bind_particleWriteBuffer, &frameState.particleBuffer );
	}
}


void Renderer::UpdateBuffers( const uint32_t currentImage )
{
	FrameState& state = frameState;

	state.globalConstants.SetPos( currentImage, 0 );
	{
		globalUboConstants_t globals = {};
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

		float intPart = 0;
		const float fracPart = modf( time, &intPart );

		globals.time = vec4f( time, intPart, fracPart, 1.0f );
		globals.generic = vec4f( g_imguiControls.heightMapHeight, g_imguiControls.roughness, 0.0f, 0.0f );
		globals.tonemap = vec4f( g_imguiControls.toneMapColor[ 0 ], g_imguiControls.toneMapColor[ 1 ], g_imguiControls.toneMapColor[ 2 ], g_imguiControls.toneMapColor[ 3 ] );
		globals.shadowParms = vec4f( 0, ShadowMapWidth, ShadowMapHeight, g_imguiControls.shadowStrength );
		globals.numSamples = vk_GetSampleCount( config.mainColorSubSamples );

		state.globalConstants.CopyData( currentImage, &globals, sizeof( globals ) );
	}

	state.viewParms.SetPos( currentImage, 0 );

	for ( uint32_t viewIx = 0; viewIx < MaxViews; ++viewIx )
	{
		const RenderView& view = views[ viewIx ];

		viewBufferObject_t viewBuffer = {};
		if( view.IsCommitted() )
		{
			const vec2i& frameSize = view.GetFrameSize();
			viewBuffer.view = view.GetViewMatrix();
			viewBuffer.proj = view.GetProjMatrix();
			viewBuffer.dimensions = vec4f( (float)frameSize[ 0 ], (float)frameSize[ 1 ], 1.0f / frameSize[ 0 ], 1.0f / frameSize[ 1 ] );
			viewBuffer.numLights = view.numLights;
		}
		state.viewParms.CopyData( currentImage, &viewBuffer, sizeof( viewBuffer ) );
	}

	for ( uint32_t viewIx = 0; viewIx < MaxViews; ++viewIx )
	{
		const RenderView& view = views[ viewIx ];
		if ( view.IsCommitted() == false ) {
			continue;
		}
		const uint32_t viewId = view.GetViewId();

		static uniformBufferObject_t uboBuffer[ MaxSurfaces ];
		assert( view.committedModelCnt < MaxSurfaces );
		for ( uint32_t surfIx = 0; surfIx < view.committedModelCnt; ++surfIx )
		{
			uniformBufferObject_t ubo;
			ubo.model = view.sortedInstances[ surfIx ].modelMatrix;
			const drawSurf_t& surf = view.merged[ view.sortedInstances[ surfIx ].surfId ];
			const uint32_t objectId = ( view.sortedInstances[ surfIx ].id + surf.objectId );
			uboBuffer[ objectId ] = ubo;
		}

		state.surfParmPartitions[ viewId ].SetPos( currentImage, 0 );
		state.surfParmPartitions[ viewId ].CopyData( currentImage, uboBuffer, sizeof( uniformBufferObject_t ) * MaxSurfaces );
	}

	state.materialBuffers.SetPos( currentImage, 0 );
	state.materialBuffers.CopyData( currentImage, materialBuffer.Ptr(), sizeof( materialBufferObject_t ) * materialBuffer.Count() );

	state.lightParms.SetPos( currentImage, 0 );
	state.lightParms.CopyData( currentImage, lightsBuffer.Ptr(), sizeof( lightBufferObject_t ) * MaxLights );

	state.particleBuffer.SetPos( currentImage, frameState.particleBuffer.GetMaxSize() );
	//state.particleBuffer.CopyData();
}


void Renderer::InitConfig()
{
	imageSamples_t samples = IMAGE_SMP_1;

#ifdef USE_VULKAN
	VkSampleCountFlags frameBufferCount = context.deviceProperties.limits.framebufferColorSampleCounts;
	VkSampleCountFlags depthBufferCount = context.deviceProperties.limits.framebufferDepthSampleCounts;
	VkSampleCountFlags counts = ( frameBufferCount & depthBufferCount );

	if ( counts & VK_SAMPLE_COUNT_64_BIT ) { samples = IMAGE_SMP_64; }
	if ( counts & VK_SAMPLE_COUNT_32_BIT ) { samples = IMAGE_SMP_32; }
	if ( counts & VK_SAMPLE_COUNT_16_BIT ) { samples = IMAGE_SMP_16; }
	if ( counts & VK_SAMPLE_COUNT_8_BIT ) { samples = IMAGE_SMP_8; }
	if ( counts & VK_SAMPLE_COUNT_4_BIT ) { samples = IMAGE_SMP_4; }
	if ( counts & VK_SAMPLE_COUNT_2_BIT ) { samples = IMAGE_SMP_2; }
#endif

	config.mainColorSubSamples = samples;
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


static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
														VkDebugUtilsMessageTypeFlagsEXT messageType,
														const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
														void* pUserData )
{

	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
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


void Renderer::MarkerBeginRegion( GfxContext& cxt, const char* pMarkerName, const vec4f& color )
{
	if ( context.debugMarkersEnabled )
	{
		VkDebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		markerInfo.color[0] = color[0];
		markerInfo.color[1] = color[1];
		markerInfo.color[2] = color[2];
		markerInfo.color[3] = color[3];
		markerInfo.pMarkerName = pMarkerName;
		context.fnCmdDebugMarkerBegin( cxt.commandBuffers[ m_bufferId ], &markerInfo );
	}
}


void Renderer::MarkerEndRegion( GfxContext& cxt )
{
	if ( context.debugMarkersEnabled )
	{
		context.fnCmdDebugMarkerEnd( cxt.commandBuffers[ m_bufferId ] );
	}
}


void Renderer::MarkerInsert( GfxContext& cxt, std::string markerName, const vec4f& color )
{
	if ( context.debugMarkersEnabled )
	{
		VkDebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		memcpy( markerInfo.color, &color[ 0 ], sizeof( float ) * 4 );
		markerInfo.pMarkerName = markerName.c_str();
		context.fnCmdDebugMarkerInsert( cxt.commandBuffers[ m_bufferId ], &markerInfo );
	}
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


void Renderer::RenderViewSurfaces( RenderView& view, GfxContext& gfxContext )
{
	const drawPass_t passBegin = view.ViewRegionPassBegin();
	const drawPass_t passEnd = view.ViewRegionPassEnd();

	// For now the pass state is the same for the entire view region
	const DrawPass* pass = view.passes[ passBegin ];
	if ( pass == nullptr ) {
		throw std::runtime_error( "Missing pass state!" );
	}

	VkRenderPassBeginInfo passInfo{ };
	passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passInfo.renderPass = pass->fb->GetVkRenderPass( pass->transitionState );
	passInfo.framebuffer = pass->fb->GetVkBuffer( pass->transitionState, m_bufferId );
	passInfo.renderArea.offset = { pass->viewport.x, pass->viewport.y };
	passInfo.renderArea.extent = { pass->viewport.width, pass->viewport.height };

	const VkClearColorValue clearColor = { pass->clearColor[ 0 ], pass->clearColor[ 1 ], pass->clearColor[ 2 ], pass->clearColor[ 3 ] };
	const VkClearDepthStencilValue clearDepth = { pass->clearDepth, pass->clearStencil };

	const uint32_t colorAttachmentsCount = pass->fb->GetColorLayers();
	const uint32_t attachmentsCount = pass->fb->GetLayers();

	passInfo.clearValueCount = 0;
	passInfo.pClearValues = nullptr;

	if( pass->transitionState.flags.clear )
	{
		std::array<VkClearValue, 5> clearValues{ };
		assert( attachmentsCount <= 5 );

		for ( uint32_t i = 0; i < colorAttachmentsCount; ++i ) {
			clearValues[ i ].color = clearColor;
		}

		for ( uint32_t i = colorAttachmentsCount; i < attachmentsCount; ++i ) {
			clearValues[ i ].depthStencil = clearDepth;
		}

		passInfo.clearValueCount = static_cast<uint32_t>( clearValues.size() );
		passInfo.pClearValues = clearValues.data();
	}

	VkCommandBuffer cmdBuffer = gfxContext.commandBuffers[ m_bufferId ];

	vkCmdBeginRenderPass( cmdBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE );

	const viewport_t& viewport = view.GetViewport();

	VkViewport vk_viewport{ };
	vk_viewport.x = static_cast<float>( viewport.x );
	vk_viewport.y = static_cast<float>( viewport.y );
	vk_viewport.width = static_cast<float>( viewport.width );
	vk_viewport.height = static_cast<float>( viewport.height );
	vk_viewport.minDepth = 0.0f;
	vk_viewport.maxDepth = 1.0f;
	vkCmdSetViewport( cmdBuffer, 0, 1, &vk_viewport );

	VkRect2D rect{ };
	rect.extent.width = viewport.width;
	rect.extent.height = viewport.height;
	vkCmdSetScissor( cmdBuffer, 0, 1, &rect );

	for ( uint32_t passIx = passBegin; passIx <= passEnd; ++passIx )
	{
		sortKey_t lastKey = {};
		lastKey.materialId = INVALID_HDL.Get();

		const DrawPass* pass = view.passes[ passIx ];
		if( pass == nullptr ) {
			continue;
		}

		MarkerBeginRegion( gfxContext, pass->name, ColorToVector( Color::White ) );

		for ( size_t surfIx = 0; surfIx < view.mergedModelCnt; surfIx++ )
		{
			drawSurf_t& surface = view.merged[ surfIx ];
			surfaceUpload_t& upload = surfUploads[ surface.uploadId ];

			if ( SkipPass( surface, drawPass_t( passIx ) ) ) {
				continue;
			}

			pipelineObject_t* pipelineObject = nullptr;
			GetPipelineObject( surface.pipelineObject[ passIx ], &pipelineObject );
			if ( pipelineObject == nullptr ) {
				continue;
			}

			MarkerInsert( gfxContext, surface.dbgName, ColorToVector( Color::LGrey ) );

			if ( passIx == DRAWPASS_DEPTH ) {
				// vkCmdSetDepthBias
				vkCmdSetStencilReference( cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, surface.stencilBit );
			}

			const uint32_t descSetCount = 1;
			VkDescriptorSet descSetArray[ descSetCount ] = { pass->parms[ m_bufferId ]->GetVkObject() };

			vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
			vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, descSetCount, descSetArray, 0, nullptr );
			lastKey = surface.sortKey;

			pushConstants_t pushConstants = { surface.objectId, surface.sortKey.materialId, uint32_t( view.GetViewId() ) };
			vkCmdPushConstants( cmdBuffer, pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

			vkCmdDrawIndexed( cmdBuffer, upload.indexCount, view.instanceCounts[ surfIx ], upload.firstIndex, upload.vertexOffset, 0 );
		}
		MarkerEndRegion( gfxContext );
	}

	if( view.GetRegion() == renderViewRegion_t::POST )
	{
#ifdef USE_IMGUI
		MarkerBeginRegion( gfxContext, "Debug Menus", ColorToVector( Color::White ) );
		ImGui_ImplVulkan_NewFrame();
		ImGui::NewFrame();

		DrawDebugMenu();

		// Render dear imgui into screen
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), cmdBuffer );
		MarkerEndRegion( gfxContext );
#endif
	}

	vkCmdEndRenderPass( cmdBuffer );
}


void Renderer::RenderViews()
{
	vkResetCommandBuffer( gfxContext.commandBuffers[ m_bufferId ], 0 );

	VkCommandBufferBeginInfo beginInfo{ };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	if ( vkBeginCommandBuffer( gfxContext.commandBuffers[ m_bufferId ], &beginInfo ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to begin recording command buffer!" );
	}

	VkBuffer vertexBuffers[] = { vb.GetVkObject( m_bufferId ) };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers( gfxContext.commandBuffers[ m_bufferId ], 0, 1, vertexBuffers, offsets );
	vkCmdBindIndexBuffer( gfxContext.commandBuffers[ m_bufferId ], ib.GetVkObject( m_bufferId ), 0, VK_INDEX_TYPE_UINT32 );

	for ( uint32_t viewIx = 0; viewIx < activeViewCount; ++viewIx )
	{
		MarkerBeginRegion( gfxContext, activeViews[ viewIx ]->GetName(), ColorToVector( Color::White ) );

		RenderViewSurfaces( *activeViews[ viewIx ], gfxContext );

		MarkerEndRegion( gfxContext );
	}

	if ( vkEndCommandBuffer( gfxContext.commandBuffers[ m_bufferId ] ) != VK_SUCCESS )
	{
		throw std::runtime_error( "Failed to record command buffer!" );
	}
}


void Renderer::AttachDebugMenu( const debugMenuFuncPtr funcPtr )
{
	debugMenus.Append( funcPtr );
}


void DeviceDebugMenu()
{
	if ( ImGui::BeginTabItem( "Device" ) )
	{
		DebugMenuDeviceProperties( context.deviceProperties );
		ImGui::EndTabItem();
	}
}


void Renderer::DrawDebugMenu()
{
	if ( ImGui::BeginMainMenuBar() )
	{
		if ( ImGui::BeginMenu( "File" ) )
		{
			if ( ImGui::MenuItem( "Open Scene", "CTRL+O" ) ) {
				g_imguiControls.openSceneFileDialog = true;
			}
			if ( ImGui::MenuItem( "Reload", "CTRL+R" ) ) {
				g_imguiControls.reloadScene = true;
			}
			if ( ImGui::MenuItem( "Import Obj", "CTRL+I" ) ) {
				g_imguiControls.openModelImportFileDialog = true;
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
		for( uint32_t i = 0; i < debugMenus.Count(); ++i ) {
			(*debugMenus[ i ])();
		}
		ImGui::EndTabBar();
	}

	ImGui::Separator();

	ImGui::InputInt( "Image Id", &g_imguiControls.dbgImageId );
	g_imguiControls.dbgImageId = Clamp( g_imguiControls.dbgImageId, -1, int( g_assets.textureLib.Count() - 1 ) );

	char entityName[ 256 ];
	if ( g_imguiControls.selectedEntityId >= 0 ) {
		sprintf_s( entityName, "%i: %s", g_imguiControls.selectedEntityId, g_assets.modelLib.FindName( g_scene->entities[ g_imguiControls.selectedEntityId ]->modelHdl ) );
	}
	else {
		memset( &entityName[ 0 ], 0, 256 );
	}

	ImGui::Text( "Mouse: (%f, %f)", (float)g_window.input.GetMouse().x, (float)g_window.input.GetMouse().y );
	ImGui::Text( "Mouse Dt: (%f, %f)", (float)g_window.input.GetMouse().dx, (float)g_window.input.GetMouse().dy );
	const vec4f cameraOrigin = g_scene->camera.GetOrigin();
	ImGui::Text( "Camera: (%f, %f, %f)", cameraOrigin[ 0 ], cameraOrigin[ 1 ], cameraOrigin[ 2 ] );

	const vec2f ndc = g_window.GetNdc( g_window.input.GetMouse().x, g_window.input.GetMouse().y );

	ImGui::Text( "NDC: (%f, %f )", (float)ndc[ 0 ], (float)ndc[ 1 ] );
	ImGui::Text( "Frame Number: %d", m_frameNumber );
	ImGui::SameLine();
	ImGui::Text( "FPS: %f", 1000.0f / renderTime );
	//ImGui::Text( "Model %i: %s", 0, models[ 0 ].name.c_str() );

	ImGui::End();
}