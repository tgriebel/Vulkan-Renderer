#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"
#include "../scene/entity.h"
#include "../render_state/rhi.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/bindings.h"
#include "../render_resources/imageArray.h"
#include "../render_tasks/RenderTask.h"
#include "../render_tasks/ImageReadbackTask.h"
#include "../render_tasks/ImageProcessTask.h"
#include "../render_tasks/imguiTask.h"

#include "../draw_passes/drawpass.h"
#include "swapChain.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../../external/imgui/backends/imgui_impl_vulkan.h"
#endif

#include "debugMenu.h"
#include "../globals/assetDefs.h"

#include "schedule.h"

void Renderer::Init( const renderConfig_t& cfg )
{
	InitApi( cfg );

	InitShaderResources();

	resources.gpuImages2D.SetRenderContext( &renderContext );
	resources.gpuImagesCube.SetRenderContext( &renderContext );

	resources.gpuImages2D.Resize( MaxImageDescriptors );
	resources.gpuImagesCube.Resize( MaxImageDescriptors );

	// Image samplers
	{
		samplerState_t samplerState{};
		samplerState.filter = SAMPLER_FILTER_BILINEAR;
		samplerState.borderColor = SAMPLER_BORDER_BLACK;
		samplerState.borderColorIsFloat = true;
		samplerState.borderTransparent = false;
		samplerState.minLod = 0.0f;
		samplerState.maxLod = 16.0f;
		samplerState.maxAniso = 16.0f;

		for( uint32_t i = 0; i < SAMPLER_ADDRESS_MODES; ++i )
		{
			samplerState.addrMode = samplerAddress_t( i );

			samplerState.filter = samplerFilter_t::SAMPLER_FILTER_BILINEAR;
			resources.bilinearSamplers[ samplerState.addrMode ].Init( samplerState, resourceLifeTime_t::REBOOT );

			samplerState.filter = samplerFilter_t::SAMPLER_FILTER_TRILINEAR;
			resources.trilinearSamplers[ samplerState.addrMode ].Init( samplerState, resourceLifeTime_t::REBOOT );
		}

		samplerState.borderColor = SAMPLER_BORDER_WHITE;
		samplerState.borderColorIsFloat = true;
		samplerState.borderTransparent = false;
		samplerState.addrMode = samplerAddress_t::SAMPLER_ADDRESS_CLAMP_BORDER;
		samplerState.maxAniso = 0.0f;
		samplerState.pcf = true;

		resources.shadowMapSampler.Init( samplerState, resourceLifeTime_t::REBOOT );
	}

	viewCount = 0;

	// Shadow Views
	for ( uint32_t i = 0; i < MaxShadowViews; ++i )
	{
		renderViewCreateInfo_t info{};
		info.name = "Shadow View";
		info.region = renderViewRegion_t::SHADOW;
		info.viewId = viewCount;
		info.context = &renderContext;
		info.resources = &resources;
		info.isCubeView = ( resources.shadowMapImage[ i ]->info.type == imageType_t::IMAGE_TYPE_CUBE );

		info.fbImages.name = "ShadowFB";
		info.fbImages.context = &renderContext;
		info.fbImages.lifetime = resourceLifeTime_t::REBOOT;
		info.fbImages.swapBuffering = swapBuffering_t::SINGLE_FRAME;
		info.fbImages.depth = resources.shadowMapImage[ i ];

		info.clear = true;
		info.clearDepth = 1.0f;

		renderPassTransition_t& t = info.transition;		
		{
			t = {};
			t.flags.readOnly = true;
			t.flags.readAfter = true;
		}

		shadowViews[ i ] = &views[ viewCount ];
		shadowViews[ i ]->Init( info );
		++viewCount;
	}

	// Raster Views
	{
		renderViewCreateInfo_t info{};
		info.name = "Main View";
		info.region = renderViewRegion_t::STANDARD_RASTER;
		info.viewId = viewCount;
		info.context = &renderContext;
		info.resources = &resources;

		info.fbImages.name = "MainFB";
		info.fbImages.context = &renderContext;
		info.fbImages.lifetime = resourceLifeTime_t::REBOOT;
		info.fbImages.swapBuffering = swapBuffering_t::SINGLE_FRAME;
		info.fbImages.color0 = resources.mainColorImage;
		info.fbImages.color1 = resources.gBufferLayerImage;
		info.fbImages.depth = &resources.depthImageView;
		info.fbImages.stencil = &resources.stencilImageView;

		info.clear = true;
		info.clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );

		renderPassTransition_t& t = info.transition;
		{
			t = {};
			t.flags.readOnly = true;
			t.flags.readAfter = true;
		}

		renderViews[ 0 ] = &views[ viewCount ];
		renderViews[ 0 ]->Init( info );	
		++viewCount;
	}

	if ( config.useCubeViews )
	{
		renderViewCreateInfo_t info{};
		info.name = "Cube View";
		info.region = renderViewRegion_t::STANDARD_RASTER;
		info.viewId = viewCount;
		info.context = &renderContext;
		info.resources = &resources;

		info.isCubeView = true;
		info.fbImages.name = "CubeFB";
		info.fbImages.context = &renderContext;
		info.fbImages.lifetime = resourceLifeTime_t::REBOOT;
		info.fbImages.swapBuffering = swapBuffering_t::SINGLE_FRAME;
		info.fbImages.color0 = resources.cubeFbColorImage;
		info.fbImages.depth = resources.cubeFbDepthImage;

		info.clear = true;
		info.clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );

		renderPassTransition_t& t = info.transition;
		{
			t = {};
			t.flags.readOnly = true;
			t.flags.readAfter = true;
		}

		renderViews[ 1 ] = &views[ viewCount ];
		renderViews[ 1 ]->Init( info );
		++viewCount;
	}

	// 2D views
	{
		renderViewCreateInfo_t info{};
		info.name = "2D";
		info.region = renderViewRegion_t::STANDARD_2D;
		info.viewId = viewCount;
		info.context = &renderContext;
		info.resources = &resources;

		info.fbImages.name = "BackBufferFB";
		info.fbImages.context = &renderContext;
		info.fbImages.lifetime = resourceLifeTime_t::RESIZE;
		info.fbImages.swapBuffering = swapBuffering_t::MULTI_FRAME;
		info.fbImages.color0 = g_swapChain.GetBackBuffer();

		info.clear = true;
		info.clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );
		info.finalize = true;

		view2Ds[ 0 ] = &views[ viewCount ];
		view2Ds[ 0 ]->Init( info );

		++viewCount;
	}

	assert( viewCount <= ( MaxViews - 1 ) ); // Last view is reserved for temp views

	for ( uint32_t i = 0; i < MaxShadowViews; ++i ) {
		shadowViews[ i ]->Commit();
	}
	renderViews[ 0 ]->Commit();

	if( config.useCubeViews ) {
		renderViews[ 1 ]->Commit();
	}
	view2Ds[ 0 ]->Commit();

	InitImGui( view2Ds[ 0 ]->passes[ 0 ][ DRAWPASS_DEBUG_2D ]->GetFrameBuffer() );

	RenderViewContext viewContext;
	viewContext.activeViews = &activeViews[0];
	viewContext.renderViews = &renderViews[0];
	viewContext.shadowViews = &shadowViews[0];
	viewContext.view2Ds = &view2Ds[0];

	BuildSceneSchedule( config, &renderContext, &resources, &viewContext, &schedule );

	uploader.Boot( &renderContext, &resources );
	UploadAssets();
}


