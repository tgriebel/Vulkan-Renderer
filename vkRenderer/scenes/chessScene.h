#pragma once
#include "../common.h"
#include "../scene.h"
#include "../render_util.h"
#include "../io.h"
#include "../window.h"
#include <chess.h>
#include <commands.h>

extern Scene scene;

typedef AssetLib< texture_t > AssetLibImages;
typedef AssetLib< Material > AssetLibMaterials;

extern AssetLib< GpuProgram >	gpuPrograms;
extern AssetLibImages			textureLib;
extern AssetLibMaterials		materialLib;
extern imguiControls_t			imguiControls;
extern Window					window;

gameConfig_t					cfg;
Chess							chessEngine;
int								selectedPieceId = -1;
int								movePieceId = -1;
std::vector< moveAction_t >		actions;
std::vector< int >				pieceEntities;
std::vector< int >				glowEntities;

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

int GetTracedEntity( const Ray& ray )
{
	int closestId = -1;
	float closestT = FLT_MAX;
	const int entityNum = static_cast<int>( scene.entities.Count() );
	for ( int i = 0; i < entityNum; ++i )
	{
		Entity* ent = scene.FindEntity( i );
		if ( !ent->HasFlag( ENT_FLAG_SELECTABLE ) ) {
			continue;
		}
		float t0, t1;
		if ( ent->GetBounds().Intersect( ray, t0, t1 ) ) {
			if ( t0 < closestT ) {
				closestT = t0;
				closestId = i;
			}
		}
	}
	return closestId;
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
	const std::vector<std::string> texturePaths = { "checker.png", "sapeli.jpg", "white.png", "black.jpg",
													"chapel_right.jpg", "chapel_left.jpg", "chapel_top.jpg", "chapel_bottom.jpg", "chapel_front.jpg", "chapel_back.jpg" };

	for ( const std::string& texturePath : texturePaths )
	{
		texture_t texture;
		if ( LoadTextureImage( ( TexturePath + texturePath ).c_str(), texture ) ) {
			textureLib.Add( texturePath.c_str(), texture );
		}
	}
	texture_t cubeMap;
	const std::string envMapName = "chapel_low";
	const std::string cubeMapPath = ( TexturePath + envMapName );
	if ( LoadTextureCubeMapImage( cubeMapPath.c_str(), "jpg", cubeMap ) ) {
		textureLib.Add( envMapName.c_str(), cubeMap );
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
		material.shaders[ DRAWPASS_DEBUG_WIREFRAME ] = gpuPrograms.RetrieveHdl( "Debug" );
		Add( "GlowSquare", material );
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
		material.textures[ 2 ] = 2;
		Add( "IMAGE2D", material );
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
		LoadModel( "rook.obj", "rook" );
		LoadModel( "knight.obj", "knight" );
		LoadModel( "bishop.obj", "bishop" );
		LoadModel( "king.obj", "king" );
		LoadModel( "queen.obj", "queen" );
		LoadModel( "chess_board.obj", "chess_board" );
	}
	{
		const int planeId = LoadModel( "plane.obj", "plane" );
		modelLib.Find( planeId )->surfs[ 0 ].materialId = materialLib.FindId( "GlowSquare" );
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

	gameConfig_t cfg;
	LoadConfig( "scenes/chessCfg/default_board.txt", cfg );
	chessEngine.Init( cfg );
	//board.SetEventCallback( &ProcessEvent );

	{
		Entity* ent = new Entity();
		scene.CreateEntity( modelLib.FindId( "_skybox" ), *ent );
		scene.entities.Add( "_skybox", ent );
	}

	{
		Entity* ent = new Entity();
		scene.CreateEntity( modelLib.FindId( "chess_board" ), *ent );
		scene.entities.Add( "chess_board", ent );
	}

	for ( int i = 0; i < 8; ++i )
	{
		for ( int j = 0; j < 8; ++j )
		{
			PieceEntity* squareEnt = new PieceEntity( GetFile( j ), GetRank( i ) );
			scene.CreateEntity( modelLib.FindId( "plane" ), *squareEnt );
			squareEnt->SetOrigin( GetSquareCenterForLocation( squareEnt->file, squareEnt->rank ) + vec3f( 0.0f, 0.0f, 0.01f ) );
			squareEnt->SetFlag( ENT_FLAG_SELECTABLE );
			squareEnt->handle = -1;
			std::string name = "plane_";
			name += std::to_string( i ) + "_" + std::to_string( j );
			int index = scene.entities.Add( name.c_str(), squareEnt );
			glowEntities.push_back( index );

			pieceInfo_t pieceInfo = chessEngine.GetInfo( j, i );
			if ( pieceInfo.onBoard == false ) {
				continue;
			}
			PieceEntity* pieceEnt = new PieceEntity( GetFile( j ), GetRank( i ) );
			scene.CreateEntity( modelLib.FindId( GetModelName( pieceInfo.piece ).c_str() ), *pieceEnt );
			pieceEnt->handle = chessEngine.FindPiece( pieceInfo.team, pieceInfo.piece, pieceInfo.instance );
			pieceEnt->SetFlag( ENT_FLAG_SELECTABLE );
			if ( pieceInfo.team == teamCode_t::WHITE ) {
				pieceEnt->materialId = materialLib.FindId( "White.001" );
			} else {
				pieceEnt->SetRotation( vec3f( 0.0f, 0.0f, 180.0f ) );
				pieceEnt->materialId = materialLib.FindId( "Chess_Black.001" );
			}
			index = scene.entities.Add( GetName( pieceInfo ).c_str(), pieceEnt );
			pieceEntities.push_back( index );
		}
	}
	//for ( int i = 0; i < 8; ++i )
	//{
	//	{
	//		Entity cubeEnt;
	//		scene.CreateEntity( modelLib.FindId( "cube" ), cubeEnt );
	//		cubeEnt.materialId = materialLib.FindId( "DEBUG_WIRE" );
	//		cubeEnt.renderFlags = static_cast<renderFlags_t>( WIREFRAME | SKIP_OPAQUE );
	//		scene.entities.Add( ( pieceNames[ i ] + "_cube" ).c_str(), cubeEnt );
	//	}
	//}
	//{
	//	Entity* ent = new Entity();
	//	scene.CreateEntity( modelLib.FindId( "sphere" ), *ent );
	//	ent->SetOrigin( vec3f( 0.0f, 0.0f, 0.0f ) );
	//	scene.entities.Add( "sphere", ent );
	//}

	for ( int i = 0; i < MaxLights; ++i )
	{
		Entity* ent = new Entity();
		scene.CreateEntity( modelLib.FindId( "diamond" ), *ent );
		ent->materialId = materialLib.FindId( "DEBUG_WIRE" );
		ent->SetRenderFlag( WIREFRAME );
		ent->SetRenderFlag( SKIP_OPAQUE );
		scene.entities.Add( ( "light" + std::string( { (char)( (int)'0' + i ) } ) + "_dbg" ).c_str(), ent );
	}

	{
		Entity* ent = new Entity();
		scene.CreateEntity( modelLib.FindId( "_postProcessQuad" ), *ent );
		scene.entities.Add( "_postProcessQuad", ent );
	}

	{
		Entity* ent = new Entity();
		scene.CreateEntity( modelLib.FindId( "_quadTexDebug" ), *ent );
		scene.entities.Add( "_quadTexDebug", ent );
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
		scene.camera.SetYaw( yawDelta );
		scene.camera.SetPitch( pitchDelta );
	}
	else if( mouse.leftDown )
	{
		const vec2f screenPoint = vec2f( mouse.x, mouse.y );
		int width, height;
		window.GetWindowFrameBufferSize( width, height );
		const vec2f ndc = vec2f( 2.0f * screenPoint[ 0 ] / width, 2.0f * screenPoint[ 1 ] / height ) - vec2f( 1.0f );

		Ray ray = scene.camera.GetViewRay( vec2f( 0.5f * ndc[ 0 ] + 0.5f, 0.5f * ndc[ 1 ] + 0.5f ) );
		selectedPieceId = GetTracedEntity( ray );
	}

	if ( selectedPieceId >= 0 )
	{
		PieceEntity* selectedPiece = reinterpret_cast<PieceEntity*>( scene.FindEntity( selectedPieceId ) );

		int selectedActionIx = -1;
		for ( int actionIx = 0; actionIx < actions.size(); ++actionIx ) {
			const moveAction_t& action = actions[ actionIx ];
			if ( ( action.y == GetRankNum( selectedPiece->rank ) ) && ( action.x == GetFileNum( selectedPiece->file ) ) ) {
				selectedActionIx = actionIx;
				break;
			}
		}

		if ( ( selectedActionIx >= 0 ) && ( movePieceId >= 0 ) )
		{
			PieceEntity* movedPiece = reinterpret_cast<PieceEntity*>( scene.FindEntity( movePieceId ) );
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
			movePieceId = selectedPieceId;
		}
		else
		{
			selectedActionIx = -1;
			selectedPieceId = -1;
			movePieceId = -1;
		}
	}

	for ( int entityIx = 0; entityIx < pieceEntities.size(); ++entityIx ) {
		const int hdl = pieceEntities[ entityIx ];
		PieceEntity* ent = reinterpret_cast<PieceEntity*>( scene.FindEntity( hdl ) );
		if ( hdl == selectedPieceId ) {
			ent->outline = true;
		//	ent->SetRenderFlag( WIREFRAME );
		} else {
			ent->outline = false;
		//	ent->ClearRenderFlag( WIREFRAME );
		}
		const pieceInfo_t info = chessEngine.GetInfo( ent->handle );
		int x, y;
		if ( chessEngine.GetLocation( ent->handle, x, y ) ) {
			ent->SetOrigin( GetSquareCenterForLocation( GetFile( x ), GetRank( y ) ) );
		}
	}
	for ( int entityIx = 0; entityIx < glowEntities.size(); ++entityIx ) {
		const int hdl = glowEntities[ entityIx ];
		PieceEntity* ent = reinterpret_cast<PieceEntity*>( scene.FindEntity( hdl ) );
		bool validTile = false;
		for ( int actionIx = 0; actionIx < actions.size(); ++actionIx ) {
			const moveAction_t& action = actions[ actionIx ];
			if ( ( action.y == GetRankNum( ent->rank ) ) && ( action.x == GetFileNum( ent->file ) ) ) {
				validTile = true;
				break;
			}
		}
		if ( validTile ) {
			ent->ClearRenderFlag( HIDDEN );
		} else {
			ent->SetRenderFlag( HIDDEN );
		}
	}

	Material* glowMat = materialLib.Find( "GlowSquare" );
	glowMat->Kd = rgbTuplef_t( 0.1f, 0.1f, 1.0f );
	glowMat->d = 0.5f * cos( 3.0f * time ) + 0.5f;
	//for ( int i = 0; i < 8; ++i )
	//{
	//	Entity* pawn = scene.entities.Find( pieceNames[ i ].c_str() );
	//	Entity* debugBox = scene.entities.Find( ( pieceNames[ i ] + "_cube" ).c_str() );
	//	AABB bounds = pawn->GetBounds();
	//	vec3f size = bounds.GetSize();
	//	vec3f center = bounds.GetCenter();
	//	debugBox->SetOrigin( vec3f( center[ 0 ], center[ 1 ], center[ 2 ] ) );
	//	debugBox->SetScale( 0.5f * vec3f( size[ 0 ], size[ 1 ], size[ 2 ] ) );
	//}

	for ( int i = 0; i < MaxLights; ++i )
	{
		Entity* debugLight = scene.FindEntity( ( "light" + std::string( { (char)( (int)'0' + i ) } ) + "_dbg" ).c_str() );
		vec3f origin = vec3f( scene.lights[ i ].lightPos[ 0 ], scene.lights[ i ].lightPos[ 1 ], scene.lights[ i ].lightPos[ 2 ] );
		debugLight->SetOrigin( origin );
		debugLight->SetScale( vec3f( 0.25f, 0.25f, 0.25f ) );
	}
}