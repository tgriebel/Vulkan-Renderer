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
		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			view.drawGroup[ passIx ].Reset();
		}

		for ( uint32_t entIx = 0; entIx < entCount; ++entIx ) {
			CommitModel( view, *scene->entities[ entIx ] );
		}

		uint32_t drawGroupOffset = 0;
		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			view.drawGroup[ passIx ].Sort();
			view.drawGroup[ passIx ].Merge();
			view.drawGroup[ passIx ].AssignGeometryResources( &geometry );

			view.drawGroupOffset[ passIx ] = drawGroupOffset;
			drawGroupOffset += view.drawGroup[ passIx ].InstanceCount();
		}
	}
	CommitViews( scene );
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
		if( model.uploadId == -1 )
		{
			model.uploadId = geometry.surfUploads.Count();
			geometry.surfUploads.Grow( model.surfCount );
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

		drawSurfInstance_t instance = {};
		drawSurf_t surf = {};

		instance.modelMatrix = ent.GetMatrix();
		instance.surfId = 0;
		instance.id = 0;
		surf.uploadId = ( model.uploadId + i );
		surf.stencilBit = ent.outline ? OutlineStencilBit : 0;
		surf.objectOffset = 0;
		surf.flags = renderFlags;	
		
		surf.sortKey = {};
		surf.sortKey.materialId = material.uploadId;
		surf.sortKey.stencilBit = surf.stencilBit;

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
			surf.pipelineObject = INVALID_HDL;
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

			surf.pipelineObject = FindPipelineObject( pass, *prog );
			assert( surf.pipelineObject != INVALID_HDL );

			view.drawGroup[ passIx ].Add( surf, instance );
		}
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
	geometry.vbBufElements = 0;
	geometry.ibBufElements = 0;
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
		Transition( &uploadContext, resources.shadowMapImage[ shadowIx ], GPU_IMAGE_NONE, GPU_IMAGE_READ );
	}
	Transition( &uploadContext, resources.mainColorImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, resources.mainColorResolvedImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, resources.depthStencilResolvedImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, resources.tempColorImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, resources.depthStencilImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );

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

	geometry.stagingBuffer.SetPos( 0 );
	textureStagingBuffer.SetPos( 0 );
	uploadContext.Begin();

	UploadModelsToGPU();

	UploadTextures();

	for ( uint32_t shadowIx = 0; shadowIx < MaxShadowMaps; ++shadowIx ) {
		Transition( &uploadContext, resources.shadowMapImage[ shadowIx ], GPU_IMAGE_NONE, GPU_IMAGE_READ );
	}	
	Transition( &uploadContext, resources.mainColorImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, resources.mainColorResolvedImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, resources.depthStencilResolvedImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, resources.tempColorImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
	Transition( &uploadContext, resources.depthStencilImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );

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

	geometry.stagingBuffer.SetPos( 0 );
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
				pass->codeImages[ 0 ] = resources.gpuImages2D[ 0 ];
				pass->codeImages[ 1 ] = resources.gpuImages2D[ 0 ];
				pass->codeImages[ 2 ] = resources.gpuImages2D[ 0 ];
			}
			else if ( ( passIx == DRAWPASS_POST_2D ) || ( passIx == DRAWPASS_DEBUG_2D ) )
			{
				pass->codeImages[ 0 ] = &resources.mainColorResolvedImage;
				pass->codeImages[ 1 ] = &resources.depthStencilResolvedImage;
				pass->codeImages[ 2 ] = &resources.mainColorResolvedImage;
			}
			else
			{
				pass->codeImages[ 0 ] = &resources.shadowMapImage[ 0 ];
				pass->codeImages[ 1 ] = &resources.shadowMapImage[ 1 ];
				pass->codeImages[ 2 ] = &resources.shadowMapImage[ 2 ];
			}

			pass->parms->Bind( bind_globalsBuffer,	&resources.globalConstants );
			pass->parms->Bind( bind_viewBuffer,		&resources.viewParms );
			pass->parms->Bind( bind_modelBuffer,	&resources.surfParmPartitions[ views[ viewIx ].GetViewId() ] );
			pass->parms->Bind( bind_image2DArray,	&resources.gpuImages2D );
			pass->parms->Bind( bind_imageCubeArray,	&resources.gpuImagesCube );
			pass->parms->Bind( bind_materialBuffer,	&resources.materialBuffers );
			pass->parms->Bind( bind_lightBuffer,	&resources.lightParms );
			pass->parms->Bind( bind_imageCodeArray,	&pass->codeImages );
			pass->parms->Bind( bind_imageStencil, ( ( passIx == DRAWPASS_POST_2D ) || ( passIx == DRAWPASS_DEBUG_2D ) ) ? &resources.stencilResolvedImageView : pass->codeImages[ 0 ] );
		}
	}

	{
		particleState.parms->Bind( bind_globalsBuffer,			&resources.globalConstants );
		particleState.parms->Bind( bind_particleWriteBuffer,	&resources.particleBuffer );
	}

	if( resolve != nullptr )
	{
		resolve->pass->codeImages.Resize( 3 );
		resolve->pass->codeImages[ 0 ] = &resources.mainColorImage;
		resolve->pass->codeImages[ 1 ] = &resources.depthImageView;
		resolve->pass->codeImages[ 2 ] = &resources.stencilImageView;

		resolve->pass->parms->Bind( bind_globalsBuffer,			&resources.globalConstants );
		resolve->pass->parms->Bind( bind_sourceImages,			&resolve->pass->codeImages );
		resolve->pass->parms->Bind( bind_imageStencil,			&resources.stencilImageView );
		resolve->pass->parms->Bind( bind_imageProcess,			&resolve->buffer );
	}

	{
		downScale.pass->codeImages.Resize( 3 );
		downScale.pass->codeImages[ 0 ] = &resources.mainColorResolvedImage;
		downScale.pass->codeImages[ 1 ] = &resources.mainColorResolvedImage;
		downScale.pass->codeImages[ 2 ] = &resources.mainColorResolvedImage;

		downScale.pass->parms->Bind( bind_globalsBuffer,		&resources.globalConstants );
		downScale.pass->parms->Bind( bind_sourceImages,			&downScale.pass->codeImages );
		downScale.pass->parms->Bind( bind_imageStencil,			&resources.stencilImageView );
		downScale.pass->parms->Bind( bind_imageProcess,			&downScale.buffer );
	}
}


