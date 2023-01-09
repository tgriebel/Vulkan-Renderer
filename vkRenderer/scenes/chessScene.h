#pragma once
#include "../common.h"
#include "../render_util.h"
#include "../io.h"
#include "../window.h"
#include "../input.h"
#include <chess.h>
#include <commands.h>
#include <syscore/timer.h>
#include <scene/entity.h>
#include <scene/scene.h>
#include <resource_types/gpuProgram.h>
#include <resource_types/model.h>
#include <io/io.h>

extern Scene scene;

extern imguiControls_t			imguiControls;
extern Window					window;

gameConfig_t					cfg;
Chess							chessEngine;
Entity*							selectedEntity = nullptr;
Entity*							movePieceId = nullptr;
std::vector< moveAction_t >		actions;
std::vector< uint32_t >			pieceEntities;
std::vector< uint32_t >			glowEntities;
std::vector< uint32_t >			boundEntities;

class PieceEntity : public Entity {
public:
	PieceEntity( const char file, const char rank ) : Entity(),
		file( file ),
		rank( rank ),
		handle( NoPiece ) { }
	char			file;	// TODO: remove
	char			rank;	// TODO: remove
	pieceHandle_t	handle;
};

class BoundEntity : public Entity {
public:
	BoundEntity() : Entity(),
		pieceId( ~0x0 ) { }
	uint32_t	pieceId;
};

Entity* GetTracedEntity( const Ray& ray )
{
	Entity* closestEnt = nullptr;
	float closestT = FLT_MAX;
	const uint32_t entityNum = static_cast<uint32_t>( scene.entities.size() );
	for ( uint32_t i = 0; i < entityNum; ++i )
	{
		Entity* ent = scene.entities[i];
		if ( !ent->HasFlag( ENT_FLAG_SELECTABLE ) ) {
			continue;
		}
		float t0, t1;
		if ( ent->GetBounds().Intersect( ray, t0, t1 ) ) {
			if ( t0 < closestT ) {
				closestT = t0;
				closestEnt = ent;
			}
		}
	}
	return closestEnt;
}

struct pieceMappingInfo_t {
	const char* name;
};

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

static std::string GetModelName( const pieceType_t type ) {
	switch ( type ) {
	case pieceType_t::PAWN:
		return "pawn";
		break;
	case pieceType_t::ROOK:
		return "rook";
		break;
	case pieceType_t::KNIGHT:
		return "knight";
		break;
	case pieceType_t::BISHOP:
		return "bishop";
		break;
	case pieceType_t::QUEEN:
		return "queen";
		break;
	case pieceType_t::KING:
		return "king";
		break;
	}
	return "<none>";
}

static std::string GetName( pieceInfo_t& pieceInfo ) {
	std::string name;
	if ( pieceInfo.team == teamCode_t::WHITE ) {
		name += "white";
	} else {
		name += "black";
	}
	name += "_" + GetModelName( pieceInfo.piece );
	name += "_" + std::to_string( pieceInfo.instance );
	return name;
}

static vec3f GetSquareCenterForLocation( const char file, const char rank ) {
	const vec3f whiteCorner = vec3f( -7.0f, -7.0f, 0.0f );
	const int x = GetFileNum( file );
	const int y = GetRankNum( rank );
	return whiteCorner + vec3f( 2.0f * x, 2.0f * ( 7 - y ), 0.0f );
}

