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
	Add( "Basic", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/simplePS.spv" ) );
	Add( "Shadow",LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/shadowPS.spv" ) );
	Add( "Prepass", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" ) );
	Add( "Terrain", LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/terrainPS.spv" ) );
	Add( "TerrainDepth", LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/depthPS.spv" ) );
	Add( "TerrainShadow", LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/shadowPS.spv" ) );
	Add( "Sky", LoadProgram( "shaders_bin/skyboxVS.spv", "shaders_bin/skyboxPS.spv" ) );
	Add( "LitOpaque", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/litPS.spv" ) );
	Add( "LitTree", LoadProgram( "shaders_bin/treeVS.spv", "shaders_bin/litPS.spv" ) ); // TODO: vert motion
	Add( "LitTrans",  LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/emissivePS.spv" ) );
	Add( "PostProcess", LoadProgram( "shaders_bin/defaultVS.spv", "shaders_bin/postProcessPS.spv" ) );
	Add( "Image2D",	LoadProgram( "shaders_bin/defaultVS.spv", "shaders_bin/simplePS.spv" ) );
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
		material.shaders[ DRAWPASS_SHADOW ] = gpuPrograms.Find( "TerrainShadow" );
		material.shaders[ DRAWPASS_DEPTH ] = gpuPrograms.Find( "TerrainDepth" );
		material.shaders[ DRAWPASS_TERRAIN ] = gpuPrograms.Find( "Terrain" );
		material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.Find( "TerrainDepth" );
		material.texture0 = textureLib.FindId( "heightmap.png" );
		material.texture1 = textureLib.FindId( "grass.jpg" );
		material.texture2 = textureLib.FindId( "desert.jpg" );
		Add( "TERRAIN", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_SKYBOX ] = gpuPrograms.Find( "Sky" );
		material.texture0 = textureLib.FindId( "skybox.jpg" );
		Add( "SKY", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_SHADOW ] = gpuPrograms.Find( "Shadow" );
		material.shaders[ DRAWPASS_DEPTH ] = gpuPrograms.Find( "TerrainDepth" );
		material.shaders[ DRAWPASS_OPAQUE ] = gpuPrograms.Find( "LitOpaque" );
		material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.Find( "TerrainDepth" );
		material.texture0 = textureLib.FindId( "viking_room.png" );
		Add( "VIKING", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_TRANS ] = gpuPrograms.Find( "LitTrans" );
		material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.Find( "TerrainDepth" );
		material.texture0 = 0;
		Add( "WATER", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_POST_2D ] = gpuPrograms.Find( "PostProcess" );
		material.texture0 = 0;
		material.texture1 = 1;
		Add( "TONEMAP", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_POST_2D ] = gpuPrograms.Find( "Image2D" );
		material.texture0 = 2;
		Add( "IMAGE2D", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_SHADOW ] = gpuPrograms.Find( "Shadow" );
		material.shaders[ DRAWPASS_DEPTH ] = gpuPrograms.Find( "TerrainDepth" );
		material.shaders[ DRAWPASS_OPAQUE ] = gpuPrograms.Find( "LitOpaque" );
		material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.Find( "TerrainDepth" );
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

void UpdateScene( const input_t& input, const float dt )
{
	// FIXME: race conditions
	// Need to do a ping-pong update

	if ( input.keys[ 'D' ] ) {
		scene.camera.MoveForward( dt * 0.01f );
	}
	if ( input.keys[ 'A' ] ) {
		scene.camera.MoveForward( dt * -0.01f );
	}
	if ( input.keys[ 'W' ] ) {
		scene.camera.MoveVertical( dt * -0.01f );
	}
	if ( input.keys[ 'S' ] ) {
		scene.camera.MoveVertical( dt * 0.01f );
	}
	if ( input.keys[ '+' ] ) {
		scene.camera.fov += dt;
	}
	if ( input.keys[ '-' ] ) {
		scene.camera.fov -= dt;
	}

	const mouse_t& mouse = input.mouse;
	if ( mouse.centered )
	{
		const float maxSpeed = dt * mouse.speed;
		const float yawDelta = maxSpeed * mouse.dx;
		const float pitchDelta = -maxSpeed * mouse.dy;
		scene.camera.SetYaw( yawDelta );
		scene.camera.SetPitch( pitchDelta );
	}

	// Skybox
	glm::mat4 skyBoxMatrix = glm::identity<glm::mat4>();
	skyBoxMatrix = glm::identity<glm::mat4>();
	skyBoxMatrix[ 3 ][ 0 ] = scene.camera.GetOrigin()[ 0 ];
	skyBoxMatrix[ 3 ][ 1 ] = scene.camera.GetOrigin()[ 1 ];
	skyBoxMatrix[ 3 ][ 2 ] = scene.camera.GetOrigin()[ 2 ] - 0.5f;
	scene.entities[ 0 ].matrix = skyBoxMatrix;
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