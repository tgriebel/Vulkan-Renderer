#pragma once

#include <queue>
#include <GfxCore/asset_types/material.h>
#include "../render_core/GpuSync.h"

class CommandContext;
class GfxContext;
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
	void RenderViewSurfaces( GfxContext* context );

public:
	RenderTask()
	{
		Init( nullptr, DRAWPASS_COUNT, DRAWPASS_COUNT );
	}

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
	std::vector< GpuTask* >	tasks;
	uint32_t				currentTask;
	
public:

	RenderSchedule() : currentTask( 0 )
	{}

	uint32_t	PendingTasks() const;
	void		Reset();
	void		Clear();
	void		Queue( GpuTask* task );
	void		IssueNext( CommandContext& context );
};