#pragma once

#include "renderer.h"

class ImageMipTask;
class ImageReadbackTask;
class ImageShaderTask;
class CopyImageTask;
class ResolveImageTask;
class ComputeTask;
class TransitionImageTask;

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
	ImageMipTask*	diffuseIBL						= nullptr;
	ImageReadbackTask*	imageDiffuseIblReadbackTask		= nullptr;
	ImageMipTask*	specularIBL						= nullptr;
	ImageReadbackTask*	imageSpecularIblReadBackTask	= nullptr;
	ImageShaderTask*	brdfLutTask						= nullptr;
	ImageReadbackTask*	readbackBrdfLut					= nullptr;
	ImageShaderTask*	noiseGenTask					= nullptr;
	ImageReadbackTask*	readbackNoiseImage				= nullptr;
	ImageMipTask*	mipCubeTask						= nullptr;
	ImageReadbackTask*	imageCubemapReadBackTask		= nullptr;

	// Core frame
	ResolveImageTask*	resolve							= nullptr;
	ResolveImageTask*	resolvePostDepth				= nullptr;
	ImageReadbackTask*	screenshotReadback				= nullptr;

	// Image-Space
	ImageMipTask*	gaussianTask					= nullptr;
	ImageMipTask*	ssaoTask						= nullptr;
	ImageMipTask*	ssaoBlurTask					= nullptr;
	ImageMipTask*	dofCocTask						= nullptr;
	ComputeTask*		dofTileTask						= nullptr;
	TransitionImageTask* transitionWriteDofTileTask		= nullptr;
	TransitionImageTask* transitionReadDofTileTask		= nullptr;
	ImageMipTask*	dofBokehTask					= nullptr;
	ImageShaderTask*	dofBlurTask						= nullptr;
	CopyImageTask*		copyPreviousLuminance			= nullptr;
	ImageMipTask*	luminanceSceneAvg				= nullptr;
	ImageMipTask*	mipTask							= nullptr;
	ImageMipTask*	bloomDownsampleTask				= nullptr;
	ImageMipTask*	bloomUpsampleTask				= nullptr;
};

void BuildSceneSchedule( const renderConfig_t& config, RenderContext* renderContext, ResourceContext* resourceContext, RenderViewContext* viewContext, TaskSchedule* schedule );

#if defined( USE_IMGUI )
void DrawScheduleDebugMenu( TaskSchedule* schedule );
#endif
