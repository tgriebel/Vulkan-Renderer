#include "renderview.h"

void RenderView::Init( const char* name, renderViewRegion_t region, const int viewId, FrameBuffer& fb )
{
	const uint32_t frameStateCount = MaxFrameStates;

	const uint32_t width = fb.GetWidth();
	const uint32_t height = fb.GetHeight();

	m_viewport.width = width;
	m_viewport.height = height;

	imageSamples_t samples = IMAGE_SMP_1;
	if ( fb.ColorLayerCount() > 0 ) {
		samples = fb.GetColor()->info.subsamples;
	} else {
		samples = fb.GetDepth()->info.subsamples;
	}

	m_name = name;
	m_region = region;
	m_framebuffer = &fb;

	for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx ) {
		passes[ passIx ] = nullptr;
	}

	const uint32_t beginPass = ViewRegionPassBegin();
	const uint32_t endPass = ViewRegionPassEnd();

	for ( uint32_t passIx = beginPass; passIx <= endPass; ++passIx )
	{
		switch( passIx )
		{
			case DRAWPASS_SHADOW:
				passes[ passIx ] = new ShadowPass();
				break;
			case DRAWPASS_DEPTH:
				passes[ passIx ] = new DepthPass();
				passes[ passIx ]->sampleRate = samples;
				break;
			case DRAWPASS_TERRAIN:
				passes[ passIx ] = new TerrainPass();
				passes[ passIx ]->sampleRate = samples;
				break;
			case DRAWPASS_OPAQUE:
				passes[ passIx ] = new OpaquePass();
				passes[ passIx ]->sampleRate = samples;
				break;
			case DRAWPASS_SKYBOX:
				passes[ passIx ] = new SkyboxPass();
				passes[ passIx ]->sampleRate = samples;
				break;
			case DRAWPASS_TRANS:
				passes[ passIx ] = new TransPass();
				passes[ passIx ]->sampleRate = samples;
				break;
			case DRAWPASS_EMISSIVE:
				passes[ passIx ] = new EmissivePass();
				passes[ passIx ]->sampleRate = samples;
				break;
			case DRAWPASS_DEBUG_3D:
				passes[ passIx ] = new Debug3dPass();
				passes[ passIx ]->sampleRate = samples;
				break;
			case DRAWPASS_DEBUG_WIREFRAME:
				passes[ passIx ] = new WireframePass();
				passes[ passIx ]->sampleRate = samples;
				break;
			case DRAWPASS_POST_2D:
				passes[ passIx ] = new PostPass();
				break;
			case DRAWPASS_DEBUG_2D:
				passes[ passIx ] = new Debug2dPass();
				break;
		}

		passes[ passIx ]->viewport.x = 0;
		passes[ passIx ]->viewport.y = 0;
		passes[ passIx ]->viewport.width = width;
		passes[ passIx ]->viewport.height = height;
		passes[ passIx ]->fb = &fb;
	}

	if( region == renderViewRegion_t::SHADOW )
	{
		m_transitionState.flags.clear = true;
		m_transitionState.flags.store = true;
		m_transitionState.flags.readOnly = true;
		m_transitionState.flags.readAfter = true;
		m_transitionState.flags.presentBefore = false;
		m_transitionState.flags.presentAfter = false;

		m_clearColor = vec4f( 0.0f, 0.0f, 0.0f, 1.0f );
		m_clearDepth = 1.0f;
		m_clearStencil = 0;
	}
	else if( region == renderViewRegion_t::STANDARD_RASTER )
	{
		m_transitionState.flags.clear = true;
		m_transitionState.flags.store = true;
		m_transitionState.flags.readOnly = true;
		m_transitionState.flags.readAfter = true;
		m_transitionState.flags.presentBefore = false;
		m_transitionState.flags.presentAfter = false;

		m_clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );
		m_clearDepth = 0.0f;
		m_clearStencil = 0;
	}
	else if ( region == renderViewRegion_t::POST )
	{
		m_transitionState.flags.clear = true;
		m_transitionState.flags.store = true;
		m_transitionState.flags.readOnly = false;
		m_transitionState.flags.readAfter = false;
		m_transitionState.flags.presentBefore = true;
		m_transitionState.flags.presentAfter = true;

		m_clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );
		m_clearDepth = 0.0f;
		m_clearStencil = 0;
	}

	m_viewId = viewId;
}


void RenderView::Resize()
{
	for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
	{
		DrawPass* pass = passes[ passIx ];
		if ( pass == nullptr ) {
			continue;
		}
		pass->viewport.width = pass->fb->GetWidth();
		pass->viewport.height = pass->fb->GetHeight();
	}
}


void RenderView::SetViewRect( const int32_t x, const int32_t y, const uint32_t width, const uint32_t height )
{
	m_viewport.x = x;
	m_viewport.y = y;
	m_viewport.width = width;
	m_viewport.height = height;
}


drawPass_t RenderView::ViewRegionPassBegin() const
{
	switch ( m_region )
	{
		case renderViewRegion_t::SHADOW:			return DRAWPASS_SHADOW_BEGIN;
		case renderViewRegion_t::STANDARD_RASTER:	return DRAWPASS_MAIN_BEGIN;
		case renderViewRegion_t::POST:				return DRAWPASS_POST_BEGIN;
	}
	return DRAWPASS_COUNT;
}


drawPass_t RenderView::ViewRegionPassEnd() const
{
	switch ( m_region )
	{
		case renderViewRegion_t::SHADOW:			return DRAWPASS_SHADOW_END;
		case renderViewRegion_t::STANDARD_RASTER:	return DRAWPASS_MAIN_END;
		case renderViewRegion_t::POST:				return DRAWPASS_POST_END;
	}
	return DRAWPASS_COUNT;
}


renderPassTransition_t RenderView::TransitionState() const
{
	return m_transitionState;
}


const vec4f& RenderView::ClearColor() const
{
	return m_clearColor;
}


float RenderView::ClearDepth() const
{
	return m_clearDepth;
}


uint32_t RenderView::ClearStencil() const
{
	return m_clearStencil;
}


vec2i RenderView::GetFrameSize() const
{
	if( m_framebuffer == nullptr ) {
		return vec2i( 0, 0 );
	}
	return vec2i( static_cast<int32_t>( m_framebuffer->GetWidth() ), static_cast<int32_t>( m_framebuffer->GetHeight() ) );
}


const viewport_t& RenderView::GetViewport() const
{
	return m_viewport;
}


const mat4x4f& RenderView::GetViewMatrix() const
{
	return m_viewMatrix;
}


const mat4x4f& RenderView::GetProjMatrix() const
{
	return m_projMatrix;
}


const mat4x4f& RenderView::GetViewprojMatrix() const
{
	return m_viewprojMatrix;
}


const int RenderView::GetViewId() const
{
	return m_viewId;
}


const void RenderView::SetViewId( const int id )
{
	m_viewId = id;
}


const char* RenderView::GetName() const
{
	return m_name;
}


const renderViewRegion_t RenderView::GetRegion() const
{
	return m_region;
}


const void RenderView::Commit()
{
	m_committed = true;;
}


const bool RenderView::IsCommitted() const
{
	return m_committed;
}


void RenderView::SetCamera( const Camera& camera, const bool reverseZ )
{
	m_viewMatrix = camera.GetViewMatrix();
	m_projMatrix = camera.GetPerspectiveMatrix( reverseZ );
	m_viewprojMatrix = m_projMatrix * m_viewMatrix;

	m_viewport.near = camera.GetNearClip();
	m_viewport.far = camera.GetFarClip();
}