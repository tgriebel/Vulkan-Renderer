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
#include <SysCore/jsmn.h>

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


void LoadImages( AssetLibImages& images )
{
	Texture cubeMap;
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
		material.AddShader( DRAWPASS_POST_2D, scene.gpuPrograms.RetrieveHdl( "PostProcess" ) );
		materials.Add( "TONEMAP", material );
	}

	{
		Material material;
		material.AddShader( DRAWPASS_POST_2D, scene.gpuPrograms.RetrieveHdl( "Image2D" ) );
		materials.Add( "IMAGE2D", material );
	}

	{
		Material material;
		material.AddShader( DRAWPASS_DEBUG_WIREFRAME, scene.gpuPrograms.RetrieveHdl( "Debug" ) );
		materials.Add( "DEBUG_WIRE", material );
	}

	{
		Material material;
		material.AddShader( DRAWPASS_DEBUG_SOLID, scene.gpuPrograms.RetrieveHdl( "Debug_Solid" ) );
		materials.Add( "DEBUG_SOLID", material );
	}
}

void LoadModels( AssetLibModels& models )
{
	{
		{
			Model model;
			if( LoadRawModel( scene, "axis.obj", ModelPath, TexturePath, model ) ) {
				scene.modelLib.Add( "axis", model );
			}
		}
		{
			Model model;
			if ( LoadRawModel( scene, "cube.obj", ModelPath, TexturePath, model ) ) {
				scene.modelLib.Add( "cube", model );
			}
		}
		{
			Model model;
			if ( LoadRawModel( scene, "diamond.obj", ModelPath, TexturePath, model ) ) {
				scene.modelLib.Add( "diamond", model );
			}
		}
		{
			Model model;
			if ( LoadRawModel( scene, "sphere.obj", ModelPath, TexturePath, model ) ) {
				scene.modelLib.Add( "sphere", model );
			}
		}
		{
			Model model;
			if ( LoadRawModel( scene, "pawn.obj", ModelPath, TexturePath, model ) ) {
				scene.modelLib.Add( "pawn", model );
			}
		}
		{
			Model model;
			if ( LoadRawModel( scene, "rook.obj", ModelPath, TexturePath, model ) ) {
				scene.modelLib.Add( "rook", model );
			}
		}
		{
			Model model;
			if ( LoadRawModel( scene, "knight.obj", ModelPath, TexturePath, model ) ) {
				scene.modelLib.Add( "knight", model );
			}
		}
		{
			Model model;
			if ( LoadRawModel( scene, "bishop.obj", ModelPath, TexturePath, model ) ) {
				scene.modelLib.Add( "bishop", model );
			}
		}
		{
			Model model;
			if ( LoadRawModel( scene, "king.obj", ModelPath, TexturePath, model ) ) {
				scene.modelLib.Add( "king", model );
			}
		}
		{
			Model model;
			if ( LoadRawModel( scene, "queen.obj", ModelPath, TexturePath, model ) ) {
				scene.modelLib.Add( "queen", model );
			}
		}
	}
	{
		Model model;
		if( LoadRawModel( scene, "plane.obj", ModelPath, TexturePath, model ) ) {
			hdl_t planeHdl = scene.modelLib.Add( "plane", model );
			models.Find( planeHdl )->Get().surfs[ 0 ].materialHdl = scene.materialLib.RetrieveHdl( "GlowSquare" );
		}
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

#define USE_JSON_SCENE 1

#if USE_JSON_SCENE
static int jsoneq( const char* json, jsmntok_t* tok, const char* s ) {
	if ( tok->type == JSMN_STRING && (int)strlen( s ) == tok->end - tok->start &&
		strncmp( json + tok->start, s, tok->end - tok->start ) == 0 ) {
		return 0;
	}
	return -1;
}

typedef int ParseObjectFunc( const std::vector<char>& file, jsmntok_t* tokens, const int r, const int tx, void* object );
template<class T>
void ParseArray( const std::vector<char>& file, jsmntok_t* tokens, const int r, const int tx, ParseObjectFunc* readFunc, void* object );

enum parseType_t : uint32_t
{
	PARSE_TYPE_STRING,
	PARSE_TYPE_FLOAT
};

void ConsumeStringValue( const std::vector<char>& file, const jsmntok_t* tokens, int& tx, void* value )
{
	std::string* s = reinterpret_cast<std::string*>( value );
	const uint32_t valueLen = ( tokens[ tx + 1 ].end - tokens[ tx + 1 ].start );
	*s = std::string( file.data() + tokens[ tx + 1 ].start, valueLen );
	tx += 2;
}


void ConsumeFloatValue( const std::vector<char>& file, const jsmntok_t* tokens, int& tx, void* value )
{
	float* f = reinterpret_cast<float*>( value );
	const uint32_t valueLen = ( tokens[ tx + 1 ].end - tokens[ tx + 1 ].start );
	std::string s = std::string( file.data() + tokens[ tx + 1 ].start, valueLen );
	*f = std::stof( s );

	tx += 2;
}


int ParseImageString( const std::vector<char>& file, jsmntok_t* tokens, const int r, const int tx, void* object )
{
	if ( tokens[ tx ].type != JSMN_STRING ) {
		return -1;
	}

	AssetLibImages* textureLib = reinterpret_cast<AssetLibImages*>( object );

	const uint32_t valueLen = ( tokens[ tx ].end - tokens[ tx ].start );
	std::string textureName = std::string( file.data() + tokens[ tx ].start, valueLen );

	TextureLoader* loader = new TextureLoader();
	loader->SetBasePath( TexturePath );
	loader->SetTextureFile( textureName );
	loader->LoadAsCubemap( false );

	textureLib->AddDeferred( textureName.c_str(), Asset<Texture>::loadHandlerPtr_t( loader ) );

	return tx + 1;
}


void ParseMaterialShaderObject( const std::vector<char>& file, jsmntok_t* tokens, const int r, int& i, void* object )
{
	if ( tokens[ i ].type != JSMN_OBJECT ) {
		return;
	}

	AssetLibGpuProgram& shaders = scene.gpuPrograms;
	Material* material = reinterpret_cast<Material*>( object );

	struct objectPair_t
	{
		const char* name;
		drawPass_t	pass;
	};

	const uint32_t objectCount = 9;
	static const objectPair_t objectMap[ objectCount ] =
	{
		{ "shadow", DRAWPASS_SHADOW },
		{ "depth", DRAWPASS_DEPTH },
		{ "opaque", DRAWPASS_OPAQUE },
		{ "trans", DRAWPASS_TRANS },
		{ "terrain", DRAWPASS_TERRAIN },
		{ "wireframe", DRAWPASS_DEBUG_WIREFRAME },
		{ "sky", DRAWPASS_SKYBOX },
		{ "postprocess", DRAWPASS_POST_2D },
		{ "debugSolid", DRAWPASS_DEBUG_SOLID }
	};

	int itemsFound = 0;
	int itemsCount = tokens[ i ].size;

	++i;
	
	while ( ( itemsFound < itemsCount ) && ( i < r ) )
	{
		for( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
		{
			if ( jsoneq( file.data(), &tokens[ i ], objectMap[ mapIx ].name ) == 0 )
			{
				const uint32_t valueLen = ( tokens[ i + 1 ].end - tokens[ i + 1 ].start );
				std::string shaderName = std::string( file.data() + tokens[ i + 1 ].start, valueLen );
				++itemsFound;
				i += 2;

				material->AddShader( objectMap[ mapIx ].pass, shaders.AddDeferred( shaderName.c_str() ) );
				break;
			}
		}
		//throw std::runtime_error( "ParseMaterialShaderObject: Could not find token!" );
	}
	return;
}


void ParseMaterialTextureObject( const std::vector<char>& file, jsmntok_t* tokens, const int r, int& i, void* object )
{
	if ( tokens[ i ].type != JSMN_OBJECT ) {
		return;
	}

	AssetLibImages& textures = scene.textureLib;
	Material* material = reinterpret_cast<Material*>( object );

	struct objectPair_t
	{
		const char* name;
		uint32_t	slot;
	};

	const uint32_t objectCount = 12;
	static const objectPair_t objectMap[ objectCount ] =
	{
		{ "colorMap", GGX_COLOR_MAP_SLOT },
		{ "normalMap", GGX_NORMAL_MAP_SLOT },
		{ "specMap", GGX_SPEC_MAP_SLOT },
		{ "colorMap0", HGT_COLOR_MAP_SLOT0 },
		{ "colorMap1", HGT_COLOR_MAP_SLOT1 },
		{ "heightmap", HGT_HEIGHT_MAP_SLOT },
		{ "cubemap_right", CUBE_RIGHT_SLOT },
		{ "cubemap_left", CUBE_LEFT_SLOT },
		{ "cubemap_top", CUBE_TOP_SLOT },
		{ "cubemap_bottom", CUBE_BOTTOM_SLOT },
		{ "cubemap_front", CUBE_FRONT_SLOT },
		{ "cubemap_back", CUBE_BACK_SLOT },
	};

	int itemsFound = 0;
	int itemsCount = tokens[ i ].size;

	++i;

	while ( ( itemsFound < itemsCount ) && ( i < r ) )
	{
		for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
		{
			if ( jsoneq( file.data(), &tokens[ i ], objectMap[ mapIx ].name ) == 0 )
			{
				const uint32_t valueLen = ( tokens[ i + 1 ].end - tokens[ i + 1 ].start );
				std::string textureName = std::string( file.data() + tokens[ i + 1 ].start, valueLen );
				++itemsFound;
				i += 2;

				material->AddTexture( objectMap[ mapIx ].slot, textures.AddDeferred( textureName.c_str() ) );
				break;
			}
		}
		//throw std::runtime_error( "ParseMaterialTextureObject: Could not find token!" );
	}
	return;
}


int ParseEntityObject( const std::vector<char>& file, jsmntok_t* tokens, const int r, const int tx, void* object )
{
	if ( tokens[ tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	std::string name;
	std::string modelName;
	std::vector<Entity*>& entities = scene.entities;

	float x = 0.0f, y = 0.0f, z = 0.0f;
	float rx = 0.0f, ry = 0.0f, rz = 0.0f;
	float sx = 1.0f, sy = 1.0f, sz = 1.0f;

	struct objectPair_t
	{
		const char*	name;
		void*		ptr;
		parseType_t	type;
	};

	const uint32_t objectCount = 11;
	static const objectPair_t objectMap[ objectCount ] =
	{
		{ "name", &name, PARSE_TYPE_STRING },
		{ "model", &modelName, PARSE_TYPE_STRING },
		{ "x", &x, PARSE_TYPE_FLOAT },
		{ "y", &y, PARSE_TYPE_FLOAT },
		{ "z", &z, PARSE_TYPE_FLOAT },
		{ "rx", &rx, PARSE_TYPE_FLOAT },
		{ "ry", &ry, PARSE_TYPE_FLOAT },
		{ "rz", &rz, PARSE_TYPE_FLOAT },
		{ "sx", &sx, PARSE_TYPE_FLOAT },
		{ "sy", &sy, PARSE_TYPE_FLOAT },
		{ "sz", &sz, PARSE_TYPE_FLOAT },
	};

	int i = tx + 1;
	int itemsFound = 0;
	int itemsCount = tokens[ tx ].size;
	while ( ( itemsFound < itemsCount ) && ( i < r ) )
	{
		for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
		{
			if ( jsoneq( file.data(), &tokens[ i ], objectMap[ mapIx ].name ) == 0 )
			{
				if ( objectMap[ mapIx ].type == PARSE_TYPE_FLOAT ) {
					ConsumeFloatValue( file, tokens, i, objectMap[ mapIx ].ptr );
				}
				else if ( objectMap[ mapIx ].type == PARSE_TYPE_STRING ) {
					ConsumeStringValue( file, tokens, i, objectMap[ mapIx ].ptr );
				}
				++itemsFound;
				break;
			}
		}
		//throw std::runtime_error( "ParseEntityObject: Could not find token!" );
	}

	using loader_t = Asset<Model>::loadHandlerPtr_t;

	Entity* ent = new Entity();
	ent->name = name;
	if( modelName == "_skybox" ) {
		ent->modelHdl = scene.modelLib.AddDeferred( modelName.c_str(), loader_t( new SkyBoxLoader() ) );
	} else {
		ModelLoader* loader = new ModelLoader();
		loader->SetModelPath( ModelPath );
		loader->SetTexturePath( TexturePath );
		loader->SetModelName( modelName );
		loader->SetSceneRef( &scene );
		ent->modelHdl = scene.modelLib.AddDeferred( modelName.c_str(), loader_t( loader ) );
	}
	ent->SetOrigin( vec3f( x, y, z ) );
	ent->SetRotation( vec3f( rx, ry, rz ) );
	ent->SetScale( vec3f( sx, sy, sz ) );
	scene.entities.push_back( ent );

	return i;
}


int ParseMaterialObject( const std::vector<char>& file, jsmntok_t* tokens, const int r, const int tx, void* object )
{
	if ( tokens[ tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	AssetLibMaterials* materials = reinterpret_cast<AssetLibMaterials*>( object );

	struct objectPair_t
	{
		const char* name;
		void*		ptr;
		parseType_t	type;
	};

	Material m;
	std::string name;

	const uint32_t objectCount = 6;
	static const objectPair_t objectMap[ objectCount ] =
	{
		{ "name", &name, PARSE_TYPE_STRING },
		{ "tr", &m.Tr, PARSE_TYPE_FLOAT },
		{ "ns", &m.Ns, PARSE_TYPE_FLOAT },
		{ "ni", &m.Ni, PARSE_TYPE_FLOAT },
		{ "d", &m.d, PARSE_TYPE_FLOAT },
		{ "illum", &m.illum, PARSE_TYPE_FLOAT },
	};

	int i = tx + 1;
	int itemsFound = 0;
	int itemsCount = tokens[ tx ].size;
	while ( ( itemsFound < itemsCount ) && ( i < r ) )
	{
		if ( jsoneq( file.data(), &tokens[ i ], "shaders" ) == 0 )
		{
			ParseMaterialShaderObject( file, tokens, r, ++i, &m );
			++itemsFound;
			continue;
		}
		else if ( jsoneq( file.data(), &tokens[ i ], "textures" ) == 0 )
		{
			ParseMaterialTextureObject( file, tokens, r, ++i, &m );
			++itemsFound;
			continue;
		}

		for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
		{
			if ( jsoneq( file.data(), &tokens[ i ], objectMap[ mapIx ].name ) == 0 )
			{
				if( objectMap[ mapIx ].type == PARSE_TYPE_FLOAT ) {
					ConsumeFloatValue( file, tokens, i, objectMap[ mapIx ].ptr );
				}
				else if ( objectMap[ mapIx ].type == PARSE_TYPE_STRING ) {
					ConsumeStringValue( file, tokens, i, objectMap[ mapIx ].ptr );
				}
				++itemsFound;
				break;
			}
		}

	//	throw std::runtime_error( "ParseMaterialObject: Could not find token!" );
	}

	materials->Add( name.c_str(), m );

	return i;
}


int ParseShaderObject( const std::vector<char>& file, jsmntok_t* tokens, const int r, const int tx, void* object )
{
	if ( tokens[ tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	std::string name;
	std::string vsShader;
	std::string psShader;
	AssetLibGpuProgram* shaders = reinterpret_cast<AssetLibGpuProgram*>( object );

	struct objectPair_t
	{
		const char*	name;
		void*		ptr;
		parseType_t	type;
	};

	const uint32_t objectCount = 3;
	static const objectPair_t objectMap[ objectCount ] =
	{
		{ "name", &name, PARSE_TYPE_STRING },
		{ "vs", &vsShader, PARSE_TYPE_STRING },
		{ "ps", &psShader, PARSE_TYPE_STRING },
	};

	int i = tx + 1;
	int itemsFound = 0;
	int itemsCount = tokens[ tx ].size;
	while ( ( itemsFound < itemsCount ) && ( i < r ) )
	{
		for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
		{
			if ( jsoneq( file.data(), &tokens[ i ], objectMap[ mapIx ].name ) == 0 )
			{
				ConsumeStringValue( file, tokens, i, objectMap[ mapIx ].ptr );
				++itemsFound;
				break;
			}
		}
	}

	GpuProgramLoader* loader = new GpuProgramLoader();
	loader->SetBasePath( "shaders_bin/" );
	loader->AddRasterPath( vsShader, psShader );
	shaders->AddDeferred( name.c_str(), Asset<GpuProgram>::loadHandlerPtr_t( loader ) );

	return i;
}


void ParseArray( const std::vector<char>& file, jsmntok_t* tokens, const int r, int& i, ParseObjectFunc* readFunc, void* object )
{
	if ( tokens[i].type != JSMN_ARRAY ) {
		return;
	}
	int itemsFound = 0;
	int itemsCount = tokens[ i ].size;

	++i;

	while( ( itemsFound < itemsCount ) && ( i < r ) )
	{
		int ret = (*readFunc)( file, tokens, r, i, object );
		if ( ret > 0 )
		{
			i = ret;
			++itemsFound;
		}
	}

	--i;
	return;
}


void LoadScene()
{
	jsmn_parser p;
	jsmntok_t t[ 1024 ];

	std::vector<char> file = ReadFile( "chess.json" );

	jsmn_init( &p );
	int r = jsmn_parse( &p, file.data(), static_cast<uint32_t>( file.size() ), t,
		sizeof( t ) / sizeof( t[ 0 ] ) );
	if ( r < 0 ) {
		std::cout << "Failed to parse JSON" << std::endl;
	}

	if ( r < 1 || t[ 0 ].type != JSMN_OBJECT ) {
		std::cout << "Object expected" << std::endl;
	}

	for ( int i = 1; i < r; i++ ) {
		if ( jsoneq( file.data(), &t[ i ], "scene" ) == 0 )
		{
			const uint32_t valueLen = ( t[ i + 1 ].end - t[ i + 1 ].start );
			std::string s = std::string( file.data() + t[ i + 1 ].start, valueLen );
			i++;
		}
		else if ( jsoneq( file.data(), &t[ i ], "shaders" ) == 0 )
		{
			if ( t[ i + 1 ].type != JSMN_ARRAY ) {
				continue;
			}
			ParseArray( file, t, r, ++i, ParseShaderObject, &scene.gpuPrograms );
		}
		else if ( jsoneq( file.data(), &t[ i ], "images" ) == 0 )
		{
			if ( t[ i + 1 ].type != JSMN_ARRAY ) {
				continue;
			}
			ParseArray( file, t, r, ++i, ParseImageString, &scene.textureLib );
		}
		else if ( jsoneq( file.data(), &t[ i ], "materials" ) == 0 )
		{
			if ( t[ i + 1 ].type != JSMN_ARRAY ) {
				continue;
			}
			ParseArray( file, t, r, ++i, ParseMaterialObject, &scene.materialLib );
		}
		else if ( jsoneq( file.data(), &t[ i ], "entities" ) == 0 )
		{
			if ( t[ i + 1 ].type != JSMN_ARRAY ) {
				continue;
			}
			ParseArray( file, t, r, ++i, ParseEntityObject, &scene.entities );
		}
	}
}
#endif

void MakeScene()
{
	const int piecesNum = 16;

	LoadScene();

	LoadMaterials( scene.materialLib );

	LoadImages( scene.textureLib );
	LoadModels( scene.modelLib );

	bool hasItems = scene.gpuPrograms.HasPendingLoads()	||
					scene.modelLib.HasPendingLoads()	||
					scene.textureLib.HasPendingLoads();
	while( hasItems )
	{
		scene.gpuPrograms.LoadAll();
		scene.textureLib.LoadAll();
		scene.modelLib.LoadAll();
		
		hasItems =	scene.gpuPrograms.HasPendingLoads()	||
					scene.modelLib.HasPendingLoads()	||
					scene.textureLib.HasPendingLoads();
	}

	const uint32_t entCount = static_cast<uint32_t>( scene.entities.size() );
	for ( uint32_t i = 0; i < entCount; ++i ) {		
		scene.CreateEntityBounds( scene.entities[ i ]->modelHdl, *scene.entities[ i ] );
	}

	gameConfig_t cfg;
	LoadConfig( "scenes/chessCfg/default_board.txt", cfg );
	chessEngine.Init( cfg );
	//board.SetEventCallback( &ProcessEvent );

	{
		Entity* ent = new Entity();
		scene.CreateEntityBounds( scene.modelLib.RetrieveHdl( "_skybox" ), *ent );
		ent->name = "_skybox";
		scene.entities.push_back( ent );
	}

	//{
	//	Entity* ent = new Entity();
	//	scene.CreateEntityBounds( scene.modelLib.RetrieveHdl( "chess_board" ), *ent );
	//	ent->name = "chess_board";
	//	scene.entities.push_back( ent );
	//}

	for ( int i = 0; i < 8; ++i )
	{
		for ( int j = 0; j < 8; ++j )
		{
			PieceEntity* squareEnt = new PieceEntity( GetFile( j ), GetRank( i ) );
			scene.CreateEntityBounds( scene.modelLib.RetrieveHdl( "plane" ), *squareEnt );
			squareEnt->SetOrigin( GetSquareCenterForLocation( squareEnt->file, squareEnt->rank ) + vec3f( 0.0f, 0.0f, 0.01f ) );
			squareEnt->SetFlag( ENT_FLAG_SELECTABLE );
			squareEnt->handle = -1;
			std::string name = "plane_";
			name += std::to_string( i ) + "_" + std::to_string( j );

			squareEnt->name = name.c_str();
			glowEntities.push_back( static_cast<uint32_t>( scene.entities.size() ) );
			scene.entities.push_back( squareEnt );

			pieceInfo_t pieceInfo = chessEngine.GetInfo( j, i );
			if ( pieceInfo.onBoard == false ) {
				continue;
			}
			PieceEntity* pieceEnt = new PieceEntity( GetFile( j ), GetRank( i ) );
			scene.CreateEntityBounds( scene.modelLib.RetrieveHdl( GetModelName( pieceInfo.piece ).c_str() ), *pieceEnt );
			pieceEnt->handle = chessEngine.FindPiece( pieceInfo.team, pieceInfo.piece, pieceInfo.instance );
			pieceEnt->SetFlag( ENT_FLAG_SELECTABLE );
			if ( pieceInfo.team == teamCode_t::WHITE ) {
				pieceEnt->materialHdl = scene.materialLib.RetrieveHdl( "White.001" );
			} else {
				pieceEnt->SetRotation( vec3f( 0.0f, 0.0f, 180.0f ) );
				pieceEnt->materialHdl = scene.materialLib.RetrieveHdl( "Chess_Black.001" );
			}
			pieceEnt->name = GetName( pieceInfo ).c_str();
			pieceEntities.push_back( static_cast<uint32_t>( scene.entities.size() ) );
			scene.entities.push_back( pieceEnt );
		}
	}

	const hdl_t cubeHdl = scene.modelLib.RetrieveHdl( "cube" );
	const uint32_t pieceCount = static_cast<uint32_t>( pieceEntities.size() );
	bool drawPieceWireframes = true;
	if( drawPieceWireframes )
	{
		for ( uint32_t i = 0; i < pieceCount; ++i )
		{
			BoundEntity* cubeEnt = new BoundEntity();
			scene.CreateEntityBounds( cubeHdl, *cubeEnt );
			cubeEnt->materialHdl = scene.materialLib.RetrieveHdl( "DEBUG_WIRE" );
			cubeEnt->SetFlag( ENT_FLAG_WIREFRAME );
			cubeEnt->name = ( scene.entities[ pieceEntities[ i ] ]->name + "_cube" ).c_str();
			cubeEnt->pieceId = pieceEntities[ i ];
			boundEntities.push_back( static_cast<uint32_t>( scene.entities.size() ) );
			scene.entities.push_back( cubeEnt );
		}
	}

	const hdl_t diamondHdl = scene.modelLib.RetrieveHdl( "diamond" );
	for ( int i = 0; i < MaxLights; ++i )
	{
		Entity* ent = new Entity();
		scene.CreateEntityBounds( diamondHdl, *ent );
		ent->materialHdl = scene.materialLib.RetrieveHdl( "DEBUG_WIRE" );
		ent->SetFlag( ENT_FLAG_WIREFRAME );
		ent->name = ( "light" + std::string( { (char)( (int)'0' + i ) } ) + "_dbg" ).c_str();
		scene.entities.push_back( ent );
	}

	{
		Entity* ent = new Entity();
		scene.CreateEntityBounds( scene.modelLib.RetrieveHdl( "_postProcessQuad" ), *ent );
		ent->name = "_postProcessQuad";
		scene.entities.push_back( ent );
	}

	{
		Entity* ent = new Entity();
		scene.CreateEntityBounds( scene.modelLib.RetrieveHdl( "_quadTexDebug" ), *ent );
		ent->name = "_quadTexDebug";
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

	Material& glowMat = scene.materialLib.Find( "GlowSquare" )->Get();
	glowMat.Kd = rgbTuplef_t( 0.1f, 0.1f, 1.0f );
	glowMat.d = 0.5f * cos( 3.0f * time ) + 0.5f;

	const uint32_t pieceBoundCount = static_cast<uint32_t>( boundEntities.size() );
	for ( uint32_t i = 0; i < pieceBoundCount; ++i )
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