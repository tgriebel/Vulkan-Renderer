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
#include "../render_binding/imageArray.h"
#include "../render_tasks/RenderTask.h"
#include "../render_tasks/ImageReadbackTask.h"
#include "../render_tasks/ImageProcessTask.h"
#include "../render_tasks/imguiTask.h"

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

	InitShaderResources();

	resources.gpuImages2D.SetRenderContext( &renderContext );
	resources.gpuImagesCube.SetRenderContext( &renderContext );

	resources.gpuImages2D.Resize( MaxImageDescriptors );
	resources.gpuImagesCube.Resize( MaxImageDescriptors );

	viewCount = 0;

	ScopedLogTimer timer( "Schedule Build", timerPrecision_t::MICROSECOND, &TimerPrint );

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
		info.fbImages.lifetime = resourceLifeTime_t::REBOOT;
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

	ImageProcessTask* diffuseIBL = nullptr;
	ImageReadbackTask* imageDiffuseIblReadbackTask = nullptr;

	ImageProcessTask* specularIBL = nullptr;
	ImageReadbackTask* imageSpecularIblReadBackTask = nullptr;

	if ( config.useCubeViews )
	{
		if ( config.computeDiffuseIbl )
		{
			imageProcessCreateInfo_t info = {};
			info.name = "DiffuseIBL";
			info.progName = "DiffuseIBL";
			info.sampleImages[ 0 ] = resources.cubeFbColorImage;
			info.context = &renderContext;
			info.resources = &resources;
			info.baseMip = 0;
			info.mipCount = 1;
			info.taskImageCount = 1;
			info.createInfos ;

			// Temp image
			{
				imageInfo_t imgInfo{};
				imgInfo.width = 32;
				imgInfo.height = 32;
				imgInfo.mipLevels = 1;
				imgInfo.layers = 6;
				imgInfo.channels = 4;
				imgInfo.subsamples = IMAGE_SMP_1;
				imgInfo.fmt = IMAGE_FMT_RGBA_16;
				imgInfo.type = IMAGE_TYPE_CUBE;
				imgInfo.aspect = IMAGE_ASPECT_COLOR_FLAG;
				imgInfo.tiling = IMAGE_TILING_MORTON;

				info.taskImageCount = 1;
				info.createInfos = { &imgInfo };
			}

			diffuseIBL = new ImageProcessTask( info );
		
			// Readback
			{
				const std::string fileName = std::string( config.cubemapName ) + "_diffuseIbl.img";

				imageReadBackCreateInfo_t info{};
				info.name = "DiffuseIblReadback";
				info.img = diffuseIBL->GetOutputImage();
				info.context = &renderContext;
				info.resources = &resources;
				info.fileName = fileName.c_str();
				info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
				info.flags |= imageReadbackFlags_t::CUBEMAP;
				info.flags |= imageReadbackFlags_t::PACKED_HDR;

				imageDiffuseIblReadbackTask = new ImageReadbackTask( info );
			}
		}

		if ( config.computeSpecularIBL )
		{
			imageProcessCreateInfo_t info = {};
			info.name = "SpecularIbl";
			info.sampleImages[ 0 ] = resources.cubeFbColorImage;
			info.context = &renderContext;
			info.resources = &resources;
			info.progName = "preCalculatedSpecularIbl";
			info.baseMip = 0;

			{
				imageInfo_t imgInfo{};
				imgInfo.width = 128;
				imgInfo.height = 128;
				imgInfo.mipLevels = MipCount( imgInfo.width, imgInfo.height );
				imgInfo.layers = 6;
				imgInfo.channels = 4;
				imgInfo.subsamples = IMAGE_SMP_1;
				imgInfo.fmt = IMAGE_FMT_RGBA_16;
				imgInfo.type = IMAGE_TYPE_CUBE;
				imgInfo.aspect = IMAGE_ASPECT_COLOR_FLAG;
				imgInfo.tiling = IMAGE_TILING_MORTON;

				info.taskImageCount = 1;
				info.createInfos = { &imgInfo };
			}

			specularIBL = new ImageProcessTask( info );

			{
				const std::string fileName = std::string( config.cubemapName ) + "_specIbl.img";

				imageReadBackCreateInfo_t info{};
				info.name = "SpecularIblReadback";
				info.img = specularIBL->GetOutputImage();
				info.context = &renderContext;
				info.resources = &resources;
				info.fileName = fileName.c_str();
				if ( config.writeCubeViews ) {
					info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
				}
				info.flags |= imageReadbackFlags_t::CUBEMAP;
				info.flags |= imageReadbackFlags_t::PACKED_HDR;

				imageSpecularIblReadBackTask = new ImageReadbackTask( info );
			}
		}
	}

	ImageShaderTask* resolve = nullptr;
	{
		imageShaderCreateInfo_t info = {};
		info.name = "ResolveMain";
		info.clear = false;
		info.resolve = true;
		if ( ForceDisableMSAA ) {
			info.progHdl = AssetLibGpuProgram::Handle( "Resolve" );
		} else {
			info.progHdl = AssetLibGpuProgram::Handle( "ResolveMSAA" );
		}
		info.outputImage = resources.mainColorResolvedImage;
		info.outputImage1 = &resources.depthResolvedImageView;
		info.context = &renderContext;
		info.resources = &resources;
		info.inputImages = 3;

		resolve = new ImageShaderTask( info );

		resolve->SetSourceImage( 0, resources.mainColorImage );
		resolve->SetSourceImage( 1, &resources.depthImageView );
		resolve->SetSourceImage( 2, &resources.stencilImageView );
	}

	ImageProcessTask* gaussianTask = nullptr;
	if ( config.gaussianBlur )
	{
		imageProcessCreateInfo_t info{};
		info.name = "Separable Gaussian";
		info.context = &renderContext;
		info.resources = &resources;
		info.outputImage = resources.blurredImage;
		info.progName = "SeparableGaussianBlur";
		info.sampleImages[ 0 ] = resources.mainColorResolvedImage;
		info.baseMip = 0;

		gaussianTask = new ImageProcessTask( info );
	}

	
	CopyImageTask* copyPreviousLuminance = nullptr;
	ImageProcessTask* luminanceSceneAvg = nullptr;

	if ( config.autoExposure )
	{
		// Copy previous luminance
		{
			copyImageParms_t srcCopy{};
			srcCopy.baseArray = 0;
			srcCopy.arrayCount = 1;
			srcCopy.baseMip = resources.currentLum->info.mipLevels - 1;
			srcCopy.mipLevels = 1;
			srcCopy.x = 0;
			srcCopy.y = 0;
			srcCopy.z = 0;
			srcCopy.width = 1;
			srcCopy.height = 1;
			srcCopy.depth = 1;

			copyImageParms_t dstCopy{};
			dstCopy.baseArray = 0;
			dstCopy.arrayCount = 1;
			dstCopy.baseMip = 0;
			dstCopy.mipLevels = 1;
			dstCopy.x = 0;
			dstCopy.y = 0;
			dstCopy.z = 0;
			dstCopy.width = 1;
			dstCopy.height = 1;
			dstCopy.depth = 1;

			copyPreviousLuminance = new CopyImageTask( resources.currentLum, srcCopy, resources.previousLum, dstCopy );
		}

		// Average scene luminance
		{
			imageProcessCreateInfo_t info{};
			info.name = "LuminanceDownsample";
			info.context = &renderContext;
			info.resources = &resources;
			info.sampleImages[ 0 ] = resources.mainColorResolvedImage;
			info.sampleImages[ 1 ] = resources.previousLum;
			info.outputImage = resources.currentLum;
			info.mipCount = resources.currentLum->info.mipLevels;
			info.progressiveSampling = true;
			info.progName = "LuminanceDownsample";

			luminanceSceneAvg = new ImageProcessTask( info );
		}
	}

	ImageProcessTask* mipTask = nullptr;
	if ( config.downsampleScene )
	{
		imageProcessCreateInfo_t info{};
		info.name = "MainColorDownsample";
		info.context = &renderContext;
		info.resources = &resources;
		info.outputImage = resources.mainColorResolvedImage;
		info.progressiveSampling = true;
		info.progName = "DownSample";
		info.baseMip = 1;

		mipTask = new ImageProcessTask( info );
	}

	ImageProcessTask* bloomDownsampleTask = nullptr;
	ImageProcessTask* bloomUpsampleTask = nullptr;
	if ( config.bloom )
	{
		imageProcessCreateInfo_t info{};
		info.name = "BloomDownsample";
		info.context = &renderContext;
		info.resources = &resources;
		info.sampleImages[ 0 ] = resources.mainColorResolvedImage;
		info.outputImage = resources.bloomDownsample;
		info.mipCount = 4;
		info.progressiveSampling = true;
		info.progName = "BloomDownsample";

		bloomDownsampleTask = new ImageProcessTask( info );

		info.name = "BloomUpsample";
		info.outputImage = resources.bloomUpsample;
		info.upsampleProcess = true;
		info.seedFromFirstResourceImageLastMIP = true;
		info.sampleImages[ 0 ] = resources.bloomDownsample;
		info.progName = "BloomUpsample";

		bloomUpsampleTask = new ImageProcessTask( info );
	}

	ImageProcessTask* mipCubeTask = nullptr;
	if ( config.useCubeViews )
	{
		imageProcessCreateInfo_t info{};
		info.name = "CubeDownsample";
		info.context = &renderContext;
		info.resources = &resources;
		info.outputImage = resources.cubeFbColorImage;
		info.progressiveSampling = true;
		info.baseMip = 1;

		mipCubeTask = new ImageProcessTask( info );
	}

	ImageReadbackTask* imageCubemapReadBackTask = nullptr;
	if ( config.useCubeViews )
	{
		const std::string fileName = std::string( config.cubemapName ) + "_env.img";

		imageReadBackCreateInfo_t info{};
		info.name = "EnvironmentMapReadback";
		info.img = resources.cubeFbColorImage;
		info.context = &renderContext;
		info.resources = &resources;
		info.fileName = fileName.c_str();
		if ( config.writeCubeViews ) {
			info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
		}
		info.flags |= imageReadbackFlags_t::CUBEMAP;
		info.flags |= imageReadbackFlags_t::PACKED_HDR;

		imageCubemapReadBackTask = new ImageReadbackTask( info );
	}

	ImageShaderTask* brdfLutTask = nullptr;
	ImageReadbackTask* readbackBrdfLut = nullptr;
	if( config.computeBrdfLut )
	{
		{
			imageShaderCreateInfo_t info = {};
			info.name = "BrdfLutCalculation";
			info.progHdl = AssetLibGpuProgram::Handle( "preCalculatedBrdfLut" );
			info.context = &renderContext;
			info.resources = &resources;

			{
				imageInfo_t imgInfo{};
				imgInfo.width = 512;
				imgInfo.height = 512;
				imgInfo.mipLevels = 1;
				imgInfo.layers = 1;
				imgInfo.subsamples = IMAGE_SMP_1;
				imgInfo.fmt = IMAGE_FMT_RGBA_16;
				imgInfo.type = IMAGE_TYPE_2D;
				imgInfo.aspect = IMAGE_ASPECT_COLOR_FLAG;
				imgInfo.tiling = IMAGE_TILING_MORTON;

				info.taskImageCount = 1;
				info.createInfos = { &imgInfo };
			}

			brdfLutTask = new ImageShaderTask( info );
		}

		{
			const std::string fileName = "brdf_lut.img";

			imageReadBackCreateInfo_t info{};
			info.name = "BrdfLutReadback";
			info.img = brdfLutTask->GetOutputImage();
			info.context = &renderContext;
			info.resources = &resources;
			info.fileName = fileName.c_str();
			info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
			info.flags |= imageReadbackFlags_t::PACKED_HDR;

			readbackBrdfLut = new ImageReadbackTask( info );
		}
	}

	ImageReadbackTask* screenshotReadback = nullptr;
	if ( config.screenshot )
	{
		imageReadBackCreateInfo_t info{};
		info.name = "ScreenshotReadback";
		info.context = &renderContext;
		info.resources = &resources;
		info.fileName = "screenshot.png";
		info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
		info.flags |= imageReadbackFlags_t::SCREENSHOT;
		info.img = resources.mainColorResolvedImage;

		screenshotReadback = new ImageReadbackTask( info );
	}

	UploadAssets();

	for ( uint32_t i = 0; i < MaxShadowViews; ++i ) {
		schedule.Link( new RenderTask( shadowViews[i], DRAWPASS_SHADOW_BEGIN, DRAWPASS_SHADOW_END));
	}
	schedule.Link( new RenderTask( renderViews[0], DRAWPASS_MAIN_BEGIN, DRAWPASS_MAIN_END));
	if ( config.useCubeViews )
	{
		schedule.Link( new RenderTask( renderViews[1], DRAWPASS_MAIN_BEGIN, DRAWPASS_MAIN_END));

		if ( config.computeDiffuseIbl )
		{
			schedule.Link( diffuseIBL );
			schedule.Link( imageDiffuseIblReadbackTask );
		}
		if ( config.computeSpecularIBL )
		{
			schedule.Link( specularIBL );
			schedule.Link( imageSpecularIblReadBackTask );
		}
		schedule.Link( mipCubeTask );
		schedule.Link( imageCubemapReadBackTask );
	}
	if ( brdfLutTask )
	{
		schedule.Link( brdfLutTask );
	}
	if ( readbackBrdfLut )
	{
		schedule.Link( readbackBrdfLut );
	}
	schedule.Link( resolve );

	if( config.screenshot ) {
		schedule.Link( screenshotReadback );
	}
	if ( config.autoExposure )
	{
		schedule.Link( copyPreviousLuminance );
		schedule.Link( luminanceSceneAvg );
	//	schedule.Link( luminanceFrameAvg );
	}
	if( config.bloom )
	{
		schedule.Link( bloomDownsampleTask );
		schedule.Link( bloomUpsampleTask );
	}
	if ( config.downsampleScene )
	{
		schedule.Link( mipTask );
	}
	if ( config.gaussianBlur )
	{
		schedule.Link( gaussianTask );
	}
	schedule.Link( new RenderTask( view2Ds[0], DRAWPASS_2D, DRAWPASS_2D ) );
	schedule.Link( new ImguiTask( view2Ds[ 0 ]->passes[ 0 ][ DRAWPASS_DEBUG_2D ], &renderContext, &resources, false ) );
	schedule.Link( new RenderTask( view2Ds[ 0 ], DRAWPASS_DEBUG_2D, DRAWPASS_DEBUG_2D ) );
	schedule.Link( new ComputeTask( "ClearParticles", &particleState ) );

	schedule.AsString();

	InitImGui( view2Ds[ 0 ]->passes[ 0 ][ DRAWPASS_DEBUG_2D ]->GetFrameBuffer() );
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
			sizeof( viewBufferObject_t ),
			bufferType_t::UNIFORM,
			renderContext.sharedMemory
		);
		resources.viewParms.Create(
			"View",
			swapBuffering_t::MULTI_FRAME,
			resourceLifeTime_t::REBOOT,
			MaxViews * MaxMultiViews,
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
			1,
			MaxGeometryUploadMemory,
			bufferType_t::STAGING,
			renderContext.sharedMemory
		);
		textureStagingBuffer.Create(
			"Texture Staging",
			swapBuffering_t::SINGLE_FRAME,
			resourceLifeTime_t::REBOOT,
			1,
			MaxTexturingUploadMemory,
			bufferType_t::STAGING,
			renderContext.sharedMemory
		);
	}
}


