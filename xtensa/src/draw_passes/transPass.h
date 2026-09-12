#pragma once

#include "drawpass.h"

class TransPass : public DrawPass
{
public:
	TransPass( RenderContext* renderContext, FrameBuffer* fb )
	{
		Init( renderContext, fb );
	}

	virtual void Init( RenderContext* renderContext, FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};