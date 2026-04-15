#include "schedule.h"

#include "../scene/entity.h"
#include "../render_state/rhi.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/bindings.h"
#include "../render_resources/imageArray.h"
#include "../render_tasks/RenderTask.h"
#include "../render_tasks/ImageReadbackTask.h"
#include "../render_tasks/ImageProcessTask.h"
#include "../render_tasks/imguiTask.h"
#include "../globals/assetDefs.h"

void BuildSceneSchedule( const renderConfig_t& config, RenderContext* renderContext, ResourceContext* resources, RenderViewContext* viewContext, RenderSchedule* schedule )
{
	ImageProcessTask* diffuseIBL = nullptr;
	ImageReadbackTask* imageDiffuseIblReadbackTask = nullptr;

	ImageProcessTask* specularIBL = nullptr;
	ImageReadbackTask* imageSpecularIblReadBackTask = nullptr;

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
			{
				imageInfo_t imgInfo {};
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

				imageReadBackCreateInfo_t info {};
				info.name = "DiffuseIblReadback";
				info.img = diffuseIBL->GetOutputImage();
				info.context = renderContext;
				info.resources = resources;
				info.fileName = fileName.c_str();
				info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
				info.flags |= imageReadbackFlags_t::CUBEMAP;
				info.flags |= imageReadbackFlags_t::PACKED_HDR;

				imageDiffuseIblReadbackTask = new ImageReadbackTask( info );
			}
		}

		if( config.computeSpecularIBL ) {
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

			specularIBL = new ImageProcessTask( info );

			{
				const std::string fileName = std::string( config.cubemapName ) + "_specIbl.img";

				imageReadBackCreateInfo_t info {};
				info.name = "SpecularIblReadback";
				info.img = specularIBL->GetOutputImage();
				info.context = renderContext;
				info.resources = resources;
				info.fileName = fileName.c_str();
				if( config.writeCubeViews ) {
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
		info.progHdl = AssetLibGpuProgram::Handle( "Resolve" );
		info.permSet = ForceDisableMSAA ? shaderPermId_t::MRT : shaderPermId_t::MSAA | shaderPermId_t::MRT;
		info.outputImage = resources->mainColorResolvedImage;
		info.outputImage1 = &resources->depthResolvedImageView;
		info.context = renderContext;
		info.resources = resources;
		info.inputImages = 3;

		resolve = new ImageShaderTask( info );

		resolve->SetSourceImage( 0, resources->mainColorImage );
		resolve->SetSourceImage( 1, &resources->depthImageView );
		resolve->SetSourceImage( 2, &resources->stencilImageView );
	}

	ImageProcessTask* gaussianTask = nullptr;
	if( config.gaussianBlur ) {
		imageProcessCreateInfo_t info {};
		info.name = "Separable Gaussian";
		info.context = renderContext;
		info.resources = resources;
		info.outputImage = resources->blurredImage;
		info.progName = "SeparableGaussianBlur";
		info.resourceImages[ 0 ] = resources->mainColorResolvedImage;
		info.baseMip = 0;

		gaussianTask = new ImageProcessTask( info );
	}


	CopyImageTask* copyPreviousLuminance = nullptr;
	ImageProcessTask* luminanceSceneAvg = nullptr;

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

			copyPreviousLuminance = new CopyImageTask( resources->currentLum, srcCopy, resources->previousLum, dstCopy );
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

			luminanceSceneAvg = new ImageProcessTask( info );
		}
	}

	ImageProcessTask* mipTask = nullptr;
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

		mipTask = new ImageProcessTask( info );
	}

	ImageProcessTask* bloomDownsampleTask = nullptr;
	ImageProcessTask* bloomUpsampleTask = nullptr;
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

		bloomDownsampleTask = new ImageProcessTask( info );

		info.name = "BloomUpsample";
		info.sourceImage = resources->bloom;
		info.outputImage = resources->bloom; // Overwrite the previous downsampled values with upsampled ones
		info.baseMip = 0;
		info.upsampleProcess = true;
		info.progName = "BloomUpsample";

		bloomUpsampleTask = new ImageProcessTask( info );
	}

	ImageProcessTask* mipCubeTask = nullptr;
	if( config.useCubeViews )
	{
		imageProcessCreateInfo_t info {};
		info.name = "CubeDownsample";
		info.context = renderContext;
		info.resources = resources;
		info.outputImage = resources->cubeFbColorImage;
		info.progressiveSampling = true;
		info.baseMip = 1;

		mipCubeTask = new ImageProcessTask( info );
	}

	ImageReadbackTask* imageCubemapReadBackTask = nullptr;
	if( config.useCubeViews )
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

			brdfLutTask = new ImageShaderTask( info );
		}

		{
			const std::string fileName = "brdf_lut.img";

			imageReadBackCreateInfo_t info {};
			info.name = "BrdfLutReadback";
			info.img = brdfLutTask->GetOutputImage();
			info.context = renderContext;
			info.resources = resources;
			info.fileName = fileName.c_str();
			info.flags |= imageReadbackFlags_t::WRITE_TO_DISK;
			info.flags |= imageReadbackFlags_t::PACKED_HDR;

			readbackBrdfLut = new ImageReadbackTask( info );
		}
	}

	ImageReadbackTask* screenshotReadback = nullptr;
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

		screenshotReadback = new ImageReadbackTask( info );
	}

	for( uint32_t i = 0; i < MaxShadowViews; ++i ) {
		schedule->Link( new RenderTask( viewContext->shadowViews[ i ], DRAWPASS_SHADOW_BEGIN, DRAWPASS_SHADOW_END ) );
	}
	schedule->Link( new RenderTask( viewContext->renderViews[ 0 ], DRAWPASS_MAIN_BEGIN, DRAWPASS_MAIN_END ) );
	if( config.useCubeViews ) {
		schedule->Link( new RenderTask( viewContext->renderViews[ 1 ], DRAWPASS_MAIN_BEGIN, DRAWPASS_MAIN_END ) );

		if( config.computeDiffuseIbl )
		{
			schedule->Link( diffuseIBL );
			schedule->Link( imageDiffuseIblReadbackTask );
		}
		if( config.computeSpecularIBL )
		{
			schedule->Link( specularIBL );
			schedule->Link( imageSpecularIblReadBackTask );
		}
		schedule->Link( mipCubeTask );
		schedule->Link( imageCubemapReadBackTask );
	}
	if( brdfLutTask ) {
		schedule->Link( brdfLutTask );
	}
	if( readbackBrdfLut ) {
		schedule->Link( readbackBrdfLut );
	}
	schedule->Link( resolve );

	if( config.screenshot ) {
		schedule->Link( screenshotReadback );
	}
	if( config.autoExposure ) {
		schedule->Link( copyPreviousLuminance );
		schedule->Link( luminanceSceneAvg );
	}
	if( config.bloom ) {
		schedule->Link( bloomDownsampleTask );
		schedule->Link( bloomUpsampleTask );
	}
	if( config.downsampleScene ) {
		schedule->Link( mipTask );
	}
	if( config.gaussianBlur ) {
		schedule->Link( gaussianTask );
	}
	schedule->Link( new RenderTask( viewContext->view2Ds[ 0 ], DRAWPASS_2D, DRAWPASS_2D ) );
	schedule->Link( new ImguiTask( viewContext->view2Ds[ 0 ]->passes[ 0 ][ DRAWPASS_DEBUG_2D ], renderContext, resources, false ) );
	schedule->Link( new RenderTask( viewContext->view2Ds[ 0 ], DRAWPASS_DEBUG_2D, DRAWPASS_DEBUG_2D ) ); // FIXME: Causes framebuffer resize issue due to multiple calls to Resize()
	//schedule->Link( new ComputeTask( "ClearParticles", &particleState ) );

	schedule->AsString();
}
