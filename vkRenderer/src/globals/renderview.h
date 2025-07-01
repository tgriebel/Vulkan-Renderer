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

static const uint32_t MaxMultiViews = 6;

enum class renderViewRegion_t : uint32_t
{
	SHADOW			= 0,
	STANDARD_RASTER = 1,
	STANDARD_2D		= 2,
	COUNT,
	UNKNOWN,
};


struct renderViewCreateInfo_t
{
	const char*					name;
	renderViewRegion_t			region;
	int							viewId;

	const ResourceContext*		resources;
	RenderContext*				context;

	bool						isCubeView;
	frameBufferCreateInfo_t		fbImages;
};


class RenderView
{
private:
	using debugMenuArray_t = Array<debugMenuFuncPtr, 12>;

	const ResourceContext*	m_resources;
	FrameBuffer*			m_framebuffers[ MaxMultiViews ];
	ImageView*				m_colorViews[ MaxMultiViews ];
	ImageView*				m_gBuffer0Views[ MaxMultiViews ];
	ImageView*				m_gBuffer1Views[ MaxMultiViews ];
	ImageView*				m_depthViews[ MaxMultiViews ];
	ImageView*				m_stencilViews[ MaxMultiViews ];
	ShaderBindParms*		m_viewParms;
	vec4f					m_clearColor;
	float					m_clearDepth;
	uint32_t				m_clearStencil;
	renderPassTransition_t	m_transitionState;
	viewport_t				m_viewport;
	mat4x4f					m_viewMatrices[ MaxMultiViews ];
	mat4x4f					m_projMatrices[ MaxMultiViews ];
	mat4x4f					m_viewprojMatrices[ MaxMultiViews ];
	const char*				m_name;
	renderViewRegion_t		m_region;
	uint32_t				m_multiViewCount;
	int						m_viewBufferId;
	int						m_surfaceBufferId;
	bool					m_committed;

public:

	RenderView()
	{
		m_viewport = viewport_t( 0, 0, DefaultDisplayWidth, DefaultDisplayHeight, 0.0f, 1.0f );

		m_multiViewCount = 1;

		m_viewBufferId = -1;
		m_surfaceBufferId = -1;
		m_committed = false;

		numLights = 0;
		memset( drawGroupOffset, 0, sizeof( drawGroupOffset ) );

		for( uint32_t multiViewIndex = 0; multiViewIndex < MaxMultiViews; ++multiViewIndex )
		{
			m_viewMatrices[ multiViewIndex ] = mat4x4f( 1.0f );
			m_projMatrices[ multiViewIndex ] = mat4x4f( 1.0f );
			m_viewprojMatrices[ multiViewIndex ] = mat4x4f( 1.0f );

			m_framebuffers[ multiViewIndex ] = nullptr;

			for ( uint32_t passIndex = 0; passIndex < DRAWPASS_COUNT; ++passIndex ) {
				passes[ multiViewIndex ][ passIndex ] = nullptr;
			}
		}
		m_region = renderViewRegion_t::UNKNOWN;
	}

	~RenderView()
	{
		for ( uint32_t multiViewIndex = 0; multiViewIndex < MaxMultiViews; ++multiViewIndex )
		{
			for ( uint32_t passIndex = 0; passIndex < DRAWPASS_COUNT; ++passIndex )
			{
				if ( passes[ multiViewIndex ] != nullptr )
				{
					delete passes[ multiViewIndex ][ passIndex ];
					passes[ multiViewIndex ][ passIndex ] = nullptr;
				}
			}
			if( m_framebuffers[ multiViewIndex ] != nullptr )
			{
				delete m_framebuffers[ multiViewIndex ];
				m_framebuffers[ multiViewIndex ] = nullptr;

				delete m_colorViews[ multiViewIndex ];
				m_colorViews[ multiViewIndex ] = nullptr;

				delete m_gBuffer0Views[ multiViewIndex ];
				m_gBuffer0Views[ multiViewIndex ] = nullptr;

				delete m_gBuffer1Views[ multiViewIndex ];
				m_gBuffer1Views[ multiViewIndex ] = nullptr;

				delete m_depthViews[ multiViewIndex ];
				m_depthViews[ multiViewIndex ] = nullptr;

				delete m_stencilViews[ multiViewIndex ];
				m_stencilViews[ multiViewIndex ] = nullptr;
			}	
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
	const ShaderBindParms*	BindParms() const;

	void					SetCamera( const Camera& camera, const bool reverseZ = true, const uint32_t multiView = 0 );
	void					SetViewRect( const int32_t x, const int32_t y, const uint32_t width, const uint32_t height );
	const viewport_t&		GetViewport() const;
	vec2i					GetFrameSize() const;
	const mat4x4f&			GetViewMatrix( const uint32_t multiView = 0 ) const;
	const mat4x4f&			GetProjMatrix( const uint32_t multiView = 0 ) const;
	const mat4x4f&			GetViewprojMatrix( const uint32_t multiView = 0 ) const;
	int						GetViewBufferId( const int multiView ) const; // TODO: Have view own it's view buffer. Eliminates indexing
	int						GetSurfaceBufferId() const; // TODO: Have view own it's surface buffer. Eliminates indexing

	uint32_t				GetMultiViewCount() const;

	const char*				GetName() const;
	const renderViewRegion_t GetRegion() const;

	const void				Commit();
	bool					IsCommitted() const;

	void					AttachDebugMenu( const debugMenuFuncPtr funcPtr );

	uint32_t				lights[ MaxLights ];
	uint32_t				numLights;
	uint32_t				drawGroupOffset[ DRAWPASS_COUNT ];
	DrawPass*				passes[ MaxMultiViews ][ DRAWPASS_COUNT ];
	DrawGroup				drawGroup[ DRAWPASS_COUNT ];
	debugMenuArray_t		debugMenus;
};