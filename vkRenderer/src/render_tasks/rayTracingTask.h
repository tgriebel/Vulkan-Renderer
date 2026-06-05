#pragma once

#include <functional>

#include "../render_core/renderer.h"
#include "../render_resources/imageView.h"

class ShaderBindParms;
class RayTracingTask;

struct rayTracingTaskCreateInfo_t
{
	const char*			name;
	const char*			rgenProgName;
	const char*			missProgName;
	const char*			hitGroupProgName;
	RenderContext*		context;
	ResourceContext*	resources;
	const Image*		image;				// Output image. Determines trace dimensions

	uint64_t			bindSetId;			// Bindset id

	const void*			constants;			// Optional, Custom shader constants pushed at execute time
	uint32_t			constantsByteSize;	// Size in bytes
};


class RayTracingTask : public GpuTask
{
private:
	RenderContext*		m_context;
	ResourceContext*	m_resources;
	ShaderBindParms*	m_parms;

	std::string			m_name;

	const Image*		m_image = nullptr;

	hdl_t				m_pipelineHdl;

	std::function<void( RayTracingTask* task, ShaderBindParms* )> m_bind;

	void Init( const rayTracingTaskCreateInfo_t& info );
	void Shutdown();

public:

	RayTracingTask( const rayTracingTaskCreateInfo_t& info )
	{
		Init( info );
	}

	~RayTracingTask()
	{
		Shutdown();
	}

	void		Resize() {}
	void		FrameBegin();
	std::string	AsString() const;

	void Execute( CommandList& context ) override;
};
