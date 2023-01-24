#pragma once

#include "../../common.h"
#include "../render_binding/gpuResources.h"

class FrameState
{
public:
	GpuImage		viewColorImage;
	GpuImage		shadowMapImage;
	GpuImage		depthImage;
	GpuImage		stencilImage;
	GpuBuffer		globalConstants;
	GpuBuffer		surfParms;
	GpuBuffer		materialBuffers;
	GpuBuffer		lightParms;
};