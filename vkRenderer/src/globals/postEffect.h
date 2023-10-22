#pragma once

#include "common.h"
#include "../draw_passes/drawpass.h"
#include "../render_core/RenderTask.h"

class ShaderBindParms;
class RenderContext;
class ResourceContext;

struct imageProcessCreateInfo_t
{
	const char*			name;
	hdl_t				progHdl;
	FrameBuffer*		fb;
	RenderContext*		context;
	ResourceContext*	resources;
	bool				clear;
	bool				present;
	bool				resolve;
};


class ImageProcess : public GpuTask
{
private:
	Asset<GpuProgram>*		m_progAsset;
	std::string				m_dbgName;
	renderPassTransition_t	m_transitionState;
	vec4f					m_clearColor;
	float					m_clearDepth;
	uint32_t				m_clearStencil;
	ResourceContext*		m_resources;
	RenderContext*			m_context;

public:
	DrawPass*			pass;
	GpuBuffer			buffer;

	ImageProcess() {}

	ImageProcess( const imageProcessCreateInfo_t& info )
	{
		Init( info );
	}

	void				Init( const imageProcessCreateInfo_t& info );
	void				Resize();
	void				Shutdown();

	void				FrameBegin();
	void				FrameEnd();

	void				SetSourceImage( const uint32_t slot, Image* image );

	void				Execute( CommandContext& cmdContext );
};