void Renderer::InitApi( const renderConfig_t& cfg )
{
	{
		// Device Set-up
		context.Create( g_window );

		InitConfig( cfg ); // Must be after device must be set-up, but before everything is initialized

		int width, height;
		g_window.GetWindowSize( width, height );
		g_swapChain.Create( &g_window, width, height );
	}

	{
		// Memory Allocations
		renderContext.sharedMemory.Create( MaxSharedMemory, memoryRegion_t::SHARED, resourceLifeTime_t::REBOOT );
		renderContext.localMemory.Create( MaxLocalMemory, memoryRegion_t::LOCAL, resourceLifeTime_t::REBOOT );
		renderContext.scratchMemory.Create( MaxScratchMemory, memoryRegion_t::LOCAL, resourceLifeTime_t::REBOOT );
	}

	{
		// Create Frame Resources
		renderContext.frameBufferMemory.Create( MaxFrameBufferMemory, memoryRegion_t::LOCAL, resourceLifeTime_t::RESIZE );

		CreateSyncObjects();
		CreateFramebuffers();

		gfxContext.Create( "GFX Context", &renderContext );
		computeContext.Create( "Compute Context", &renderContext );
		uploadContext.Create( "Upload Context", &renderContext );
	}

	{
		ShaderBindSet* bindset = nullptr;

		bindset = &renderContext.bindSets[ bindset_global ];
		bindset->Create( "GlobalBindings", g_globalBindings, COUNTARRAY( g_globalBindings ) );

		bindset = &renderContext.bindSets[ bindset_view ];
		bindset->Create( "ViewBindings", g_viewBindings, COUNTARRAY( g_viewBindings ) );

		bindset = &renderContext.bindSets[ bindset_pass ];
		bindset->Create( "PassBindings", g_passBindings, COUNTARRAY( g_passBindings ) );

		bindset = &renderContext.bindSets[ bindset_particle ];
		bindset->Create( "ParticleBindings", g_particleBindings, COUNTARRAY( g_particleBindings ) );

		bindset = &renderContext.bindSets[ bindset_compute ];
		bindset->Create( "ComputeBindings", g_computeBindings, COUNTARRAY( g_computeBindings ) );

		bindset = &renderContext.bindSets[ bindset_imageProcess ];
		bindset->Create( "ImageProcessBindings", g_imageProcessBindings, COUNTARRAY( g_imageProcessBindings ) );
	}
}


