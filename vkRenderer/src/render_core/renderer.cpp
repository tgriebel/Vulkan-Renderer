#include "renderer.h"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <map>
#include <sstream>

#include <gfxcore/primitives/geom.h>
#include <gfxcore/primitives/geoBuilder.h>
#include <gfxcore/math/vector.h>

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
#include "../globals/render_util.h"
#include "../globals/assetDefs.h"

#include "../scene/sceneBase.h"
#include "../scene/entity.h"
#include "../asset_types/texture.h"
#include "../asset_types/gpuProgram.h"
#include "../asset_types/assetLib.h"

#include "../app/window.h"
#include "../app/input.h"
#include "../io/io.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/backends/imgui_impl_vulkan.h"
#include "../app/imguiInterface.h"
#endif

#define SHADER_STRUCTS_CPP
#include "../../shaders/gpuShared.h"

extern imguiControls_t g_imguiControls;

extern Scene* g_scene;

SwapChain g_swapChain;
renderConstants_t rc;

#if defined( USE_IMGUI )
static ImGui_ImplVulkanH_Window imguiMainWindowData;
#endif


void RenderUploader::Boot( RenderContext* context, ResourceContext* resources )
{
	imageFreeSlot = 0;

	renderContext = context;
	resourceContext = resources;

	textureStagingBuffer.Create(
		"Texture Staging",
		swapBuffering_t::SINGLE_FRAME,
		resourceLifeTime_t::REBOOT,
		1,
		MaxTexturingUploadMemory,
		bufferType_t::STAGING,
		renderContext->sharedMemory
	);
}


void RenderUploader::Shutdown()
{
	imageFreeSlot = 0;
}


void RenderUploader::UploadImage( Asset<Image>* imageAsset )
{
	if( imageAsset->IsLoaded() == false ) {
		return;
	}
	if( imageAsset->IsUploaded() ) {
		return;
	}

	Image* image = &imageAsset->Get();

	if( image == nullptr ) {
		return;
	}

	if( ( image->gpuImage == nullptr ) || ( image->gpuImage->GetId() < 0 ) )
	{
		uploadTextures.insert( imageAsset->Handle() );
	}
	if( imageAsset->IsUploaded() )
	{
		updateTextures.insert( imageAsset->Handle() );
	}
}


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

	Asset<Model>* modelAsset = ModelLib().Find( ent.modelHdl );
	Model& model = modelAsset->Get();

	for ( uint32_t i = 0; i < model.surfCount; ++i )
	{
		if( model.uploadId == -1 )
		{
			model.uploadId = geometry.surfUploads.Count();
			geometry.surfUploads.Grow( model.surfCount );
		}

		hdl_t materialHdl = ent.materialHdl.IsValid() ? ent.materialHdl : model.surfs[ i ].materialHdl;
		const Asset<Material>* materialAsset = MaterialLib().Find( materialHdl );
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
		else if ( view.GetRegion() == renderViewRegion_t::STANDARD_2D )
		{
			if ( ( material.GetShader( DRAWPASS_2D ) == INVALID_HDL ) && ( material.GetShader( DRAWPASS_DEBUG_2D ) == INVALID_HDL ) ) {
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
		instance.envMapId = TextureLib().Find( ent.envMap )->Get().gpuImage->GetId();
		instance.diffuseIblId = TextureLib().Find( ent.diffuseIblMap )->Get().gpuImage->GetId();

		surf.uploadId = ( model.uploadId + i );
		surf.stencilBit = ent.outline ? OutlineStencilBit : 0;
		surf.objectOffset = 0;
		surf.flags = renderFlags;	
		
		surf.sortKey = {};
		surf.sortKey.materialId = material.uploadId;
		surf.sortKey.stencilBit = surf.stencilBit;
		surf.sortKey.customId = ent.GetSortOrder();

		surf.dbgName = materialAsset->GetName().c_str();

		if( materialAsset->IsUploaded() == false ) {
			uploadMaterials.insert( materialHdl );
		}

		for ( uint32_t t = 0; t < MaxMaterialTextures; ++t )
		{
			const hdl_t texHandle = material.GetTexture( t );
			if ( texHandle.IsValid() )
			{
				Asset<Image>* imageAsset = TextureLib().Find( texHandle );
				uploader.UploadImage( imageAsset );
			}
		}

		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			surf.pipelineObject = INVALID_HDL;
			if ( material.GetShader( drawPass_t( passIx ) ).IsValid() == false ) {
				continue;
			}

			Asset<GpuProgram>* prog = GpuProgramLib().Find( material.GetShader( drawPass_t( passIx ) ) );
			if ( prog == nullptr ) {
				continue;
			}

			const DrawPass* pass = view.passes[ 0 ][ passIx ];
			if( pass == nullptr ){
				continue;
			}

			shaderPermId_t permSet = static_cast<shaderPermId_t>( material.GetShaderPerms( drawPass_t( passIx ) ) );
			if ( pass->GetFrameBuffer()->ColorLayerCount() > 1 ) {
				SetFlags( permSet, shaderPermId_t::MRT );
			}

			surf.pipelineObject = FindPipelineObject( pass, *prog, permSet ); // Search the cache first
			if( surf.pipelineObject == INVALID_HDL )
			{
				CreateGraphicsPipeline( &renderContext, pass, *prog, permSet );
				surf.pipelineObject = FindPipelineObject( pass, *prog, permSet );
			}
			assert( surf.pipelineObject != INVALID_HDL );

			view.drawGroup[ passIx ].Add( surf, instance );
		}
	}
}


