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
#include <gfxcore/scene/scene.h>
#include "common.h"
#include "../render_core/drawpass.h"

enum class renderViewRegion_t : uint32_t
{
	SHADOW			= 0,
	STANDARD_RASTER = 1,
	POST			= 2,
	COUNT,
	UNKNOWN,
};

class RenderView
{
private:
	using debugMenuArray_t = Array<debugMenuFuncPtr, 12>;

	const FrameBuffer*	m_framebuffer;
	viewport_t			m_viewport;
	mat4x4f				m_viewMatrix;
	mat4x4f				m_projMatrix;
	mat4x4f				m_viewprojMatrix;
	const char*			m_name;
	renderViewRegion_t	m_region;
	int					m_viewId;
	bool				m_committed;

public:

	RenderView()
	{
		m_viewport = viewport_t( 0, 0, DefaultDisplayWidth, DefaultDisplayHeight, 0.0f, 1.0f );

		committedModelCnt = 0;
		mergedModelCnt = 0;

		for( uint32_t i = 0; i < DRAWPASS_COUNT; ++i ) {
			passes[ i ] = nullptr;
		}

		m_viewMatrix = mat4x4f( 1.0f );
		m_projMatrix = mat4x4f( 1.0f );
		m_viewprojMatrix = mat4x4f( 1.0f );

		m_viewId = -1;
		m_committed = false;

		m_framebuffer = nullptr;
		m_region = renderViewRegion_t::UNKNOWN;

		memset( surfaces, 0, MaxSurfaces );
		memset( sortedSurfaces, 0, MaxSurfaces );
		memset( merged, 0, MaxSurfaces );
		memset( sortedInstances, 0, MaxSurfaces );
		memset( instances, 0, MaxSurfaces );
		memset( instanceCounts, 0, MaxSurfaces );
	}

	~RenderView()
	{
		for ( uint32_t i = 0; i < DRAWPASS_COUNT; ++i )
		{
			delete passes[ i ];
			passes[ i ] = nullptr;
		}
	}

	drawPass_t				ViewRegionPassBegin();
	drawPass_t				ViewRegionPassEnd();

	void					Init( const char* name, renderViewRegion_t region, const int viewId, FrameBuffer& fb );
	void					Resize();

	void					SetCamera( const Camera& camera, const bool reverseZ = true );
	void					SetViewRect( const int32_t x, const int32_t y, const uint32_t width, const uint32_t height );
	const viewport_t&		GetViewport() const;
	vec2i					GetFrameSize() const;
	const mat4x4f&			GetViewMatrix() const;
	const mat4x4f&			GetProjMatrix() const;
	const mat4x4f&			GetViewprojMatrix() const;
	const int				GetViewId() const;
	const void				SetViewId( const int id );

	const char*				GetName() const;
	const renderViewRegion_t GetRegion() const;

	const void				Commit();
	const bool				IsCommitted() const;

	uint32_t				lights[ MaxLights ];
	uint32_t				numLights;
	DrawPass*				passes[ DRAWPASS_COUNT ];

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
	debugMenuArray_t		debugMenus;
};