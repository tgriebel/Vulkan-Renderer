#pragma once

#include "common.h"
#include "../render_core/drawpass.h"

class ShaderBindParms;

class PostEffect
{
private:
	DrawPass*		pass;
	std::string		dbgName;

public:
	void		Init( const char* name, FrameBuffer& fb, const bool clear, const bool present );
	void		Shutdown();

	void		Execute( ShaderBindParms* parms );
};