#pragma once

#include "RenderTask.h"

class CommandContext;
class RenderContext;
struct ComputeState;
enum gpuImageStateFlags_t : uint8_t;


class ComputeTask : public GpuTask
{
private:
	ComputeState*	m_state;
	hdl_t			m_progHdl;
	std::string		m_name;

public:
	ComputeTask( const char* csName, ComputeState* state );

	void			Resize() {}
	void			FrameBegin() {}
	void			FrameEnd() {}
	std::string		AsString() const;
	void			Execute( CommandContext& context ) override;

	~ComputeTask() {}
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
		m_img      = img;
		m_srcState = srcState;
		m_dstState = dstState;
	}

	void			Resize() {}
	void			FrameBegin() {}
	void			FrameEnd() {}
	std::string		AsString() const;
	void			Execute( CommandContext& context ) override;

	~TransitionImageTask() {}
};


class ResolveImageTask : public GpuTask
{
private:
	static const uint32_t	MaxResolves = 8;
	resolveImageInfo_t		m_infos[ MaxResolves ];
	uint32_t				m_count;

public:
	ResolveImageTask( const resolveImageInfo_t* infos, const uint32_t count )
	{
		assert( count <= MaxResolves );
		m_count = Min( count, MaxResolves );
		for ( uint32_t i = 0; i < m_count; ++i ) {
			m_infos[ i ] = infos[ i ];
		}
	}

	ResolveImageTask( const resolveImageInfo_t& info ) : ResolveImageTask( &info, 1 ) {}

	void			Resize() {}
	void			FrameBegin() {}
	void			FrameEnd() {}
	std::string		AsString() const;
	void			Execute( CommandContext& context ) override;

	~ResolveImageTask() {}
};


class CopyImageTask : public GpuTask
{
private:
	Image*				m_src;
	Image*				m_dst;
	copyImageParms_t	m_srcParms;
	copyImageParms_t	m_dstParms;

public:
	CopyImageTask( Image* src, Image* dst );
	CopyImageTask( Image* src, const copyImageParms_t& srcParms, Image* dst, const copyImageParms_t& dstParms );

	void			Resize() {}
	void			FrameBegin() {}
	void			FrameEnd() {}
	std::string		AsString() const;

	void			SetSourceParms( const copyImageParms_t& src );
	void			SetDestinationParms( const copyImageParms_t& dst );

	void			Execute( CommandContext& context ) override;

	~CopyImageTask() {}
};
