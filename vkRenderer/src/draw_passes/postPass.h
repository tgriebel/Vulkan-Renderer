#pragma once

#include "drawpass.h"

class PostPass : public DrawPass
{
public:
	PostPass()
	{
	}

	PostPass( RenderContext* renderContext, FrameBuffer* fb )
	{
		Init( renderContext, fb );
	}

	virtual void Init( RenderContext* renderContext, FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};