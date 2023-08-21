#pragma once

#include "common.h"
#include "../render_core/drawpass.h"

class ShaderBindParms;

class ImageProcess
{
private:
	Asset<GpuProgram>*	progAsset;
	GpuBuffer			buffer;
	std::string			dbgName;

public:
	DrawPass*			pass;

	void				Init( const char* name, const hdl_t progHdl, FrameBuffer& fb, const bool clear, const bool present );
	void				Shutdown();

	void				Execute( CommandContext& cmdContext );
};