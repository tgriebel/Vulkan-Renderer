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
#include <map>
#include "renderer.h"
#include <gfxcore/scene/entity.h>
#include "../render_state/rhi.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/bindings.h"
#include "../render_core/RenderTask.h"

#include "../draw_passes/drawpass.h"
#include "swapChain.h"

#if defined( USE_IMGUI )
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../external/imgui/backends/imgui_impl_vulkan.h"
#endif

#include "debugMenu.h"

void CreateImage( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory, Image& outImage )
{
	outImage.info = info;
	outImage.info.layers = ( info.type == IMAGE_TYPE_CUBE ) ? 6 : info.layers;

	outImage.subResourceView.baseArray = 0;
	outImage.subResourceView.arrayCount = outImage.info.layers;
	outImage.subResourceView.baseMip = 0;
	outImage.subResourceView.mipLevels = outImage.info.mipLevels;

	outImage.gpuImage = new GpuImage();
	outImage.gpuImage->Create( name, outImage.info, flags, memory );
}

void Renderer::Init()
{
	InitApi();

	resources.gpuImages2D.Resize( MaxImageDescriptors );
	resources.gpuImagesCube.Resize( MaxImageDescriptors );

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
		info.fb = &shadowMap[ i ];

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
		info.fb = &mainColor;

		renderViews[ 0 ] = &views[ viewCount ];
		renderViews[ 0 ]->Init( info );
		++viewCount;

		for ( uint32_t i = 1; i < Max3DViews; ++i )
		{
			renderViewCreateInfo_t info{};
			info.name = "Cube View";
			info.region = renderViewRegion_t::STANDARD_RASTER;
			info.viewId = viewCount;
			info.context = &renderContext;
			info.resources = &resources;
			info.fb = &cubeMapFrameBuffer[ i - 1 ];

			renderViews[ i ] = &views[ viewCount ];
			renderViews[ i ]->Init( info );
			++viewCount;
		}
	}

	// 2D views
	{
		renderViewCreateInfo_t info{};
		info.name = "2D";
		info.region = renderViewRegion_t::STANDARD_2D;
		info.viewId = viewCount;
		info.context = &renderContext;
		info.resources = &resources;
		info.fb = g_swapChain.GetFrameBuffer();

		view2Ds[ 0 ] = &views[ viewCount ];
		view2Ds[ 0 ]->Init( info );
		++viewCount;
	}

	assert( viewCount <= ( MaxViews - 1 ) ); // Last view is reserved for temp views

	for ( uint32_t i = 0; i < MaxShadowViews; ++i ) {
		shadowViews[ i ]->Commit();
	}
	renderViews[ 0 ]->Commit();

	const bool useCubeViews = true;
	if( useCubeViews ) {
		for ( uint32_t i = 1; i < Max3DViews; ++i ) {
			renderViews[ i ]->Commit();
		}
	}
	view2Ds[ 0 ]->Commit();

	for ( uint32_t i = 0; i < 6; ++i )
	{
		imageProcessCreateInfo_t info = {};
		info.name = "DiffuseIBL";
		info.clear = false;
		info.progHdl = AssetLibGpuProgram::Handle( "DiffuseIBL" );
		info.fb = &diffuseIblFrameBuffer[ i ];
		info.context = &renderContext;
		info.resources = &resources;
		info.inputImages = 1;

		Camera camera = Camera( vec4f( 0.0f, 0.0f, 0.0f, 0.0f ) );
		camera.SetFov( Radians( 90.0f ) );
		camera.SetAspectRatio( 1.0f );

		switch( i )
		{
			case IMAGE_CUBE_FACE_X_POS:	camera.Pan( 0.0f * PI );	break;
			case IMAGE_CUBE_FACE_Y_POS:	camera.Pan( 0.5f * PI );	break;
			case IMAGE_CUBE_FACE_X_NEG:	camera.Pan( 1.0f * PI );	break;
			case IMAGE_CUBE_FACE_Y_NEG:	camera.Pan( 1.5f * PI );	break;
			case IMAGE_CUBE_FACE_Z_POS:	camera.Tilt( -0.5f * PI );	break;
			case IMAGE_CUBE_FACE_Z_NEG:	camera.Tilt( 0.5f * PI );	break;
		}

		diffuseIBL[ i ] = new ImageProcess( info );

		mat4x4f viewMatrix = camera.GetViewMatrix().Transpose(); // FIXME: row/column-order
		viewMatrix[ 3 ][ 3 ] = 0.0f;

		diffuseIBL[ i ]->SetSourceImage( 0, &resources.cubeFbImageView );
		diffuseIBL[ i ]->SetConstants( &viewMatrix, sizeof( mat4x4f ) );
	}

	{
		imageProcessCreateInfo_t info = {};
		info.name = "ResolveMain";
		info.clear = false;
		info.resolve = true;
		if ( ForceDisableMSAA ) {
			info.progHdl = AssetLibGpuProgram::Handle( "Resolve" );
		} else {
			info.progHdl = AssetLibGpuProgram::Handle( "ResolveMSAA" );
		}
		info.fb = &mainColorResolved;
		info.context = &renderContext;
		info.resources = &resources;
		info.inputImages = 3;

		resolve = new ImageProcess( info );

		resolve->SetSourceImage( 0, &resources.mainColorImage );
		resolve->SetSourceImage( 1, &resources.depthImageView );
		resolve->SetSourceImage( 2, &resources.stencilImageView );
	}

	{
		imageProcessCreateInfo_t info = {};
		info.name = "Separable Gaussian";
		info.progHdl = AssetLibGpuProgram::Handle( "SeparableGaussianBlur" );
		info.fb = &tempColor;
		info.context = &renderContext;
		info.resources = &resources;
		info.inputImages = 1;

		uint32_t verticalPass = 0;

		pingPongQueue[ 0 ] = new ImageProcess( info );
		pingPongQueue[ 0 ]->SetSourceImage( 0, &resources.mainColorResolvedImage );
		pingPongQueue[ 0 ]->SetConstants( &verticalPass, sizeof( uint32_t ) );

		verticalPass = 1;

		info.fb = &mainColorResolved;
		pingPongQueue[ 1 ] = new ImageProcess( info );
		pingPongQueue[ 1 ]->SetSourceImage( 0, &resources.tempColorImage );
		pingPongQueue[ 1 ]->SetConstants( &verticalPass, sizeof( uint32_t ) );
	}

	/*
	MipImageTask* mipTask;
	{
		mipProcessCreateInfo_t info{};
		info.name = "MainColorDownsample";
		info.context = &renderContext;
		info.resources = &resources;
		info.img = &resources.mainColorResolvedImage;
		info.mode = downSampleMode_t::DOWNSAMPLE_LINEAR;

		mipTask = new MipImageTask( info );
	}
	*/

	ImageWritebackTask* imageWriteBackTask;
	{
		imageWriteBackCreateInfo_t info{};
		info.name = "EnvironmentMapWriteback";
		info.context = &renderContext;
		info.resources = &resources;
		//info.img = &resources.diffuseIblImageViews[ 0 ];
		info.fileName = "hdrEnvmap.img";
		info.writeToDiskOnFrameEnd = true;
		info.cubemap = true;

		for( uint32_t i = 0; i < 6; ++i ) {
			info.imgCube[ i ] = &resources.diffuseIblImageViews[ i ];
		}

		imageWriteBackTask = new ImageWritebackTask( info );
	}

	InitShaderResources();

	InitImGui( *view2Ds[ 0 ] );

	UploadAssets();

	for ( uint32_t i = 0; i < MaxShadowViews; ++i ) {
		schedule.Queue( new RenderTask( shadowViews[ i ], DRAWPASS_SHADOW_BEGIN, DRAWPASS_SHADOW_END ) );
	}
	schedule.Queue( new RenderTask( renderViews[ 0 ], DRAWPASS_MAIN_BEGIN, DRAWPASS_MAIN_END ) );
	if ( useCubeViews ) {
		for ( uint32_t i = 1; i < Max3DViews; ++i ) {
			schedule.Queue( new RenderTask( renderViews[ i ], DRAWPASS_MAIN_BEGIN, DRAWPASS_MAIN_END ) );
		}
		for ( uint32_t i = 0; i < 6; ++i ) {
			schedule.Queue( diffuseIBL[ i ] );
		}
	}
	schedule.Queue( resolve );
	//schedule.Queue( imageWriteBackTask );
	//schedule.Queue( new CopyImageTask( &resources.mainColorResolvedImage, &resources.tempWritebackImage ) );
	//schedule.Queue( new TransitionImageTask( &mainColorDownsampled, GPU_IMAGE_NONE, GPU_IMAGE_TRANSFER_DST ) );
	//schedule.Queue( new CopyImageTask( &mainColorResolvedImage, &mainColorDownsampled ) );
	//schedule.Queue( new MipImageTask( &mainColorDownsampled ) );
	//schedule.Queue( new TransitionImageTask( &resources.mainColorResolvedImage, GPU_IMAGE_NONE, GPU_IMAGE_READ ) );
	//schedule.Queue( mipTask );
	//schedule.Queue( pingPongQueue[ 0 ] );
	//schedule.Queue( pingPongQueue[ 1 ] );
	schedule.Queue( new RenderTask( view2Ds[ 0 ], DRAWPASS_MAIN_BEGIN, DRAWPASS_MAIN_END ) );
	schedule.Queue( new ComputeTask( "ClearParticles", &particleState ) );
}


