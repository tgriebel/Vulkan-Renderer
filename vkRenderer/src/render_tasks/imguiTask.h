#pragma once

#include <queue>
#include "RenderTask.h"
#include "../render_resources/imageArray.h"

class CommandList;
class GfxCmdList;
class RenderView;
class RenderContext;
class ResourceContext;
class Image;
class ShaderBindParms;
struct ComputeState;

static const uint32_t ImageStatHistogramBins = 256;

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

	// Image Viewer.
	GpuBuffer				m_imageStatBuffer;
	GpuBuffer				m_imageStatParmsBuffer;
	ShaderBindParms*		m_imageStatParms;
	ImageArray				m_imageStatImages;
	const Image*			m_imageStatImage;
	bool					m_imageStatDispatched;
	uint32_t				m_imageStatHistogram[ ImageStatHistogramBins ];
	vec4f					m_pickLocationSample;

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

	void				Execute( CommandList& context ) override;
};