void Renderer::UpdateBuffers()
{
	resources.globalConstants.SetPos( 0 );
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

		resources.globalConstants.CopyData( &globals, sizeof( globals ) );
	}

	resources.viewParms.SetPos( 0 );

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
		resources.viewParms.CopyData( &viewBuffer, sizeof( viewBuffer ) );
	}

	for ( uint32_t viewIx = 0; viewIx < MaxViews; ++viewIx )
	{
		const RenderView& view = views[ viewIx ];
		if ( view.IsCommitted() == false ) {
			continue;
		}
		const uint32_t viewId = view.GetViewId();

		static surfaceBufferObject_t surfBuffer[ MaxSurfaces ];

		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			assert( view.drawGroup[ passIx ].InstanceCount() < MaxSurfaces );

			if( view.drawGroup[ passIx ].InstanceCount() == 0 ) {
				continue;
			}

			const drawSurfInstance_t* instances = view.drawGroup[ passIx ].Instances();
			for ( uint32_t surfIx = 0; surfIx < view.drawGroup[ passIx ].InstanceCount(); ++surfIx )
			{
				const uint32_t instanceId = view.drawGroupOffset[ passIx ] + view.drawGroup[ passIx ].InstanceId( surfIx );
				surfBuffer[ instanceId ].model = instances[ surfIx ].modelMatrix.Transpose();
			}
		}

		resources.surfParmPartitions[ viewId ].SetPos( 0 );
		resources.surfParmPartitions[ viewId ].CopyData( surfBuffer, sizeof( surfaceBufferObject_t ) * MaxSurfaces );
	}

	resources.materialBuffers.SetPos( 0 );
	resources.materialBuffers.CopyData( materialBuffer.Ptr(), sizeof( materialBufferObject_t ) * materialBuffer.Count() );

	resources.lightParms.SetPos( 0 );
	resources.lightParms.CopyData( committedLights.Ptr(), sizeof( lightBufferObject_t ) * MaxLights );

	resources.particleBuffer.SetPos( resources.particleBuffer.GetMaxSize() );
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