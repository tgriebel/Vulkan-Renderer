#include "schedule.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/imgui.h"
#endif

#include "../globals/assetDefs.h"
#include "../render_tasks/ImageShaderTask.h"
#include "../render_tasks/ImageProcessTask.h"
#include "../render_tasks/ImageReadbackTask.h"
#include "../render_tasks/RenderTask.h"
#include "../render_tasks/UtilTasks.h"
#include "../render_tasks/imguiTask.h"

static availableTasks_t tasks;

void BuildSceneSchedule( const renderConfig_t& config, RenderContext* renderContext, ResourceContext* resources, RenderViewContext* viewContext, TaskSchedule* schedule )
{
	SCOPED_TIMER_PRINT( ScheduleBuild, MILLISECOND )

	if( config.useCubeViews )
	{
		if( config.computeDiffuseIbl )
		{
			imageProcessCreateInfo_t info = {};
			info.name = "DiffuseIBL";
			info.progName = "DiffuseIBL";
			info.resourceImages[ 0 ] = resources->cubeFbColorImage;
			info.context = renderContext;
			info.resources = resources;
			info.baseMip = 0;
			info.mipCount = 1;
			info.taskImageCount = 1;
			info.createInfos;

			// Temp image
			imageInfo_t imgInfo {};
			{				
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

			tasks.diffuseIBL = new ImageProcessTask( info );

			// Readback
			{
				const std::string fileName = std::string( config.cubemapName ) + "_diffuseIbl.img";

				imageReadBackCreateInfo_t info {};
				info.name = "DiffuseIblReadback";
				info.name = "DiffuseIblReadback";
				info.img = tasks.diffuseIBL->GetOutputImage();
				info.context = renderContext;
				info.resources = resources;
				info.fileName = fileName.c_str();
				info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
				info.flags |= imageReadbackFlags_t::CUBEMAP;
				info.flags |= imageReadbackFlags_t::PACKED_HDR;

				tasks.imageDiffuseIblReadbackTask = new ImageReadbackTask( info );
			}
		}

		if( config.computeSpecularIBL )
		{
			imageProcessCreateInfo_t info = {};
			info.name = "SpecularIbl";
			info.resourceImages[ 0 ] = resources->cubeFbColorImage;
			info.context = renderContext;
			info.resources = resources;
			info.progName = "preCalculatedSpecularIbl";
			info.baseMip = 0;

			{
				imageInfo_t imgInfo {};
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

			tasks.specularIBL = new ImageProcessTask( info );

			{
				const std::string fileName = std::string( config.cubemapName ) + "_specIbl.img";

				imageReadBackCreateInfo_t info {};
				info.name = "SpecularIblReadback";
				info.img = tasks.specularIBL->GetOutputImage();
				info.context = renderContext;
				info.resources = resources;
				info.fileName = fileName.c_str();
				if( config.writeCubeViews ) {
					info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
				}
				info.flags |= imageReadbackFlags_t::CUBEMAP;
				info.flags |= imageReadbackFlags_t::PACKED_HDR;

				tasks.imageSpecularIblReadBackTask = new ImageReadbackTask( info );
			}
		}
	}

	// Prepass resolve
	if( ForceDisableMSAA == false )
	{
		resolveTaskCreateInfo_t info{};
		info.count = 2;
		info.context = renderContext;
		info.resources = resources;
		info.resolves[ 0 ].info.src = resources->gBufferLayerImage0;
		info.resolves[ 0 ].info.dst = resources->gBufferLayerResolvedImage0;
		info.resolves[ 0 ].info.mode = resolveMode_t::AVERAGE;
		info.resolves[ 0 ].info.baseArray = 0;
		info.resolves[ 0 ].info.arrayCount = 1;
		info.resolves[ 0 ].info.baseMip = 0;
		info.resolves[ 0 ].info.transitionSourceFromWrite = false;
		info.resolves[ 0 ].info.writeSourceAfterResolve = false;
		info.resolves[ 0 ].useApi = true;

		info.resolves[ 1 ].info.src = resources->depthStencilImage;
		info.resolves[ 1 ].info.dst = resources->depthStencilResolvedImage;
		info.resolves[ 1 ].useApi = false;

		tasks.resolvePostDepth = new ResolveImageTask( info );
	}

	// Main scene resolve
	if( ForceDisableMSAA == false )
	{
		resolveTaskCreateInfo_t info{};
		info.count = 1;
		info.context = renderContext;
		info.resources = resources;
		info.resolves[ 0 ].info.src = resources->mainColorImage;
		info.resolves[ 0 ].info.dst = resources->mainColorResolvedImage;
		info.resolves[ 0 ].info.mode = resolveMode_t::AVERAGE;
		info.resolves[ 0 ].info.baseArray = 0;
		info.resolves[ 0 ].info.arrayCount = 1;
		info.resolves[ 0 ].info.baseMip = 0;
		info.resolves[ 0 ].info.transitionSourceFromWrite = false;
		info.resolves[ 0 ].info.writeSourceAfterResolve = false;
		info.resolves[ 0 ].useApi = true;

		tasks.resolve = new ResolveImageTask( info );
	}

	if( config.gaussianBlur )
	{
		imageProcessCreateInfo_t info {};
		info.name = "Separable Gaussian";
		info.context = renderContext;
		info.resources = resources;
		info.outputImage = resources->blurredImage;
		info.progName = "SeparableGaussianBlur";
		info.resourceImages[ 0 ] = resources->mainColorResolvedImage;
		info.baseMip = 0;

		tasks.gaussianTask = new ImageProcessTask( info );
	}

	if( config.ssao )
	{
		struct ssaoConstants_t
		{
			float    radius;      // World-space sampling radius (meters)
			uint32_t numSamples;  // Sample count — 8 (fast) to 32 (quality)
			float    bias;        // Depth bias to prevent self-occlusion (meters)
			float    strength;    // AO multiplier: 1 = standard, higher = darker
		};

		const ssaoConstants_t ssaoDefaults = { 0.5f, 16, 0.025f, 1.5f };

		imageProcessCreateInfo_t info{};
		info.name = "SSAO";
		info.context = renderContext;
		info.resources = resources;
		info.outputImage = resources->ssaoImage;
		info.progName = "SSAO";
		info.resourceImages[ 0 ] = resources->depthStencilResolvedImage;
		info.baseMip = 0;
		info.constants = &ssaoDefaults;
		info.constantsByteSize = sizeof( ssaoDefaults );

		tasks.ssaoTask = new ImageProcessTask( info );

#if defined( USE_IMGUI )
		tasks.ssaoTask->RegisterControls( [ ssaoTask = tasks.ssaoTask ]()
		{
			ssaoConstants_t& c = *ssaoTask->GetConstants<ssaoConstants_t>();
			bool changed = false;
			changed |= ImGui::SliderFloat( "Radius",   &c.radius,           0.1f, 2.0f );
			changed |= ImGui::SliderInt  ( "Samples",  (int*)&c.numSamples, 4,    32   );
			changed |= ImGui::SliderFloat( "Bias",     &c.bias,             0.0f, 0.1f );
			changed |= ImGui::SliderFloat( "Strength", &c.strength,         0.5f, 4.0f );
			if ( changed )
			{
				ssaoTask->UpdateConstants();
			}
		} );
#endif
	}

	if( config.autoExposure )
	{
		// Copy previous luminance
		{
			copyImageParms_t srcCopy {};
			srcCopy.baseArray = 0;
			srcCopy.arrayCount = 1;
			srcCopy.baseMip = resources->currentLum->info.mipLevels - 1;
			srcCopy.mipLevels = 1;
			srcCopy.x = 0;
			srcCopy.y = 0;
			srcCopy.z = 0;
			srcCopy.width = 1;
			srcCopy.height = 1;
			srcCopy.depth = 1;

			copyImageParms_t dstCopy {};
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

			tasks.copyPreviousLuminance = new CopyImageTask( resources->currentLum, srcCopy, resources->previousLum, dstCopy );
		}

		// Average scene luminance
		{
			imageProcessCreateInfo_t info {};
			info.name = "LuminanceDownsample";
			info.context = renderContext;
			info.resources = resources;
			info.resourceImages[ 0 ] = resources->mainColorResolvedImage;
			info.resourceImages[ 1 ] = resources->previousLum;
			info.outputImage = resources->currentLum;
			info.mipCount = resources->currentLum->info.mipLevels;
			info.progressiveSampling = true;
			info.progName = "LuminanceDownsample";

			tasks.luminanceSceneAvg = new ImageProcessTask( info );
		}
	}


	if( config.downsampleScene )
	{
		imageProcessCreateInfo_t info {};
		info.name = "MainColorDownsample";
		info.context = renderContext;
		info.resources = resources;
		info.outputImage = resources->mainColorResolvedImage;
		info.progressiveSampling = true;
		info.progName = "DownSample";
		info.baseMip = 1;

		tasks.mipTask = new ImageProcessTask( info );
	}


	if( config.bloom )
	{
		imageProcessCreateInfo_t info {};
		info.name = "BloomDownsample";
		info.context = renderContext;
		info.resources = resources;
		info.sourceImage = resources->mainColorResolvedImage;
		info.outputImage = resources->bloom;
		info.baseMip = 1;
		info.mipCount = 4;
		info.progressiveSampling = true;
		info.progName = "BloomDownsample";

		tasks.bloomDownsampleTask = new ImageProcessTask( info );

		struct bloomUpsampleConstants_t
		{
			float filterRadius;   // Tent filter radius in UV space
		};

		const bloomUpsampleConstants_t bloomUpsampleDefaults = { 0.005f };

		info.name = "BloomUpsample";
		info.sourceImage = resources->bloom;
		info.outputImage = resources->bloom; // Overwrite the previous downsampled values with upsampled ones
		info.baseMip = 0;
		info.upsampleProcess = true;
		info.progName = "BloomUpsample";
		info.constants = &bloomUpsampleDefaults;
		info.constantsByteSize = sizeof( bloomUpsampleDefaults );

		tasks.bloomUpsampleTask = new ImageProcessTask( info );

#if defined( USE_IMGUI )
		tasks.bloomUpsampleTask->RegisterControls( [ bloomUpsampleTask = tasks.bloomUpsampleTask ]()
		{
			bloomUpsampleConstants_t& c = *bloomUpsampleTask->GetConstants<bloomUpsampleConstants_t>();
			if ( ImGui::SliderFloat( "Filter Radius", &c.filterRadius, 0.001f, 0.02f ) )
			{
				bloomUpsampleTask->UpdateConstants();
			}
		} );
#endif
	}


	if( config.cubeDownsample )
	{
		imageProcessCreateInfo_t info {};
		info.name = "CubeDownsample";
		info.context = renderContext;
		info.resources = resources;
		info.outputImage = resources->cubeFbColorImage;
		info.progressiveSampling = true;
		info.baseMip = 1;

		tasks.mipCubeTask = new ImageProcessTask( info );
	}


	if( config.computeEnvMap )
	{
		const std::string fileName = std::string( config.cubemapName ) + "_env.img";

		imageReadBackCreateInfo_t info {};
		info.name = "EnvironmentMapReadback";
		info.img = resources->cubeFbColorImage;
		info.context = renderContext;
		info.resources = resources;
		info.fileName = fileName.c_str();
		if( config.writeCubeViews ) {
			info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
		}
		info.flags |= imageReadbackFlags_t::CUBEMAP;
		info.flags |= imageReadbackFlags_t::PACKED_HDR;

		tasks.imageCubemapReadBackTask = new ImageReadbackTask( info );
	}


	if( config.computeBrdfLut )
	{
		{
			imageShaderCreateInfo_t info = {};
			info.name = "BrdfLutCalculation";
			info.progHdl = AssetLibGpuProgram::Handle( "preCalculatedBrdfLut" );
			info.context = renderContext;
			info.resources = resources;

			{
				imageInfo_t imgInfo {};
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

			tasks.brdfLutTask = new ImageShaderTask( info );
		}

		{
			const std::string fileName = "brdf_lut.img";

			imageReadBackCreateInfo_t info {};
			info.name = "BrdfLutReadback";
			info.img = tasks.brdfLutTask->GetOutputImage();
			info.context = renderContext;
			info.resources = resources;
			info.fileName = fileName.c_str();
			info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
			info.flags |= imageReadbackFlags_t::PACKED_HDR;

			tasks.readbackBrdfLut = new ImageReadbackTask( info );
		}
	}

	if( config.computeNoiseImage )
	{
		{
			imageShaderCreateInfo_t info = {};
			info.name = "NoiseImageCalculation";
			info.progHdl = AssetLibGpuProgram::Handle( "NoiseGen" );
			info.context = renderContext;
			info.resources = resources;

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

			tasks.noiseGenTask = new ImageShaderTask( info );
		}

		{
			const std::string fileName = "noise.img";

			imageReadBackCreateInfo_t info{};
			info.name = "NoiseImageReadback";
			info.img = tasks.noiseGenTask->GetOutputImage();
			info.context = renderContext;
			info.resources = resources;
			info.fileName = fileName.c_str();
			info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
			info.flags |= imageReadbackFlags_t::PACKED_HDR;

			tasks.readbackNoiseImage = new ImageReadbackTask( info );
		}
	}

	if( config.screenshot )
	{
		imageReadBackCreateInfo_t info {};
		info.name = "ScreenshotReadback";
		info.context = renderContext;
		info.resources = resources;
		info.fileName = "screenshot.png";
		info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
		info.flags |= imageReadbackFlags_t::SCREENSHOT;
		info.img = resources->mainColorResolvedImage;

		tasks.screenshotReadback = new ImageReadbackTask( info );
	}

	for( uint32_t i = 0; i < MaxShadowViews; ++i ) {
		schedule->Link( new RenderTask( viewContext->shadowViews[ i ], DRAWPASS_SHADOW_BEGIN, DRAWPASS_SHADOW_END ) );
	}
	schedule->Link( new RenderTask( viewContext->renderViews[ 0 ], DRAWPASS_DEPTH, DRAWPASS_DEPTH ) );
	if( tasks.resolvePostDepth )
	{
		schedule->Link( tasks.resolvePostDepth );
	}
	if( tasks.ssaoTask )
	{
		schedule->Link( tasks.ssaoTask );
	}
	schedule->Link( new RenderTask( viewContext->renderViews[ 0 ], DRAWPASS_OPAQUE_COLOR_BEGIN, DRAWPASS_MAIN_END ) );

	if( config.useCubeViews ) {
		schedule->Link( new RenderTask( viewContext->renderViews[ 1 ], DRAWPASS_MAIN_BEGIN, DRAWPASS_MAIN_END ) );

		if( config.computeDiffuseIbl )
		{
			schedule->Link( tasks.diffuseIBL );
			schedule->Link( tasks.imageDiffuseIblReadbackTask );
		}
		if( config.computeSpecularIBL )
		{
			schedule->Link( tasks.specularIBL );
			schedule->Link( tasks.imageSpecularIblReadBackTask );
		}
		if( config.computeEnvMap )
		{
			schedule->Link( tasks.mipCubeTask );
			schedule->Link( tasks.imageCubemapReadBackTask );
		}
	}
	if( tasks.brdfLutTask ) {
		schedule->Link( tasks.brdfLutTask );
	}
	if( tasks.readbackBrdfLut ) {
		schedule->Link( tasks.readbackBrdfLut );
	}
	if( tasks.noiseGenTask ) {
		schedule->Link( tasks.noiseGenTask );
	}
	if( tasks.readbackBrdfLut ) {
		schedule->Link( tasks.readbackNoiseImage );
	}
	schedule->Link( tasks.resolve );

	if( config.screenshot ) {
		schedule->Link( tasks.screenshotReadback );
	}
	if( config.autoExposure ) {
		schedule->Link( tasks.copyPreviousLuminance );
		schedule->Link( tasks.luminanceSceneAvg );
	}
	if( config.bloom ) {
		schedule->Link( tasks.bloomDownsampleTask );
		schedule->Link( tasks.bloomUpsampleTask );
	}
	if( config.downsampleScene ) {
		schedule->Link( tasks.mipTask );
	}
	if( config.gaussianBlur ) {
		schedule->Link( tasks.gaussianTask );
	}
	schedule->Link( new RenderTask( viewContext->view2Ds[ 0 ], DRAWPASS_2D, DRAWPASS_2D ) );
	schedule->Link( new ImguiTask( viewContext->view2Ds[ 0 ]->passes[ 0 ][ DRAWPASS_DEBUG_2D ], renderContext, resources, false ) );
	schedule->Link( new RenderTask( viewContext->view2Ds[ 0 ], DRAWPASS_DEBUG_2D, DRAWPASS_DEBUG_2D ) ); // FIXME: Causes framebuffer resize issue due to multiple calls to Resize()
	//schedule->Link( new ComputeTask( "ClearParticles", &particleState ) );

	schedule->AsString();
}

#if defined( USE_IMGUI )
void DrawScheduleDebugMenu( TaskSchedule* schedule )
{
	if ( ImGui::BeginTabItem( "Schedule" ) )
	{
		int index = 0;
		GpuTask* task = schedule->GetHead();
		while ( task != nullptr )
		{
			ImGui::PushID( index );

			const bool open = ImGui::CollapsingHeader( task->AsString().c_str() );

			bool enabled = task->IsEnabled();
			ImGui::SameLine();
			if ( ImGui::Checkbox( "##enabled", &enabled ) )
			{
				task->SetEnabled( enabled );
			}

			if ( open )
			{
				ImGui::Indent();
				task->SetControls();
				ImGui::Unindent();
			}

			ImGui::PopID();
			task = task->GetChild();
			++index;
		}
		ImGui::EndTabItem();
	}
}
#endif
