#include "assetLib_GpuProgs.h"

void AssetLibGpuProgram::Create()
{
	programs[ RENDER_PROGRAM_BASIC ]			= CreateShaders( "shaders_bin/simpleVS.spv", "shaders_bin/simplePS.spv" );
	programs[ RENDER_PROGRAM_SHADOW ]			= CreateShaders( "shaders_bin/simpleVS.spv", "shaders_bin/shadowPS.spv" );
	programs[ RENDER_PROGRAM_DEPTH_PREPASS ]	= CreateShaders( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" );
	programs[ RENDER_PROGRAM_SHADOW ]			= CreateShaders( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" );
	programs[ RENDER_PROGRAM_TERRAIN ]			= CreateShaders( "shaders_bin/terrainVS.spv", "shaders_bin/terrainPS.spv" );
	programs[ RENDER_PROGRAM_TERRAIN_DEPTH ]	= CreateShaders( "shaders_bin/terrainVS.spv", "shaders_bin/depthPS.spv" );
	programs[ RENDER_PROGRAM_TERRAIN_SHADOW ]	= CreateShaders( "shaders_bin/terrainVS.spv", "shaders_bin/shadowPS.spv" );
	programs[ RENDER_PROGRAM_SKY ]				= CreateShaders( "shaders_bin/skyboxVS.spv", "shaders_bin/skyboxPS.spv" );
	programs[ RENDER_PROGRAM_LIT_OPAQUE ]		= CreateShaders( "shaders_bin/simpleVS.spv", "shaders_bin/litPS.spv" );
	programs[ RENDER_PROGRAM_LIT_TREE ]			= CreateShaders( "shaders_bin/treeVS.spv", "shaders_bin/litPS.spv" );
	programs[ RENDER_PROGRAM_LIT_TRANS ]		= CreateShaders( "shaders_bin/simpleVS.spv", "shaders_bin/emissivePS.spv" );
	programs[ RENDER_PROGRAM_POST_PROCESS ]		= CreateShaders( "shaders_bin/defaultVS.spv", "shaders_bin/postProcessPS.spv" );
	programs[ RENDER_PROGRAM_IMAGE_2D ]			= CreateShaders( "shaders_bin/defaultVS.spv", "shaders_bin/simplePS.spv" );
}