void Renderer::InitApi()
{
	{
		// Device Set-up
		context.Create( g_window );

		int width, height;
		g_window.GetWindowSize( width, height );
		g_swapChain.Create( &g_window, width, height );
	}

	{
		// Memory Allocations
		renderContext.sharedMemory.Create( MaxSharedMemory, memoryRegion_t::SHARED );
		renderContext.localMemory.Create( MaxLocalMemory, memoryRegion_t::LOCAL );
	}

	InitConfig();

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );

	{
		// Create Frame Resources
		renderContext.frameBufferMemory.Create( MaxFrameBufferMemory, memoryRegion_t::LOCAL );

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


void Renderer::InitShaderResources()
{
	const ShaderBindSet& globalBindSet = renderContext.bindSets[ bindset_global ];
	const ShaderBindSet& viewBindSet = renderContext.bindSets[ bindset_view ];
	const ShaderBindSet& passBindSet = renderContext.bindSets[ bindset_pass ];
	const ShaderBindSet& imageProcessBindSet = renderContext.bindSets[ bindset_imageProcess ];
	const ShaderBindSet& particleBindSet = renderContext.bindSets[ bindset_particle ];

	{
		const uint32_t programCount = g_assets.gpuPrograms.Count();
		for ( uint32_t i = 0; i < programCount; ++i )
		{
			GpuProgram& prog = g_assets.gpuPrograms.Find( i )->Get();
			
			prog.bindsetCount = 0;

			if( prog.type == pipelineType_t::RASTER )
			{
				prog.bindsets[ prog.bindsetCount ] = &globalBindSet;
				prog.bindsetCount += 1;

				if ( ( prog.flags & SHADER_FLAG_IMAGE_SHADER ) == 0 )
				{			
					prog.bindsets[ prog.bindsetCount ] = &viewBindSet;
					prog.bindsetCount += 1;
				}
			}

			{
				auto it = renderContext.bindSets.find( prog.bindHash );
				if( it != renderContext.bindSets.end() ) {
					prog.bindsets[ prog.bindsetCount ] = &it->second;
				} else {
					prog.bindsets[ prog.bindsetCount ] = &passBindSet;
				}
				prog.bindsetCount += 1;
			}
		}
	}

	renderContext.globalParms = renderContext.RegisterBindParm( &globalBindSet );

	{
		particleState.parms = renderContext.RegisterBindParm( &particleBindSet );
		particleState.x = ( MaxParticles / 256 );
	}

	materialBuffer.Reset();

	{
		rc.redImage = &g_assets.textureLib.Find( "_red" )->Get();
		rc.blueImage = &g_assets.textureLib.Find( "_green" )->Get();
		rc.greenImage = &g_assets.textureLib.Find( "_blue" )->Get();
		rc.whiteImage = &g_assets.textureLib.Find( "_white" )->Get();
		rc.blackImage = &g_assets.textureLib.Find( "_black" )->Get();
		rc.lightGreyImage = &g_assets.textureLib.Find( "_lightGrey" )->Get();
		rc.darkGreyImage = &g_assets.textureLib.Find( "_darkGrey" )->Get();
		rc.brownImage = &g_assets.textureLib.Find( "_brown" )->Get();
		rc.cyanImage = &g_assets.textureLib.Find( "_cyan" )->Get();
		rc.yellowImage = &g_assets.textureLib.Find( "_yellow" )->Get();
		rc.purpleImage = &g_assets.textureLib.Find( "_purple" )->Get();
		rc.orangeImage = &g_assets.textureLib.Find( "_orange" )->Get();
		rc.pinkImage = &g_assets.textureLib.Find( "_pink" )->Get();
		rc.goldImage = &g_assets.textureLib.Find( "_gold" )->Get();
		rc.albImage = &g_assets.textureLib.Find( "_alb" )->Get();
		rc.nmlImage = &g_assets.textureLib.Find( "_nml" )->Get();
		rc.rghImage = &g_assets.textureLib.Find( "_rgh" )->Get();
		rc.mtlImage = &g_assets.textureLib.Find( "_mtl" )->Get();
		rc.checkerboardImage = &g_assets.textureLib.Find( "_checkerboard" )->Get();
	}

	{
		resources.globalConstants.Create( "Globals", LIFETIME_PERSISTENT, 1, sizeof( viewBufferObject_t ), bufferType_t::UNIFORM, renderContext.sharedMemory );
		resources.viewParms.Create( "View", LIFETIME_PERSISTENT, MaxViews, sizeof( viewBufferObject_t ), bufferType_t::STORAGE, renderContext.sharedMemory );
		resources.surfParms.Create( "Surf", LIFETIME_PERSISTENT, MaxViews * MaxSurfaces, sizeof( surfaceBufferObject_t ), bufferType_t::STORAGE, renderContext.sharedMemory );
		resources.materialBuffers.Create( "Material", LIFETIME_PERSISTENT, MaxMaterials, sizeof( materialBufferObject_t ), bufferType_t::STORAGE, renderContext.sharedMemory );
		resources.lightParms.Create( "Light", LIFETIME_PERSISTENT, MaxLights, sizeof( lightBufferObject_t ), bufferType_t::STORAGE, renderContext.sharedMemory );
		resources.particleBuffer.Create( "Particle", LIFETIME_PERSISTENT, MaxParticles, sizeof( particleBufferObject_t ), bufferType_t::STORAGE, renderContext.sharedMemory );

		for ( size_t v = 0; v < MaxViews; ++v ) {
			resources.surfParmPartitions[ v ] = resources.surfParms.GetView( v * MaxSurfaces, MaxSurfaces );
		}

		geometry.vb.Create( "VB", LIFETIME_TEMP, MaxVertices, sizeof( vsInput_t ), bufferType_t::VERTEX, renderContext.localMemory );
		geometry.ib.Create( "IB", LIFETIME_TEMP, MaxIndices, sizeof( uint32_t ), bufferType_t::INDEX, renderContext.localMemory );

		geometry.stagingBuffer.Create( "Geo Staging", LIFETIME_TEMP, 1, 16 * MB_1, bufferType_t::STAGING, renderContext.sharedMemory );
		textureStagingBuffer.Create( "Texture Staging", LIFETIME_TEMP, 1, 192 * MB_1, bufferType_t::STAGING, renderContext.sharedMemory );
	}
}


void Renderer::InitImGui( RenderView& view )
{
#if defined( USE_IMGUI )
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForVulkan( g_window.window, true );

	ImGui_ImplVulkan_InitInfo vkInfo = {};
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

	assert( view.passes[ DRAWPASS_DEBUG_2D ] != nullptr );
	const renderPassTransition_t transitionState = view.TransitionState();
	ImGui_ImplVulkan_Init( &vkInfo, view.passes[ DRAWPASS_DEBUG_2D ]->GetFrameBuffer()->GetVkRenderPass( transitionState ) );

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Upload Fonts
	{
		BeginUploadCommands( uploadContext );
		VkCommandBuffer commandBuffer = uploadContext.CommandBuffer();
		ImGui_ImplVulkan_CreateFontsTexture( commandBuffer );
		EndUploadCommands( uploadContext );

		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}
	ImGui_ImplGlfw_NewFrame();

	g_imguiControls.raytraceScene = false;
	g_imguiControls.rasterizeScene = false;
	g_imguiControls.rebuildRaytraceScene = false;
	g_imguiControls.rebuildShaders = false;
	g_imguiControls.heightMapHeight = 1.0f;
	g_imguiControls.roughness = 0.9f;
	g_imguiControls.shadowStrength = 0.99f;
	g_imguiControls.toneMapColor[ 0 ] = 1.0f;
	g_imguiControls.toneMapColor[ 1 ] = 1.0f;
	g_imguiControls.toneMapColor[ 2 ] = 1.0f;
	g_imguiControls.toneMapColor[ 3 ] = 1.0f;
	g_imguiControls.dbgImageId = -1;
	g_imguiControls.selectedEntityId = -1;
	g_imguiControls.selectedModelOrigin = vec3f( 0.0f );

#endif
}


void Renderer::BuildPipelines()
{
	const uint32_t programCount = g_assets.gpuPrograms.Count();

	std::vector< Asset<GpuProgram>* > invalidAssets;
	invalidAssets.reserve( programCount );

	// 1. Collect stale shaders	
	for ( uint32_t progIx = 0; progIx < programCount; ++progIx )
	{
		Asset<GpuProgram>* progAsset = g_assets.gpuPrograms.Find( progIx );
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

	FlushGPU();

	// 2. Destroy shaders
	for ( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		GpuProgram& prog = progAsset->Get();
		for ( uint32_t shaderIx = 0; shaderIx < prog.shaderCount; ++shaderIx ) {
			if ( prog.vk_shaders[ shaderIx ] != VK_NULL_HANDLE ) {
				vkDestroyShaderModule( context.device, prog.vk_shaders[ shaderIx ], nullptr );
			}
		}
	}

	// 3. Create shaders
	for ( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		GpuProgram& prog = progAsset->Get();
		for ( uint32_t shaderIx = 0; shaderIx < prog.shaderCount; ++shaderIx ) {
			prog.vk_shaders[ shaderIx ] = vk_CreateShaderModule( prog.shaders[ shaderIx ].blob );
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

		for ( int passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			const DrawPass* pass = views[ viewIx ].passes[ passIx ];
			if( pass != nullptr ) {
				passes.push_back( pass );
			}
		}
	}

	// 5. Destroy pipelines. Own pass so cache isn't destroyed per iteration
	for ( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		progAsset->CompleteUpload();

		GpuProgram& prog = progAsset->Get();
		if ( prog.shaders[ 0 ].type == shaderType_t::COMPUTE )
		{
			assert( prog.shaderCount == 1 );
			DestroyComputePipeline( *progAsset );
			continue;
		}

		const uint32_t passCount = static_cast<uint32_t>( passes.size() );
		for ( uint32_t passIx = 0; passIx < passCount; ++passIx ) {
			DestroyGraphicsPipeline( passes[ passIx ], *progAsset );
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
		for ( uint32_t passIx = 0; passIx < passCount; ++passIx ) {
			CreateGraphicsPipeline( &renderContext, passes[ passIx ], *progAsset );
		}	
	}
}


void Renderer::CreateFramebuffers()
{
	int width = 0;
	int height = 0;
	g_window.GetWindowFrameBufferSize( width, height );

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

		CreateImage( "shadowMap", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resources.shadowMapImage[ shadowIx ] );
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

		CreateImage( "mainColor", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resources.mainColorImage );
		CreateImage( "gBufferLayer", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resources.gBufferLayerImage );
		
		info.fmt = IMAGE_FMT_D_32_S8;
		info.type = IMAGE_TYPE_2D;
		info.aspect = imageAspectFlags_t( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG );

		CreateImage( "viewDepth", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resources.depthStencilImage );
	}

	const int glslCubeMapping[ 6 ] = { 4, 5, 1, 0, 2, 3 };

	// Cube images
	{
		imageInfo_t colorInfo{};
		colorInfo.width = 512;
		colorInfo.height = 512;
		colorInfo.mipLevels = 1;
		colorInfo.layers = 6;
		colorInfo.subsamples = IMAGE_SMP_1;
		colorInfo.fmt = IMAGE_FMT_RGBA_16;
		colorInfo.type = IMAGE_TYPE_CUBE;
		colorInfo.aspect = IMAGE_ASPECT_COLOR_FLAG;
		colorInfo.tiling = IMAGE_TILING_MORTON;

		CreateImage( "cubeColor", colorInfo, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resources.cubeFbColorImage );

		imageInfo_t depthInfo = colorInfo;
		depthInfo.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		depthInfo.fmt = IMAGE_FMT_D_16;

		CreateImage( "cubeDepth", depthInfo, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resources.cubeFbDepthImage );

		resources.cubeFbImageView.Init( resources.cubeFbColorImage, colorInfo );

		for ( uint32_t i = 0; i < 6; ++i )
		{
			imageSubResourceView_t subView;
			subView.arrayCount = 1;
			subView.baseArray = glslCubeMapping[ i ];
			subView.baseMip = 0;
			subView.mipLevels = 1;

			colorInfo.type = IMAGE_TYPE_2D;
			depthInfo.type = IMAGE_TYPE_2D;

			resources.cubeImageViews[ i ].Init( resources.cubeFbColorImage, colorInfo, subView );
			resources.cubeDepthImageViews[ i ].Init( resources.cubeFbDepthImage, depthInfo, subView );
		}
	}

	// Diffuse IBL images
	{
		imageInfo_t colorInfo{};
		colorInfo.width = 128;
		colorInfo.height = 128;
		colorInfo.mipLevels = 1;
		colorInfo.layers = 6;
		colorInfo.channels = 4;
		colorInfo.subsamples = IMAGE_SMP_1;
		colorInfo.fmt = IMAGE_FMT_RGBA_16;
		colorInfo.type = IMAGE_TYPE_CUBE;
		colorInfo.aspect = IMAGE_ASPECT_COLOR_FLAG;
		colorInfo.tiling = IMAGE_TILING_MORTON;

		CreateImage( "diffuseIblColor", colorInfo, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resources.diffuseIblImage );

		for ( uint32_t i = 0; i < 6; ++i )
		{
			imageSubResourceView_t subView;
			subView.arrayCount = 1;
			subView.baseArray = glslCubeMapping[ i ];
			subView.baseMip = 0;
			subView.mipLevels = 1;

			colorInfo.type = IMAGE_TYPE_2D;

			resources.diffuseIblImageViews[ i ].Init( resources.diffuseIblImage, colorInfo, subView );
		}
	}

	// Resolve image
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = MipCount( info.width, info.height );
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = resources.mainColorImage.info.fmt;
		info.type = IMAGE_TYPE_2D;
		info.aspect = resources.mainColorImage.info.aspect;
		info.tiling = resources.mainColorImage.info.tiling;

		CreateImage( "mainColorResolvedImage", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST, renderContext.frameBufferMemory, resources.mainColorResolvedImage );

		info.mipLevels = 1;
		resources.mainColorResolvedImageView.Init( resources.mainColorResolvedImage, info );
	}

	// Depth-stencil views
	{
		imageInfo_t depthInfo = resources.depthStencilImage.info;
		depthInfo.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		resources.depthImageView.Init( resources.depthStencilImage, depthInfo );

		imageInfo_t stencilInfo = resources.depthStencilImage.info;
		stencilInfo.aspect = IMAGE_ASPECT_STENCIL_FLAG;
		resources.stencilImageView.Init( resources.depthStencilImage, stencilInfo );
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
		info.tiling = resources.depthStencilImage.info.tiling;

		CreateImage( "depthStencilResolvedImage", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resources.depthStencilResolvedImage );
	}

	// Depth-stencil views
	{
		resources.depthResolvedImageView.Init( resources.depthStencilResolvedImage, resources.depthStencilResolvedImage.info );
		resources.stencilResolvedImageView.Init( resources.depthStencilResolvedImage, resources.depthStencilResolvedImage.info );
	}

	// Temp image
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_RGBA_16;
		info.type = IMAGE_TYPE_2D;
		info.aspect = IMAGE_ASPECT_COLOR_FLAG;
		info.tiling = IMAGE_TILING_MORTON;

		CreateImage( "tempColor", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resources.tempColorImage );
	}

	// Image writeback
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_RGBA_8_UNORM;
		info.type = IMAGE_TYPE_2D;
		info.aspect = IMAGE_ASPECT_COLOR_FLAG;
		info.tiling = IMAGE_TILING_LINEAR;

		CreateImage( "tempWritebackImage", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, renderContext.sharedMemory, resources.tempWritebackImage );
	}

	// Main Color Resolved Frame buffer
	{
		frameBufferCreateInfo_t fbInfo;
		fbInfo.name = "MainColorResolveFB";
		fbInfo.color0[ 0 ] = &resources.mainColorResolvedImageView;
		fbInfo.color1[ 0 ] = &resources.depthStencilResolvedImage;
		fbInfo.lifetime = LIFETIME_TEMP;

		mainColorResolved.Create( fbInfo );
	}

	// Temp Frame buffer
	{
		frameBufferCreateInfo_t fbInfo;
		fbInfo.name = "TempColorFB";
		fbInfo.color0[ 0 ] = &resources.tempColorImage;
		fbInfo.lifetime = LIFETIME_TEMP;

		tempColor.Create( fbInfo );
	}

	// Shadow map
	for ( uint32_t shadowIx = 0; shadowIx < MaxShadowMaps; ++shadowIx )
	{	
		frameBufferCreateInfo_t fbInfo;
		fbInfo.name = "ShadowMapFB";
		fbInfo.depth[ 0 ] = &resources.shadowMapImage[ shadowIx ];
		fbInfo.lifetime = LIFETIME_TEMP;

		shadowMap[ shadowIx ].Create( fbInfo );
	}

	// Main Scene 3D Render
	{
		frameBufferCreateInfo_t fbInfo;
		fbInfo.name = "MainColorFB";
		for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx )
		{
			fbInfo.color0[ frameIx ] = &resources.mainColorImage;
			fbInfo.color1[ frameIx ] = &resources.gBufferLayerImage;
			fbInfo.depth[ frameIx ] = &resources.depthImageView;
			fbInfo.stencil[ frameIx ] = &resources.stencilImageView;
		}
		fbInfo.lifetime = LIFETIME_TEMP;

		mainColor.Create( fbInfo );
	}

	// Cubemap Render
	{
		for ( uint32_t i = 0; i < 6; ++i ) {
			frameBufferCreateInfo_t fbInfo;
			fbInfo.name = "CubeColorFB";
			for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx ) {
				fbInfo.color0[ frameIx ] = &resources.cubeImageViews[ i ];
				fbInfo.depth[ frameIx ] = &resources.cubeDepthImageViews[ i ];
			}
			fbInfo.lifetime = LIFETIME_TEMP;

			cubeMapFrameBuffer[ i ].Create( fbInfo );
		}
	}

	// Diffuse IBL Render
	{
		for ( uint32_t i = 0; i < 6; ++i ) {
			frameBufferCreateInfo_t fbInfo;
			fbInfo.name = "DiffuseIblFB";
			for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx ) {
				fbInfo.color0[ frameIx ] = &resources.diffuseIblImageViews[ i ];
			}
			fbInfo.lifetime = LIFETIME_TEMP;

			diffuseIblFrameBuffer[ i ].Create( fbInfo );
		}
	}
}


