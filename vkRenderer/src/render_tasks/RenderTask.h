#pragma once

#include "../render_core/GpuSync.h"
#include "../render_state/frameBuffer.h"
#include "../render_resources/imageView.h"

class CommandContext;
class GfxCmdList;
class RenderView;
class RenderContext;
class ResourceContext;
class Image;

class GpuTask
{
protected:
	GpuTask* m_child = nullptr;

public:
	virtual void			FrameBegin() {};
	virtual void			FrameEnd() {};
	virtual void			Resize() = 0;
	virtual void			Execute( CommandContext& context ) = 0;
	virtual std::string		AsString() const = 0;
	GpuTask*				GetChild() { return m_child; };

	void SetChild( GpuTask* child )
	{
		if ( child == nullptr ) {
			return;
		}
		m_child = child;
	};

	virtual ~GpuTask() {};
};


class RenderTask : public GpuTask
{
private:
	RenderView*		m_renderView;
	drawPass_t		m_beginPass;
	drawPass_t		m_endPass;
	GpuSemaphore	m_finishedSemaphore;

	void Init( RenderView* view, drawPass_t begin, drawPass_t end );
	void Shutdown();
	void RenderViewSurfaces( GfxCmdList* context, const uint32_t multiViewIndex );

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

	void			FrameBegin();
	void			FrameEnd();
	void			Resize();
	std::string		AsString() const;

	void			Execute( CommandContext& context ) override;
};
