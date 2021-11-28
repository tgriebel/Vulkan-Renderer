#include "common.h"
#include "scene.h"
#include "util.h"
#include "io.h"

extern Scene scene;

typedef AssetLib< texture_t > AssetLibImages;
extern AssetLib< GpuProgram > gpuPrograms;
extern AssetLibImages textureLib;

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
		assets[ RENDER_PROGRAM_DEPTH_PREPASS ] = LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" );
	}
	{
		names[ RENDER_PROGRAM_TERRAIN ] = "Terrain";
		assets[ RENDER_PROGRAM_TERRAIN ] = LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/terrainPS.spv" );
	}
	{
		names[ RENDER_PROGRAM_TERRAIN_DEPTH ] = "TerrainDepth";
		assets[ RENDER_PROGRAM_TERRAIN_DEPTH ] = LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/depthPS.spv" );
	}
	{
		names[ RENDER_PROGRAM_TERRAIN_SHADOW ] = "TerrainShadow";
		assets[ RENDER_PROGRAM_TERRAIN_SHADOW ] = LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/shadowPS.spv" );
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

void AssetLib< texture_t >::Create()
{
	const std::vector<std::string> texturePaths = { "heightmap.png", "grass.jpg", "checker.png", "skybox.jpg", "viking_room.png",
													"checker.png", "desert.jpg", "palm_tree_diffuse.jpg", "checker.png", "checker.png", };

	for ( const std::string& texturePath : texturePaths )
	{
		texture_t texture;
		if ( LoadTextureImage( ( TexturePath + texturePath ).c_str(), texture ) ) {
			texture.uploaded = false;
			texture.mipLevels = static_cast<uint32_t>( std::floor( std::log2( std::max( texture.width, texture.height ) ) ) ) + 1;
			textureLib.Add( texturePath.c_str(), texture );
		}
	}
}

void AssetLib< material_t >::Create()
{
	{
		material_t material;
		material.shaders[ DRAWPASS_SHADOW ] = gpuPrograms.Find( RENDER_PROGRAM_TERRAIN_SHADOW );
		material.shaders[ DRAWPASS_DEPTH ] = gpuPrograms.Find( RENDER_PROGRAM_TERRAIN_DEPTH );
		material.shaders[ DRAWPASS_TERRAIN ] = gpuPrograms.Find( RENDER_PROGRAM_TERRAIN );
		material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.Find( RENDER_PROGRAM_TERRAIN_DEPTH );
		material.texture0 = textureLib.FindId( "heightmap.png" );
		material.texture1 = textureLib.FindId( "grass.jpg" );
		material.texture2 = textureLib.FindId( "desert.jpg" );
		Add( "TERRAIN", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_SKYBOX ] = gpuPrograms.Find( RENDER_PROGRAM_SKY );
		material.texture0 = textureLib.FindId( "skybox.jpg" );
		Add( "SKY", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_SHADOW ] = gpuPrograms.Find( RENDER_PROGRAM_SHADOW );
		material.shaders[ DRAWPASS_DEPTH ] = gpuPrograms.Find( RENDER_PROGRAM_DEPTH_PREPASS );
		material.shaders[ DRAWPASS_OPAQUE ] = gpuPrograms.Find( RENDER_PROGRAM_LIT_OPAQUE );
		material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.Find( RENDER_PROGRAM_DEPTH_PREPASS );
		material.texture0 = textureLib.FindId( "viking_room.png" );
		Add( "VIKING", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_TRANS ] = gpuPrograms.Find( RENDER_PROGRAM_LIT_TRANS );
		material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.Find( RENDER_PROGRAM_DEPTH_PREPASS );
		material.texture0 = 0;
		Add( "WATER", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_POST_2D ] = gpuPrograms.Find( RENDER_PROGRAM_POST_PROCESS );
		material.texture0 = 0;
		material.texture1 = 1;
		Add( "TONEMAP", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_POST_2D ] = gpuPrograms.Find( RENDER_PROGRAM_IMAGE_2D );
		material.texture0 = 2;
		Add( "IMAGE2D", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_SHADOW ] = gpuPrograms.Find( RENDER_PROGRAM_SHADOW );
		material.shaders[ DRAWPASS_DEPTH ] = gpuPrograms.Find( RENDER_PROGRAM_DEPTH_PREPASS );
		material.shaders[ DRAWPASS_OPAQUE ] = gpuPrograms.Find( RENDER_PROGRAM_LIT_OPAQUE );
		material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.Find( RENDER_PROGRAM_DEPTH_PREPASS );
		material.texture0 = textureLib.FindId( "palm_tree_diffuse.jpg" );
		Add( "PALM", material );
	}
}

void AssetLib< modelSource_t >::Create()
{
	{
		modelSource_t model;
		CreateSkyBoxSurf( model );
		modelLib.Add( "_skybox", model );
	}
	{
		modelSource_t model;
		CreateTerrainSurface( model );
		modelLib.Add( "_terrain", model );
	}
	{
		modelSource_t model;
		CreateStaticModel( "sphere.obj", "PALM", model );
		modelLib.Add( "palmTree", model );
	}
	{
		modelSource_t model;
		CreateWaterSurface( model );
		modelLib.Add( "_water", model );
	}
	{
		modelSource_t model;
		CreateQuadSurface2D( "TONEMAP", model, glm::vec2( 1.0f, 1.0f ), glm::vec2( 2.0f ) );
		modelLib.Add( "_postProcessQuad", model );
	}
	{
		modelSource_t model;
		CreateQuadSurface2D( "IMAGE2D", model, glm::vec2( 1.0f, 1.0f ), glm::vec2( 1.0f * ( 9.0 / 16.0f ), 1.0f ) );
		modelLib.Add( "_quadTexDebug", model );
	}
}

void MakeBeachScene()
{
	const int palmTreesNum = 300;

	int entId = 0;
	scene.entities.resize( 5 + palmTreesNum );
	scene.CreateEntity( 0, scene.entities[ entId ] );
	++entId;
	scene.CreateEntity( 1, scene.entities[ entId ] );
	++entId;
	scene.CreateEntity( 3, scene.entities[ entId ] );
	++entId;
	scene.CreateEntity( 4, scene.entities[ entId ] );
	++entId;
	scene.CreateEntity( 5, scene.entities[ entId ] );
	++entId;

	for ( int i = 0; i < palmTreesNum; ++i )
	{
		const int palmModelId = modelLib.FindId( "palmTree" );
		scene.CreateEntity( palmModelId, scene.entities[ entId ] );
		++entId;
	}

	// Terrain
	scene.entities[ 1 ].matrix = glm::identity<glm::mat4>();

	// Model
	const float scale = 0.0025f;
	{
		for ( int i = 5; i < ( 5 + palmTreesNum ); ++i )
		{
			glm::vec2 randPoint;
			RandPlanePoint( randPoint );
			randPoint = 10.0f * ( randPoint - 0.5f );
			scene.entities[ i ].matrix = scale * glm::identity<glm::mat4>();
			scene.entities[ i ].matrix[ 3 ][ 0 ] = randPoint.x;
			scene.entities[ i ].matrix[ 3 ][ 1 ] = randPoint.y;
			scene.entities[ i ].matrix[ 3 ][ 2 ] = 0.0f;
			scene.entities[ i ].matrix[ 3 ][ 3 ] = 1.0f;
		}
	}
}