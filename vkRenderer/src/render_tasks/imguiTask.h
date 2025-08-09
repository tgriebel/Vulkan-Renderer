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
	const DrawPass*			m_pass;

	void Init( const DrawPass* pass, const bool finalizeImage );
	void Shutdown();

public:
	ImguiTask( const DrawPass* pass, const bool finalizeImage )
	{
		Init( pass, finalizeImage );
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