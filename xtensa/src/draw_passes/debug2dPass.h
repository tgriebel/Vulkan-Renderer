#pragma once

#include "drawpass.h"

class Debug2dPass : public DrawPass
{
public:
	Debug2dPass( RenderContext* renderContext, FrameBuffer* fb )
	{
		Init( renderContext, fb );
	}

	virtual void Init( RenderContext* renderContext, FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};
