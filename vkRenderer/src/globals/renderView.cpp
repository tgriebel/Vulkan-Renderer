#include "renderview.h"

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


void RenderView::SetCamera( const Camera& camera, const bool reverseZ )
{
	m_viewMatrix = camera.GetViewMatrix();
	m_projMatrix = camera.GetPerspectiveMatrix( reverseZ );
	m_viewprojMatrix = m_projMatrix * m_viewMatrix;

	m_viewport.near = camera.GetNearClip();
	m_viewport.far = camera.GetFarClip();
}