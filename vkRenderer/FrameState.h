#pragma once

#include "common.h"

class FrameState
{
public:
	DeviceImage		viewColorImages;
	DeviceImage		shadowMapsImages;
	DeviceBuffer	globalConstants;
	DeviceBuffer	surfParms;
	DeviceBuffer	materialBuffers;
	DeviceBuffer	lightParms;
	DeviceBuffer	vb;
	DeviceBuffer	ib;
};