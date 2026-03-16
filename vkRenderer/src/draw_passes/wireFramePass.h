#pragma once

#include "drawpass.h"

class WireframePass : public DrawPass
{
public:
	WireframePass( RenderContext* renderContext, FrameBuffer* fb )
	{
		Init( renderContext, fb );
	}

	virtual void Init( RenderContext* renderContext, FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};