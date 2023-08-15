#pragma once

#include <cstdint>
#include <gfxcore/scene/scene.h>
#include "common.h"

class GpuBuffer;

class DrawGroup
{
public:

	DrawGroup()
	{
		committedModelCnt = 0;
		mergedModelCnt = 0;

		ib = nullptr;
		vb = nullptr;

		memset( surfaces,			0, MaxSurfaces );
		memset( sortedSurfaces,		0, MaxSurfaces );
		memset( merged,				0, MaxSurfaces );
		memset( uploads,			0, MaxSurfaces );
		memset( sortedInstances,	0, MaxSurfaces );
		memset( instances,			0, MaxSurfaces );
		memset( instanceCounts,		0, MaxSurfaces );
	}

	GpuBuffer*				ib;	// FIXME: don't use a pointer
	GpuBuffer*				vb;

	uint32_t				committedModelCnt;
	uint32_t				mergedModelCnt;
	drawSurf_t				surfaces[ MaxSurfaces ];
	drawSurf_t				sortedSurfaces[ MaxSurfaces ];
	drawSurfInstance_t		instances[ MaxSurfaces ];
	drawSurfInstance_t		sortedInstances[ MaxSurfaces ];
	drawSurf_t				merged[ MaxSurfaces ];
	surfaceUpload_t			uploads[ MaxSurfaces ];
	uint32_t				instanceCounts[ MaxSurfaces ];
};