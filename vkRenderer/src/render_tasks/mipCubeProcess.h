#pragma once

#include "common.h"
#include "../draw_passes/drawpass.h"
#include "../render_tasks/RenderTask.h"
#include "imageProcess.h"

class ShaderBindParms;
class RenderContext;
class ResourceContext;
class MipImageTask;

struct mipCubeProcessCreateInfo_t
{
	const char*			name;
	Image*				img;
	Image*				sampleImage;
	hdl_t				progHdl;
	RenderContext*		context;
	ResourceContext*	resources;
	downSampleMode_t	mode;
	bool				clear;
};


class MipCubeProcess : public GpuTask
{
private:
	Asset<GpuProgram>*		m_progAsset;
	std::string				m_name;
	const Image*			m_image;
	ImageView				m_outputFaceImage[ 6 ];
	mat4x4f					m_viewMatrices[ 6 ];
	MipImageTask*			m_mipProcesses[ 6 ];

public:
	MipCubeProcess() {}

	~MipCubeProcess()
	{
		Shutdown();
	}

	MipCubeProcess( const mipCubeProcessCreateInfo_t& info )
	{
		Init( info );
	}

	void				Init( const mipCubeProcessCreateInfo_t& info );
	void				Resize();
	void				Shutdown();

	void				FrameBegin();
	void				FrameEnd();
	std::string			AsString() const;

	void				Execute( CommandContext& cmdContext );
};