void Renderer::InitGPU()
{
	{
		// Memory Allocations
		renderContext.sharedMemory.Create( MaxSharedMemory, memoryRegion_t::SHARED, resourceLifeTime_t::REBOOT );
		renderContext.localMemory.Create( MaxLocalMemory, memoryRegion_t::LOCAL, resourceLifeTime_t::REBOOT );
	}

	InitShaderResources();
	RecreateSwapChain();
	UploadAssets();
}


void Renderer::ShutdownGPU()
{
	FlushGPU();
	ShutdownShaderResources();

	uploader.Shutdown();

	geometry.vbBufElements = 0;
	geometry.ibBufElements = 0;
}


void Renderer::RecreateSwapChain()
{
	int width = 0, height = 0;
	g_window.QueryWindowFrameBufferSize( width, height, true );

	FlushGPU();

	RenderResource::Cleanup( resourceLifeTime_t::RESIZE );
	g_swapChain.Destroy();

	renderContext.frameBufferMemory.Create( MaxFrameBufferMemory, memoryRegion_t::LOCAL, resourceLifeTime_t::RESIZE );

	g_swapChain.Create( &g_window, width, height );
	CreateFramebuffers();

	schedule.Resize();
}


void Renderer::Resize()
{
	RecreateSwapChain();
	renderContext.RefreshRegisteredBindParms();

	uploadContext.Begin();

	RenderResource::TransitionImages( &uploadContext, resourceLifeTime_t::RESIZE );
	RenderResource::TransitionImages( &uploadContext, resourceLifeTime_t::TASK );

	Transition( &uploadContext, *g_swapChain.GetBackBuffer(), swapBuffering_t::MULTI_FRAME, GPU_IMAGE_NONE, GPU_IMAGE_PRESENT );

	uploadContext.End();
	uploadContext.Submit();

	FlushGPU();
}


uint32_t Renderer::OutputImageCount()
{
	return resources.OutputImageCount();
}


const Image* Renderer::FindOutputImage( const uint32_t id )
{
	if( id < resources.OutputImageCount() )
	{
		return &resources.gpuOutput2D[ id ];
	}
	return nullptr;
}


void Renderer::UploadAssets()
{
	// Assign upload indices for all scene assets
	{
		const uint32_t materialCount = MaterialLib().Count();
		for( uint32_t i = 0; i < materialCount; ++i )
		{
			AssetInterface* materialAsset = MaterialLib().Find( i );
			if( materialAsset->IsLoaded() == false )
			{
				continue;
			}
			if( materialAsset->IsUploaded() )
			{
				continue;
			}
			uploadMaterials.insert( materialAsset->Handle() );
		}

		const uint32_t textureCount = TextureLib().Count();
		for( uint32_t i = 0; i < textureCount; ++i )
		{
			uploader.UploadImage( TextureLib().Find( i ) );
		}

		const uint32_t modelCount = ModelLib().Count();

		for( uint32_t m = 0; m < modelCount; ++m )
		{
			Asset<Model>* modelAsset = ModelLib().Find( m );
			if( modelAsset->IsLoaded() == false )
			{
				continue;
			}

			if( modelAsset->IsUploaded() )
			{
				continue;
			}
			Model& model = modelAsset->Get();
			if( model.uploadId != -1 )
			{
				continue;
			}

			for( uint32_t i = 0; i < model.surfCount; ++i )
			{
				model.uploadId = geometry.surfUploads.Count();
				geometry.surfUploads.Grow( model.surfCount );
			}
		}
	}

	ClearPipelineCache();
	BuildPipelines();

	geometry.stagingBuffer.SetPos( 0 );

	// Upload queue commands
	{
		
		uploadContext.Begin();
		uploadContext.MarkerBeginRegion( "UploadAssets", ColorToVector( Color::Green ) );

		UploadModelsToGPU( &uploadContext );

		uploader.UploadTextures( &uploadContext );

		RenderResource::TransitionImages( &uploadContext, resourceLifeTime_t::REBOOT );
		RenderResource::TransitionImages( &uploadContext, resourceLifeTime_t::RESIZE );
		RenderResource::TransitionImages( &uploadContext, resourceLifeTime_t::TASK );

		Transition( &uploadContext, *g_swapChain.GetBackBuffer(), swapBuffering_t::MULTI_FRAME, GPU_IMAGE_NONE, GPU_IMAGE_PRESENT );

		uploadContext.MarkerEndRegion();

		uploadContext.End();
		uploadContext.Submit();

		UpdateGpuMaterials();
	}
	FlushGPU();
}


