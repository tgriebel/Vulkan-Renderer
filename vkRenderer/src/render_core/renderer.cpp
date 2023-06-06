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

#include <raytracer/scene.h>
#include <raytracer/raytrace.h>

#include "debugMenu.h"
#include "gpuImage.h"
#include "swapChain.h"
#include "raytracerInterface.h"

#include "../render_state/rhi.h"
#include "../render_state/deviceContext.h"
#include "../render_state/FrameState.h"
#include "../render_binding/bindings.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/shaderBinding.h"

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

	Model& source = g_assets.modelLib.Find( ent.modelHdl )->Get();
	for ( uint32_t i = 0; i < source.surfCount; ++i ) {
		drawSurfInstance_t& instance = view.instances[ view.committedModelCnt ];
		drawSurf_t& surf = view.surfaces[ view.committedModelCnt ];
		surfaceUpload_t& upload = source.upload[ i ];

		hdl_t materialHdl = ent.materialHdl.IsValid() ? ent.materialHdl : source.surfs[ i ].materialHdl;
		const Asset<Material>* materialAsset = g_assets.materialLib.Find( materialHdl );
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
		else if ( view.region == renderViewRegion_t::STANDARD_RASTER )
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
				Image& texture = g_assets.textureLib.Find( texHandle )->Get();
				if( texture.uploadId < 0 ) {
					uploadTextures.insert( texHandle );
				}
				if ( texture.dirty ) {
					updateTextures.insert( texHandle );
				}
			}
		}

		for ( int passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			surf.pipelineObject[ passIx ] = INVALID_HDL;
			if ( material.GetShader( passIx ).IsValid() == false ) {
				continue;
			}

			Asset<GpuProgram>* prog = g_assets.gpuPrograms.Find( material.GetShader( passIx ) );
			if ( prog == nullptr ) {
				continue;
			}

			drawPass_t drawPass = drawPass_t( passIx );
			const DrawPass* pass = GetDrawPass( drawPass );
			assert( pass != nullptr );

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
		VkMemoryPropertyFlagBits type = VkMemoryPropertyFlagBits( VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		vk_AllocateDeviceMemory( MaxSharedMemory, type, sharedMemory );
		type = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		vk_AllocateDeviceMemory( MaxLocalMemory, type, localMemory );
	}

	InitShaderResources();
	RecreateSwapChain();
	UploadAssets();
	CreatePipelineObjects();
}


void Renderer::ShutdownGPU()
{
	FlushGPU();
	ShutdownShaderResources();

	imageFreeSlot = 0;
	materialFreeSlot = 0;
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

	vk_AllocateDeviceMemory( MaxFrameBufferMemory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameBufferMemory );
	frameBufferMemory.Reset();

	g_swapChain.Create( &g_window, width, height );
	CreateFramebuffers();

	RenderView* views[ 3 ] = { &shadowView, &renderView, &view2D }; // FIXME: TEMP!!

	for ( uint32_t viewIx = 0; viewIx < 3; ++viewIx ) {
		views[ viewIx ]->Resize();
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
		Asset<Material>* materialAsset = g_assets.materialLib.Find( i );
		if ( materialAsset->IsLoaded() == false ) {
			continue;
		}
		Material& material = materialAsset->Get();
		if ( material.uploadId != -1 ) {
			continue;
		}
		uploadMaterials.insert( materialAsset->Handle() );
	}

	const uint32_t textureCount = g_assets.textureLib.Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		Asset<Image>* textureAsset = g_assets.textureLib.Find( i );
		if ( textureAsset->IsLoaded() == false ) {
			continue;
		}
		Image& texture = textureAsset->Get();
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


void Renderer::RenderScene( const Scene* scene )
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

	if( g_imguiControls.rebuildRaytraceScene ) {
		BuildRayTraceScene( scene );
	}

	if ( g_imguiControls.raytraceScene ) {
		TraceScene( false );
	}

	if ( g_imguiControls.rasterizeScene ) {
		TraceScene( true );
	}

	localMemory.Pack();
	sharedMemory.Pack();
}


