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
struct ComputeState;

enum gpuImageStateFlags_t : uint8_t;

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
		if( child == nullptr ) {
			return;
		}
		if ( m_child != nullptr )
		{
			delete m_child;
			m_child = nullptr;
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
		Init( nullptr, DRAWPASS_COUNT, DRAWPASS_COUNT);
	}

	RenderTask( RenderView* view, drawPass_t begin, drawPass_t end )
	{
		Init( view, begin, end );
	}

	~RenderTask()
	{
		Shutdown();
	}

	void				FrameBegin();
	void				FrameEnd();
	void				Resize();
	std::string			AsString() const;

	void				Execute( CommandContext& context ) override;
};


class ComputeTask : public GpuTask
{
private:
	ComputeState*		m_state;
	hdl_t				m_progHdl;
	std::string			m_name;

public:
	ComputeTask( const char* csName, ComputeState* state );

	void Resize() {}

	void			FrameBegin();
	void			FrameEnd();
	std::string		AsString() const;

	void			Execute( CommandContext& context ) override;
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

	void Resize() {}

	void			FrameBegin() {}
	void			FrameEnd() {}
	std::string		AsString() const;
	const char*		GetName() const;

	void Execute( CommandContext& context ) override;
	~TransitionImageTask()
	{}
};


class CopyImageTask : public GpuTask
{
private:
	Image*	m_src;
	Image*	m_dst;

	copyImageParms_t m_srcParms;
	copyImageParms_t m_dstParms;

public:

	CopyImageTask( Image* src, Image* dst )
	{
		m_src = src;
		m_dst = dst;

		m_srcParms.baseArray = 0;
		m_srcParms.arrayCount = m_src->subResourceView.arrayCount;
		m_srcParms.baseMip = 0;
		m_srcParms.mipLevels = m_src->subResourceView.mipLevels;
		m_srcParms.x = 0;
		m_srcParms.y = 0;
		m_srcParms.z = 0;
		m_srcParms.width = m_src->info.width;
		m_srcParms.height = m_src->info.height;
		m_srcParms.depth = 1;

		m_dstParms.baseArray = 0;
		m_dstParms.arrayCount = m_dst->subResourceView.arrayCount;
		m_dstParms.baseMip = 0;
		m_dstParms.mipLevels = m_dst->subResourceView.mipLevels;
		m_dstParms.x = 0;
		m_dstParms.y = 0;
		m_dstParms.z = 0;
		m_dstParms.width = m_dst->info.width;
		m_dstParms.height = m_dst->info.height;
		m_dstParms.depth = 1;
	}

	CopyImageTask( Image* src, const copyImageParms_t& srcParms, Image* dst, const copyImageParms_t& dstParms )
	{
		m_src = src;
		m_dst = dst;

		m_srcParms = srcParms;
		m_dstParms = dstParms;
	}

	void Resize() {}

	void			FrameBegin() {}
	void			FrameEnd() {}
	std::string		AsString() const;
	const char*		GetName() const;

	void SetSourceParms( const copyImageParms_t& src );
	void SetDestinationParms( const copyImageParms_t& dst );

	void Execute( CommandContext& context ) override;
	~CopyImageTask()
	{}
};


class RenderSchedule
{
private:
	GpuTask*	tasks;
	GpuTask*	end;
	GpuTask*	currentTask;
	uint32_t	taskCount;
	
public:

	RenderSchedule() : tasks( nullptr ), end( nullptr ), currentTask( nullptr ), taskCount( 0 )
	{}

	uint32_t	TaskCount() const;
	bool		HasPendingTasks() const;
	void		Clear();
	void		Link( GpuTask* task );
	void		FrameBegin();
	void		FrameEnd();
	void		Resize();
	void		IssueNext( CommandContext& context );
	void		AsString() const;
};
