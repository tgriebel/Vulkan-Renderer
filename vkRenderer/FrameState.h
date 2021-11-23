#pragma once

#include "common.h"

class FrameState
{
public:
	DeviceImage		viewColorImage;
	DeviceImage		shadowMapImage;
	DeviceImage		depthImage;
	DeviceBuffer	globalConstants;
	DeviceBuffer	surfParms;
	DeviceBuffer	materialBuffers;
	DeviceBuffer	lightParms;
};