DrawPass* Renderer::GetDrawPass( const drawPass_t pass )
{
	if ( pass == DRAWPASS_SHADOW ) {
		return shadowView.passes[ pass ];
	} else if ( pass == DRAWPASS_POST_2D ) {
		return view2D.passes[ pass ];
	} else {
		return renderView.passes[ pass ];
	}
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

	UpdateBuffers( m_bufferId );

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

		result = vkQueuePresentKHR( context.presentQueue, &presentInfo );

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

	// Main view
	{
		renderView.SetViewport( viewport_t( 0, 0, width, height, 0.0f, 1.0f ) );
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
		shadowView.SetViewport( viewport_t( 0, 0, ShadowMapWidth, ShadowMapHeight, 0.0f, 1.0f ) );
		shadowView.viewMatrix = MatrixFromVector( shadowLightDir );
		shadowView.viewMatrix = shadowView.viewMatrix;
		const vec4f shadowLightPos = shadowView.viewMatrix * renderView.lights[ 0 ].lightPos;
		shadowView.viewMatrix[ 3 ][ 0 ] = -shadowLightPos[ 0 ];
		shadowView.viewMatrix[ 3 ][ 1 ] = -shadowLightPos[ 1 ];
		shadowView.viewMatrix[ 3 ][ 2 ] = -shadowLightPos[ 2 ];

		Camera shadowCam;
		shadowCam = Camera( vec4f( 0.0f, 0.0f, 0.0f, 0.0f ) );
		shadowCam.SetClip( shadowNearPlane, shadowFarPlane );
		shadowCam.SetFov( Radians( 90.0f ) );
		shadowCam.SetAspectRatio( ( ShadowMapWidth / (float)ShadowMapHeight ) );
		shadowView.projMatrix = shadowCam.GetPerspectiveMatrix( false );
		shadowView.viewprojMatrix = shadowView.projMatrix * shadowView.viewMatrix;
	}

	// Post view
	{
		view2D.SetViewport( viewport_t( 0, 0, width, height, 0.0f, 1.0f ) );
		view2D.viewMatrix = mat4x4f( 1.0f );
		view2D.projMatrix = mat4x4f( 1.0f );
		shadowView.viewprojMatrix = mat4x4f( 1.0f );
	}
}


void Renderer::UpdateBindSets( const uint32_t currentImage )
{
	const uint32_t i = currentImage;

	RenderView* views[ 3 ] = { &shadowView, &renderView, &view2D }; // FIXME: TEMP!!

	for ( uint32_t viewIx = 0; viewIx < 3; ++viewIx )
	{
		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			DrawPass* pass = views[ viewIx ]->passes[ passIx ];
			if( pass == nullptr ) {
				continue;
			}

			pass->codeImages[ currentImage ].Resize( 2 );
			if ( passIx == DRAWPASS_SHADOW )
			{				
				pass->codeImages[ currentImage ][ 0 ] = gpuImages2D[ 0 ];
				pass->codeImages[ currentImage ][ 1 ] = gpuImages2D[ 0 ];
			}
			else if ( passIx == DRAWPASS_POST_2D )
			{
				pass->codeImages[ currentImage ][ 0 ] = &frameState[ currentImage ].viewColorImage;
				pass->codeImages[ currentImage ][ 1 ] = &frameState[ currentImage ].depthImageView;
			}
			else
			{
				pass->codeImages[ currentImage ][ 0 ] = &frameState[ currentImage ].shadowMapImage;
				pass->codeImages[ currentImage ][ 1 ] = &frameState[ currentImage ].shadowMapImage;
			}

			pass->parms[ i ]->Bind( bind_globalsBuffer, &frameState[ i ].globalConstants );
			pass->parms[ i ]->Bind( bind_viewBuffer, &frameState[ i ].viewParms );
			pass->parms[ i ]->Bind( bind_modelBuffer, &frameState[ i ].surfParmPartitions[ int( views[ viewIx ]->region ) ] );
			pass->parms[ i ]->Bind( bind_image2DArray, &gpuImages2D );
			pass->parms[ i ]->Bind( bind_imageCubeArray, &gpuImagesCube );
			pass->parms[ i ]->Bind( bind_materialBuffer, &frameState[ i ].materialBuffers );
			pass->parms[ i ]->Bind( bind_lightBuffer, &frameState[ i ].lightParms );
			pass->parms[ i ]->Bind( bind_imageCodeArray, &pass->codeImages[ i ] );
			pass->parms[ i ]->Bind( bind_imageStencil, ( passIx == DRAWPASS_POST_2D ) ? &frameState[ currentImage ].stencilImageView : pass->codeImages[ i ][ 0 ] );
		}
	}

	{
		particleState.parms[ i ]->Bind( bind_globalsBuffer, &frameState[ i ].globalConstants );
		particleState.parms[ i ]->Bind( bind_particleWriteBuffer, &frameState[ i ].particleBuffer );
	}
}