void Renderer::AssignBindSetsToGpuProgs()
{
	const ShaderBindSet& globalBindSet = renderContext.bindSets[ bindset_global ];
	const ShaderBindSet& viewBindSet = renderContext.bindSets[ bindset_view ];
	const ShaderBindSet& passBindSet = renderContext.bindSets[ bindset_pass ];
	const ShaderBindSet& imageProcessBindSet = renderContext.bindSets[ bindset_imageProcess ];

	{
		const uint32_t programCount = GpuProgramLib().Count();
		for ( uint32_t i = 0; i < programCount; ++i )
		{
			GpuProgram& prog = GpuProgramLib().Find( i )->Get();

			prog.bindsetCount = 0;

			if ( prog.type == pipelineType_t::RASTER )
			{
				prog.bindsets[ prog.bindsetCount ] = &globalBindSet;
				prog.bindsetCount += 1;

				if ( ( prog.flags & shaderFlags_t::IMAGE_SHADER ) == shaderFlags_t::NONE )
				{
					prog.bindsets[ prog.bindsetCount ] = &viewBindSet;
					prog.bindsetCount += 1;
				}
			}

			{
				auto it = renderContext.bindSets.find( prog.bindHash );
				if ( it != renderContext.bindSets.end() ) {
					prog.bindsets[ prog.bindsetCount ] = &it->second;
				}
				else {
					prog.bindsets[ prog.bindsetCount ] = &passBindSet;
				}
				prog.bindsetCount += 1;
			}
		}
	}
}


