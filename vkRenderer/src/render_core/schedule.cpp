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
#include "../render_tasks/ComputeTask.h"
#include "../render_binding/bindings.h"

static availableTasks_t tasks;

void BuildSceneSchedule( const renderConfig_t& config, RenderContext* renderContext, ResourceContext* resources, RenderViewContext* viewContext, TaskSchedule* schedule )
{
	SCOPED_TIMER_PRINT( ScheduleBuild, MILLISECOND )

	const uint32_t displayWidth = renderContext->GetDisplayWidth();
	const uint32_t displayHeight = renderContext->GetDisplayHeight();

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
		info.multiPass = true;

		tasks.gaussianTask = new ImageProcessTask( info );
	}

	if( config.ssao )
	{
		// Image
		{
			imageInfo_t info{};
			info.width = displayWidth;
			info.height = displayHeight;
			info.mipLevels = 1;
			info.layers = 1;
			info.subsamples = IMAGE_SMP_1;
			info.fmt = IMAGE_FMT_R_16;
			info.type = IMAGE_TYPE_2D;
			info.tiling = IMAGE_TILING_MORTON;

			resources->ssaoImage->Create(
				info,
				"FB_ssao", GPU_IMAGE_RW, resourceLifeTime_t::RESIZE
			);
		}

		// Image Process
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
			info.resourceImages[ 1 ] = resources->gBufferLayerResolvedImage0;
			info.baseMip = 0;
			info.viewId = viewContext->renderViews[ 0 ]->GetViewBufferId( 0 );
			info.constants = &ssaoDefaults;
			info.constantsByteSize = sizeof( ssaoDefaults );

			tasks.ssaoTask = new ImageProcessTask( info );

#if defined( USE_IMGUI )
			tasks.ssaoTask->RegisterControls( [ ssaoTask = tasks.ssaoTask ]()
				{
					ssaoConstants_t& c = *ssaoTask->GetConstants<ssaoConstants_t>();
					bool changed = false;
					changed |= ImGui::SliderFloat( "Radius", &c.radius, 0.1f, 2.0f );
					changed |= ImGui::SliderInt( "Samples", (int*)&c.numSamples, 4, 32 );
					changed |= ImGui::SliderFloat( "Bias", &c.bias, 0.0f, 0.1f );
					changed |= ImGui::SliderFloat( "Strength", &c.strength, 0.5f, 4.0f );
					if( changed )
					{
						ssaoTask->UpdateConstants();
					}
				} );
#endif
		}

		// Blur
		{
			// Blur Image
			{
				imageInfo_t info{};
				info.width = displayWidth;
				info.height = displayHeight;
				info.mipLevels = 1;
				info.layers = 1;
				info.subsamples = IMAGE_SMP_1;
				info.fmt = IMAGE_FMT_R_16;
				info.type = IMAGE_TYPE_2D;
				info.tiling = IMAGE_TILING_MORTON;

				resources->ssaoBlurImage->Create(
					info,
					"FB_ssaoBlurredImage", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER, resourceLifeTime_t::RESIZE
				);
			}

			struct bilateralConstants_t
			{
				float kDepth;
				float pad0;
				float pad1;
				float pad2;
			};

			const bilateralConstants_t bilateralDefaults = { 1.0f, 0.0f, 0.0f, 0.0f };

			imageProcessCreateInfo_t info{};
			info.name = "SSAO Bilateral Blur";
			info.context = renderContext;
			info.resources = resources;
			info.outputImage = resources->ssaoBlurImage;
			info.progName = "BilateralBlur";
			info.resourceImages[ 0 ] = resources->ssaoImage;
			info.resourceImages[ 1 ] = resources->depthStencilResolvedImage;
			info.baseMip = 0;
			info.mipCount = 1;
			info.multiPass = true;
			info.constants = &bilateralDefaults;
			info.constantsByteSize = sizeof( bilateralDefaults );

			tasks.ssaoBlurTask = new ImageProcessTask( info );

#if defined( USE_IMGUI )
			tasks.ssaoBlurTask->RegisterControls( [ blurTask = tasks.ssaoBlurTask ]()
				{
					bilateralConstants_t& c = *blurTask->GetConstants<bilateralConstants_t>();
					if( ImGui::SliderFloat( "Edge Stop", &c.kDepth, 0.0f, 1.0f ) )
					{
						blurTask->UpdateConstants();
					}
				} );
#endif
		}
	}

	if( config.dof )
	{
		const RenderView* view = viewContext->renderViews[ 0 ];

		// Image
		{
			imageInfo_t info{};
			info.width = ( displayWidth / 2 );
			info.height = ( displayHeight / 2 );
			info.mipLevels = 1;
			info.layers = 1;
			info.subsamples = IMAGE_SMP_1;
			info.fmt = IMAGE_FMT_R11G11B10_US;
			info.type = IMAGE_TYPE_2D;
			info.tiling = IMAGE_TILING_MORTON;

			resources->dofCocImage->Create(
				info,
				"FB_dofCoc", GPU_IMAGE_RW, resourceLifeTime_t::RESIZE
			);

			resources->dofCocImage->RegisterResize( [ info ]( uint32_t w, uint32_t h )->imageInfo_t
			{
				imageInfo_t resized = info;
				resized.width = ( w / 2 );
				resized.height = ( h / 2 );
				return resized;
			} );
		}

		// Constants for all DoF Passes

		struct dofUserConstants_t
		{
			vec4f		srcDepthDimensions;
			uint32_t	viewId;
			float		focalLengthMM;
			float		focalPlaneDistanceMM;
			float		apertureDiameterMM;
			float		maxCocRadius;
		};

		struct dofShaderConstants_t
		{
			vec4f		srcDepthDimensions;
			uint32_t	viewId;
			float		focalLength;
			float		focalPlaneDistance;
			float		apertureDiameter;
			float		maxCocRadius;
		};

		static dofUserConstants_t userParms{};

		userParms.focalLengthMM = 14.0f;
		userParms.focalPlaneDistanceMM = 10000.0f;
		userParms.apertureDiameterMM = 25.0f;
		userParms.maxCocRadius = 16.0f;
		userParms.viewId = view->GetViewBufferId();
		userParms.srcDepthDimensions.x = (float)resources->depthStencilResolvedImage->info.width;
		userParms.srcDepthDimensions.y = (float)resources->depthStencilResolvedImage->info.height;
		userParms.srcDepthDimensions.z = 1.0f / userParms.srcDepthDimensions.x;
		userParms.srcDepthDimensions.w = 1.0f / userParms.srcDepthDimensions.y;

		// DoF Circle-of-Confusion Calculation
		{
			imageProcessCreateInfo_t info{};
			info.name = "DoF CoC";
			info.context = renderContext;
			info.resources = resources;
			info.outputImage = resources->dofCocImage;
			info.progName = "DofCoc";
			info.resourceImages[ 0 ] = resources->depthStencilResolvedImage;
			info.baseMip = 0;
			info.mipCount = 1;
			info.viewId = viewContext->renderViews[ 0 ]->GetViewBufferId( 0 );
			info.constants = &userParms;
			info.constantsByteSize = sizeof( userParms );

			tasks.dofCocTask = new ImageProcessTask( info );
		}

		// DoF Tile min/max CoC binning
		{
			{
				imageInfo_t info{};
				info.width = ( displayWidth / 16 );
				info.height = ( displayHeight / 16 );
				info.mipLevels = 1;
				info.layers = 1;
				info.subsamples = IMAGE_SMP_1;
				info.fmt = IMAGE_FMT_RGBA_32;
				info.type = IMAGE_TYPE_2D;
				info.tiling = IMAGE_TILING_MORTON;

				resources->dofTileCocImage->Create(
					info,
					"FB_dofTileCoc", GPU_IMAGE_STORAGE | GPU_IMAGE_READ, resourceLifeTime_t::RESIZE
				);

				resources->dofTileCocImage->RegisterResize( [ info ]( uint32_t w, uint32_t h )->imageInfo_t
				{
					imageInfo_t resized = info;
					resized.width = ( w / 16 );
					resized.height = ( h / 16 );
					return resized;
				} );
			}

			BINDING( dofSourceImage, IMAGE_2D, 1, BIND_STATE_CS );

			const uint64_t bindset_dofTile = renderContext->CreateBindSet( "bindset_dofTile", {
				BINDING_NAME( globalsBuffer ),
				BINDING_NAME( viewBuffer ),
				BINDING_NAME( dofSourceImage ),
				BINDING_NAME( computeWriteImage ),
			} );

			Asset<GpuProgram>* progAsset = GpuProgramLib().Find( "DofTileMinMax" );
			GpuProgram& prog = progAsset->Get();
			prog.bindsets[ 0 ] = renderContext->LookupBindSet( "bindset_dofTile" );
			prog.bindsetCount = 1;

			computeTaskCreateInfo_t info{};
			info.name = "DoF Tile MinMax";
			info.context = renderContext;
			info.resources = resources;
			info.progName = "DofTileMinMax";
			info.imageTileSizeX = 16;
			info.imageTileSizeY = 16;
			info.image = resources->depthStencilResolvedImage;
			info.constants = &userParms;
			info.constantsByteSize = sizeof( userParms );
			info.bindSetId = bindset_dofTile;
			info.bind = [ resources, view ]( ComputeTask* task, ShaderBindParms* p )
			{
				p->Bind( BINDING_NAME( globalsBuffer ), &resources->globalConstants );
				p->Bind( BINDING_NAME( viewBuffer ), &resources->viewParms );
				p->Bind( BINDING_NAME( dofSourceImage ), resources->depthStencilResolvedImage );
				p->Bind( BINDING_NAME( computeWriteImage ), resources->dofTileCocImage );
			};

			tasks.dofTileTask = new ComputeTask( info );

#if defined( USE_IMGUI )
			tasks.dofTileTask->RegisterControls( [ resources, view, task = tasks.dofTileTask ]()
			{
				bool changed = false;
				changed |= ImGui::SliderFloat( "Focal Length (mm)", &userParms.focalLengthMM, 14.0f, 200.0f );
				changed |= ImGui::SliderFloat( "Aperture Diameter (mm)", &userParms.apertureDiameterMM, 1.0f, 50.0f );
				changed |= ImGui::SliderFloat( "Focus Distance (mm)", &userParms.focalPlaneDistanceMM, 500.0f, 1000000.0f, "%.0f", ImGuiSliderFlags_Logarithmic );
				changed |= ImGui::SliderFloat( "Max CoC (pixels)", &userParms.maxCocRadius, 4.0f, 32.0f );
			} );
#endif
			tasks.dofTileTask->RegisterFrameBeginCallback( [ resources, view, task = tasks.dofTileTask ]()
			{
				dofShaderConstants_t& dofParms = *task->GetConstants<dofShaderConstants_t>();

				dofParms.viewId = view->GetViewBufferId();
				dofParms.srcDepthDimensions.x = (float)resources->depthStencilResolvedImage->info.width;
				dofParms.srcDepthDimensions.y = (float)resources->depthStencilResolvedImage->info.height;
				dofParms.srcDepthDimensions.z = 1.0f / dofParms.srcDepthDimensions.x;
				dofParms.srcDepthDimensions.w = 1.0f / dofParms.srcDepthDimensions.y;

				dofParms.apertureDiameter = userParms.apertureDiameterMM / 1000.0f;
				dofParms.focalPlaneDistance = userParms.focalPlaneDistanceMM / 1000.0f;
				dofParms.focalLength = userParms.focalLengthMM / 1000.0f;
				dofParms.maxCocRadius = userParms.maxCocRadius;
			} );
		}

		// DoF Blur Calculation
		{
			struct dofBokeh_t
			{
				float cocRadius;
				float apertureAngle;
				int32_t ngonBlades;
			};

			// Image
			{
				imageInfo_t info{};
				info.width = ( displayWidth / 2 );
				info.height = ( displayHeight / 2 );
				info.mipLevels = 1;
				info.layers = 1;
				info.subsamples = IMAGE_SMP_1;
				info.fmt = IMAGE_FMT_R11G11B10_US;
				info.type = IMAGE_TYPE_2D;
				info.tiling = IMAGE_TILING_MORTON;

				resources->dofBlur->Create(
					info,
					"FB_dofBlur", GPU_IMAGE_RW, resourceLifeTime_t::RESIZE
				);

				resources->dofBlur->RegisterResize( [ info ]( uint32_t w, uint32_t h )->imageInfo_t
					{
						imageInfo_t resized = info;
						resized.width = ( w / 2 );
						resized.height = ( h / 2 );
						return resized;
					} );
			}

			static dofBokeh_t dofBokehDefaults{};
			dofBokehDefaults.cocRadius = 4.0f;
			dofBokehDefaults.apertureAngle = PI / 4.0f;
			dofBokehDefaults.ngonBlades = 6;

			imageProcessCreateInfo_t info{};
			info.name = "DoF Blur";
			info.context = renderContext;
			info.resources = resources;
			info.outputImage = resources->dofBlur;
			info.progName = "DofBokeh";
			info.resourceImages[ 0 ] = resources->mainColorResolvedImage;
			info.resourceImages[ 1 ] = resources->dofCocImage;
		//	info.resourceImages[ 1 ] = resources->dofTileCocImage;
			info.baseMip = 0;
			info.mipCount = 1;
			info.viewId = viewContext->renderViews[ 0 ]->GetViewBufferId( 0 );
			info.constants = &dofBokehDefaults;
			info.constantsByteSize = sizeof( dofBokehDefaults );

			tasks.dofBlurTask = new ImageProcessTask( info );

#if defined( USE_IMGUI )
			tasks.dofBlurTask->RegisterControls( [ resources, view, task = tasks.dofBlurTask ]()
				{
					dofBokeh_t& dofBokeh = *task->GetConstants<dofBokeh_t>();

					bool changed = false;
					changed |= ImGui::SliderFloat( "Focal Length (mm)", &dofBokeh.cocRadius, 4.0f, 32.0f );
					changed |= ImGui::SliderFloat( "Aperture Angle (mm)", &dofBokeh.apertureAngle, 0.0f, 2.0f * PI );
					changed |= ImGui::SliderInt( "Ngon Blades (5=pentagon)", &dofBokeh.ngonBlades, 0, 12 );

					if( changed ) {
						task->UpdateConstants();
					}
				} );
#endif
		}
	}

	if( config.autoExposure )
	{
		// Images
		{
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
				info.tiling = IMAGE_TILING_MORTON;

				resources->previousLum->Create(
					info,
					"FB_previousLuminance", GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_DST, resourceLifeTime_t::REBOOT
				);
			}

			// Luminance MIP-chain
			{
				//imageInfo_t info{};
				//info.width = 1024;
				//info.height = 1024;
				//info.mipLevels = MipCount( info.width, info.height );
				//info.layers = 1;
				//info.subsamples = IMAGE_SMP_1;
				//info.fmt = IMAGE_FMT_R_16;
				//info.type = IMAGE_TYPE_2D;
				//info.aspect = IMAGE_ASPECT_COLOR_FLAG;
				//info.tiling = IMAGE_TILING_MORTON;

				//resources->currentLum->Create(
				//	info,
				//	nullptr,
				//	new GpuImage( "FB_currentLuminance", info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC, resourceLifeTime_t::RESIZE )
				//);

				//resources->currentLum->RegisterResize( nullptr );
			}
		}

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
		// Create images
		{
			imageInfo_t info{};
			info.width = displayWidth;
			info.height = displayHeight;
			info.mipLevels = MipCount( displayWidth, displayHeight );
			info.layers = 1;
			info.subsamples = IMAGE_SMP_1;
			info.fmt = IMAGE_FMT_RGBA_16;
			info.type = IMAGE_TYPE_2D;
			info.tiling = IMAGE_TILING_MORTON;

			resources->bloom->Create(
				info,
				"FB_bloom", GPU_IMAGE_RW, resourceLifeTime_t::RESIZE
			);
		}

		// First pass: Downsample
		{
			imageProcessCreateInfo_t info{};
			info.name = "BloomDownsample";
			info.context = renderContext;
			info.resources = resources;
			info.sourceImage = resources->mainColorResolvedImage;
			info.outputImage = resources->bloom;
			info.baseMip = 1;
			info.mipCount = 4; // TODO: define relative to resolution
			info.progressiveSampling = true;
			info.progName = "BloomDownsample";

			tasks.bloomDownsampleTask = new ImageProcessTask( info );
		}

		// Second pass: Upsample
		{
			struct bloomUpsampleConstants_t
			{
				float filterRadius;   // Tent filter radius in UV space
			};

			const bloomUpsampleConstants_t bloomUpsampleDefaults = { 0.005f };

			imageProcessCreateInfo_t info{};
			info.name = "BloomUpsample";
			info.context = renderContext;
			info.resources = resources;
			info.sourceImage = resources->bloom;
			info.outputImage = resources->bloom; // Overwrite the previous downsampled values with upsampled ones
			info.baseMip = 0;
			info.mipCount = 4;
			info.progressiveSampling = true;
			info.upsampleProcess = true;
			info.progName = "BloomUpsample";
			info.constants = &bloomUpsampleDefaults;
			info.constantsByteSize = sizeof( bloomUpsampleDefaults );

			tasks.bloomUpsampleTask = new ImageProcessTask( info );

#if defined( USE_IMGUI )
			tasks.bloomUpsampleTask->RegisterControls( [ bloomUpsampleTask = tasks.bloomUpsampleTask ]()
				{
					bloomUpsampleConstants_t& c = *bloomUpsampleTask->GetConstants<bloomUpsampleConstants_t>();
					if( ImGui::SliderFloat( "Filter Radius", &c.filterRadius, 0.001f, 0.02f ) )
					{
						bloomUpsampleTask->UpdateConstants();
					}
				} );
#endif
		}
	}


	if( config.cubeDownsample )
	{
		assert( config.useCubeViews );

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
		assert( config.useCubeViews );

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
				imgInfo.tiling = IMAGE_TILING_MORTON;

				info.taskImageCount = 1;
				info.createInfos = { &imgInfo };
			}

			tasks.brdfLutTask = new ImageShaderTask( info );
		}

		{
			imageReadBackCreateInfo_t info {};
			info.name = "BrdfLutReadback";
			info.img = tasks.brdfLutTask->GetOutputImage();
			info.context = renderContext;
			info.resources = resources;
			info.fileName = "brdf_lut.img";
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
		if( tasks.ssaoBlurTask ) {
			schedule->Link( tasks.ssaoBlurTask );
		}
	}
	if( tasks.dofCocTask )
	{
		schedule->Link( tasks.dofCocTask );
	}
	if( tasks.dofTileTask )
	{
		schedule->Link( tasks.dofTileTask );
	}
	if( tasks.dofBlurTask )
	{
		schedule->Link( tasks.dofBlurTask );
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
	if( tasks.readbackNoiseImage ) {
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