ShaderBindParms* RenderContext::RegisterBindParm( const ShaderBindSet* set )
{
	ShaderBindParms parms = ShaderBindParms( set );

	pendingIndices.Append( bindParmsList.Count() );
	bindParmsList.Append( parms );

	return &bindParmsList[ bindParmsList.Count() - 1 ];
}


ShaderBindParms* RenderContext::RegisterBindParm( const uint64_t setId )
{
	return RegisterBindParm( &bindSets[ setId ] );
}


ShaderBindParms* RenderContext::RegisterBindParm( const char* setName )
{
	return RegisterBindParm( &bindSets[ Hash( setName ) ] );
}


const ShaderBindSet* RenderContext::LookupBindSet( const uint64_t setId ) const
{
	auto it = bindSets.find( setId );
	if( it != bindSets.end() ) {
		return &it->second;
	} else {
		return nullptr;
	}
}


const ShaderBindSet* RenderContext::LookupBindSet( const char* name ) const
{
	return LookupBindSet( Hash( name ) );
}


void RenderContext::AllocRegisteredBindParms()
{
	//SCOPED_TIMER_PRINT( AllocRegisteredBindParms )

	const uint32_t pendingParmCount = pendingIndices.Count();
	if( pendingParmCount == 0 ) {
		return;
	}

	std::vector<VkDescriptorSetLayout> layouts;
	std::vector<VkDescriptorSet> descSets;

	layouts.reserve( MaxFrameStates * pendingParmCount );
	descSets.reserve( MaxFrameStates * pendingParmCount );

	for ( uint32_t i = 0; i < pendingParmCount; ++i )
	{
		const uint32_t bindIx = pendingIndices[ i ];
		ShaderBindParms& parms = bindParmsList[ bindIx ];
		const ShaderBindSet* set = parms.GetSet();

		for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx )
		{
			layouts.push_back( set->GetVkObject() );
			descSets.push_back( VK_NULL_HANDLE );
		}
	}

	VkDescriptorSetAllocateInfo allocInfo{ };
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = context.descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>( layouts.size() );
	allocInfo.pSetLayouts = layouts.data();

	VK_CHECK_RESULT( vkAllocateDescriptorSets( context.device, &allocInfo, descSets.data() ) );

	for ( uint32_t i = 0; i < pendingParmCount; ++i )
	{
		const uint32_t bindIx = pendingIndices[ i ];
		ShaderBindParms& parms = bindParmsList[ bindIx ];
		parms.SetVkObject( &descSets[ MaxFrameStates * bindIx ] );
	}
	pendingIndices.Reset();
}


void RenderContext::FreeRegisteredBindParms()
{
	std::vector<VkDescriptorSet> descSets;
	descSets.reserve( bindParmsList.Count() );

	for ( uint32_t i = 0; i < bindParmsList.Count(); ++i )
	{
		ShaderBindParms& parms = bindParmsList[ i ];
		descSets.push_back( parms.GetVkObject() );
	}

	vkFreeDescriptorSets( context.device, context.descriptorPool, static_cast<uint32_t>( descSets.size() ), descSets.data() );

	pendingIndices.Reset();
	bindParmsList.Reset();
}


void RenderContext::RefreshRegisteredBindParms()
{
	for ( uint32_t i = 0; i < bindParmsList.Count(); ++i ) {
		bindParmsList[ i ].Clear();
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