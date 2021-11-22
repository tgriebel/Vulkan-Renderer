#pragma once

#include "common.h"

// GPU programs are all hardcoded because a general compiler is a fair amount of work

enum RenderProgramName
{
	RENDER_PROGRAM_BASIC,
	RENDER_PROGRAM_SHADOW,
	RENDER_PROGRAM_DEPTH_PREPASS,
	RENDER_PROGRAM_TERRAIN,
	RENDER_PROGRAM_TERRAIN_DEPTH,
	RENDER_PROGRAM_TERRAIN_SHADOW,
	RENDER_PROGRAM_SKY,
	RENDER_PROGRAM_LIT_OPAQUE,
	RENDER_PROGRAM_LIT_TREE,
	RENDER_PROGRAM_LIT_TRANS,
	RENDER_PROGRAM_POST_PROCESS,
	RENDER_PROGRAM_IMAGE_2D,
	RENDER_PROGRAM_COUNT,
};

RenderProgram CreateShaders( const std::string& vsFile, const std::string& psFile );

class AssetLibGpuProgram {
public:
	RenderProgram programs[ RENDER_PROGRAM_COUNT ];

	void Create();
};