void Renderer::Render()
{
	WaitForEndFrame();

	frameTimer.Start();

	BuildPipelines();

	geometry.stagingBuffer.SetPos( 0 );
	uploadContext.Begin();

	UploadModelsToGPU( &uploadContext );

	uploader.UploadTextures( &uploadContext );
	uploader.UpdateTextureData( &uploadContext );

	uploadContext.End();
	uploadContext.Signal( &uploadFinishedSemaphore );
	uploadContext.Submit();

	UpdateGpuMaterials();
	UpdateBuffers();
	UpdateBindSets();

	SubmitFrame();

	frameTimer.Stop();

	g_renderDebugData.frameTimeMs = static_cast<float>( frameTimer.GetElapsed() );
	g_renderDebugData.frameNumber = renderContext.frameNumber;
}


void Renderer::WaitForEndFrame()
{
	// Wait for the *oldest* frame to finish, reuse its fence
	// Fences are inserted at the end of the graphics queue
	{
	//	SCOPED_TIMER_PRINT( WaitForFrame );
		gfxContext.frameFence[ context.bufferId ].Wait();
	}

	g_swapChain.WaitOnFlip( gfxContext.presentSemaphore );

#ifdef USE_IMGUI
	ImGui_ImplVulkan_NewFrame();
#endif

	++renderContext.frameNumber;
}


