#include "common.h"
#include "assetLib.h"

RenderProgram CreateShaders( const std::string& vsFile, const std::string& psFile );

void  AssetLib< RenderProgram >::Create()
{
	assets.resize( RENDER_PROGRAM_COUNT );
	assets[ RENDER_PROGRAM_BASIC ]			= CreateShaders( "shaders_bin/simpleVS.spv", "shaders_bin/simplePS.spv" );
	assets[ RENDER_PROGRAM_SHADOW ]			= CreateShaders( "shaders_bin/simpleVS.spv", "shaders_bin/shadowPS.spv" );
	assets[ RENDER_PROGRAM_DEPTH_PREPASS ]	= CreateShaders( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" );
	assets[ RENDER_PROGRAM_SHADOW ]			= CreateShaders( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" );
	assets[ RENDER_PROGRAM_TERRAIN ]		= CreateShaders( "shaders_bin/terrainVS.spv", "shaders_bin/terrainPS.spv" );
	assets[ RENDER_PROGRAM_TERRAIN_DEPTH ]	= CreateShaders( "shaders_bin/terrainVS.spv", "shaders_bin/depthPS.spv" );
	assets[ RENDER_PROGRAM_TERRAIN_SHADOW ]	= CreateShaders( "shaders_bin/terrainVS.spv", "shaders_bin/shadowPS.spv" );
	assets[ RENDER_PROGRAM_SKY ]			= CreateShaders( "shaders_bin/skyboxVS.spv", "shaders_bin/skyboxPS.spv" );
	assets[ RENDER_PROGRAM_LIT_OPAQUE ]		= CreateShaders( "shaders_bin/simpleVS.spv", "shaders_bin/litPS.spv" );
	assets[ RENDER_PROGRAM_LIT_TREE ]		= CreateShaders( "shaders_bin/treeVS.spv", "shaders_bin/litPS.spv" ); // TODO: vert motion
	assets[ RENDER_PROGRAM_LIT_TRANS ]		= CreateShaders( "shaders_bin/simpleVS.spv", "shaders_bin/emissivePS.spv" );
	assets[ RENDER_PROGRAM_POST_PROCESS ]	= CreateShaders( "shaders_bin/defaultVS.spv", "shaders_bin/postProcessPS.spv" );
	assets[ RENDER_PROGRAM_IMAGE_2D ]		= CreateShaders( "shaders_bin/defaultVS.spv", "shaders_bin/simplePS.spv" );
}