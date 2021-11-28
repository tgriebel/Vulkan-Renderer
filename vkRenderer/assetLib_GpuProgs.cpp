#include "common.h"
#include "io.h"
#include "assetLib.h"

GpuProgram LoadProgram( const std::string& vsFile, const std::string& psFile );

static GpuProgram LoadProgram( const std::string& vsFile, const std::string& psFile )
{
	GpuProgram program;
	program.vsName = vsFile;
	program.psName = psFile;
	program.vsBlob = ReadFile( vsFile );
	program.psBlob = ReadFile( psFile );
	return program;
}

void AssetLib< GpuProgram >::Create()
{
	assets.resize( RENDER_PROGRAM_COUNT );
	names.resize( RENDER_PROGRAM_COUNT );
	{
		names[ RENDER_PROGRAM_BASIC ] = "Basic";
		assets[ RENDER_PROGRAM_BASIC ] = LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/simplePS.spv" );
	}
	{
		names[ RENDER_PROGRAM_SHADOW ] = "Shadow";
		assets[ RENDER_PROGRAM_SHADOW ] = LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/shadowPS.spv" );
	}
	{
		names[ RENDER_PROGRAM_DEPTH_PREPASS ] = "Prepass";
		assets[ RENDER_PROGRAM_DEPTH_PREPASS ]	= LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" );
	}
	{
		names[ RENDER_PROGRAM_TERRAIN ] = "Terrain";
		assets[ RENDER_PROGRAM_TERRAIN ] = LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/terrainPS.spv" );
	}
	{
		names[ RENDER_PROGRAM_TERRAIN_DEPTH ] = "TerrainDepth";
		assets[ RENDER_PROGRAM_TERRAIN_DEPTH ]	= LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/depthPS.spv" );
	}
	{
		names[ RENDER_PROGRAM_TERRAIN_SHADOW ] = "TerrainShadow";
		assets[ RENDER_PROGRAM_TERRAIN_SHADOW ]	= LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/shadowPS.spv" );
	}
	{
		names[ RENDER_PROGRAM_SKY ] = "Sky";
		assets[ RENDER_PROGRAM_SKY ] = LoadProgram( "shaders_bin/skyboxVS.spv", "shaders_bin/skyboxPS.spv" );
	}
	{
		names[ RENDER_PROGRAM_LIT_OPAQUE ] = "LitOpaque";
		assets[ RENDER_PROGRAM_LIT_OPAQUE ] = LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/litPS.spv" );
	}
	{
		names[ RENDER_PROGRAM_LIT_TREE ] = "LitTree";
		assets[ RENDER_PROGRAM_LIT_TREE ] = LoadProgram( "shaders_bin/treeVS.spv", "shaders_bin/litPS.spv" ); // TODO: vert motion
	}
	{
		names[ RENDER_PROGRAM_LIT_TRANS ] = "LitTrans";
		assets[ RENDER_PROGRAM_LIT_TRANS ] = LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/emissivePS.spv" );
	}
	{
		names[ RENDER_PROGRAM_POST_PROCESS ] = "PostProcess";
		assets[ RENDER_PROGRAM_POST_PROCESS ] = LoadProgram( "shaders_bin/defaultVS.spv", "shaders_bin/postProcessPS.spv" );
	}
	{
		names[ RENDER_PROGRAM_IMAGE_2D ] = "Image2D";
		assets[ RENDER_PROGRAM_IMAGE_2D ] = LoadProgram( "shaders_bin/defaultVS.spv", "shaders_bin/simplePS.spv" );
	}
}