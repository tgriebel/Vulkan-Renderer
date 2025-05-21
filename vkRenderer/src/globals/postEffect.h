#pragma once

#include "common.h"
#include "../draw_passes/drawpass.h"
#include "../render_core/RenderTask.h"

class ShaderBindParms;
class RenderContext;
class ResourceContext;

class ImageProcess;
// A callback is used instead of inheritance for now since most image processes will be very similar
typedef void imageProcessFrameBeginCallback_t( ImageProcess* imageProcess );

struct imageProcessCreateInfo_t
{
	const char*			name;
	hdl_t				progHdl;
	FrameBuffer*		fb;
	RenderContext*		context;
	ResourceContext*	resources;
	uint32_t			inputImages;
	bool				clear;
	bool				present;
	bool				resolve;

	imageProcessFrameBeginCallback_t * callback;
};

class ImageProcess : public GpuTask
{
private:
	static const uint32_t	MaxBufferSizeInBytes = 256;
	static const uint32_t	ReservedConstantSizeInBytes = 16;
	static const uint32_t	MaxConstantBlockSizeInBytes = 240;

	Asset<GpuProgram>*		m_progAsset;
	std::string				m_dbgName;
	renderPassTransition_t	m_transitionState;
	vec4f					m_clearColor;
	float					m_clearDepth;
	uint32_t				m_clearStencil;
	ResourceContext*		m_resources;
	RenderContext*			m_context;
	DrawPass*				m_pass;
	GpuBuffer				m_buffer;

	imageProcessFrameBeginCallback_t* m_callback = nullptr;

public:
	ImageProcess() {}

	~ImageProcess()
	{
		Shutdown();
	}

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
	void				SetConstants( const void* dataBlock, const uint32_t sizeInBytes );

	void				Execute( CommandContext& cmdContext );
};