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

	uint32_t		TaskCount() const;
	bool			HasPendingTasks() const;
	void			Clear();
	void			Link( GpuTask* task );
	void			FrameBegin();
	void			FrameEnd();
	void			Resize();
	void			IssueNext( CommandContext& context );
	void			AsString() const;

	const GpuTask*	GetHead() const { return tasks; }
	GpuTask*		GetHead() { return tasks; }
};
