#include "imageArray.h"
#include "../render_core/renderer.h"

void ImageArray::SetRenderContext( RenderContext* context )
{
	m_context = context;
}


void ImageArray::BindIndex( const uint32_t index, const Image* image, const bool forceUpdate )
{
	assert( m_context );

	if( ( (*this)[ index ] != image ) || forceUpdate )
	{
		m_lastFrameUpdate[ index ] = m_context->FrameNumber(); // Conservative marking since any assignment could set the same pointer

		BaseImageArray::operator[]( index ) = image;
	}
}


[[nodiscard]]
bool ImageArray::HasPossibleUpdates() const
{
	uint32_t count = Count();
	for( uint32_t i = 0; i < count; ++i )
	{
		if( m_lastFrameUpdate[ i ] == m_context->FrameNumber() )
		{
			return true;
		}
	}
	return false;
}