void Renderer::SubmitFrame()
{
	schedule.FrameBegin();

	{
		computeContext.Begin();
		gfxContext.Begin();

		renderContext.UpdateBindParms();

		while( schedule.HasPendingTasks() ) {
			schedule.IssueNext( gfxContext );
		}

		gfxContext.End();
		computeContext.End();
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

	schedule.FrameEnd();

	context.bufferId = ( context.bufferId + 1 ) % g_swapChain.GetBufferCount();
}


void Renderer::CommitLight( const light_t& light )
{
	if ( ( light.flags & LIGHT_FLAGS_HIDDEN ) != 0 ) {
		return;
	}

	gpuLight_t lightObject = {};
	lightObject.intensity = light.intensity * ColorToVector( light.color );
	lightObject.lightDir = light.dir;
	lightObject.lightPos = light.pos;

	if ( ( light.flags & LIGHT_FLAGS_SHADOW ) == 0 ) {
		lightObject.shadowViewId = 0xFF;
	}
	else
	{
		lightObject.shadowViewId = shadowCount;

		shadowViews[ shadowCount ]->SetViewRect( 0, 0, ShadowMapWidth, ShadowMapHeight );

		if( HasFlags( light.flags, LIGHT_FLAGS_POINT ) == false )
		{
			Camera shadowCam;
			shadowCam = Camera( light.pos, MatrixFromVector( light.dir.xyz.Reverse() ) );
			shadowCam.SetClip( 0.1f, 1000.0f );
			shadowCam.SetFov( Radians( 90.0f ), ( ShadowMapWidth / (float)ShadowMapHeight ) );

			shadowViews[ shadowCount ]->SetCamera( shadowCam, false );
		}
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
		renderViews[ 0 ]->SetCamera( *scene->mainCamera );

		renderViews[ 0 ]->numLights = lightCount;
		for ( uint32_t lightIx = 0; lightIx < lightCount; ++lightIx ) {
			renderViews[ 0 ]->lights[ lightIx ] = lightIx;
		}

		if( config.useCubeViews )
		{
			renderViews[ 1 ]->SetViewRect( 0, 0, 256, 256 );
			for ( uint32_t cubeViewIx = 0; cubeViewIx < 6; ++cubeViewIx ) {
				renderViews[ 1 ]->SetCamera( scene->cameras[ 1 + cubeViewIx ], true, cubeViewIx );
			}

			renderViews[ 1 ]->numLights = lightCount;
			for ( uint32_t lightIx = 0; lightIx < lightCount; ++lightIx ) {
				renderViews[ 1 ]->lights[ lightIx ] = lightIx;
			}
		}
	}

	// Post view
	{
		view2Ds[ 0 ]->SetViewRect( 0, 0, width, height );
		view2Ds[ 0 ]->SetCamera2D( scene->camera2D, vec4f( 0.0f, (float)width, 0.0f, (float)height ) );

		extern void DrawDebugMenu( RenderView& view );
		DrawDebugMenu( *view2Ds[ 0 ] );
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
	ShaderBindParms* globalParms = renderContext.globalParms;

	globalParms->Bind( BINDING_NAME( globalsBuffer ),				&resources.globalConstants );
	globalParms->Bind( BINDING_NAME( viewBuffer ),					&resources.viewParms );
	globalParms->Bind( BINDING_NAME( image2DArray ),				&resources.gpuImages2D );
	globalParms->Bind( BINDING_NAME( imageCubeArray ),				&resources.gpuImagesCube );
	globalParms->Bind( BINDING_NAME( materialBuffer ),				&resources.materialBuffers );
	globalParms->Bind( BINDING_NAME( bilinearWrapSampler ),			&resources.bilinearSamplers[ samplerAddress_t::SAMPLER_ADDRESS_WRAP ]);
	globalParms->Bind( BINDING_NAME( bilinearClampEdgeSampler ),	&resources.bilinearSamplers[ samplerAddress_t::SAMPLER_ADDRESS_CLAMP_EDGE ] );
	globalParms->Bind( BINDING_NAME( bilinearClampBorderSampler ),	&resources.bilinearSamplers[ samplerAddress_t::SAMPLER_ADDRESS_CLAMP_BORDER ] );
	globalParms->Bind( BINDING_NAME( depthShadowSampler ),			&resources.shadowMapSampler );

	{
		particleState.parms->Bind( BINDING_NAME( globalsBuffer ),		&resources.globalConstants );
		particleState.parms->Bind( BINDING_NAME( particleWriteBuffer ),	&resources.particleBuffer );
	}
}


void Renderer::UpdateBuffers()
{
	resources.globalConstants.SetPos( 0 );
	{
		gpuGlobals_t globals = {};
		static std::chrono::steady_clock::time_point startTime = std::chrono::high_resolution_clock::now();
		static std::chrono::steady_clock::time_point currentTime = startTime;
		
		auto previousTime = currentTime;
		currentTime = std::chrono::high_resolution_clock::now();

		auto deltaTime = ( currentTime - previousTime );
				
		float elapsedTime = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();
		renderContext.deltaTimeMs = std::chrono::duration<float, std::chrono::milliseconds::period>( deltaTime ).count();

		float timeIntPart = 0;
		const float timeFracPart = modf( elapsedTime, &timeIntPart );

		postProcessControls_t& postProcess = g_imguiControls.postProcess;

		globals.time = vec4f( elapsedTime, timeIntPart, timeFracPart, renderContext.deltaTimeMs );
#if defined( USE_IMGUI )
		globals.generic = vec4f( g_imguiControls.pbr.roughnessScale, g_imguiControls.pbr.roughnessBias, g_imguiControls.pbr.metalnessScale, g_imguiControls.pbr.metalnessBias );
		globals.toneMapTint = vec4f( postProcess.toneMapColor[ 0 ], postProcess.toneMapColor[ 1 ], postProcess.toneMapColor[ 2 ], postProcess.toneMapColor[ 3 ] );
		globals.bloom = vec4f( postProcess.bloomEnable, postProcess.bloomBlendWeight, 0.0f, 0.0f );
		globals.exposure = vec4f( postProcess.exposureMidGray, postProcess.exposureAdaptation, postProcess.exposureWhitePoint, postProcess.exposureDarkLimit );
		globals.exposure2 = vec4f( ( postProcess.autoExposureEnable && config.autoExposure ) ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f );
		globals.shadowParms = vec4f( 0, ShadowMapWidth, ShadowMapHeight, g_imguiControls.shadowStrength );
		globals.dof = vec4f( postProcess.dofEnable ? 1.0f : 0.0f, postProcess.dofFocalDepth, postProcess.dofFocalRange, 0.0f );
		globals.chromaticAberration = vec4f( postProcess.caEnable ? 1.0f : 0.0f, postProcess.caIntensity, 0.0f, 0.0f );
		globals.useDiffuseIBL = g_imguiControls.pbr.useDiffuseIBL ? 1 : 0;
		globals.useSpecularIBL = g_imguiControls.pbr.useSpecularIBL ? 1 : 0;
		globals.debugLightingMode = static_cast<gpuDebugLightingMode_t>( g_imguiControls.pbr.debugLightingMode );
#endif
		globals.numSamples = vk_GetSampleCount( config.mainColorSubSamples );
		globals.whiteId = rc.whiteImage->gpuImage->GetId();
		globals.blackId = rc.blackImage->gpuImage->GetId();
		globals.defaultAlbedoId = rc.albImage->gpuImage->GetId();
		globals.defaultNormalId = rc.nmlImage->gpuImage->GetId();
		globals.defaultRoughnessId = rc.rghImage->gpuImage->GetId();
		globals.defaultMetalId = rc.mtlImage->gpuImage->GetId();
		globals.defaultImageId = rc.defaultImage->gpuImage->GetId();
		globals.brdfLutId = TextureLib().Find( "code_assets/brdf_lut.img" )->Get().gpuImage->GetId();
		globals.isTextured = g_imguiControls.isTextured;
		
		globals.shadow2dCount = 0;
		globals.shadowCubeCount = 0;
		for( uint32_t i = 0; i < MaxShadowMaps; ++i )
		{
			globals.shadow2dCount += ( resources.shadowMapImage[ i ]->info.type == imageType_t::IMAGE_TYPE_2D );
			globals.shadowCubeCount += ( resources.shadowMapImage[ i ]->info.type == imageType_t::IMAGE_TYPE_CUBE );
		}
		globals.textureCount = TextureLib().Count();
		globals.materialCount = MaterialLib().Count();

		resources.globalConstants.CopyData( &globals, sizeof( globals ) );
	}

	resources.viewParms.SetPos( 0 );

	for ( uint32_t viewIx = 0; viewIx < MaxViews; ++viewIx )
	{
		const RenderView& view = views[ viewIx ];
		if( view.IsCommitted() == false ) {
			continue;
		}
		
		const uint32_t multiViewCount = view.GetMultiViewCount();
		for( uint32_t multiViewIndex = 0; multiViewIndex < multiViewCount; ++multiViewIndex )
		{
			gpuView_t viewBuffer = {};

			const vec2i& frameSize = view.GetFrameSize();
			viewBuffer.viewMat = view.GetViewMatrix( multiViewIndex );
			viewBuffer.projMat = view.GetProjMatrix( multiViewIndex );
			viewBuffer.viewOrigin = view.GetViewOrigin();
			viewBuffer.dimensions = vec4f( (float)frameSize[ 0 ], (float)frameSize[ 1 ], 1.0f / frameSize[ 0 ], 1.0f / frameSize[ 1 ] );
			viewBuffer.numLights = view.numLights;

			resources.viewParms.CopyData( &viewBuffer, sizeof( viewBuffer ) );
		}	
	}

	for ( uint32_t viewIx = 0; viewIx < MaxViews; ++viewIx )
	{
		const RenderView& view = views[ viewIx ];
		if ( view.IsCommitted() == false ) {
			continue;
		}
		const uint32_t viewId = view.GetSurfaceBufferId();

		static gpuSurface_t surfBuffer[ MaxSurfaces ];

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
				surfBuffer[ instanceId ].diffuseIblCubeId = instances[ surfIx ].diffuseIblId;
				surfBuffer[ instanceId ].envCubeId = instances[ surfIx ].envMapId;
			}
		}

		resources.surfParmPartitions[ viewId ].SetPos( 0 );
		resources.surfParmPartitions[ viewId ].CopyData( surfBuffer, sizeof( gpuSurface_t ) * MaxSurfaces );
	}

	resources.materialBuffers.SetPos( 0 );
	resources.materialBuffers.CopyData( materialBuffer.Ptr(), sizeof( gpuMaterial_t ) * materialBuffer.Count() );

	resources.lightParms.SetPos( 0 );
	resources.lightParms.CopyData( committedLights.Ptr(), sizeof( gpuLight_t ) * MaxLights );

	resources.particleBuffer.SetPos( resources.particleBuffer.GetMaxSize() );
	//state.particleBuffer.CopyData();
}


void Renderer::InitConfig( const renderConfig_t& cfg )
{
	imageSamples_t maxSamples = IMAGE_SMP_1;

#ifdef USE_VULKAN
	maxSamples = vk_MaxImageSamples();
#endif

	config = cfg;

	config.mainColorSubSamples = maxSamples;
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
