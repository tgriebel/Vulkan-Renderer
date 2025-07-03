#pragma once

#include "common.h"
#include "../draw_passes/drawpass.h"
#include "../render_tasks/RenderTask.h"
#include "imageProcess.h"

class ShaderBindParms;
class RenderContext;
class ResourceContext;

struct imageCubeProcessCreateInfo_t
{
	const char*			name;
	hdl_t				progHdl;
	Image*				image;
	RenderContext*		context;
	ResourceContext*	resources;
	uint32_t			inputImages;
	uint32_t			inputCubeImages;
	bool				clear;
};

class ImageCubeProcess : public GpuTask
{
private:
	Asset<GpuProgram>*		m_progAsset;
	std::string				m_dbgName;
	ImageView				m_imgViews[ 6 ];

public:
	ImageCubeProcess() {}

	~ImageCubeProcess()
	{
		Shutdown();
	}

	ImageCubeProcess( const imageCubeProcessCreateInfo_t& info )
	{
		Init( info );
	}

	void				Init( const imageCubeProcessCreateInfo_t& info );
	void				Resize();
	void				Shutdown();

	void				FrameBegin();
	void				FrameEnd();
	std::string			AsString() const;

	void				SetSourceImage( const uint32_t slot, Image* image );
	void				SetSourceCubeImage( const uint32_t slot, Image* image );

	void				Execute( CommandContext& cmdContext );
};