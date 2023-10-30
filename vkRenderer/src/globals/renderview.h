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
#include "drawGroup.h"
#include "../draw_passes/drawpass.h"

class ResourceContext;
class RenderContext;

enum class renderViewRegion_t : uint32_t
{
	SHADOW			= 0,
	STANDARD_RASTER = 1,
	POST			= 2,
	COUNT,
	UNKNOWN,
};


struct renderViewCreateInfo_t
{
	const char*				name;
	renderViewRegion_t		region;
	int						viewId;
	const ResourceContext*	resources;
	RenderContext*			context;
	FrameBuffer*			fb;
};


class RenderView
{
private:
	using debugMenuArray_t = Array<debugMenuFuncPtr, 12>;

	const ResourceContext*	m_resources;
	const FrameBuffer*		m_framebuffer;
	ShaderBindParms*		m_viewParms;
	vec4f					m_clearColor;
	float					m_clearDepth;
	uint32_t				m_clearStencil;
	renderPassTransition_t	m_transitionState;
	viewport_t				m_viewport;
	mat4x4f					m_viewMatrix;
	mat4x4f					m_projMatrix;
	mat4x4f					m_viewprojMatrix;
	const char*				m_name;
	renderViewRegion_t		m_region;
	int						m_viewId;
	bool					m_committed;

public:

	RenderView()
	{
		m_viewport = viewport_t( 0, 0, DefaultDisplayWidth, DefaultDisplayHeight, 0.0f, 1.0f );

		for( uint32_t i = 0; i < DRAWPASS_COUNT; ++i ) {
			passes[ i ] = nullptr;
		}

		m_viewMatrix = mat4x4f( 1.0f );
		m_projMatrix = mat4x4f( 1.0f );
		m_viewprojMatrix = mat4x4f( 1.0f );

		m_viewId = -1;
		m_committed = false;

		numLights = 0;
		memset( drawGroupOffset, 0, sizeof( drawGroupOffset ) );

		m_framebuffer = nullptr;
		m_region = renderViewRegion_t::UNKNOWN;
	}

	~RenderView()
	{
		for ( uint32_t i = 0; i < DRAWPASS_COUNT; ++i )
		{
			delete passes[ i ];
			passes[ i ] = nullptr;
		}
	}

	void					Init( const renderViewCreateInfo_t& info );
	void					FrameBegin();
	void					FrameEnd();
	void					Resize();

	drawPass_t				ViewRegionPassBegin() const;
	drawPass_t				ViewRegionPassEnd() const;
	renderPassTransition_t	TransitionState() const;
	const vec4f&			ClearColor() const;
	float					ClearDepth() const;
	uint32_t				ClearStencil() const;

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
	uint32_t				drawGroupOffset[ DRAWPASS_COUNT ];
	DrawPass*				passes[ DRAWPASS_COUNT ];
	DrawGroup				drawGroup[ DRAWPASS_COUNT ];
	debugMenuArray_t		debugMenus;
};