void Renderer::InitShaderResources()
{
	const ShaderBindSet& globalBindSet = renderContext.bindSets[ bindset_global ];
	const ShaderBindSet& particleBindSet = renderContext.bindSets[ bindset_particle ];

	renderContext.globalParms = renderContext.RegisterBindParm( &globalBindSet );

	{
		particleState.parms = renderContext.RegisterBindParm( &particleBindSet );
		particleState.x = ( MaxParticles / 256 );
	}

	materialBuffer.Reset();

	{
		rc.redImage = &TextureLib().Find( "_red" )->Get();
		rc.blueImage = &TextureLib().Find( "_green" )->Get();
		rc.greenImage = &TextureLib().Find( "_blue" )->Get();
		rc.whiteImage = &TextureLib().Find( "_white" )->Get();
		rc.blackImage = &TextureLib().Find( "_black" )->Get();
		rc.lightGreyImage = &TextureLib().Find( "_lightGrey" )->Get();
		rc.darkGreyImage = &TextureLib().Find( "_darkGrey" )->Get();
		rc.brownImage = &TextureLib().Find( "_brown" )->Get();
		rc.cyanImage = &TextureLib().Find( "_cyan" )->Get();
		rc.yellowImage = &TextureLib().Find( "_yellow" )->Get();
		rc.purpleImage = &TextureLib().Find( "_purple" )->Get();
		rc.orangeImage = &TextureLib().Find( "_orange" )->Get();
		rc.pinkImage = &TextureLib().Find( "_pink" )->Get();
		rc.goldImage = &TextureLib().Find( "_gold" )->Get();
		rc.albImage = &TextureLib().Find( "_alb" )->Get();
		rc.nmlImage = &TextureLib().Find( "_nml" )->Get();
		rc.rghImage = &TextureLib().Find( "_rgh" )->Get();
		rc.mtlImage = &TextureLib().Find( "_mtl" )->Get();
		rc.defaultImage = &TextureLib().Find( "_default" )->Get();
		rc.defaultImageCube = &TextureLib().Find( "_defaultCube" )->Get();

		rc.defaultImageArray.SetRenderContext( &renderContext );
		rc.defaultImageArray.Resize( 1 );
		rc.defaultImageArray.BindIndex( 0, rc.defaultImage );

		rc.defaultImageCubeArray.SetRenderContext( &renderContext );
		rc.defaultImageCubeArray.Resize( 1 );
		rc.defaultImageCubeArray.BindIndex( 0, rc.defaultImageCube );
	}

	// Buffers
	{
		resources.globalConstants.Create( 
			"Globals",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			1,
			sizeof( gpuGlobals_t ),
			bufferType_t::UNIFORM,
			renderContext.sharedMemory
		);
		resources.viewParms.Create(
			"View",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxViews * MaxMultiViews,
			sizeof( gpuView_t ),
			bufferType_t::STORAGE,
			renderContext.sharedMemory
		);
		resources.surfParms.Create(
			"Surf",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxViews * MaxSurfaces,
			sizeof( gpuSurface_t ),
			bufferType_t::STORAGE,
			renderContext.sharedMemory
		);
		resources.materialBuffers.Create(
			"Material",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxMaterials,
			sizeof( gpuMaterial_t ),
			bufferType_t::STORAGE,
			renderContext.sharedMemory
		);
		resources.lightParms.Create(
			"Light",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxLights,
			sizeof( gpuLight_t ),
			bufferType_t::STORAGE,
			renderContext.sharedMemory
		);
		resources.particleBuffer.Create(
			"Particle",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxParticles,
			sizeof( gpuParticle_t ),
			bufferType_t::STORAGE,
			renderContext.sharedMemory
		);
		resources.defaultUniformBuffer.Create(
			"DefaultUniformBuffer",
			swapBuffering_t::SINGLE_FRAME,
			resourceLifeTime_t::REBOOT,
			1,
			1024,
			bufferType_t::UNIFORM,
			renderContext.sharedMemory
		);

		for ( size_t v = 0; v < MaxViews; ++v ) {
			resources.surfParmPartitions[ v ] = resources.surfParms.GetView( v * MaxSurfaces, MaxSurfaces );
		}
	}
}


void Renderer::InitImGui( const FrameBuffer* fb )
{
#if defined( USE_IMGUI )
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// Setup Platform/Renderer bindings

#if defined( USE_VULKAN ) && defined( USE_GLFW )
	ImGui_ImplGlfw_InitForVulkan( g_window.window, true );
#endif

	assert( fb != nullptr );

	renderPassTransition_t transitionState {};
	transitionState.flags.clear = false;
	transitionState.flags.presentAfter = false;
	transitionState.flags.presentBefore = false;
	transitionState.flags.readOnly = true;
	transitionState.flags.readAfter = true;

	ImGui_ImplVulkan_InitInfo vkInfo = {};
	vkInfo.ApiVersion = VK_API_VERSION_1_2;
	vkInfo.Instance = context.instance;
	vkInfo.PhysicalDevice = context.physicalDevice;
	vkInfo.Device = context.device;
	vkInfo.QueueFamily = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
	vkInfo.Queue = context.gfxContext;
	vkInfo.PipelineCache = nullptr;
	vkInfo.DescriptorPool = context.descriptorPool;
	vkInfo.Allocator = nullptr;
	vkInfo.MinImageCount = MaxFrameStates;
	vkInfo.ImageCount = MaxFrameStates;
	vkInfo.CheckVkResultFn = nullptr;
#ifdef USE_VULKAN
	vkInfo.PipelineInfoMain.RenderPass = fb->GetVkRenderPass( transitionState );
#endif

#ifdef USE_VULKAN
	ImGui_ImplVulkan_Init( &vkInfo );
#endif

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
#ifdef USE_GLFW
	ImGui_ImplGlfw_NewFrame();
#endif
#endif
}


