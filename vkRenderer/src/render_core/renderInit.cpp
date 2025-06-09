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

void Renderer::Init( const renderConfig_t& cfg )
{
	InitApi( cfg );

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

	if( config.useCubeViews ) {
		for ( uint32_t i = 1; i < Max3DViews; ++i ) {
			renderViews[ i ]->Commit();
		}
	}
	view2Ds[ 0 ]->Commit();

	ImageProcess* diffuseIBL[ 6 ] = {};
	MipImageTask* specularIBL[ 6 ] = {};
	CopyImageTask* copyCubeToSpecularIbl = nullptr;
	if ( config.useCubeViews )
	{
		const int glslCubeMapping[ 6 ] = { 4, 5, 1, 0, 2, 3 };

		for ( uint32_t i = 0; i < 6; ++i )
		{
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

			if ( config.computeDiffuseIbl )
			{
				imageProcessCreateInfo_t info = {};
				info.name = "DiffuseIBL";
				info.clear = false;
				info.progHdl = AssetLibGpuProgram::Handle( "DiffuseIBL" );
				info.fb = &diffuseIblFrameBuffer[ i ];
				info.context = &renderContext;
				info.resources = &resources;
				info.inputCubeImages = 1;

				diffuseIBL[ i ] = new ImageProcess( info );

				mat4x4f viewMatrix = camera.GetViewMatrix().Transpose(); // FIXME: row/column-order
				viewMatrix[ 3 ][ 3 ] = 0.0f;

				diffuseIBL[ i ]->SetSourceCubeImage( 0, &g_assets.textureLib.Find( "code_assets/hdrEnvmap.img" )->Get() ); // FIXME: Stub
				diffuseIBL[ i ]->SetConstants( &viewMatrix, sizeof( mat4x4f ) );
			}

			if ( config.computeSpecularIBL )
			{
				copyCubeToSpecularIbl = new CopyImageTask( &resources.cubeFbColorImage, &resources.specularIblImage );

				mipProcessCreateInfo_t info = {};
				info.name = "SpecularIbl";
				info.img = &resources.specularIblImage;
				info.layer = glslCubeMapping[ i ];
				info.context = &renderContext;
				info.resources = &resources;
				info.mode = downSampleMode_t::DOWNSAMPLE_SPECULAR_IBL;

				struct SpecularIblConstants
				{
					mat4x4f		viewMat;
					float		roughness;
				} specConstants;

				specConstants.viewMat = camera.GetViewMatrix().Transpose(); // FIXME: row/column-order

				specularIBL[ i ] = new MipImageTask( info );

				const uint32_t mipLevels = specularIBL[ i ]->GetMipCount();
				for( uint32_t mip = 1; mip < mipLevels; ++mip )
				{
					specConstants.roughness = mip / static_cast<float>( mipLevels - 1 );
					specularIBL[ i ]->SetConstantsForLevel( mip, &specConstants, sizeof( specConstants ) );
					specularIBL[ i ]->SetSourceImageForLevel( mip, &g_assets.textureLib.Find( "code_assets/hdrEnvmap.img" )->Get() ); // FIXME: Stub
				}
			}
		}
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

	if ( config.gaussianBlur )
	{
		imageProcessCreateInfo_t info = {};
		info.name = "Separable Gaussian";
		info.progHdl = AssetLibGpuProgram::Handle( "SeparableGaussianBlur" );
		info.context = &renderContext;
		info.resources = &resources;
		info.inputImages = 1;

		uint32_t verticalPass = 0;

		const uint32_t mipCount = resources.blurredImage.info.mipLevels;
		const uint32_t imagePassCount = 2 * mipCount;

		pingPongQueue.resize( imagePassCount );

		// Foreach Mip-Level: Downscale Image -> Blur Horizontal -> Temp -> Blur Vertical -> Blurred Image
		for ( uint32_t passNum = 0; passNum < mipCount; ++passNum )
		{
			info.fb = &tempColor;
			pingPongQueue[ 2 * passNum + 0 ] = new ImageProcess( info );
			pingPongQueue[ 2 * passNum + 0 ]->SetSourceImage( 0, &resources.mainColorResolvedImageViews[ passNum ] );
			pingPongQueue[ 2 * passNum + 0 ]->SetConstants( &verticalPass, sizeof( uint32_t ) );

			verticalPass = 1;

			info.fb = &blurredImageFrameBuffers[ passNum ];
			pingPongQueue[ 2 * passNum + 1 ] = new ImageProcess( info );
			pingPongQueue[ 2 * passNum + 1 ]->SetSourceImage( 0, &resources.tempColorImage );
			pingPongQueue[ 2 * passNum + 1 ]->SetConstants( &verticalPass, sizeof( uint32_t ) );
		}
	}

	MipImageTask* mipTask = nullptr;
	if ( config.downsampleScene )
	{
		mipProcessCreateInfo_t info{};
		info.name = "MainColorDownsample";
		info.context = &renderContext;
		info.resources = &resources;
		info.img = &resources.mainColorResolvedImage;
		info.mode = downSampleMode_t::DOWNSAMPLE_LINEAR;

		mipTask = new MipImageTask( info );
	}

	MipImageTask* mipCubeTask = nullptr;
	if ( config.useCubeViews )
	{
		mipProcessCreateInfo_t info{};
		info.name = "CubeDownsample";
		info.context = &renderContext;
		info.resources = &resources;
		info.img = &resources.cubeFbColorImage;
		info.mode = downSampleMode_t::DOWNSAMPLE_LINEAR;

		mipCubeTask = new MipImageTask( info );
	}

	ImageWritebackTask* imageCubemapWriteBackTask = nullptr;
	if ( config.writeCubeViews )
	{
		imageWriteBackCreateInfo_t info{};
		info.name = "EnvironmentMapWriteback";
		info.context = &renderContext;
		info.resources = &resources;
		info.fileName = "hdrEnvmap.img";
		info.writeToDiskOnFrameEnd = true;
		info.cubemap = true;

		for ( uint32_t i = 0; i < 6; ++i ) {
			info.imgCube[ i ] = &resources.cubeImageViews[ i ];
		}

		imageCubemapWriteBackTask = new ImageWritebackTask( info );
	}

	ImageWritebackTask* imageDiffuseIblWriteBackTask = nullptr;
	if ( config.computeDiffuseIbl )
	{
		imageWriteBackCreateInfo_t info{};
		info.name = "DiffuseIblWriteback";
		info.context = &renderContext;
		info.resources = &resources;
		info.fileName = "hdrDiffuse.img";
		info.writeToDiskOnFrameEnd = true;
		info.cubemap = true;

		for( uint32_t i = 0; i < 6; ++i ) {
			info.imgCube[ i ] = &resources.diffuseIblImageViews[ i ];
		}

		imageDiffuseIblWriteBackTask = new ImageWritebackTask( info );
	}

	ImageWritebackTask* imageSpecularIblWriteBackTask = nullptr;
	if ( config.computeSpecularIBL )
	{
		imageWriteBackCreateInfo_t info{};
		info.name = "SpecularIblWriteback";
		info.context = &renderContext;
		info.resources = &resources;
		info.fileName = "hdrSpecular.img";
		info.writeToDiskOnFrameEnd = true;
		info.cubemap = true;

		for ( uint32_t i = 0; i < 6; ++i ) {
			info.imgCube[ i ] = &resources.specularIblImageViews[ i ];
		}

		imageSpecularIblWriteBackTask = new ImageWritebackTask( info );
	}

	InitShaderResources();

	InitImGui( *view2Ds[ 0 ] );

	UploadAssets();

	for ( uint32_t i = 0; i < MaxShadowViews; ++i ) {
		schedule.Queue( new RenderTask( shadowViews[ i ], DRAWPASS_SHADOW_BEGIN, DRAWPASS_SHADOW_END ) );
	}
	schedule.Queue( new RenderTask( renderViews[ 0 ], DRAWPASS_MAIN_BEGIN, DRAWPASS_MAIN_END ) );
	if ( config.useCubeViews )
	{
		for ( uint32_t i = 1; i < Max3DViews; ++i ) {
			schedule.Queue( new RenderTask( renderViews[ i ], DRAWPASS_MAIN_BEGIN, DRAWPASS_MAIN_END ) );
		}
		if ( config.computeDiffuseIbl )
		{
			for ( uint32_t i = 0; i < 6; ++i ) {
				schedule.Queue( diffuseIBL[ i ] );
			}
		}
		if ( config.computeSpecularIBL )
		{
			schedule.Queue( copyCubeToSpecularIbl );
			for ( uint32_t i = 0; i < 6; ++i ) {
				schedule.Queue( specularIBL[ i ] );
			}
		}
	//	schedule.Queue( mipCubeTask );
	}
	schedule.Queue( resolve );
	if ( config.writeCubeViews ) {
		schedule.Queue( imageCubemapWriteBackTask );
	}
	if ( config.computeDiffuseIbl ) {
		schedule.Queue( imageDiffuseIblWriteBackTask );
	}
	if ( config.computeSpecularIBL ) {
		schedule.Queue( imageSpecularIblWriteBackTask );
	}
	if ( config.downsampleScene ) {
		schedule.Queue( mipTask );
	}
	if ( config.gaussianBlur )
	{
		for ( uint32_t i = 0; i < pingPongQueue.size(); ++i )
		{
			schedule.Queue( pingPongQueue[i] );
		}
	}
	schedule.Queue( new RenderTask( view2Ds[ 0 ], DRAWPASS_MAIN_BEGIN, DRAWPASS_MAIN_END ) );
	schedule.Queue( new ComputeTask( "ClearParticles", &particleState ) );
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
		const uint32_t programCount = g_assets.gpuPrograms.Count();
		for ( uint32_t i = 0; i < programCount; ++i )
		{
			GpuProgram& prog = g_assets.gpuPrograms.Find( i )->Get();

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
		rc.defaultImage = &g_assets.textureLib.Find( "_default" )->Get();
		rc.defaultImageCube = &g_assets.textureLib.Find( "_defaultCube" )->Get();

		rc.defaultImageArray.Resize( 1 );
		rc.defaultImageArray[ 0 ] = rc.defaultImage;

		rc.defaultImageCubeArray.Resize( 1 );
		rc.defaultImageCubeArray[ 0 ] = rc.defaultImageCube;
	}

	// Buffers
	{
		resources.globalConstants.Create( 
			"Globals",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			1,
			sizeof( viewBufferObject_t ),
			bufferType_t::UNIFORM,
			renderContext.sharedMemory
		);
		resources.viewParms.Create(
			"View",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxViews,
			sizeof( viewBufferObject_t ),
			bufferType_t::STORAGE,
			renderContext.sharedMemory
		);
		resources.surfParms.Create(
			"Surf",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxViews * MaxSurfaces,
			sizeof( surfaceBufferObject_t ),
			bufferType_t::STORAGE,
			renderContext.sharedMemory
		);
		resources.materialBuffers.Create(
			"Material",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxMaterials,
			sizeof( materialBufferObject_t ),
			bufferType_t::STORAGE,
			renderContext.sharedMemory
		);
		resources.lightParms.Create(
			"Light",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxLights,
			sizeof( lightBufferObject_t ),
			bufferType_t::STORAGE,
			renderContext.sharedMemory
		);
		resources.particleBuffer.Create(
			"Particle",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxParticles,
			sizeof( particleBufferObject_t ),
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

		geometry.vb.Create(
			"VB",
			swapBuffering_t::SINGLE_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxVertices,
			sizeof( vsInput_t ),
			bufferType_t::VERTEX,
			renderContext.localMemory
		);
		geometry.ib.Create(
			"IB",
			swapBuffering_t::SINGLE_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxIndices,
			sizeof( uint32_t ),
			bufferType_t::INDEX,
			renderContext.localMemory
		);

		geometry.stagingBuffer.Create(
			"Geo Staging",
			swapBuffering_t::SINGLE_FRAME,
			resourceLifeTime_t::REBOOT,
			1, 16 * MB_1,
			bufferType_t::STAGING,
			renderContext.sharedMemory
		);
		textureStagingBuffer.Create(
			"Texture Staging",
			swapBuffering_t::SINGLE_FRAME,
			resourceLifeTime_t::REBOOT,
			1,
			192 * MB_1,
			bufferType_t::STAGING,
			renderContext.sharedMemory
		);
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
	g_imguiControls.shaderHdl = INVALID_HDL;
	g_imguiControls.heightMapHeight = 1.0f;
	g_imguiControls.roughness = 0.9f;
	g_imguiControls.shadowStrength = 0.99f;
	g_imguiControls.toneMapColor[ 0 ] = 1.0f;
	g_imguiControls.toneMapColor[ 1 ] = 1.0f;
	g_imguiControls.toneMapColor[ 2 ] = 1.0f;
	g_imguiControls.toneMapColor[ 3 ] = 1.0f;
	g_imguiControls.dofFocalDepth = 0.01f;
	g_imguiControls.dofFocalRange = 0.25f;
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

	AssignBindSetsToGpuProgs();

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

		resources.shadowMapImage[ shadowIx ].Create(
			info,
			nullptr,
			new GpuImage( "shadowMap", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
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

		resources.mainColorImage.Create(
			info,
			nullptr,
			new GpuImage( "mainColor", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
		
		resources.gBufferLayerImage.Create(
			info,
			nullptr,
			new GpuImage( "gBufferLayer", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
		
		info.fmt = IMAGE_FMT_D_32_S8;
		info.type = IMAGE_TYPE_2D;
		info.aspect = imageAspectFlags_t( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG );

		resources.depthStencilImage.Create(
			info,
			nullptr,
			new GpuImage( "viewDepth", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
	}

	const int glslCubeMapping[ 6 ] = { 4, 5, 1, 0, 2, 3 };

	// Cube images
	{
		imageInfo_t colorInfo{};
		colorInfo.width = 512;
		colorInfo.height = 512;
		colorInfo.mipLevels = MipCount( colorInfo.width, colorInfo.height );
		colorInfo.layers = 6;
		colorInfo.subsamples = IMAGE_SMP_1;
		colorInfo.fmt = IMAGE_FMT_RGBA_16;
		colorInfo.type = IMAGE_TYPE_CUBE;
		colorInfo.aspect = IMAGE_ASPECT_COLOR_FLAG;
		colorInfo.tiling = IMAGE_TILING_MORTON;

		resources.cubeFbColorImage.Create(
			colorInfo,
			nullptr,
			new GpuImage( "cubeColor", colorInfo, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);

		resources.cubeFbColorImage.sampler.addrMode = SAMPLER_ADDRESS_CLAMP_EDGE;
		resources.cubeFbColorImage.sampler.filter = SAMPLER_FILTER_BILINEAR;

		imageInfo_t depthInfo = colorInfo;
		depthInfo.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		depthInfo.fmt = IMAGE_FMT_D_16;

		resources.cubeFbDepthImage.Create(
			depthInfo,
			nullptr,
			new GpuImage( "cubeDepth", depthInfo, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);

		resources.cubeFbImageView.Init( resources.cubeFbColorImage, colorInfo, resourceLifeTime_t::RESIZE );

		resources.cubeFbImageView.sampler.addrMode = SAMPLER_ADDRESS_CLAMP_EDGE;
		resources.cubeFbImageView.sampler.filter = SAMPLER_FILTER_BILINEAR;

		for ( uint32_t i = 0; i < 6; ++i )
		{
			imageSubResourceView_t subView;
			subView.arrayCount = 1;
			subView.baseArray = glslCubeMapping[ i ];
			subView.baseMip = 0;
			subView.mipLevels = 1;

			colorInfo.type = IMAGE_TYPE_2D;
			depthInfo.type = IMAGE_TYPE_2D;

			resources.cubeImageViews[ i ].Init( resources.cubeFbColorImage, colorInfo, subView, resourceLifeTime_t::RESIZE );
			resources.cubeDepthImageViews[ i ].Init( resources.cubeFbDepthImage, depthInfo, subView, resourceLifeTime_t::RESIZE );

			resources.cubeImageViews[ i ].sampler.addrMode = SAMPLER_ADDRESS_CLAMP_EDGE;
			resources.cubeImageViews[ i ].sampler.filter = SAMPLER_FILTER_BILINEAR;
		}
	}

	// Diffuse IBL images
	{
		imageInfo_t colorInfo{};
		colorInfo.width = 32;
		colorInfo.height = 32;
		colorInfo.mipLevels = 1;
		colorInfo.layers = 6;
		colorInfo.channels = 4;
		colorInfo.subsamples = IMAGE_SMP_1;
		colorInfo.fmt = IMAGE_FMT_RGBA_16;
		colorInfo.type = IMAGE_TYPE_CUBE;
		colorInfo.aspect = IMAGE_ASPECT_COLOR_FLAG;
		colorInfo.tiling = IMAGE_TILING_MORTON;

		resources.diffuseIblImage.Create(
			colorInfo,
			nullptr,
			new GpuImage( "diffuseIblColor", colorInfo, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);

		for ( uint32_t i = 0; i < 6; ++i )
		{
			imageSubResourceView_t subView;
			subView.arrayCount = 1;
			subView.baseArray = glslCubeMapping[ i ];
			subView.baseMip = 0;
			subView.mipLevels = 1;

			colorInfo.type = IMAGE_TYPE_2D;

			resources.diffuseIblImageViews[ i ].Init( resources.diffuseIblImage, colorInfo, subView, resourceLifeTime_t::RESIZE );
		}
	}

	// Specular IBL images
	{
		imageInfo_t colorInfo{};
		colorInfo.width = 128;
		colorInfo.height = 128;
		colorInfo.mipLevels = MipCount( colorInfo.width, colorInfo.height );
		colorInfo.layers = 6;
		colorInfo.channels = 4;
		colorInfo.subsamples = IMAGE_SMP_1;
		colorInfo.fmt = IMAGE_FMT_RGBA_16;
		colorInfo.type = IMAGE_TYPE_CUBE;
		colorInfo.aspect = IMAGE_ASPECT_COLOR_FLAG;
		colorInfo.tiling = IMAGE_TILING_MORTON;

		resources.specularIblImage.Create(
			colorInfo,
			nullptr,
			new GpuImage( "specularIblColor", colorInfo, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);

		for ( uint32_t i = 0; i < 6; ++i )
		{
			imageSubResourceView_t subView;
			subView.arrayCount = 1;
			subView.baseArray = glslCubeMapping[ i ];
			subView.baseMip = 0;
			subView.mipLevels = 1;

			colorInfo.type = IMAGE_TYPE_2D;

			resources.specularIblImageViews[ i ].Init( resources.specularIblImage, colorInfo, subView, resourceLifeTime_t::RESIZE );
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

		resources.mainColorResolvedImage.Create(
			info,
			nullptr,
			new GpuImage( "mainColorResolvedImage", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
		resources.blurredImage.Create(
			info,
			nullptr,
			new GpuImage( "blurredImage", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
		info.mipLevels = 1;

		const uint32_t mipLevelCount = resources.mainColorResolvedImage.info.mipLevels;
		resources.mainColorResolvedImageViews.resize( mipLevelCount );
		for ( uint32_t i = 0; i < mipLevelCount; ++i )
		{
			imageSubResourceView_t subView = {};
			subView.baseMip = i;
			subView.mipLevels = 1;
			subView.baseArray = 0;
			subView.arrayCount = 1;

			resources.mainColorResolvedImageViews[ i ].Init( resources.mainColorResolvedImage, info, subView, resourceLifeTime_t::RESIZE );
		}
		resources.blurredImageViews.resize( mipLevelCount );
		for ( uint32_t i = 0; i < mipLevelCount; ++i )
		{
			imageSubResourceView_t subView = {};
			subView.baseMip = i;
			subView.mipLevels = 1;
			subView.baseArray = 0;
			subView.arrayCount = 1;

			resources.blurredImageViews[ i ].Init( resources.blurredImage, info, subView, resourceLifeTime_t::RESIZE );
		}
	}

	// Depth-stencil views
	{
		imageInfo_t depthInfo = resources.depthStencilImage.info;
		depthInfo.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		resources.depthImageView.Init( resources.depthStencilImage, depthInfo, resourceLifeTime_t::RESIZE );

		imageInfo_t stencilInfo = resources.depthStencilImage.info;
		stencilInfo.aspect = IMAGE_ASPECT_STENCIL_FLAG;
		resources.stencilImageView.Init( resources.depthStencilImage, stencilInfo, resourceLifeTime_t::RESIZE );
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

		resources.depthStencilResolvedImage.Create(
			info,
			nullptr,
			new GpuImage( "depthStencilResolvedImage", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
	}

	// Depth-stencil views
	{
		resources.depthResolvedImageView.Init( resources.depthStencilResolvedImage, resources.depthStencilResolvedImage.info, resourceLifeTime_t::RESIZE );
		resources.stencilResolvedImageView.Init( resources.depthStencilResolvedImage, resources.depthStencilResolvedImage.info, resourceLifeTime_t::RESIZE );
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

		resources.tempColorImage.Create(
			info,
			nullptr,
			new GpuImage( "tempColor", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
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

		resources.tempWritebackImage.Create(
			info,
			nullptr,
			new GpuImage( "tempWritebackImage", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, renderContext.sharedMemory, resourceLifeTime_t::RESIZE )
		);
	}

	// Main Color Resolved Frame buffer
	{
		frameBufferCreateInfo_t fbInfo;
		fbInfo.name = "MainColorResolveFB";
		fbInfo.color0 = &resources.mainColorResolvedImageViews[0];
		fbInfo.color1 = &resources.depthStencilResolvedImage;
		fbInfo.swapBuffering = swapBuffering_t::SINGLE_FRAME;

		mainColorResolved.Create( fbInfo );
	}

	// Blurred Image Frame buffer
	blurredImageFrameBuffers.resize( resources.blurredImageViews.size() );
	for ( uint32_t i = 0; i < blurredImageFrameBuffers.size(); ++i )
	{
		frameBufferCreateInfo_t fbInfo;
		fbInfo.name = "blurredImageFB";
		fbInfo.color0 = &resources.blurredImageViews[ i ];
		fbInfo.swapBuffering = swapBuffering_t::SINGLE_FRAME;

		blurredImageFrameBuffers[ i ].Create( fbInfo );
	}

	// Temp Frame buffer
	{
		frameBufferCreateInfo_t fbInfo;
		fbInfo.name = "TempColorFB";
		fbInfo.color0 = &resources.tempColorImage;
		fbInfo.swapBuffering = swapBuffering_t::SINGLE_FRAME;

		tempColor.Create( fbInfo );
	}

	// Shadow map
	for ( uint32_t shadowIx = 0; shadowIx < MaxShadowMaps; ++shadowIx )
	{	
		frameBufferCreateInfo_t fbInfo;
		fbInfo.name = "ShadowMapFB";
		fbInfo.depth = &resources.shadowMapImage[ shadowIx ];
		fbInfo.swapBuffering = swapBuffering_t::SINGLE_FRAME;

		shadowMap[ shadowIx ].Create( fbInfo );
	}

	// Main Scene 3D Render
	{
		frameBufferCreateInfo_t fbInfo;
		fbInfo.name = "MainColorFB";
		for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx )
		{
			fbInfo.color0 = &resources.mainColorImage;
			fbInfo.color1 = &resources.gBufferLayerImage;
			fbInfo.depth = &resources.depthImageView;
			fbInfo.stencil = &resources.stencilImageView;
		}
		fbInfo.swapBuffering = swapBuffering_t::SINGLE_FRAME;

		mainColor.Create( fbInfo );
	}

	// Cubemap Render
	{
		for ( uint32_t i = 0; i < 6; ++i ) {
			frameBufferCreateInfo_t fbInfo;
			fbInfo.name = "CubeColorFB";
			for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx ) {
				fbInfo.color0 = &resources.cubeImageViews[ i ];
				fbInfo.depth = &resources.cubeDepthImageViews[ i ];
			}
			fbInfo.swapBuffering = swapBuffering_t::SINGLE_FRAME;

			cubeMapFrameBuffer[ i ].Create( fbInfo );
		}
	}

	// Diffuse IBL Render
	{
		for ( uint32_t i = 0; i < 6; ++i ) {
			frameBufferCreateInfo_t fbInfo;
			fbInfo.name = "DiffuseIblFB";
			for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx ) {
				fbInfo.color0 = &resources.diffuseIblImageViews[ i ];
			}
			fbInfo.swapBuffering = swapBuffering_t::SINGLE_FRAME;

			diffuseIblFrameBuffer[ i ].Create( fbInfo );
		}
	}

	// Specular IBL Render
	{
		for ( uint32_t i = 0; i < 6; ++i ) {
			frameBufferCreateInfo_t fbInfo;
			fbInfo.name = "SpecularIblFB";
			for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx ) {
				fbInfo.color0 = &resources.specularIblImageViews[ i ];
			}
			fbInfo.swapBuffering = swapBuffering_t::SINGLE_FRAME;

			specularIblFrameBuffer[ i ].Create( fbInfo );
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