void Renderer::InitImGui( const FrameBuffer* fb )
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

	assert( fb != nullptr );

	renderPassTransition_t transitionState {};
	transitionState.flags.clear = false;
	transitionState.flags.presentAfter = false;
	transitionState.flags.presentBefore = false;
	transitionState.flags.readOnly = true;
	transitionState.flags.readAfter = true;

	ImGui_ImplVulkan_Init( &vkInfo, fb->GetVkRenderPass( transitionState ) );

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
	g_imguiControls.roughnessScale = 1.0f;
	g_imguiControls.roughnessBias = 0.0f;
	g_imguiControls.metalnessScale = 1.0f;
	g_imguiControls.metalnessBias = 0.0f;
	g_imguiControls.shadowStrength = 0.99f;
	g_imguiControls.toneMapColor[ 0 ] = 1.0f;
	g_imguiControls.toneMapColor[ 1 ] = 1.0f;
	g_imguiControls.toneMapColor[ 2 ] = 1.0f;
	g_imguiControls.toneMapColor[ 3 ] = 1.0f;
	g_imguiControls.exposureMidGray = 0.18f;
	g_imguiControls.exposureAdaptation = 1.0f;
	g_imguiControls.exposureWhitePoint = 1.0f;
	g_imguiControls.exposureDarkLimit = 0.0005f;
	g_imguiControls.dofEnable = false;
	g_imguiControls.dofFocalDepth = 0.01f;
	g_imguiControls.dofFocalRange = 0.25f;
	g_imguiControls.dbgImageId = -1;
	g_imguiControls.selectedFrameBufferImageId = -1;
	g_imguiControls.isTextured = true;
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
			prog.vk_shaders[ shaderIx ] = vk_CreateShaderModule( prog.shaders[ shaderIx ].blob, progAsset->GetName().c_str() );
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

	resources.RegisterOutputImages();

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

		resources.mainColorImage->Create(
			info,
			nullptr,
			new GpuImage( "mainColor", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
		
		resources.gBufferLayerImage->Create(
			info,
			nullptr,
			new GpuImage( "gBufferLayer", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
		
		info.fmt = IMAGE_FMT_D_32_S8;
		info.type = IMAGE_TYPE_2D;
		info.aspect = imageAspectFlags_t( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG );

		resources.depthStencilImage->Create(
			info,
			nullptr,
			new GpuImage( "viewDepth", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
	}

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

		resources.cubeFbColorImage->Create(
			colorInfo,
			nullptr,
			new GpuImage( "cubeColor", colorInfo, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);

		resources.cubeFbColorImage->sampler.addrMode = SAMPLER_ADDRESS_CLAMP_EDGE;
		resources.cubeFbColorImage->sampler.filter = SAMPLER_FILTER_BILINEAR;

		imageInfo_t depthInfo = colorInfo;
		depthInfo.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		depthInfo.fmt = IMAGE_FMT_D_16;

		resources.cubeFbDepthImage->Create(
			depthInfo,
			nullptr,
			new GpuImage( "cubeDepth", depthInfo, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
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
			new GpuImage( "mainColorResolvedImage", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
		resources.mainColorResolvedImage->sampler.addrMode = SAMPLER_ADDRESS_CLAMP_EDGE;

		resources.blurredImage->Create(
			info,
			nullptr,
			new GpuImage( "blurredImage", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
		resources.blurredImage->sampler.addrMode = SAMPLER_ADDRESS_CLAMP_EDGE;
	}

	// Depth-stencil views
	{
		imageInfo_t depthInfo = resources.depthStencilImage->info;
		depthInfo.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		resources.depthImageView.Init( resources.depthStencilImage, depthInfo, resourceLifeTime_t::RESIZE );

		imageInfo_t stencilInfo = resources.depthStencilImage->info;
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
		info.tiling = resources.depthStencilImage->info.tiling;

		resources.depthStencilResolvedImage->Create(
			info,
			nullptr,
			new GpuImage( "depthStencilResolvedImage", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
	}

	// Depth-stencil views
	{
		resources.depthResolvedImageView.Init( resources.depthStencilResolvedImage, resources.depthStencilResolvedImage->info, resourceLifeTime_t::RESIZE );
		resources.stencilResolvedImageView.Init( resources.depthStencilResolvedImage, resources.depthStencilResolvedImage->info, resourceLifeTime_t::RESIZE );
	}

	// Bloom
	{
		uint32_t bloomWidth, bloomHeight;
		MipDimensions( 1, width, height, &bloomWidth, &bloomHeight );

		imageInfo_t info{};
		info.width = bloomWidth;
		info.height = bloomHeight;
		info.mipLevels = MipCount( bloomWidth, bloomHeight );
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_RGBA_16;
		info.type = IMAGE_TYPE_2D;
		info.aspect = IMAGE_ASPECT_COLOR_FLAG;
		info.tiling = IMAGE_TILING_MORTON;

		resources.bloomDownsample->Create(
			info,
			nullptr,
			new GpuImage( "bloomDownsample", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
		resources.bloomDownsample->sampler.addrMode = SAMPLER_ADDRESS_CLAMP_EDGE;

		info.width = width;
		info.height = height;
		info.mipLevels = MipCount( width, height );

		resources.bloomUpsample->Create(
			info,
			nullptr,
			new GpuImage( "bloomUpsample", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
		);
		resources.bloomUpsample->sampler.addrMode = SAMPLER_ADDRESS_CLAMP_EDGE;
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
			new GpuImage( "tempColor", info, GPU_IMAGE_RW, renderContext.frameBufferMemory, resourceLifeTime_t::RESIZE )
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
			new GpuImage( "previousLuminance", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_DST, renderContext.frameBufferMemory, resourceLifeTime_t::REBOOT )
		);
		resources.previousLum->sampler.addrMode = SAMPLER_ADDRESS_CLAMP_EDGE;
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
			new GpuImage( "currentLuminance", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, renderContext.frameBufferMemory, resourceLifeTime_t::REBOOT )
		);
		resources.currentLum->sampler.addrMode = SAMPLER_ADDRESS_CLAMP_EDGE;
	}
}


ShaderBindParms* RenderContext::RegisterBindParm( const ShaderBindSet* set )
{
	const uint32_t id = bindParmsList.Count();

	ShaderBindParms parms = ShaderBindParms( set, id );

	pendingIndices.Append( id );
	bindParmsList.Append( parms );

	return &bindParmsList[ id ];
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
	std::vector<const char*> descNames;

	layouts.reserve( MaxFrameStates * pendingParmCount );
	descSets.reserve( MaxFrameStates * pendingParmCount );
	descNames.reserve( MaxFrameStates * pendingParmCount );

	for ( uint32_t i = 0; i < pendingParmCount; ++i )
	{
		const uint32_t bindIx = pendingIndices[ i ];
		ShaderBindParms& parms = bindParmsList[ bindIx ];
		const ShaderBindSet* set = parms.GetSet();

		for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx )
		{
			layouts.push_back( set->GetVkObject() );
			descSets.push_back( VK_NULL_HANDLE );
			descNames.push_back( set->GetName() );
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
		vk_SetObjectName( (uint64_t)descSets[ i ], VK_OBJECT_TYPE_DESCRIPTOR_SET, vk_BuildObjectName( "DescriptorSet", descNames[ i ] ).c_str() );
	}

	for ( uint32_t i = 0; i < pendingParmCount; ++i )
	{
		const uint32_t bindIx = pendingIndices[ i ];
		ShaderBindParms& parms = bindParmsList[ bindIx ];
		parms.SetVkObject( &descSets[ MaxFrameStates * i ] );
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