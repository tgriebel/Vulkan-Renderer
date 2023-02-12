/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once
#include <cstdint>
#include <scene/scene.h>
#include "common.h"

enum class renderViewRegion_t : uint32_t
{
	UNKNOWN,
	SHADOW,
	MAIN,
	POST
};

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

		region = renderViewRegion_t::UNKNOWN;

		memset( surfaces, 0, MaxSurfaces );
		memset( merged, 0, MaxSurfaces );
		memset( instances, 0, MaxSurfaces );
		memset( instanceCounts, 0, MaxSurfaces );
	}

	const char*				name;
	renderViewRegion_t		region;
	mat4x4f					viewMatrix;
	mat4x4f					projMatrix;
	mat4x4f					viewprojMatrix;
	viewport_t				viewport;
	light_t					lights[ MaxLights ];

	uint32_t				committedModelCnt;
	uint32_t				mergedModelCnt;
	drawSurf_t				surfaces[ MaxSurfaces ];
	drawSurf_t				merged[ MaxSurfaces ];
	drawSurfInstance_t		instances[ MaxSurfaces ];
	uint32_t				instanceCounts[ MaxSurfaces ];
};