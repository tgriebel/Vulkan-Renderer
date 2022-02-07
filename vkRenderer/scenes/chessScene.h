#pragma once
#include "../common.h"
#include "../scene.h"
#include "../render_util.h"
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

static std::string pieceNames[ 8 ] = {
	"white_pawn_0",
	"white_pawn_1",
	"white_pawn_2",
	"white_pawn_3",
	"white_pawn_4",
	"white_pawn_5",
	"white_pawn_6",
	"white_pawn_7",
};

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
	Add( "Debug", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" ) );
	Add( "Debug_Solid", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/litPS.spv" ) );
	Add( "PostProcess", LoadProgram( "shaders_bin/defaultVS.spv", "shaders_bin/postProcessPS.spv" ) );
	Add( "Image2D", LoadProgram( "shaders_bin/defaultVS.spv", "shaders_bin/simplePS.spv" ) );
}

void AssetLib< texture_t >::Create()
{
	const std::vector<std::string> texturePaths = { "checker.png", "sapeli.jpg",
													"chapel_right.jpg", "chapel_left.jpg", "chapel_top.jpg", "chapel_bottom.jpg", "chapel_front.jpg", "chapel_back.jpg" };

	for ( const std::string& texturePath : texturePaths )
	{
		texture_t texture;
		if ( LoadTextureImage( ( TexturePath + texturePath ).c_str(), texture ) ) {
			textureLib.Add( texturePath.c_str(), texture );
		}
	}
	texture_t cubeMap;
	const std::string cubeMapPath = ( TexturePath + "chapel_low" );
	if ( LoadTextureCubeMapImage( cubeMapPath.c_str(), "jpg", cubeMap ) ) {
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
		//material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.RetrieveHdl( "TerrainDepth" );
		material.textures[ 0 ] = textureLib.RetrieveHdl( "heightmap.png" );
		material.textures[ 1 ] = textureLib.RetrieveHdl( "grass.jpg" );
		material.textures[ 2 ] = textureLib.RetrieveHdl( "desert.jpg" );
		Add( "TERRAIN", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_SKYBOX ] = gpuPrograms.RetrieveHdl( "Sky" );
		material.textures[ 0 ] = textureLib.RetrieveHdl( "chapel_right.jpg" );
		material.textures[ 1 ] = textureLib.RetrieveHdl( "chapel_left.jpg" );
		material.textures[ 2 ] = textureLib.RetrieveHdl( "chapel_top.jpg" );
		material.textures[ 3 ] = textureLib.RetrieveHdl( "chapel_bottom.jpg" );
		material.textures[ 4 ] = textureLib.RetrieveHdl( "chapel_front.jpg" );
		material.textures[ 5 ] = textureLib.RetrieveHdl( "chapel_back.jpg" );
		Add( "SKY", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_SHADOW ] = gpuPrograms.RetrieveHdl( "Shadow" );
		material.shaders[ DRAWPASS_DEPTH ] = gpuPrograms.RetrieveHdl( "LitDepth" );
		material.shaders[ DRAWPASS_OPAQUE ] = gpuPrograms.RetrieveHdl( "LitOpaque" );
		//material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.RetrieveHdl( "LitDepth" );
		material.textures[ 0 ] = textureLib.RetrieveHdl( "sapeli.jpg" );
		Add( "WHITE_WOOD", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_TRANS ] = gpuPrograms.RetrieveHdl( "LitTrans" );
		//material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.RetrieveHdl( "TerrainDepth" );
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
		material.textures[ 1 ] = 1;
		Add( "IMAGE2D", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_SHADOW ] = gpuPrograms.RetrieveHdl( "Shadow" );
		material.shaders[ DRAWPASS_DEPTH ] = gpuPrograms.RetrieveHdl( "LitDepth" );
		material.shaders[ DRAWPASS_OPAQUE ] = gpuPrograms.RetrieveHdl( "LitOpaque" );
		//material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.RetrieveHdl( "LitDepth" );
		material.textures[ 0 ] = textureLib.RetrieveHdl( "palm_tree_diffuse.jpg" );
		Add( "PALM", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_DEBUG_WIREFRAME ] = gpuPrograms.RetrieveHdl( "Debug" );
		Add( "DEBUG_WIRE", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_DEBUG_SOLID ] = gpuPrograms.RetrieveHdl( "Debug_Solid" );
		Add( "DEBUG_SOLID", material );
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
		LoadModel( "diamond.obj", "diamond" );
		LoadModel( "sphere.obj", "sphere" );
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
		CreateQuadSurface2D( "TONEMAP", model, vec2f( 1.0f, 1.0f ), vec2f( 2.0f ) );
		modelLib.Add( "_postProcessQuad", model );
	}
	{
		modelSource_t model;
		CreateQuadSurface2D( "IMAGE2D", model, vec2f( 1.0f, 1.0f ), vec2f( 1.0f * ( 9.0 / 16.0f ), 1.0f ) );
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

	vec3f whiteCorner = vec3f( -7.0f, -7.0f, 0.0f );

	for ( int i = 0; i < 8; ++i )
	{
		entity_t ent;
		scene.CreateEntity( modelLib.FindId( "pawn" ), ent );
		ent.SetOrigin( whiteCorner + vec3f( 2.0f * i , 2.0f, 0.0f ) );
		scene.entities.Add( pieceNames[ i ].c_str(), ent );
		{
			entity_t cubeEnt;
			scene.CreateEntity( modelLib.FindId( "cube" ), cubeEnt );
			cubeEnt.materialId = materialLib.FindId( "DEBUG_WIRE" );
			cubeEnt.flags = static_cast<renderFlags_t>( WIREFRAME | SKIP_OPAQUE );
			scene.entities.Add( ( pieceNames[ i ] + "_cube" ).c_str(), cubeEnt );
		}
	}

	{
		entity_t ent;
		scene.CreateEntity( modelLib.FindId( "sphere" ), ent );
		ent.SetOrigin( vec3f( 0.0f, 0.0f, 0.0f ) );
		scene.entities.Add( "sphere", ent );
	}

	for ( int i = 0; i < MaxLights; ++i )
	{
		entity_t ent;
		scene.CreateEntity( modelLib.FindId( "diamond" ), ent );
		ent.materialId = materialLib.FindId( "DEBUG_WIRE" );
		ent.flags = static_cast<renderFlags_t>( WIREFRAME | SKIP_OPAQUE );
		scene.entities.Add( ( "light" + std::string( { (char)( (int)'0' + i ) } ) + "_dbg" ).c_str(), ent );
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

	{
		scene.lights[ 0 ].lightPos = vec4f( 0.0f, 0.0f, 6.0f, 0.0f );
		scene.lights[ 0 ].intensity = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
		scene.lights[ 0 ].lightDir = vec4f( 0.0f, 0.0f, -1.0f, 0.0f );

		scene.lights[ 1 ].lightPos = vec4f( 0.0f, 0.0f, 5.0f, 0.0f );
		scene.lights[ 1 ].intensity = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
		scene.lights[ 1 ].lightDir = vec4f( 0.0f, 0.0f, 1.0f, 0.0f );

		scene.lights[ 2 ].lightPos = vec4f( 0.0f, 1.0f, 1.0f, 0.0f );
		scene.lights[ 2 ].intensity = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
		scene.lights[ 2 ].lightDir = vec4f( 0.0f, 0.0f, 1.0f, 0.0f );
	}
}

void UpdateSceneLocal()
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

	scene.lights[ 0 ].lightPos = vec4f( 5.0f * cos( time ), 5.0f * sin( time ), 8.0f, 0.0f );

	for ( int i = 0; i < 8; ++i )
	{
		entity_t* pawn = scene.entities.Find( pieceNames[ i ].c_str() );
		entity_t* debugBox = scene.entities.Find( ( pieceNames[ i ] + "_cube" ).c_str() );
		AABB bounds = pawn->GetBounds();
		vec3f size = bounds.GetSize();
		vec3f center = bounds.GetCenter();
		debugBox->SetOrigin( vec3f( center[ 0 ], center[ 1 ], center[ 2 ] ) );
		debugBox->SetScale( 0.5f * vec3f( size[ 0 ], size[ 1 ], size[ 2 ] ) );
	}
	scene.entities.Find( pieceNames[ 0 ].c_str() )->outline = true;

	for ( int i = 0; i < MaxLights; ++i )
	{
		entity_t* debugLight = scene.entities.Find( ( "light" + std::string( { (char)( (int)'0' + i ) } ) + "_dbg" ).c_str() );
		vec3f origin = vec3f( scene.lights[ i ].lightPos[ 0 ], scene.lights[ i ].lightPos[ 1 ], scene.lights[ i ].lightPos[ 2 ] );
		debugLight->SetOrigin( origin );
		debugLight->SetScale( vec3f( 0.25f, 0.25f, 0.25f ) );
	}
}