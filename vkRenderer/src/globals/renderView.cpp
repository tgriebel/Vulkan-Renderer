#include "renderview.h"

void RenderView::Init( const char* name, renderViewRegion_t region, const int viewId, FrameBuffer fb[ MAX_FRAMES_STATES ] )
{
	const uint32_t frameStateCount = MAX_FRAMES_STATES;

	const viewport_t& viewport = GetViewport();
	const uint32_t width = viewport.width;
	const uint32_t height = viewport.height;

	m_name = name;
	m_region = region;

	m_frameBufferSize = vec2i( fb[ 0 ].GetWidth(), fb[ 0 ].GetHeight() );

	for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
	{
		passes[ passIx ] = nullptr;

		if ( passIx < ViewRegionPassBegin() ) {
			continue;
		}
		if ( passIx > ViewRegionPassEnd() ) {
			continue;
		}
		DrawPass* pass = new DrawPass();

		pass->name = GetPassDebugName( drawPass_t( passIx ) );
		pass->viewport.x = 0;
		pass->viewport.y = 0;
		pass->viewport.width = Min( width, fb[ 0 ].GetWidth() );
		pass->viewport.height = Min( height, fb[ 0 ].GetHeight() );
		for ( uint32_t i = 0; i < frameStateCount; ++i ) {
			pass->fb[ i ] = &fb[ i ];
		}

		pass->transitionState.bits = 0;
		pass->stateBits = GFX_STATE_NONE;
		pass->passId = drawPass_t( passIx );

		if ( passIx == DRAWPASS_SHADOW )
		{
			pass->transitionState.flags.clear = true;
			pass->transitionState.flags.store = true;
			pass->transitionState.flags.readAfter = true;
			pass->transitionState.flags.presentAfter = false;

			pass->stateBits |= GFX_STATE_DEPTH_TEST;
			pass->stateBits |= GFX_STATE_DEPTH_WRITE;
			pass->stateBits |= GFX_STATE_DEPTH_OP_0;

			pass->clearColor = vec4f( 0.0f, 0.0f, 0.0f, 1.0f );
			pass->clearDepth = 1.0f;
			pass->clearStencil = 0;

			pass->sampleRate = imageSamples_t::IMAGE_SMP_1;
		}
		else if ( passIx == DRAWPASS_POST_2D )
		{
			pass->transitionState.flags.clear = true;
			pass->transitionState.flags.store = true;
			pass->transitionState.flags.readAfter = false;
			pass->transitionState.flags.presentAfter = true;

			pass->clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );
			pass->clearDepth = 0.0f;
			pass->clearStencil = 0;

			pass->stateBits |= GFX_STATE_BLEND_ENABLE;
			pass->sampleRate = imageSamples_t::IMAGE_SMP_1;
		}
		else
		{
			pass->transitionState.flags.clear = false;
			pass->transitionState.flags.store = true;
			pass->transitionState.flags.readAfter = true;
			pass->transitionState.flags.presentAfter = false;

			gfxStateBits_t stateBits = GFX_STATE_NONE;
			if ( passIx == DRAWPASS_DEPTH )
			{
				stateBits |= GFX_STATE_DEPTH_TEST;
				stateBits |= GFX_STATE_DEPTH_WRITE;
				stateBits |= GFX_STATE_COLOR_MASK;
				stateBits |= GFX_STATE_MSAA_ENABLE;
				stateBits |= GFX_STATE_CULL_MODE_BACK;
				stateBits |= GFX_STATE_STENCIL_ENABLE;

				pass->transitionState.flags.clear = true;
			}
			else if ( passIx == DRAWPASS_TRANS )
			{
				stateBits |= GFX_STATE_DEPTH_TEST;
				stateBits |= GFX_STATE_CULL_MODE_BACK;
				stateBits |= GFX_STATE_MSAA_ENABLE;
				stateBits |= GFX_STATE_BLEND_ENABLE;
			}
			else if ( passIx == DRAWPASS_DEBUG_WIREFRAME )
			{
				stateBits |= GFX_STATE_WIREFRAME_ENABLE;
				stateBits |= GFX_STATE_MSAA_ENABLE;

				pass->transitionState.flags.readAfter = true;
			}
			else if ( passIx == DRAWPASS_DEBUG_SOLID )
			{
				stateBits |= GFX_STATE_CULL_MODE_BACK;
				stateBits |= GFX_STATE_BLEND_ENABLE;
				stateBits |= GFX_STATE_MSAA_ENABLE;
			}
			else
			{
				stateBits |= GFX_STATE_DEPTH_TEST;
				stateBits |= GFX_STATE_DEPTH_WRITE;
				stateBits |= GFX_STATE_CULL_MODE_BACK;
				stateBits |= GFX_STATE_MSAA_ENABLE;
			}
			pass->stateBits = stateBits;

			pass->clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );
			pass->clearDepth = 0.0f;
			pass->clearStencil = 0;

			if( fb[ 0 ].GetColorLayers() > 0 ) {
				pass->sampleRate = fb[ 0 ].GetColor()->info.subsamples;
			} else {
				pass->sampleRate = fb[ 0 ].GetDepth()->info.subsamples;
			}
		}

		passes[ passIx ] = pass;
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
		pass->viewport.width = pass->fb[ 0 ]->GetWidth();
		pass->viewport.height = pass->fb[ 0 ]->GetHeight();
	}
}


void RenderView::SetViewRect( const int32_t x, const int32_t y, const uint32_t width, const uint32_t height )
{
	m_viewport.x = x;
	m_viewport.y = y;
	m_viewport.width = width;
	m_viewport.height = height;
}


drawPass_t RenderView::ViewRegionPassBegin()
{
	switch ( m_region )
	{
		case renderViewRegion_t::SHADOW:			return DRAWPASS_SHADOW_BEGIN;
		case renderViewRegion_t::STANDARD_RASTER:	return DRAWPASS_MAIN_BEGIN;
		case renderViewRegion_t::POST:				return DRAWPASS_POST_BEGIN;
	}
	return DRAWPASS_COUNT;
}


drawPass_t RenderView::ViewRegionPassEnd()
{
	switch ( m_region )
	{
		case renderViewRegion_t::SHADOW:			return DRAWPASS_SHADOW_END;
		case renderViewRegion_t::STANDARD_RASTER:	return DRAWPASS_MAIN_END;
		case renderViewRegion_t::POST:				return DRAWPASS_POST_END;
	}
	return DRAWPASS_COUNT;
}


vec2i RenderView::GetFrameSize() const
{
	return m_frameBufferSize;
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