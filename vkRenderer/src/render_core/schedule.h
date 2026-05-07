#pragma once

#include "renderer.h"

class ImageProcessTask;
class ImageReadbackTask;
class ImageShaderTask;
class CopyImageTask;
class ResolveImageTask;

// FIXME: Temp, remove once interface becomes clear
// Intentionally lazy pointers to arrays because this will be removed
struct RenderViewContext
{
	RenderView** activeViews;
	RenderView** renderViews;
	RenderView** shadowViews;
	RenderView** view2Ds;
};


struct availableTasks_t
{
	// Prebaking Tasks
	ImageProcessTask*	diffuseIBL						= nullptr;
	ImageReadbackTask*	imageDiffuseIblReadbackTask		= nullptr;
	ImageProcessTask*	specularIBL						= nullptr;
	ImageReadbackTask*	imageSpecularIblReadBackTask	= nullptr;
	ImageShaderTask*	brdfLutTask						= nullptr;
	ImageReadbackTask*	readbackBrdfLut					= nullptr;
	ImageShaderTask*	noiseGenTask					= nullptr;
	ImageReadbackTask*	readbackNoiseImage				= nullptr;
	ImageProcessTask*	mipCubeTask						= nullptr;
	ImageReadbackTask*	imageCubemapReadBackTask		= nullptr;

	// Core frame
	ResolveImageTask*	resolve							= nullptr;
	ResolveImageTask*	resolvePostDepth				= nullptr;
	ImageReadbackTask*	screenshotReadback				= nullptr;

	// Image-Space
	ImageProcessTask*	gaussianTask					= nullptr;
	ImageProcessTask*	ssaoTask						= nullptr;
	ImageProcessTask*	ssaoBlurTask					= nullptr;
	CopyImageTask*		copyPreviousLuminance			= nullptr;
	ImageProcessTask*	luminanceSceneAvg				= nullptr;
	ImageProcessTask*	mipTask							= nullptr;
	ImageProcessTask*	bloomDownsampleTask				= nullptr;
	ImageProcessTask*	bloomUpsampleTask				= nullptr;
};

void BuildSceneSchedule( const renderConfig_t& config, RenderContext* renderContext, ResourceContext* resourceContext, RenderViewContext* viewContext, TaskSchedule* schedule );

#if defined( USE_IMGUI )
void DrawScheduleDebugMenu( TaskSchedule* schedule );
#endif
