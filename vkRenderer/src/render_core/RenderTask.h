#pragma once

#include <vector>

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
	RenderView* view;

	void RenderViewSurfaces( CommandContext& context );

public:
	RenderTask( RenderView* view ) : view( view ) {}

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