void Renderer::UpdateBuffers( const uint32_t currentImage )
{
	static globalUboConstants_t globalsBuffer;
	{
		globalUboConstants_t globals;
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

		float intPart = 0;
		const float fracPart = modf( time, &intPart );

		const float viewWidth = static_cast<float>( renderView.m_viewport.width );
		const float viewHeight = static_cast<float>( renderView.m_viewport.height );

		globals.time = vec4f( time, intPart, fracPart, 1.0f );
		globals.generic = vec4f( g_imguiControls.heightMapHeight, g_imguiControls.roughness, 0.0f, 0.0f );
		globals.dimensions = vec4f( viewWidth, viewHeight, 1.0f / viewWidth, 1.0f / viewHeight );
		globals.tonemap = vec4f( g_imguiControls.toneMapColor[ 0 ], g_imguiControls.toneMapColor[ 1 ], g_imguiControls.toneMapColor[ 2 ], g_imguiControls.toneMapColor[ 3 ] );
		globals.shadowParms = vec4f( ShadowObjectOffset, ShadowMapWidth, ShadowMapHeight, g_imguiControls.shadowStrength );
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

	frameState[ currentImage ].surfParmPartitions[ int( renderView.region ) ].SetPos();
	frameState[ currentImage ].surfParmPartitions[ int( renderView.region ) ].CopyData( uboBuffer, sizeof( uniformBufferObject_t ) * MaxSurfaces );
	frameState[ currentImage ].surfParmPartitions[ int( shadowView.region ) ].SetPos();
	frameState[ currentImage ].surfParmPartitions[ int( shadowView.region ) ].CopyData( shadowUboBuffer, sizeof( uniformBufferObject_t ) * MaxSurfaces );
	frameState[ currentImage ].surfParmPartitions[ int( view2D.region ) ].SetPos();
	frameState[ currentImage ].surfParmPartitions[ int( view2D.region ) ].CopyData( postUboBuffer, sizeof( uniformBufferObject_t ) * MaxSurfaces );

	frameState[ currentImage ].materialBuffers.SetPos();
	frameState[ currentImage ].materialBuffers.CopyData( materialBuffer, sizeof( materialBufferObject_t ) * materialFreeSlot );

	frameState[ currentImage ].lightParms.SetPos();
	frameState[ currentImage ].lightParms.CopyData( lightBuffer, sizeof( lightBufferObject_t ) * MaxLights );

	frameState[ currentImage ].particleBuffer.SetPos( frameState[ currentImage ].particleBuffer.GetMaxSize() );
	//frameState[ currentImage ].particleBuffer.CopyData();
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


drawPass_t Renderer::ViewRegionPassBegin( const renderViewRegion_t region )
{
	switch( region )
	{
		case renderViewRegion_t::SHADOW:			return DRAWPASS_SHADOW_BEGIN;
		case renderViewRegion_t::STANDARD_RASTER:	return DRAWPASS_MAIN_BEGIN;
		case renderViewRegion_t::POST:				return DRAWPASS_POST_BEGIN;
	}
	return DRAWPASS_COUNT;
}


drawPass_t Renderer::ViewRegionPassEnd( const renderViewRegion_t region )
{
	switch ( region )
	{
		case renderViewRegion_t::SHADOW:			return DRAWPASS_SHADOW_END;
		case renderViewRegion_t::STANDARD_RASTER:	return DRAWPASS_MAIN_END;
		case renderViewRegion_t::POST:				return DRAWPASS_POST_END;
	}
	return DRAWPASS_COUNT;
}


void Renderer::RenderViewSurfaces( RenderView& view, GfxContext& gfxContext )
{
	const drawPass_t passBegin = ViewRegionPassBegin( view.region );
	const drawPass_t passEnd = ViewRegionPassEnd( view.region );

	// For now the pass state is the same for the entire view region
	const DrawPass* pass = view.passes[ passBegin ];
	if ( pass == nullptr ) {
		throw std::runtime_error( "Missing pass state!" );
	}

	renderPassTransitionFlags_t transitionState = {};
	transitionState.flags.readAfter = pass->transitionState.flags.readAfter;
	transitionState.flags.presentAfter = pass->transitionState.flags.presentAfter;
	transitionState.flags.store = true;
	transitionState.flags.clear = true;

	VkRenderPassBeginInfo passInfo{ };
	passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passInfo.renderPass = pass->fb[ m_bufferId ]->GetVkRenderPass( transitionState );
	passInfo.framebuffer = pass->fb[ m_bufferId ]->GetVkBuffer( transitionState );
	passInfo.renderArea.offset = { pass->viewport.x, pass->viewport.y };
	passInfo.renderArea.extent = { pass->viewport.width, pass->viewport.height };

	const VkClearColorValue clearColor = { pass->clearColor[ 0 ], pass->clearColor[ 1 ], pass->clearColor[ 2 ], pass->clearColor[ 3 ] };
	const VkClearDepthStencilValue clearDepth = { pass->clearDepth, pass->clearStencil };

	const uint32_t colorAttachmentsCount = pass->fb[ m_bufferId ]->GetColorLayers();
	const uint32_t attachmentsCount = pass->fb[ m_bufferId ]->GetLayers();

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

	VkCommandBuffer cmdBuffer = gfxContext.commandBuffers[ m_bufferId ];

	vkCmdBeginRenderPass( cmdBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE );

	VkViewport viewport{ };
	viewport.x = static_cast<float>( view.m_viewport.x );
	viewport.y = static_cast<float>( view.m_viewport.y );
	viewport.width = static_cast<float>( view.m_viewport.width );
	viewport.height = static_cast<float>( view.m_viewport.height );
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport( cmdBuffer, 0, 1, &viewport );

	VkRect2D rect{ };
	rect.extent.width = view.m_viewport.width;
	rect.extent.height = view.m_viewport.height;
	vkCmdSetScissor( cmdBuffer, 0, 1, &rect );

	for ( uint32_t passIx = passBegin; passIx <= passEnd; ++passIx )
	{
		sortKey_t lastKey = {};
		lastKey.materialId = INVALID_HDL.Get();

		MarkerBeginRegion( gfxContext, view.passes[ passIx ]->name, ColorToVector( Color::White ) );
		for ( size_t surfIx = 0; surfIx < view.mergedModelCnt; surfIx++ )
		{
			drawSurf_t& surface = view.merged[ surfIx ];	

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
			VkDescriptorSet descSetArray[ descSetCount ] = { view.passes[ passIx ]->parms[ m_bufferId ]->GetVkObject() };

			vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
			vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, descSetCount, descSetArray, 0, nullptr );
			lastKey = surface.sortKey;

			pushConstants_t pushConstants = { surface.objectId, surface.sortKey.materialId, uint32_t( view.region ) };
			vkCmdPushConstants( cmdBuffer, pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

			vkCmdDrawIndexed( cmdBuffer, surface.indicesCnt, view.instanceCounts[ surfIx ], surface.firstIndex, surface.vertexOffset, 0 );
		}
		MarkerEndRegion( gfxContext );
	}

	if( view.region == renderViewRegion_t::POST )
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

	VkBuffer vertexBuffers[] = { vb.GetVkObject() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers( gfxContext.commandBuffers[ m_bufferId ], 0, 1, vertexBuffers, offsets );
	vkCmdBindIndexBuffer( gfxContext.commandBuffers[ m_bufferId ], ib.GetVkObject(), 0, VK_INDEX_TYPE_UINT32 );

	// Shadow View
	{
		MarkerBeginRegion( gfxContext, shadowView.name, ColorToVector( Color::White ) );

		RenderViewSurfaces( shadowView, gfxContext );

		MarkerEndRegion( gfxContext );
	}

	// Main View
	{
		MarkerBeginRegion( gfxContext, renderView.name, ColorToVector( Color::White ) );

		RenderViewSurfaces( renderView, gfxContext );
		
		MarkerEndRegion( gfxContext );
	}

	// 2D View
	{
		MarkerBeginRegion( gfxContext, view2D.name, ColorToVector( Color::White ) );

		RenderViewSurfaces( view2D, gfxContext );
		
		MarkerEndRegion( gfxContext );
	}


	if ( vkEndCommandBuffer( gfxContext.commandBuffers[ m_bufferId ] ) != VK_SUCCESS )
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
		if ( ImGui::BeginTabItem( "Debug" ) )
		{
			g_imguiControls.rebuildShaders = ImGui::Button( "Reload Shaders" );
			g_imguiControls.rebuildRaytraceScene = ImGui::Button( "Rebuild Raytrace Scene" );
			ImGui::SameLine();
			g_imguiControls.raytraceScene = ImGui::Button( "Raytrace Scene" );
			ImGui::SameLine();
			g_imguiControls.rasterizeScene = ImGui::Button( "Rasterize Scene" );

			ImGui::InputFloat( "Heightmap Height", &g_imguiControls.heightMapHeight, 0.1f, 1.0f );
			ImGui::SliderFloat( "Roughness", &g_imguiControls.roughness, 0.1f, 1.0f );
			ImGui::SliderFloat( "Shadow Strength", &g_imguiControls.shadowStrength, 0.0f, 1.0f );
			ImGui::InputFloat( "Tone Map R", &g_imguiControls.toneMapColor[ 0 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map G", &g_imguiControls.toneMapColor[ 1 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map B", &g_imguiControls.toneMapColor[ 2 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map A", &g_imguiControls.toneMapColor[ 3 ], 0.1f, 1.0f );
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Device" ) )
		{
			DebugMenuDeviceProperties( context.deviceProperties );
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Assets" ) )
		{
			const uint32_t matCount = g_assets.materialLib.Count();
			if( ImGui::TreeNode( "Materials", "Materials (%i)", matCount ) )
			{
				for ( uint32_t m = 0; m < matCount; ++m )
				{
					Asset<Material>* matAsset = g_assets.materialLib.Find( m );
					Material& mat = matAsset->Get();
					const char* matName = g_assets.materialLib.FindName(m);

					if ( ImGui::TreeNode( matAsset->GetName().c_str() ) )
					{
						DebugMenuMaterialEdit( matAsset );
						ImGui::Separator();
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			const uint32_t modelCount = g_assets.modelLib.Count();
			if ( ImGui::TreeNode( "Models", "Models (%i)", modelCount ) )
			{
				for ( uint32_t m = 0; m < modelCount; ++m )
				{
					Asset<Model>* modelAsset = g_assets.modelLib.Find( m );
					DebugMenuModelTreeNode( modelAsset );
				}
				ImGui::TreePop();
			}
			const uint32_t texCount = g_assets.textureLib.Count();
			if ( ImGui::TreeNode( "Textures", "Textures (%i)", texCount ) )
			{
				for ( uint32_t t = 0; t < texCount; ++t )
				{
					Asset<Image>* texAsset = g_assets.textureLib.Find( t );
					DebugMenuTextureTreeNode( texAsset );
				}
				ImGui::TreePop();
			}
			const uint32_t shaderCount = g_assets.gpuPrograms.Count();
			if ( ImGui::TreeNode( "Shaders", "Shaders (%i)", shaderCount ) )
			{
				for ( uint32_t s = 0; s < shaderCount; ++s )
				{
					Asset<GpuProgram>* shaderAsset = g_assets.gpuPrograms.Find( s );
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
			Entity* ent = g_scene->FindEntity( currentIdx );
			const char* previewValue = ent->name.c_str();
			if ( ImGui::BeginCombo( "Entity", previewValue ) )
			{
				const uint32_t modelCount = g_assets.modelLib.Count();
				for ( uint32_t e = 0; e < g_scene->EntityCount(); ++e )
				{
					Entity* comboEnt = g_scene->FindEntity( e );

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
					boundEnt->materialHdl = g_assets.materialLib.RetrieveHdl( "DEBUG_WIRE" );

					g_scene->entities.push_back( boundEnt );
					g_scene->CreateEntityBounds( g_assets.modelLib.RetrieveHdl( "cube" ), *boundEnt );
				}

				ImGui::SameLine();
				if( ImGui::Button( "Export Model" ) )
				{
					Asset<Model>* asset = g_assets.modelLib.Find( ent->modelHdl );
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
			const char* previewValue = g_assets.modelLib.FindName( currentIdx );
			if ( ImGui::BeginCombo( "Model", previewValue ) )
			{
				const uint32_t modelCount = g_assets.modelLib.Count();
				for ( uint32_t m = 0; m < modelCount; ++m )
				{
					Asset<Model>* modelAsset = g_assets.modelLib.Find( m );

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
				g_scene->entities.push_back( ent );
				g_scene->CreateEntityBounds( g_assets.modelLib.RetrieveHdl( g_assets.modelLib.FindName( currentIdx ) ), *ent );
			}

			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Outliner" ) )
		{
			static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

			DebugMenuLightEdit( g_scene );

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

	ImGui::InputInt( "Image Id", &g_imguiControls.dbgImageId );
	g_imguiControls.dbgImageId = Clamp( g_imguiControls.dbgImageId, -1, int(g_assets.textureLib.Count() - 1) );

	ImGui::Text( "Mouse: (%f, %f)", (float)g_window.input.GetMouse().x, (float)g_window.input.GetMouse().y );
	ImGui::Text( "Mouse Dt: (%f, %f)", (float)g_window.input.GetMouse().dx, (float)g_window.input.GetMouse().dy );
	const vec4f cameraOrigin = g_scene->camera.GetOrigin();
	ImGui::Text( "Camera: (%f, %f, %f)", cameraOrigin[ 0 ], cameraOrigin[ 1 ], cameraOrigin[ 2 ] );
	const vec2f ndc = g_window.GetNdc( g_window.input.GetMouse().x, g_window.input.GetMouse().y );

	char entityName[ 256 ];
	if ( g_imguiControls.selectedEntityId >= 0 ) {
		sprintf_s( entityName, "%i: %s", g_imguiControls.selectedEntityId, g_assets.modelLib.FindName( g_scene->entities[ g_imguiControls.selectedEntityId ]->modelHdl ) );
	}
	else {
		memset( &entityName[ 0 ], 0, 256 );
	}

	ImGui::Text( "NDC: (%f, %f )", (float)ndc[ 0 ], (float)ndc[ 1 ] );
	ImGui::Text( "Frame Number: %d", m_frameNumber );
	ImGui::SameLine();
	ImGui::Text( "FPS: %f", 1000.0f / renderTime );
	//ImGui::Text( "Model %i: %s", 0, models[ 0 ].name.c_str() );
	ImGui::End();
#endif
}