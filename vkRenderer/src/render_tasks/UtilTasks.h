#pragma once

#include "RenderTask.h"

class CommandList;
class RenderContext;
enum gpuImageStateFlags_t : uint8_t;


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
	void			Execute( CommandList& context ) override;

	~TransitionImageTask() {}
};


class ImageShaderTask;
class RenderContext;
class ResourceContext;

static const uint32_t MaxResolves = 8;

struct perResolveInfo_t
{
	resolveImageInfo_t	info;
	bool				useApi;
};

struct resolveTaskCreateInfo_t
{
	perResolveInfo_t	resolves[ MaxResolves ];
	uint32_t			count;
	RenderContext*		context;
	ResourceContext*	resources;
};

class ResolveImageTask : public GpuTask
{
private:
	resolveTaskCreateInfo_t	m_createInfo;
	uint32_t				m_count;
	RenderContext*			m_context;
	ResourceContext*		m_resources;
	ImageShaderTask*		m_shaderTasks[ MaxResolves ] = {};

	void					InitShaderTasks();

public:
	ResolveImageTask( const resolveTaskCreateInfo_t& createInfo );

	void			Resize();
	void			FrameBegin();
	void			FrameEnd();
	std::string		AsString() const;
	void			Execute( CommandList& context ) override;

	~ResolveImageTask();
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

	void			Execute( CommandList& context ) override;

	~CopyImageTask() {}
};
