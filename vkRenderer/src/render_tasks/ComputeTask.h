#pragma once

#include "../render_core/renderer.h"
#include "../render_resources/imageView.h"
#include "../render_resources/gpuBuffer.h"

struct computeTaskCreateInfo_t
{
	const char*			name;
	const char*			programName;
	RenderContext*		context;
	ResourceContext*	resources;

	uint32_t			dispatchX;
	uint32_t			dispatchY;
	uint32_t			dispatchZ;

	Image*				resourceImage;		// Optional
	GpuBuffer*			outputBuffer;		// Optional

	const void*			parmBufferData;		// Optional
	uint32_t			parmBufferSize;		// Optional

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
	std::string				m_programName;

	uint32_t				m_dispatchX;
	uint32_t				m_dispatchY;
	uint32_t				m_dispatchZ;

	Image*					m_resourceImage;
	ImageArray				m_imageArray;

	GpuBuffer*				m_outputBuffer;

	GpuBuffer				m_parmBuffer;
	bool					m_hasParmBuffer;

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
