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


void RenderView::SetViewport( const viewport_t& viewport )
{
	m_viewport = viewport;
}


const viewport_t& RenderView::GetViewport() const
{
	return m_viewport;
}