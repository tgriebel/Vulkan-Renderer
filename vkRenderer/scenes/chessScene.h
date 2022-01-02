#pragma once
#include "../common.h"
#include "../scene.h"
#include "../util.h"
#include "../io.h"
#include "../window.h"

extern Scene scene;

typedef AssetLib< texture_t > AssetLibImages;
typedef AssetLib< Material > AssetLibMaterials;

extern AssetLib< GpuProgram >	gpuPrograms;
extern AssetLibImages			textureLib;
extern AssetLibMaterials		materialLib;
extern imguiControls_t			imguiControls;
extern Window					window;

void AssetLib< GpuProgram >::Create()
{
	Add( "Basic", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/simplePS.spv" ) );
	Add( "Shadow", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/shadowPS.spv" ) );
	Add( "Prepass", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" ) );
	Add( "Terrain", LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/terrainPS.spv" ) );
	Add( "TerrainDepth", LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/depthPS.spv" ) );
	Add( "TerrainShadow", LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/shadowPS.spv" ) );
	Add( "Sky", LoadProgram( "shaders_bin/skyboxVS.spv", "shaders_bin/skyboxPS.spv" ) );
	Add( "LitDepth", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" ) );
	Add( "LitOpaque", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/litPS.spv" ) );
	Add( "LitTree", LoadProgram( "shaders_bin/treeVS.spv", "shaders_bin/litPS.spv" ) ); // TODO: vert motion
	Add( "LitTrans", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/emissivePS.spv" ) );
	Add( "PostProcess", LoadProgram( "shaders_bin/defaultVS.spv", "shaders_bin/postProcessPS.spv" ) );
	Add( "Image2D", LoadProgram( "shaders_bin/defaultVS.spv", "shaders_bin/simplePS.spv" ) );
}

void AssetLib< texture_t >::Create()
{
	const std::vector<std::string> texturePaths = { "heightmap.png", "grass.jpg", "checker.png", "skybox.jpg", "sapeli.jpg",
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

void AssetLib< Material >::Create()
{
	{
		Material material;
		material.shaders[ DRAWPASS_SHADOW ] = gpuPrograms.RetrieveHdl( "TerrainShadow" );
		material.shaders[ DRAWPASS_DEPTH ] = gpuPrograms.RetrieveHdl( "TerrainDepth" );
		material.shaders[ DRAWPASS_TERRAIN ] = gpuPrograms.RetrieveHdl( "Terrain" );
		material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.RetrieveHdl( "TerrainDepth" );
		material.textures[ 0 ] = textureLib.RetrieveHdl( "heightmap.png" );
		material.textures[ 1 ] = textureLib.RetrieveHdl( "grass.jpg" );
		material.textures[ 2 ] = textureLib.RetrieveHdl( "desert.jpg" );
		Add( "TERRAIN", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_SKYBOX ] = gpuPrograms.RetrieveHdl( "Sky" );
		material.textures[ 0 ] = textureLib.RetrieveHdl( "skybox.jpg" );
		Add( "SKY", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_SHADOW ] = gpuPrograms.RetrieveHdl( "Shadow" );
		material.shaders[ DRAWPASS_DEPTH ] = gpuPrograms.RetrieveHdl( "LitDepth" );
		material.shaders[ DRAWPASS_OPAQUE ] = gpuPrograms.RetrieveHdl( "LitOpaque" );
		material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.RetrieveHdl( "LitDepth" );
		material.textures[ 0 ] = textureLib.RetrieveHdl( "sapeli.jpg" );
		Add( "WHITE_WOOD", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_TRANS ] = gpuPrograms.RetrieveHdl( "LitTrans" );
		material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.RetrieveHdl( "TerrainDepth" );
		Add( "WATER", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_POST_2D ] = gpuPrograms.RetrieveHdl( "PostProcess" );
		material.textures[ 0 ] = 0;
		material.textures[ 1 ] = 1;
		Add( "TONEMAP", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_POST_2D ] = gpuPrograms.RetrieveHdl( "Image2D" );
		material.textures[ 0 ] = 0;
		Add( "IMAGE2D", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_SHADOW ] = gpuPrograms.RetrieveHdl( "Shadow" );
		material.shaders[ DRAWPASS_DEPTH ] = gpuPrograms.RetrieveHdl( "LitDepth" );
		material.shaders[ DRAWPASS_OPAQUE ] = gpuPrograms.RetrieveHdl( "LitOpaque" );
		material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.RetrieveHdl( "LitDepth" );
		material.textures[ 0 ] = textureLib.RetrieveHdl( "palm_tree_diffuse.jpg" );
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
		LoadModel( "pawn.obj", "pawn" );
		modelSource_t* model = modelLib.Find( "pawn"  );
		model->materialId = materialLib.FindId( "WHITE_WOOD" );
	}
	{
		LoadModel( "chess_board.obj", "board" );
		modelSource_t* model = modelLib.Find( "board" );
		model->materialId = materialLib.FindId( "WHITE_WOOD" );
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


void MakeScene()
{
	const int piecesNum = 16;

	int entId = 0;
	scene.entities.resize( 5 + piecesNum );
	scene.CreateEntity( modelLib.FindId( "_skybox" ), scene.entities[ entId ] );
	++entId;
	scene.CreateEntity( modelLib.FindId( "board" ), scene.entities[ entId ] );
	++entId;
	scene.CreateEntity( modelLib.FindId( "_water" ), scene.entities[ entId ] );
	++entId;
	scene.CreateEntity( modelLib.FindId( "_postProcessQuad" ), scene.entities[ entId ] );
	++entId;
	scene.CreateEntity( modelLib.FindId( "_quadTexDebug" ), scene.entities[ entId ] );
	++entId;

	for ( int i = 0; i < piecesNum; ++i )
	{
		const int palmModelId = modelLib.FindId( "pawn" );
		scene.CreateEntity( palmModelId, scene.entities[ entId ] );
		++entId;
	}

	// Board
	scene.entities[ 1 ].matrix = glm::identity<glm::mat4>();

	// Pieces
	const float scale = 1.0f;// 0.0025f;
	{
		for ( int i = 5; i < ( 5 + piecesNum ); ++i )
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