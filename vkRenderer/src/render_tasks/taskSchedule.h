#pragma once

#include "RenderTask.h"

class TaskSchedule
{
private:
	GpuTask*	tasks;
	GpuTask*	end;
	GpuTask*	currentTask;
	uint32_t	taskCount;

public:

	TaskSchedule() : tasks( nullptr ), end( nullptr ), currentTask( nullptr ), taskCount( 0 )
	{}

	~TaskSchedule()
	{
		Clear();
	}

	uint32_t		TaskCount() const;
	bool			HasPendingTasks() const;
	void			Clear();
	void			Link( GpuTask* task );
	void			FrameBegin();
	void			FrameEnd();
	void			Resize();
	void			IssueNext( CommandList& context );
	void			DrainPending();
	void			AsString() const;

	const GpuTask*	GetHead() const { return tasks; }
	GpuTask*		GetHead() { return tasks; }
};


class SubScheduleTask : public GpuTask
{
private:
	TaskSchedule	m_subSchedule;
	std::string		m_name;

public:
	SubScheduleTask( const std::string& name, const uint32_t constantsByteSize = 0 )
	{
		m_shaderConstantsByteSize = constantsByteSize;
		m_name = name;
	}

	void Link( GpuTask* task ) { m_subSchedule.Link( task ); }

	TaskSchedule* GetSubSchedule() override { return &m_subSchedule; }

	void FrameBegin() override
	{
		OnFrameBegin();
		m_subSchedule.FrameBegin();
	}

	void FrameEnd() override
	{
		if ( m_subSchedule.HasPendingTasks() )
			m_subSchedule.DrainPending();
		m_subSchedule.FrameEnd();
	}

	void Resize() override { m_subSchedule.Resize(); }

	void Execute( CommandList& context ) override
	{
		while ( m_subSchedule.HasPendingTasks() )
			m_subSchedule.IssueNext( context );
	}

	std::string AsString() const override { return m_name; }
};
