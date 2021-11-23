#pragma once

#include "common.h"

class FrameState
{
public:
	GpuImage		viewColorImage;
	GpuImage		shadowMapImage;
	GpuImage		depthImage;
	GpuBuffer		globalConstants;
	GpuBuffer		surfParms;
	GpuBuffer		materialBuffers;
	GpuBuffer		lightParms;
};