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
#include "../render_state/frameBuffer.h"
#include "../render_state/cmdContext.h"
#include "../render_binding/bindings.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/shaderBinding.h"
#include "../render_binding/bufferObjects.h"
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
		view.drawGroup.committedModelCnt = 0;
		for ( uint32_t entIx = 0; entIx < entCount; ++entIx ) {
			CommitModel( view, *scene->entities[ entIx ] );
		}
		MergeSurfaces( view );

		view.drawGroup.vb = &vb;
		view.drawGroup.ib = &ib;
	}
	CommitViews( scene );
}


void Renderer::MergeSurfaces( RenderView& view )
{
	view.drawGroup.Sort();
	view.drawGroup.Merge();

	// FIXME
	{
		uint32_t totalCount = 0;
		for ( uint32_t i = 0; i < view.drawGroup.mergedModelCnt; ++i ) {
			view.drawGroup.uploads[ i ] = surfUploads[ view.drawGroup.merged[ i ].uploadId ];
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
		drawSurfInstance_t& instance = view.drawGroup.instances[ view.drawGroup.committedModelCnt ];
		drawSurf_t& surf = view.drawGroup.surfaces[ view.drawGroup.committedModelCnt ];

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
												DRAWPASS_DEBUG_3D,
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
		surf.stencilBit = ent.outline ? OutlineStencilBit : 0;
		surf.objectId = 0;
		surf.flags = renderFlags;	
		surf.hash = Hash( surf );
		surf.dbgName = materialAsset->GetName().c_str();

		surf.sortKey = {};
		surf.sortKey.materialId = material.uploadId;
		surf.sortKey.stencilBit = surf.stencilBit;

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

		++view.drawGroup.committedModelCnt;
	}
}


void Renderer::InitGPU()
{
	{
		// Memory Allocations
		renderContext.sharedMemory.Create( MaxSharedMemory, memoryRegion_t::SHARED );
		renderContext.localMemory.Create( MaxLocalMemory, memoryRegion_t::LOCAL );
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

	FlushGPU();

	DestroyFramebuffers();
	g_swapChain.Destroy();

	renderContext.frameBufferMemory.Create( MaxFrameBufferMemory, memoryRegion_t::LOCAL );

	g_swapChain.Create( &g_window, width, height );
	CreateFramebuffers();

	for ( uint32_t viewIx = 0; viewIx < viewCount; ++viewIx )
	{
		views[ viewIx ].Resize();
		views[ viewIx ].SetViewRect( 0, 0, width, height );
	}

	if( resolve != nullptr ) {
		resolve->Resize();
	}
}


void Renderer::Resize()
{
	RecreateSwapChain();
	RefreshRegisteredBindParms();

	uploadContext.Begin();
	for ( uint32_t shadowIx = 0; shadowIx < MaxShadowMaps; ++shadowIx ) {
		Transition( &uploadContext, shadowMapImage[ shadowIx ], GPU_IMAGE_NONE, GPU_IMAGE_READ );
	}
	Transition( &uploadContext, mainColorImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, mainColorResolvedImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, depthStencilResolvedImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, tempColorImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, depthStencilImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );

	for ( uint32_t i = 0; i < g_swapChain.GetBufferCount(); ++i ) {
		Transition( &uploadContext, *g_swapChain.GetBackBuffer( i ), GPU_IMAGE_NONE, GPU_IMAGE_PRESENT );
	}

	uploadContext.End();
	uploadContext.Submit();

	FlushGPU();
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

	geoStagingBuffer.SetPos( 0 );
	textureStagingBuffer.SetPos( 0 );
	uploadContext.Begin();

	UploadModelsToGPU();

	UploadTextures();

	for ( uint32_t shadowIx = 0; shadowIx < MaxShadowMaps; ++shadowIx ) {
		Transition( &uploadContext, shadowMapImage[ shadowIx ], GPU_IMAGE_NONE, GPU_IMAGE_READ );
	}	
	Transition( &uploadContext, mainColorImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, mainColorResolvedImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, depthStencilResolvedImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, tempColorImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, depthStencilImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );

	for ( uint32_t i = 0; i < g_swapChain.GetBufferCount(); ++i ) {
		Transition( &uploadContext, *g_swapChain.GetBackBuffer( i ), GPU_IMAGE_NONE, GPU_IMAGE_PRESENT );
	}

	uploadContext.End();
	uploadContext.Submit();

	UpdateGpuMaterials();

	FlushGPU();
}


void Renderer::Render()
{
	WaitForEndFrame();

	frameTimer.Start();

	BuildPipelines();

	geoStagingBuffer.SetPos( 0 );
	textureStagingBuffer.SetPos( 0 );
	uploadContext.Begin();

	UploadModelsToGPU();

	UploadTextures();
	UpdateTextureData();

	uploadContext.End();
	uploadContext.Signal( &uploadFinishedSemaphore );
	uploadContext.Submit();

	UpdateGpuMaterials();
	UpdateBuffers();
	UpdateBindSets();
	UpdateFrameDescSet();

	SubmitFrame();

	frameTimer.Stop();

	g_renderDebugData.frameTimeMs = static_cast<float>( frameTimer.GetElapsed() );
	g_renderDebugData.frameNumber = m_frameNumber;
}


void Renderer::WaitForEndFrame()
{
	context.bufferId = ( context.bufferId + 1 ) % MaxFrameStates;

	{
		//SCOPED_TIMER_PRINT( WaitForFrame );
		gfxContext.frameFence[ context.bufferId ].Wait();
	}

	VK_CHECK_RESULT( vkAcquireNextImageKHR( context.device, g_swapChain.GetVkObject(), UINT64_MAX, gfxContext.presentSemaphore.GetVkObject(), VK_NULL_HANDLE, &context.swapChainIndex ) );

#ifdef USE_IMGUI
	ImGui_ImplVulkan_NewFrame();
#endif

	++m_frameNumber;
}


void Renderer::FlushGPU()
{
	vkDeviceWaitIdle( context.device );
}


void Renderer::SubmitFrame()
{
	{
		computeContext.Begin();

		const hdl_t progHdl = g_assets.gpuPrograms.RetrieveHdl( "ClearParticles" );

		computeContext.Dispatch( progHdl, *particleState.parms, MaxParticles / 256 );

		computeContext.End();
	}

	{
		gfxContext.Begin();

		schedule.Reset();
		while( schedule.PendingTasks() > 0 ) {
			schedule.IssueNext( gfxContext );
		}

		gfxContext.End();
	}

	computeContext.Submit();

	{
		gfxContext.Wait( &gfxContext.presentSemaphore );
		gfxContext.Wait( &uploadFinishedSemaphore );
		gfxContext.Signal( &gfxContext.renderFinishedSemaphore );
		gfxContext.Submit( &gfxContext.frameFence[ context.bufferId ] );
	}

	if ( g_swapChain.Present( gfxContext ) == false ) {
		g_window.RequestImageResize();
	}
}


void Renderer::CommitLight( const light_t& light )
{
	if ( ( light.flags & LIGHT_FLAGS_HIDDEN ) != 0 ) {
		return;
	}

	lightBufferObject_t lightObject = {};
	lightObject.intensity = light.intensity * ColorToVector( light.color );
	lightObject.lightDir = light.dir;
	lightObject.lightPos = light.pos;

	if ( ( light.flags & LIGHT_FLAGS_SHADOW ) == 0 ) {
		lightObject.shadowViewId = 0xFF;
	}
	else
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
	committedLights.Append( lightObject );

	++shadowCount;
	assert( shadowCount < MaxShadowMaps );
}


void Renderer::CommitViews( const Scene* scene )
{
	int width;
	int height;
	g_window.GetWindowSize( width, height );

	const uint32_t lightCount = static_cast<uint32_t>( scene->lights.size() );
	assert( lightCount <= MaxLights );

	shadowCount = 0;
	committedLights.Reset();

	for( uint32_t i = 0; i < lightCount; ++i ) {
		CommitLight( scene->lights[ i ] );
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

	activeViewCount = 0;
	for( uint32_t i = 0; i < viewCount; ++i )
	{
		if( views[ i ].IsCommitted() )
		{
			activeViews[ activeViewCount ] = &views[ i ];
			++activeViewCount;
		}	
	}
}


void Renderer::UpdateBindSets()
{
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

			pass->codeImages.Resize( 3 );
			if ( passIx == DRAWPASS_SHADOW )
			{				
				pass->codeImages[ 0 ] = gpuImages2D[ 0 ];
				pass->codeImages[ 1 ] = gpuImages2D[ 0 ];
				pass->codeImages[ 2 ] = gpuImages2D[ 0 ];
			}
			else if ( ( passIx == DRAWPASS_POST_2D ) || ( passIx == DRAWPASS_DEBUG_2D ) )
			{
				pass->codeImages[ 0 ] = &mainColorResolvedImage;
				pass->codeImages[ 1 ] = &depthStencilResolvedImage;
				pass->codeImages[ 2 ] = &mainColorResolvedImage;
			}
			else
			{
				pass->codeImages[ 0 ] = &shadowMapImage[ 0 ];
				pass->codeImages[ 1 ] = &shadowMapImage[ 1 ];
				pass->codeImages[ 2 ] = &shadowMapImage[ 2 ];
			}

			pass->parms->Bind( bind_globalsBuffer,	&renderContext.globalConstants );
			pass->parms->Bind( bind_viewBuffer,		&renderContext.viewParms );
			pass->parms->Bind( bind_modelBuffer,	&renderContext.surfParmPartitions[ views[ viewIx ].GetViewId() ] );
			pass->parms->Bind( bind_image2DArray,	&gpuImages2D );
			pass->parms->Bind( bind_imageCubeArray,	&gpuImagesCube );
			pass->parms->Bind( bind_materialBuffer,	&renderContext.materialBuffers );
			pass->parms->Bind( bind_lightBuffer,	&renderContext.lightParms );
			pass->parms->Bind( bind_imageCodeArray,	&pass->codeImages );
			pass->parms->Bind( bind_imageStencil, ( ( passIx == DRAWPASS_POST_2D ) || ( passIx == DRAWPASS_DEBUG_2D ) ) ? &stencilResolvedImageView : pass->codeImages[ 0 ] );
		}
	}

	{
		particleState.parms->Bind( bind_globalsBuffer,			&renderContext.globalConstants );
		particleState.parms->Bind( bind_particleWriteBuffer,	&renderContext.particleBuffer );
	}

	if( resolve != nullptr )
	{
		resolve->pass->codeImages.Resize( 3 );
		resolve->pass->codeImages[ 0 ] = &mainColorImage;
		resolve->pass->codeImages[ 1 ] = &renderContext.depthImageView;
		resolve->pass->codeImages[ 2 ] = &renderContext.stencilImageView;

		resolve->pass->parms->Bind( bind_globalsBuffer,			&renderContext.globalConstants );
		resolve->pass->parms->Bind( bind_sourceImages,			&resolve->pass->codeImages );
		resolve->pass->parms->Bind( bind_imageStencil,			&renderContext.stencilImageView );
		resolve->pass->parms->Bind( bind_imageProcess,			&resolve->buffer );
	}

	{
		downScale.pass->codeImages.Resize( 3 );
		downScale.pass->codeImages[ 0 ] = &mainColorResolvedImage;
		downScale.pass->codeImages[ 1 ] = &mainColorResolvedImage;
		downScale.pass->codeImages[ 2 ] = &mainColorResolvedImage;

		downScale.pass->parms->Bind( bind_globalsBuffer,		&renderContext.globalConstants );
		downScale.pass->parms->Bind( bind_sourceImages,			&downScale.pass->codeImages );
		downScale.pass->parms->Bind( bind_imageStencil,			&renderContext.stencilImageView );
		downScale.pass->parms->Bind( bind_imageProcess,			&downScale.buffer );
	}
}


void Renderer::UpdateBuffers()
{
	renderContext.globalConstants.SetPos( 0 );
	{
		globalUboConstants_t globals = {};
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

		float intPart = 0;
		const float fracPart = modf( time, &intPart );

		globals.time = vec4f( time, intPart, fracPart, 1.0f );
#if defined( USE_IMGUI )
		globals.generic = vec4f( g_imguiControls.heightMapHeight, g_imguiControls.roughness, 0.0f, 0.0f );
		globals.tonemap = vec4f( g_imguiControls.toneMapColor[ 0 ], g_imguiControls.toneMapColor[ 1 ], g_imguiControls.toneMapColor[ 2 ], g_imguiControls.toneMapColor[ 3 ] );
		globals.shadowParms = vec4f( 0, ShadowMapWidth, ShadowMapHeight, g_imguiControls.shadowStrength );
#else
		globals.generic = vec4f( 0.0f, 0.0f, 0.0f, 0.0f );
		globals.tonemap = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
		globals.shadowParms = vec4f( 0, ShadowMapWidth, ShadowMapHeight, 0.5f );
#endif
		globals.numSamples = vk_GetSampleCount( config.mainColorSubSamples );

		renderContext.globalConstants.CopyData( &globals, sizeof( globals ) );
	}

	renderContext.viewParms.SetPos( 0 );

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
		renderContext.viewParms.CopyData( &viewBuffer, sizeof( viewBuffer ) );
	}

	for ( uint32_t viewIx = 0; viewIx < MaxViews; ++viewIx )
	{
		const RenderView& view = views[ viewIx ];
		if ( view.IsCommitted() == false ) {
			continue;
		}
		const uint32_t viewId = view.GetViewId();

		static uniformBufferObject_t uboBuffer[ MaxSurfaces ];
		assert( view.drawGroup.committedModelCnt < MaxSurfaces );
		for ( uint32_t surfIx = 0; surfIx < view.drawGroup.committedModelCnt; ++surfIx )
		{
			uniformBufferObject_t ubo;
			ubo.model = view.drawGroup.sortedInstances[ surfIx ].modelMatrix;
			const drawSurf_t& surf = view.drawGroup.merged[ view.drawGroup.sortedInstances[ surfIx ].surfId ];
			const uint32_t objectId = ( view.drawGroup.sortedInstances[ surfIx ].id + surf.objectId );
			uboBuffer[ objectId ] = ubo;
		}

		renderContext.surfParmPartitions[ viewId ].SetPos( 0 );
		renderContext.surfParmPartitions[ viewId ].CopyData( uboBuffer, sizeof( uniformBufferObject_t ) * MaxSurfaces );
	}

	renderContext.materialBuffers.SetPos( 0 );
	renderContext.materialBuffers.CopyData( materialBuffer.Ptr(), sizeof( materialBufferObject_t ) * materialBuffer.Count() );

	renderContext.lightParms.SetPos( 0 );
	renderContext.lightParms.CopyData( committedLights.Ptr(), sizeof( lightBufferObject_t ) * MaxLights );

	renderContext.particleBuffer.SetPos( renderContext.particleBuffer.GetMaxSize() );
	//state.particleBuffer.CopyData();
}


void Renderer::InitConfig()
{
	imageSamples_t samples = IMAGE_SMP_1;

#ifdef USE_VULKAN
	samples = vk_MaxImageSamples();
#endif

	config.mainColorSubSamples = samples;
}


void Renderer::AttachDebugMenu( const debugMenuFuncPtr funcPtr )
{
	view2Ds[ 0 ]->debugMenus.Append( funcPtr );
}


void DeviceDebugMenu()
{
#if defined( USE_IMGUI )
	if ( ImGui::BeginTabItem( "Device" ) )
	{
		DebugMenuDeviceProperties( context.deviceProperties, context.deviceFeatures );
		ImGui::EndTabItem();
	}
#endif
}