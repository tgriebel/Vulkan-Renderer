#pragma once

#include <queue>
#include <GfxCore/asset_types/material.h>
#include "../render_core/GpuSync.h"

class CommandContext;
class GfxContext;
class RenderView;
class Image;
struct ComputeState;

enum gpuImageStateFlags_t : uint8_t;

class GpuTask
{
public:
	virtual void	FrameBegin() = 0;
	virtual void	FrameEnd() = 0;
	virtual void	Execute( CommandContext& context ) = 0;

	virtual ~GpuTask() {};
};


class RenderTask : public GpuTask
{
private:
	RenderView*		renderView;
	drawPass_t		beginPass;
	drawPass_t		endPass;
	GpuSemaphore	finishedSemaphore;

	void Init( RenderView* view, drawPass_t begin, drawPass_t end );
	void Shutdown();
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
		Shutdown();
	}

	void FrameBegin();
	void FrameEnd();

	void Execute( CommandContext& context ) override;
};


class ComputeTask : public GpuTask
{
private:
	ComputeState*		m_state;
	hdl_t				m_progHdl;

public:
	ComputeTask( const char* csName, ComputeState* state );

	void FrameBegin();
	void FrameEnd();

	void Execute( CommandContext& context ) override;
	~ComputeTask()
	{}
};


class TransitionImageTask : public GpuTask
{
private:
	Image*					m_img;
	gpuImageStateFlags_t	m_srcState;
	gpuImageStateFlags_t	m_dstState;

public:

	TransitionImageTask( Image* img, const gpuImageStateFlags_t srcState, const gpuImageStateFlags_t dstState )
	{
		m_img = img;
		m_srcState = srcState;
		m_dstState = dstState;
	}

	void FrameBegin() {}
	void FrameEnd() {}

	void Execute( CommandContext& context ) override;
	~TransitionImageTask()
	{}
};


class CopyImageTask : public GpuTask
{
private:
	Image*	m_src;
	Image*	m_dst;

public:

	CopyImageTask( Image* src, Image* dst )
	{
		m_src = src;
		m_dst = dst;
	}

	void FrameBegin() {}
	void FrameEnd() {}

	void Execute( CommandContext& context ) override;
	~CopyImageTask()
	{}
};


class MipImageTask : public GpuTask
{
private:
	Image* m_img;

public:

	MipImageTask( Image* img )
	{
		m_img = img;
	}

	void FrameBegin() {}
	void FrameEnd() {}

	void Execute( CommandContext& context ) override;
	~MipImageTask()
	{}
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
	void		Clear();
	void		Queue( GpuTask* task );
	void		FrameBegin();
	void		FrameEnd();
	void		IssueNext( CommandContext& context );
};