#pragma once

#include <queue>
#include "RenderTask.h"

class CommandContext;
class GfxCmdList;
class RenderView;
class RenderContext;
class ResourceContext;
class Image;
struct ComputeState;

enum gpuImageStateFlags_t : uint8_t;

class ImguiTask : public GpuTask
{
private:
	static const uint32_t	MaxBufferSizeInBytes = 256;

	renderPassTransition_t	m_transitionState = {};
	ResourceContext*		m_resources;
	RenderContext*			m_context;
	const DrawPass*			m_imguiPass;
	DrawPass*				m_imagePass;
	GpuBuffer				m_buffer;

	void Init( const DrawPass* pass, RenderContext* renderContext, ResourceContext* resourceContext, const bool finalizeImage );
	void Shutdown();

public:
	ImguiTask( const DrawPass* pass, RenderContext* renderContext, ResourceContext* resourceContext, const bool finalizeImage )
	{
		Init( pass, renderContext, resourceContext, finalizeImage );
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