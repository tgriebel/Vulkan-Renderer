#include "RenderTask.h"
#include "../render_state/cmdContext.h"
#include "../globals/renderview.h"

#if defined( USE_IMGUI )
#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../external/imgui/backends/imgui_impl_vulkan.h"
#endif

void RenderTask::RenderViewSurfaces( CommandContext& cmdContext )
{
	assert( 0 );
}


void RenderTask::Init( RenderView* view, drawPass_t begin, drawPass_t end )
{
	renderView = view;
	beginPass = begin;
	endPass= end;

	finishedSemaphore.Create( "Task Finished" );
}


void RenderTask::Shuntdown()
{
	finishedSemaphore.Destroy();
}


void RenderTask::Execute( CommandContext& context )
{
	context.MarkerBeginRegion( renderView->GetName(), ColorToVector( Color::White ) );

	RenderViewSurfaces( context );

	context.MarkerEndRegion();
}