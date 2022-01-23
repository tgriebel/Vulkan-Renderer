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
	const std::vector<std::string> texturePaths = { "heightmap.png", "grass.jpg", "checker.png", "sapeli.jpg",
													"checker.png", "desert.jpg", "palm_tree_diffuse.jpg", "checker.png", "checker.png", };

	for ( const std::string& texturePath : texturePaths )
	{
		texture_t texture;
		if ( LoadTextureImage( ( TexturePath + texturePath ).c_str(), texture ) ) {
			texture.uploaded = false;
			texture.info.mipLevels = static_cast<uint32_t>( std::floor( std::log2( std::max( texture.info.width, texture.info.height ) ) ) ) + 1;
			textureLib.Add( texturePath.c_str(), texture );
		}
	}
	texture_t cubeMap;
	const std::string cubeMapPath = ( TexturePath + "chapel" );
	if ( LoadTextureCubeMapImage( cubeMapPath.c_str(), "jpg", cubeMap ) ) {
		cubeMap.uploaded = false;
		cubeMap.info.mipLevels = 1;
		textureLib.Add( cubeMapPath.c_str(), cubeMap );
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
		material.textures[ 0 ] = textureLib.RetrieveHdl( "skybox2.jpg" );
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
		LoadModel( "axis.obj", "axis" );
		LoadModel( "cube.obj", "cube" );
		LoadModel( "pawn.obj", "pawn" );
		LoadModel( "chess_board.obj", "chess_board" );
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

	{
		entity_t ent;
		scene.CreateEntity( modelLib.FindId( "_skybox" ), ent );
		scene.entities.Add( "_skybox", ent );
	}

	{
		entity_t ent;
		scene.CreateEntity( modelLib.FindId( "chess_board" ), ent );
		scene.entities.Add( "chess_board", ent );
	}

	{
		entity_t ent;
		scene.CreateEntity( modelLib.FindId( "pawn" ), ent );
		scene.entities.Add( "white_pawn_0", ent );
	}

	{
		entity_t ent;
		scene.CreateEntity( modelLib.FindId( "cube" ), ent );
		ent.flags = static_cast< renderFlags_t >( WIREFRAME | SKIP_OPAQUE );
		scene.entities.Add( "white_pawn_0_cube", ent );
	}

	{
		entity_t ent;
		scene.CreateEntity( modelLib.FindId( "_postProcessQuad" ), ent );
		scene.entities.Add( "_postProcessQuad", ent );
	}

	{
		entity_t ent;
		scene.CreateEntity( modelLib.FindId( "_quadTexDebug" ), ent );
		scene.entities.Add( "_quadTexDebug", ent );
	}

	//// Pieces
	//const float scale = 1.0f;// 0.0025f;
	//{
	//	for ( int i = entId; i < ( entId + piecesNum ); ++i )
	//	{
	//		glm::vec2 randPoint;
	//		RandPlanePoint( randPoint );
	//		randPoint = 10.0f * ( randPoint - 0.5f );
	//		scene.entities[ i ].matrix = scale * glm::identity<glm::mat4>();
	//		scene.entities[ i ].matrix[ 3 ][ 0 ] = randPoint.x;
	//		scene.entities[ i ].matrix[ 3 ][ 1 ] = randPoint.y;
	//		scene.entities[ i ].matrix[ 3 ][ 2 ] = 0.0f;
	//		scene.entities[ i ].matrix[ 3 ][ 3 ] = 1.0f;
	//	}
	//}
}