void Renderer::BuildPipelines()
{
	const uint32_t programCount = GpuProgramLib().Count();

	std::vector< Asset<GpuProgram>* > invalidAssets;
	invalidAssets.reserve( programCount );

	// 1. Collect stale shaders	
	for ( uint32_t progIx = 0; progIx < programCount; ++progIx )
	{
		Asset<GpuProgram>* progAsset = GpuProgramLib().Find( progIx );
		if ( progAsset == nullptr ) {
			continue;
		}

		if ( progAsset->IsUploaded() ) {
			continue;
		}
		invalidAssets.push_back( progAsset );
	}

	if( invalidAssets.size() == 0 ) {
		return;
	}

	AssignBindSetsToGpuProgs();

	FlushGPU();

	// 2. Destroy shaders
	for ( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		GpuProgram& prog = progAsset->Get();

		for ( uint32_t permIx = 0; permIx < prog.permCount; ++permIx )
		{
			for ( uint32_t shaderIx = 0; shaderIx < prog.shaderCount; ++shaderIx )
			{
				if ( prog.vk_shaders[ permIx ][ shaderIx ] != VK_NULL_HANDLE )
				{
					vkDestroyShaderModule( context.device, prog.vk_shaders[ permIx ][ shaderIx ], nullptr );
				}
			}
		}
	}

	// 3. Create shaders
	for ( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		GpuProgram& prog = progAsset->Get();

		for ( uint32_t permIx = 0; permIx < prog.permCount; ++permIx )
		{
			for ( uint32_t shaderIx = 0; shaderIx < prog.shaderCount; ++shaderIx )
			{
				const shaderBin_t& shaderBin = prog.shaderBins[ permIx ][ shaderIx ];
				prog.vk_shaders[ permIx ][ shaderIx ] = vk_CreateShaderModule( shaderBin.blob, shaderBin.binName.c_str() );
			}
		}
	}

	// 4. Collect all passes in active views
	std::vector<const DrawPass*> passes;
	passes.reserve( MaxViews * DRAWPASS_COUNT );

	for ( uint32_t viewIx = 0; viewIx < MaxViews; ++viewIx )
	{
		if( views[ viewIx ].IsCommitted() == false ) {
			continue;
		}

		const uint32_t multiViewCount = views[ viewIx ].GetMultiViewCount();
		for ( uint32_t multiViewIndex = 0; multiViewIndex < multiViewCount; ++multiViewIndex )
		{
			for ( int passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
			{
				const DrawPass* pass = views[ viewIx ].passes[ multiViewIndex ][ passIx ];
				if( pass != nullptr ) {
					passes.push_back( pass );
				}
			}
		}
	}

	// 5. Destroy pipelines. Own pass so cache isn't destroyed per iteration
	for ( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		GpuProgram& prog = progAsset->Get();

		if ( prog.shaders[ 0 ].type == shaderType_t::COMPUTE )
		{
			assert( prog.shaderCount == 1 );
			DestroyComputePipeline( *progAsset );
			continue;
		}

		const uint32_t passCount = static_cast< uint32_t >( passes.size() );
		for( uint32_t passIx = 0; passIx < passCount; ++passIx )
		{
			DestroyGraphicsPipeline( passes[ passIx ], *progAsset );

			uint32_t permSet = ( uint32_t )prog.permSet;
			if( permSet == 0 ) {
				continue;
			}

			uint32_t permBit = 0x01;

			while( permSet != 0 )
			{
				if( ( permSet & permBit ) == 0 )
				{
					permBit <<= 1;
					continue;
				}
				permSet &= ~( permSet & permBit );
				DestroyGraphicsPipeline( passes[ passIx ], *progAsset, static_cast< shaderPermId_t >( permBit ) );

				permBit <<= 1;
			}
		}
	}

	// 6. Create pipelines
	for ( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		progAsset->CompleteUpload();

		GpuProgram& prog = progAsset->Get();
		if ( prog.shaders[ 0 ].type == shaderType_t::COMPUTE )
		{
			assert( prog.shaderCount == 1 );
			CreateComputePipeline( *progAsset );
			continue;
		}

		const uint32_t passCount = static_cast<uint32_t>( passes.size() );
		for ( uint32_t passIx = 0; passIx < passCount; ++passIx )
		{
			CreateGraphicsPipeline( &renderContext, passes[ passIx ], *progAsset );

			uint32_t permSet = (uint32_t)prog.permSet;
			if( permSet == 0 ) {
				continue;
			}

			uint32_t permBit = 0x01;

			while( permSet != 0 )
			{	
				if( ( permSet & permBit ) == 0 )
				{
					permBit <<= 1;
					continue;
				}
				permSet &= ~( permSet & permBit );

				CreateGraphicsPipeline( &renderContext, passes[ passIx ], *progAsset, static_cast< shaderPermId_t >( permBit ) );

				permBit <<= 1;
			}
		}	
	}
}


void Renderer::CreateFramebuffers()
{
	int width = 0;
	int height = 0;
	g_window.QueryWindowFrameBufferSize( width, height );

	resources.RegisterOutputImages();

	// TODO: Force all FrameBuffers to be resize for now
	// TODO: Need new function or have resize callback/function that adjusts dimentions if marked as RESIZE
	const resourceLifeTime_t lifeTime = resourceLifeTime_t::RESIZE;

	// Shadow images
	for ( uint32_t shadowIx = 0; shadowIx < MaxShadowMaps; ++shadowIx )
	{
		imageInfo_t info{};
		info.width = ShadowMapWidth;
		info.height = ShadowMapHeight;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_D_32;
		info.type = IMAGE_TYPE_2D;
		info.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		info.tiling = IMAGE_TILING_MORTON;

		resources.shadowMapImage[ shadowIx ]->Create(
			info,
			nullptr,
			new GpuImage( "shadowMap", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, lifeTime )
		);
	}

	// Main images
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = config.mainColorSubSamples;
		info.fmt = IMAGE_FMT_RGBA_16;
		info.type = IMAGE_TYPE_2D;
		info.aspect = IMAGE_ASPECT_COLOR_FLAG;
		info.tiling = IMAGE_TILING_MORTON;

		resources.mainColorImage->Create(
			info,
			nullptr,
			new GpuImage( "mainColor", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, lifeTime )
		);
		
		resources.gBufferLayerImage->Create(
			info,
			nullptr,
			new GpuImage( "gBufferLayer", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, lifeTime )
		);
		
		info.fmt = IMAGE_FMT_D_32_S8;
		info.type = IMAGE_TYPE_2D;
		info.aspect = imageAspectFlags_t( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG );

		resources.depthStencilImage->Create(
			info,
			nullptr,
			new GpuImage( "viewDepth", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, lifeTime )
		);
	}

	// Cube images
	{
		extern CVar r_cubeWidth;
		extern CVar r_cubeHeight;

		imageInfo_t colorInfo{};
		colorInfo.width = r_cubeWidth.GetInt();
		colorInfo.height = r_cubeHeight.GetInt();
		colorInfo.mipLevels = MipCount( colorInfo.width, colorInfo.height );
		colorInfo.layers = 6;
		colorInfo.subsamples = IMAGE_SMP_1;
		colorInfo.fmt = IMAGE_FMT_RGBA_16;
		colorInfo.type = IMAGE_TYPE_CUBE;
		colorInfo.aspect = IMAGE_ASPECT_COLOR_FLAG;
		colorInfo.tiling = IMAGE_TILING_MORTON;

		resources.cubeFbColorImage->Create(
			colorInfo,
			nullptr,
			new GpuImage( "cubeColor", colorInfo, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, renderContext.frameBufferMemory, lifeTime )
		);

		imageInfo_t depthInfo = colorInfo;
		depthInfo.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		depthInfo.fmt = IMAGE_FMT_D_16;

		resources.cubeFbDepthImage->Create(
			depthInfo,
			nullptr,
			new GpuImage( "cubeDepth", depthInfo, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, lifeTime )
		);
	}

	// Resolve image
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = MipCount( info.width, info.height );
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = resources.mainColorImage->info.fmt;
		info.type = IMAGE_TYPE_2D;
		info.aspect = resources.mainColorImage->info.aspect;
		info.tiling = resources.mainColorImage->info.tiling;

		resources.mainColorResolvedImage->Create(
			info,
			nullptr,
			new GpuImage( "mainColorResolvedImage", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, renderContext.frameBufferMemory, lifeTime )
		);

		resources.blurredImage->Create(
			info,
			nullptr,
			new GpuImage( "blurredImage", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, renderContext.frameBufferMemory, lifeTime )
		);
	}

	// Depth-stencil views
	{
		imageInfo_t depthInfo = resources.depthStencilImage->info;
		depthInfo.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		resources.depthImageView.Init( resources.depthStencilImage, depthInfo, lifeTime );

		imageInfo_t stencilInfo = resources.depthStencilImage->info;
		stencilInfo.aspect = IMAGE_ASPECT_STENCIL_FLAG;
		resources.stencilImageView.Init( resources.depthStencilImage, stencilInfo, lifeTime );
	}

	// Resolve depth-stencil image
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_RG_32;
		info.type = IMAGE_TYPE_2D;
		info.aspect = IMAGE_ASPECT_COLOR_FLAG;
		info.tiling = resources.depthStencilImage->info.tiling;

		resources.depthStencilResolvedImage->Create(
			info,
			nullptr,
			new GpuImage( "depthStencilResolvedImage", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, lifeTime )
		);
	}

	// Depth-stencil views
	{
		resources.depthResolvedImageView.Init( resources.depthStencilResolvedImage, resources.depthStencilResolvedImage->info, lifeTime );
		resources.stencilResolvedImageView.Init( resources.depthStencilResolvedImage, resources.depthStencilResolvedImage->info, lifeTime );
	}

	// Bloom
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = MipCount( width, height );
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_RGBA_16;
		info.type = IMAGE_TYPE_2D;
		info.aspect = IMAGE_ASPECT_COLOR_FLAG;
		info.tiling = IMAGE_TILING_MORTON;

		resources.bloom->Create(
			info,
			nullptr,
			new GpuImage( "bloom", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, lifeTime )
		);
	}

	// Temp image
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = MipCount( width, height );
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_RGBA_16;
		info.type = IMAGE_TYPE_2D;
		info.aspect = IMAGE_ASPECT_COLOR_FLAG;
		info.tiling = IMAGE_TILING_MORTON;

		resources.tempColorImage->Create(
			info,
			nullptr,
			new GpuImage( "tempColor", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, lifeTime )
		);
	}

	// Previous Luminance
	{
		imageInfo_t info{};
		info.width = 1;
		info.height = 1;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_R_16;
		info.type = IMAGE_TYPE_2D;
		info.aspect = IMAGE_ASPECT_COLOR_FLAG;
		info.tiling = IMAGE_TILING_MORTON;

		resources.previousLum->Create(
			info,
			nullptr,
			new GpuImage( "previousLuminance", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_DST, renderContext.frameBufferMemory, lifeTime )
		);
	}

	// Luminance MIP-chain
	{
		imageInfo_t info{};
		info.width = 1024;
		info.height = 1024;
		info.mipLevels = MipCount( info.width, info.height );
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_R_16;
		info.type = IMAGE_TYPE_2D;
		info.aspect = IMAGE_ASPECT_COLOR_FLAG;
		info.tiling = IMAGE_TILING_MORTON;

		resources.currentLum->Create(
			info,
			nullptr,
			new GpuImage( "currentLuminance", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, lifeTime )
		);
	}
}


void Renderer::CreateSyncObjects()
{
	gfxContext.presentSemaphore.Create( "PresentSemaphore" );
	gfxContext.renderFinishedSemaphore.Create( "RenderSemaphore" );
	computeContext.semaphore.Create( "ComputeSemaphore" );

	uploadFinishedSemaphore.Create( "UploadSemaphore" );

	for ( size_t i = 0; i < MaxFrameStates; ++i ) {
		gfxContext.frameFence[ i ].Create( "FrameFence" );
	}

#ifdef USE_VULKAN
	uploadFinishedSemaphore.waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	gfxContext.presentSemaphore.waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
#endif
}
