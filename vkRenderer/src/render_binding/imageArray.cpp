#include "imageArray.h"
#include "../render_core/renderer.h"

void ImageArray::SetRenderContext( RenderContext* context )
{
	m_context = context;
}

const Image* const& ImageArray::operator[]( uint32_t index ) const
{
	return BaseImageArray::operator[]( index );
}

const Image*& ImageArray::operator[]( uint32_t index )
{
	assert( m_context );
	m_lastFrameUpdate[ index ] = m_context->FrameNumber(); // Conservative marking since any assignment could set the same pointer

	return BaseImageArray::operator[]( index );
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