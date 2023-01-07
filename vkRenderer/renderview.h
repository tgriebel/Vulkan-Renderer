#pragma once
#include <scene/scene.h>
#include "common.h"

class RenderView
{
public:
	RenderView()
	{
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)DEFAULT_DISPLAY_WIDTH;
		viewport.height = (float)DEFAULT_DISPLAY_HEIGHT;
		viewport.near = 1.0f;
		viewport.far = 0.0f;

		committedModelCnt = 0;
		mergedModelCnt = 0;
	}

	mat4x4f										viewMatrix;
	mat4x4f										projMatrix;
	mat4x4f										viewprojMatrix;
	viewport_t									viewport;
	light_t										lights[ MaxLights ];

	uint32_t									committedModelCnt;
	uint32_t									mergedModelCnt;
	drawSurf_t									surfaces[ MaxModels ];
	drawSurf_t									merged[ MaxModels ];
	drawSurfInstance_t							instances[ MaxModels ];
	uint32_t									instanceCounts[ MaxModels ];
};