void LoadShaders( AssetLibGpuProgram& progs )
{
	progs.Add( "Basic", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/simplePS.spv" ) );
	progs.Add( "Shadow", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/shadowPS.spv" ) );
	progs.Add( "Prepass", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" ) );
	progs.Add( "Terrain", LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/terrainPS.spv" ) );
	progs.Add( "TerrainDepth", LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/depthPS.spv" ) );
	progs.Add( "TerrainShadow", LoadProgram( "shaders_bin/terrainVS.spv", "shaders_bin/shadowPS.spv" ) );
	progs.Add( "Sky", LoadProgram( "shaders_bin/skyboxVS.spv", "shaders_bin/skyboxPS.spv" ) );
	progs.Add( "LitDepth", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" ) );
	progs.Add( "LitOpaque", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/litPS.spv" ) );
	progs.Add( "LitTree", LoadProgram( "shaders_bin/treeVS.spv", "shaders_bin/litPS.spv" ) ); // TODO: vert motion
	progs.Add( "LitTrans", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/emissivePS.spv" ) );
	progs.Add( "Debug", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/depthPS.spv" ) );
	progs.Add( "Debug_Solid", LoadProgram( "shaders_bin/simpleVS.spv", "shaders_bin/litPS.spv" ) );
	progs.Add( "PostProcess", LoadProgram( "shaders_bin/defaultVS.spv", "shaders_bin/postProcessPS.spv" ) );
	progs.Add( "Image2D", LoadProgram( "shaders_bin/defaultVS.spv", "shaders_bin/simplePS.spv" ) );
}

void LoadImages( AssetLibImages& images )
{
	const std::vector<std::string> texturePaths = { "checker.png", "sapeli.jpg", "white.png", "black.jpg",
													"chapel_right.jpg", "chapel_left.jpg", "chapel_top.jpg", "chapel_bottom.jpg", "chapel_front.jpg", "chapel_back.jpg" };

	for ( const std::string& texturePath : texturePaths )
	{
		texture_t texture;
		if ( LoadTextureImage( ( TexturePath + texturePath ).c_str(), texture ) ) {
			images.Add( texturePath.c_str(), texture );
		}
	}
	texture_t cubeMap;
	const std::string envMapName = "chapel_low";
	const std::string cubeMapPath = ( TexturePath + envMapName );
	if ( LoadTextureCubeMapImage( cubeMapPath.c_str(), "jpg", cubeMap ) ) {
		images.Add( envMapName.c_str(), cubeMap );
	}
}

void LoadMaterials( AssetLibMaterials& materials )
{
	{
		Material material;
		material.shaders[ DRAWPASS_SHADOW ] = scene.gpuPrograms.RetrieveHdl( "TerrainShadow" );
		material.shaders[ DRAWPASS_DEPTH ] = scene.gpuPrograms.RetrieveHdl( "TerrainDepth" );
		material.shaders[ DRAWPASS_TERRAIN ] = scene.gpuPrograms.RetrieveHdl( "Terrain" );
		//material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.RetrieveHdl( "TerrainDepth" );
		material.textures[ 0 ] = scene.textureLib.RetrieveHdl( "heightmap.png" );
		material.textures[ 1 ] = scene.textureLib.RetrieveHdl( "grass.jpg" );
		material.textures[ 2 ] = scene.textureLib.RetrieveHdl( "desert.jpg" );
		materials.Add( "TERRAIN", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_SKYBOX ] = scene.gpuPrograms.RetrieveHdl( "Sky" );
		material.textures[ 0 ] = scene.textureLib.RetrieveHdl( "chapel_right.jpg" );
		material.textures[ 1 ] = scene.textureLib.RetrieveHdl( "chapel_left.jpg" );
		material.textures[ 2 ] = scene.textureLib.RetrieveHdl( "chapel_top.jpg" );
		material.textures[ 3 ] = scene.textureLib.RetrieveHdl( "chapel_bottom.jpg" );
		material.textures[ 4 ] = scene.textureLib.RetrieveHdl( "chapel_front.jpg" );
		material.textures[ 5 ] = scene.textureLib.RetrieveHdl( "chapel_back.jpg" );
		materials.Add( "SKY", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_SHADOW ] = scene.gpuPrograms.RetrieveHdl( "Shadow" );
		material.shaders[ DRAWPASS_DEPTH ] = scene.gpuPrograms.RetrieveHdl( "LitDepth" );
		material.shaders[ DRAWPASS_OPAQUE ] = scene.gpuPrograms.RetrieveHdl( "LitOpaque" );
		//material.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.RetrieveHdl( "LitDepth" );
		material.textures[ 0 ] = scene.textureLib.RetrieveHdl( "sapeli.jpg" );
		materials.Add( "WHITE_WOOD", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_TRANS ] = scene.gpuPrograms.RetrieveHdl( "LitTrans" );
		material.shaders[ DRAWPASS_DEBUG_WIREFRAME ] = scene.gpuPrograms.RetrieveHdl( "Debug" );
		materials.Add( "GlowSquare", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_POST_2D ] = scene.gpuPrograms.RetrieveHdl( "PostProcess" );
	//	material.textures[ 0 ] = 0;
	//	material.textures[ 1 ] = 1;
		materials.Add( "TONEMAP", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_POST_2D ] = scene.gpuPrograms.RetrieveHdl( "Image2D" );
	//	material.textures[ 0 ] = 0;
	//	material.textures[ 1 ] = 1;
	//	material.textures[ 2 ] = 2;
		materials.Add( "IMAGE2D", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_DEBUG_WIREFRAME ] = scene.gpuPrograms.RetrieveHdl( "Debug" );
		materials.Add( "DEBUG_WIRE", material );
	}

	{
		Material material;
		material.shaders[ DRAWPASS_DEBUG_SOLID ] = scene.gpuPrograms.RetrieveHdl( "Debug_Solid" );
		materials.Add( "DEBUG_SOLID", material );
	}
}

void LoadModels( AssetLibModels& models )
{
	{
		Model model;
		CreateSkyBoxSurf( model );
		models.Add( "_skybox", model );
	}
	{
		Timer t;
		t.Start();
		hdl_t handle = LoadRawModel( scene, "axis.obj", "axis", ModelPath, TexturePath );
		t.Stop();
		std::cout << t.GetElapsed() << std::endl;
		t.Start();
		WriteModel( scene, BakePath + ModelPath + handle.String() + BakedModelExtension, handle ); // FIXME
		t.Stop();
		std::cout << t.GetElapsed() << std::endl;
		t.Start();
		LoadModel( scene, handle, BakePath, ModelPath, BakedModelExtension );
		t.Stop();
		std::cout << t.GetElapsed() << std::endl;


		LoadRawModel( scene, "cube.obj", "cube", ModelPath, TexturePath );
		LoadRawModel( scene, "diamond.obj", "diamond", ModelPath, TexturePath );
		LoadRawModel( scene, "sphere.obj", "sphere", ModelPath, TexturePath );
		LoadRawModel( scene, "pawn.obj", "pawn", ModelPath, TexturePath );
		LoadRawModel( scene, "rook.obj", "rook", ModelPath, TexturePath );
		LoadRawModel( scene, "knight.obj", "knight", ModelPath, TexturePath );
		LoadRawModel( scene, "bishop.obj", "bishop", ModelPath, TexturePath );
		LoadRawModel( scene, "king.obj", "king", ModelPath, TexturePath );
		LoadRawModel( scene, "queen.obj", "queen", ModelPath, TexturePath );
		LoadRawModel( scene, "chess_board.obj", "chess_board", ModelPath, TexturePath );
	}
	{
		const hdl_t planeHdl = LoadRawModel( scene, "plane.obj", "plane", ModelPath, TexturePath );
		models.Find( planeHdl )->surfs[ 0 ].materialHdl = scene.materialLib.RetrieveHdl( "GlowSquare" );
	}
	{
		Model model;
		CreateQuadSurface2D( "TONEMAP", model, vec2f( 1.0f, 1.0f ), vec2f( 2.0f ) );
		models.Add( "_postProcessQuad", model );
	}
	{
		Model model;
		CreateQuadSurface2D( "IMAGE2D", model, vec2f( 1.0f, 1.0f ), vec2f( 1.0f * ( 9.0 / 16.0f ), 1.0f ) );
		models.Add( "_quadTexDebug", model );
	}
}


void MakeScene()
{
	const int piecesNum = 16;

	LoadShaders( scene.gpuPrograms );
	LoadImages( scene.textureLib );
	LoadMaterials( scene.materialLib );
	LoadModels( scene.modelLib );

	gameConfig_t cfg;
	LoadConfig( "scenes/chessCfg/default_board.txt", cfg );
	chessEngine.Init( cfg );
	//board.SetEventCallback( &ProcessEvent );

	{
		Entity* ent = new Entity();
		scene.CreateEntity( scene.modelLib.RetrieveHdl( "_skybox" ), *ent );
		ent->dbgName = "_skybox";
		scene.entities.push_back( ent );
	}

	{
		Entity* ent = new Entity();
		scene.CreateEntity( scene.modelLib.RetrieveHdl( "chess_board" ), *ent );
		ent->dbgName = "chess_board";
		scene.entities.push_back( ent );
	}

	for ( int i = 0; i < 8; ++i )
	{
		for ( int j = 0; j < 8; ++j )
		{
			PieceEntity* squareEnt = new PieceEntity( GetFile( j ), GetRank( i ) );
			scene.CreateEntity( scene.modelLib.RetrieveHdl( "plane" ), *squareEnt );
			squareEnt->SetOrigin( GetSquareCenterForLocation( squareEnt->file, squareEnt->rank ) + vec3f( 0.0f, 0.0f, 0.01f ) );
			squareEnt->SetFlag( ENT_FLAG_SELECTABLE );
			squareEnt->handle = -1;
			std::string name = "plane_";
			name += std::to_string( i ) + "_" + std::to_string( j );

			squareEnt->dbgName = name.c_str();
			glowEntities.push_back( static_cast<uint32_t>( scene.entities.size() ) );
			scene.entities.push_back( squareEnt );

			pieceInfo_t pieceInfo = chessEngine.GetInfo( j, i );
			if ( pieceInfo.onBoard == false ) {
				continue;
			}
			PieceEntity* pieceEnt = new PieceEntity( GetFile( j ), GetRank( i ) );
			scene.CreateEntity( scene.modelLib.RetrieveHdl( GetModelName( pieceInfo.piece ).c_str() ), *pieceEnt );
			pieceEnt->handle = chessEngine.FindPiece( pieceInfo.team, pieceInfo.piece, pieceInfo.instance );
			pieceEnt->SetFlag( ENT_FLAG_SELECTABLE );
			if ( pieceInfo.team == teamCode_t::WHITE ) {
				pieceEnt->materialHdl = scene.materialLib.RetrieveHdl( "White.001" );
			} else {
				pieceEnt->SetRotation( vec3f( 0.0f, 0.0f, 180.0f ) );
				pieceEnt->materialHdl = scene.materialLib.RetrieveHdl( "Chess_Black.001" );
			}
			pieceEnt->dbgName = GetName( pieceInfo ).c_str();
			pieceEntities.push_back( static_cast<uint32_t>( scene.entities.size() ) );
			scene.entities.push_back( pieceEnt );
		}
	}

	const hdl_t cubeHdl = scene.modelLib.RetrieveHdl( "cube" );
	const uint32_t pieceCount = static_cast<uint32_t>( pieceEntities.size() );
	bool drawPieceWireframes = true;
	if( drawPieceWireframes )
	{
		for ( int i = 0; i < pieceCount; ++i )
		{
			BoundEntity* cubeEnt = new BoundEntity();
			scene.CreateEntity( cubeHdl, *cubeEnt );
			cubeEnt->materialHdl = scene.materialLib.RetrieveHdl( "DEBUG_WIRE" );
			cubeEnt->SetFlag( ENT_FLAG_WIREFRAME );
			cubeEnt->dbgName = ( scene.entities[ pieceEntities[ i ] ]->dbgName + "_cube" ).c_str();
			cubeEnt->pieceId = pieceEntities[ i ];
			boundEntities.push_back( static_cast<uint32_t>( scene.entities.size() ) );
			scene.entities.push_back( cubeEnt );
		}
	}

	const hdl_t diamondHdl = scene.modelLib.RetrieveHdl( "diamond" );
	for ( int i = 0; i < MaxLights; ++i )
	{
		Entity* ent = new Entity();
		scene.CreateEntity( diamondHdl, *ent );
		ent->materialHdl = scene.materialLib.RetrieveHdl( "DEBUG_WIRE" );
		ent->SetFlag( ENT_FLAG_WIREFRAME );
		ent->dbgName = ( "light" + std::string( { (char)( (int)'0' + i ) } ) + "_dbg" ).c_str();
		scene.entities.push_back( ent );
	}

	{
		Entity* ent = new Entity();
		scene.CreateEntity( scene.modelLib.RetrieveHdl( "_postProcessQuad" ), *ent );
		ent->dbgName = "_postProcessQuad";
		scene.entities.push_back( ent );
	}

	{
		Entity* ent = new Entity();
		scene.CreateEntity( scene.modelLib.RetrieveHdl( "_quadTexDebug" ), *ent );
		ent->dbgName = "_quadTexDebug";
		scene.entities.push_back( ent );
	}

	{
		scene.lights[ 0 ].lightPos = vec4f( 0.0f, 0.0f, 6.0f, 0.0f );
		scene.lights[ 0 ].intensity = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
		scene.lights[ 0 ].lightDir = vec4f( 0.0f, 0.0f, -1.0f, 0.0f );

		scene.lights[ 1 ].lightPos = vec4f( 0.0f, 10.0f, 5.0f, 0.0f );
		scene.lights[ 1 ].intensity = vec4f( 0.5f, 0.5f, 0.5f, 1.0f );
		scene.lights[ 1 ].lightDir = vec4f( 0.0f, 0.0f, 1.0f, 0.0f );

		scene.lights[ 2 ].lightPos = vec4f( 0.0f, -10.0f, 5.0f, 0.0f );
		scene.lights[ 2 ].intensity = vec4f( 0.5f, 0.5f, 0.5f, 1.0f );
		scene.lights[ 2 ].lightDir = vec4f( 0.0f, 0.0f, 1.0f, 0.0f );
	}
}

void UpdateSceneLocal( const float dt )
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

	scene.lights[ 0 ].lightPos = vec4f( 5.0f * cos( time ), 5.0f * sin( time ), 8.0f, 0.0f );

	const mouse_t& mouse = window.input.GetMouse();
	if ( mouse.centered )
	{
		const float maxSpeed = mouse.speed;
		const float yawDelta = maxSpeed * mouse.dx;
		const float pitchDelta = -maxSpeed * mouse.dy;
		scene.camera.AdjustYaw( yawDelta );
		scene.camera.AdjustPitch( pitchDelta );
	}
	else if( mouse.leftDown )
	{
		Ray ray = scene.camera.GetViewRay( vec2f( 0.5f * mouse.x + 0.5f, 0.5f * mouse.y + 0.5f ) );
		selectedEntity = GetTracedEntity( ray );
	}

	if ( selectedEntity != nullptr )
	{
		PieceEntity* selectedPiece = reinterpret_cast<PieceEntity*>( selectedEntity );

		int selectedActionIx = -1;
		for ( int actionIx = 0; actionIx < actions.size(); ++actionIx ) {
			const moveAction_t& action = actions[ actionIx ];
			if ( ( action.y == GetRankNum( selectedPiece->rank ) ) && ( action.x == GetFileNum( selectedPiece->file ) ) ) {
				selectedActionIx = actionIx;
				break;
			}
		}

		if ( ( selectedActionIx > 0 ) && ( movePieceId != nullptr ) )
		{
			PieceEntity* movedPiece = reinterpret_cast<PieceEntity*>( movePieceId );
			const pieceInfo_t movedPieceInfo = chessEngine.GetInfo( GetFileNum( movedPiece->file ), GetRankNum( movedPiece->rank ) );
			const moveAction_t& action = actions[ selectedActionIx ];
			
			command_t cmd;
			cmd.instance = movedPieceInfo.instance;
			cmd.team = movedPieceInfo.team;
			cmd.pieceType = movedPieceInfo.piece;
			cmd.x = action.x;
			cmd.y = action.y;
	
			const resultCode_t result = chessEngine.Execute( cmd );
			actions.resize( 0 );
		}
		else if( selectedPiece->handle >= 0 )
		{
			actions.resize( 0 );
			chessEngine.EnumerateActions( selectedPiece->handle, actions );
			movePieceId = selectedEntity;
		}
		else
		{
			selectedActionIx = -1;
			selectedEntity = nullptr;
			movePieceId = nullptr;
		}
	}

	for ( uint32_t entityIx = 0; entityIx < static_cast<uint32_t>( pieceEntities.size() ); ++entityIx ) {
		const uint32_t pieceIx = pieceEntities[ entityIx ];
		PieceEntity* ent = reinterpret_cast<PieceEntity*>( scene.entities[ pieceIx ] );
		if ( ent == selectedEntity ) {
			ent->outline = true;
		} else {
			ent->outline = false;
		}
		const pieceInfo_t info = chessEngine.GetInfo( ent->handle );
		int x, y;
		if ( chessEngine.GetLocation( ent->handle, x, y ) ) {
			ent->SetOrigin( GetSquareCenterForLocation( GetFile( x ), GetRank( y ) ) );
		}
	}

	for ( uint32_t entityIx = 0; entityIx < static_cast<uint32_t>( glowEntities.size() ); ++entityIx ) {
		const uint32_t glowIx = glowEntities[ entityIx ];
		PieceEntity* ent = reinterpret_cast<PieceEntity*>( scene.FindEntity( glowIx ) );
		bool validTile = false;
		for ( int actionIx = 0; actionIx < actions.size(); ++actionIx ) {
			const moveAction_t& action = actions[ actionIx ];
			if ( ( action.y == GetRankNum( ent->rank ) ) && ( action.x == GetFileNum( ent->file ) ) ) {
				validTile = true;
				break;
			}
		}
		if ( validTile ) {
			ent->ClearFlag( ENT_FLAG_NO_DRAW );
		} else {
			ent->SetFlag( ENT_FLAG_NO_DRAW );
		}
	}

	Material* glowMat = scene.materialLib.Find( "GlowSquare" );
	glowMat->Kd = rgbTuplef_t( 0.1f, 0.1f, 1.0f );
	glowMat->d = 0.5f * cos( 3.0f * time ) + 0.5f;

	const uint32_t pieceBoundCount = static_cast<uint32_t>( boundEntities.size() );
	for ( int i = 0; i < pieceBoundCount; ++i )
	{
		BoundEntity* boundEnt = reinterpret_cast<BoundEntity*>( scene.entities[ boundEntities[i] ] );
		if( boundEnt->pieceId == ~0x0 )
			continue;
		Entity* pieceEnt = scene.entities[ boundEnt->pieceId ];
		AABB bounds = pieceEnt->GetBounds();
		vec3f size = bounds.GetSize();
		vec3f center = bounds.GetCenter();
		boundEnt->SetOrigin( vec3f( center[ 0 ], center[ 1 ], center[ 2 ] ) );
		boundEnt->SetScale( 0.5f * vec3f( size[ 0 ], size[ 1 ], size[ 2 ] ) );
	}

	for ( int i = 0; i < MaxLights; ++i )
	{
		Entity* debugLight = scene.FindEntity( ( "light" + std::string( { (char)( (int)'0' + i ) } ) + "_dbg" ).c_str() );
		if ( debugLight == nullptr ) {
			continue;
		}
		vec3f origin = vec3f( scene.lights[ i ].lightPos[ 0 ], scene.lights[ i ].lightPos[ 1 ], scene.lights[ i ].lightPos[ 2 ] );
		debugLight->SetOrigin( origin );
		debugLight->SetScale( vec3f( 0.25f, 0.25f, 0.25f ) );
	}
}