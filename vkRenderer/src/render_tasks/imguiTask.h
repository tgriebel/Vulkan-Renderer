#pragma once

#include <queue>
#include "RenderTask.h"

class CommandContext;
class GfxContext;
class RenderView;
class RenderContext;
class ResourceContext;
class Image;
struct ComputeState;

enum gpuImageStateFlags_t : uint8_t;

class ImguiTask : public GpuTask
{
private:
	renderPassTransition_t	m_transitionState = {};
	DrawPass*				m_pass;

	void Init( RenderView* view, const bool presentAfter );
	void Shutdown();

public:
	ImguiTask( RenderView* view, const bool presentAfter )
	{
		Init( view, presentAfter );
	}

	~ImguiTask()
	{
		Shutdown();
	}

	void				FrameBegin();
	void				FrameEnd();
	void				Resize();
	std::string			AsString() const;

	void				Execute( CommandContext& context ) override;
};