#include "TaskSchedule.h"
#include "../render_core/renderResource.h"


uint32_t TaskSchedule::TaskCount() const
{
	return taskCount;
}


bool TaskSchedule::HasPendingTasks() const
{
	return ( currentTask != nullptr );
}


void TaskSchedule::Clear()
{
	RenderResource::Cleanup( resourceLifeTime_t::TASK );

	GpuTask* t = tasks;
	while( t != nullptr )
	{
		GpuTask* next = t->GetChild();
		delete t;
		t = next;
	}
	tasks       = nullptr;
	end         = nullptr;
	currentTask = nullptr;
	taskCount   = 0;
}


void TaskSchedule::Link( GpuTask* task )
{
	assert( task );
	if( end == nullptr )
	{
		assert( tasks == nullptr );
		tasks = task;
		end = task;
	}
	else
	{
		end->SetChild( task );
		end = task;
	}

	++taskCount;
}


void TaskSchedule::FrameBegin()
{
	currentTask = tasks;

	GpuTask* t = tasks;
	while ( t != nullptr )
	{
		t->FrameBegin();
		t = t->GetChild();
	}
}


void TaskSchedule::FrameEnd()
{
	GpuTask* t = tasks;
	while ( t != nullptr )
	{
		t->FrameEnd();
		t = t->GetChild();
	}
	assert( currentTask == nullptr );
}


void TaskSchedule::Resize()
{
	GpuTask* t = tasks;
	while ( t != nullptr )
	{
		t->Resize();
		t = t->GetChild();
	}
}


void TaskSchedule::IssueNext( CommandList& context )
{
	if ( currentTask->IsEnabled() )
	{
		currentTask->Execute( context );
	}
	currentTask = currentTask->GetChild();
}


void TaskSchedule::AsString() const
{
	std::cout << "Schedule\n";

	GpuTask* t = tasks;
	while ( t != nullptr )
	{
		std::cout << "+ <" << t->AsString() << ">\n";
		t = t->GetChild();
	}
	std::cout << std::flush;
}
