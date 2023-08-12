#pragma once

#include <vector>
#include <GfxCore/asset_types/material.h>
#include "../render_core/GpuSync.h"

class CommandContext;
class RenderView;

class GpuTask
{
public:
	virtual void Execute( CommandContext& context ) = 0;
};


class RenderTask : public GpuTask
{
private:
	RenderView*		renderView;
	drawPass_t		beginPass;
	drawPass_t		endPass;
	GpuSemaphore	finishedSemaphore;

	void Init( RenderView* view, drawPass_t begin, drawPass_t end );
	void Shuntdown();
	void RenderViewSurfaces( CommandContext& context );

public:
	RenderTask( RenderView* view, drawPass_t begin, drawPass_t end )
	{
		Init( view, begin, end );
	}

	~RenderTask()
	{
		Shuntdown();
	}

	void Execute( CommandContext& context ) override;
};


class ComputeTask : public GpuTask
{
public:
	void Execute( CommandContext& context ) override;
};


class RenderSchedule
{
private:
	std::vector<GpuTask*> tasks;
	
public:

	void AddTask( GpuTask* task );
};