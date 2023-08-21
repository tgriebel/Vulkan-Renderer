#pragma once

#include "common.h"
#include "../render_core/drawpass.h"

class ShaderBindParms;
class RenderContext;

struct imageProcessCreateInfo_t
{
	const char*		name;
	hdl_t			progHdl;
	FrameBuffer*	fb;
	RenderContext*	context;
	bool			clear;
	bool			present;
};


class ImageProcess
{
private:
	Asset<GpuProgram>*	progAsset;
	std::string			dbgName;

public:
	DrawPass*			pass;
	GpuBuffer			buffer;

	void				Init( const imageProcessCreateInfo_t& info );
	void				Shutdown();

	void				Execute( CommandContext& cmdContext );
};