#pragma once

#include <functional>

#include "../render_core/renderer.h"
#include "../render_resources/imageView.h"
#include "../render_resources/gpuBuffer.h"

class ShaderBindParms;

struct computeTaskCreateInfo_t
{
	const char*			name;
	const char*			progName;
	RenderContext*		context;
	ResourceContext*	resources;

	uint64_t			bindSetId; // Hash id of the target bindset (e.g. bindset_compute)

	uint32_t			dispatchX;
	uint32_t			dispatchY;
	uint32_t			dispatchZ;

	// Invoked once per frame to populate the bindset. The caller is responsible
	// for matching the slot names / resource types declared by bindSetId.
	std::function<void( ShaderBindParms* )> bind;

	const void*			pushConstants;		// Optional. Set at init time
	uint32_t			pushConstantsSize;	// Optional
};


class ComputeTask : public GpuTask
{
private:
	RenderContext*			m_context;
	ResourceContext*		m_resources;
	ShaderBindParms*		m_parms;

	std::string				m_name;
	
	hdl_t					m_progHdl;
	uint32_t				m_dispatchX;
	uint32_t				m_dispatchY;
	uint32_t				m_dispatchZ;

	std::function<void( ShaderBindParms* )> m_bind;

	std::vector<uint8_t>	m_pushConstants;

	void Init( const computeTaskCreateInfo_t& info );
	void Shutdown();

public:

	ComputeTask( const computeTaskCreateInfo_t& info )
	{
		Init( info );
	}

	~ComputeTask()
	{
		Shutdown();
	}

	void			Resize() {}
	void			FrameBegin();
	std::string		AsString() const;

	void Execute( CommandList& context ) override;
};
