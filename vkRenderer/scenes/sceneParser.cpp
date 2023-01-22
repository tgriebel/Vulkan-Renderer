#include "sceneParser.h"

#include <algorithm>
#include <string>

#include "../common.h"
#include "../render_util.h"
#include <scene/entity.h>
#include <scene/scene.h>
#include <resource_types/gpuProgram.h>
#include <resource_types/model.h>
#include <io/io.h>
#include <SysCore/jsmn.h>
#include "chessScene.h"

static jsmn_parser p;
static jsmntok_t t[ 1024 ];

static int jsoneq( const char* json, jsmntok_t* tok, const char* s ) {
	if ( tok->type == JSMN_STRING && (int)strlen( s ) == tok->end - tok->start &&
		strncmp( json + tok->start, s, tok->end - tok->start ) == 0 ) {
		return 0;
	}
	return -1;
}

struct parseState_t
{
	std::vector<char>*	file;
	jsmntok_t*			tokens;
	int					r;
	int					tx;
	Scene*				scene;
	AssetManager*		assets;
};

struct enumString_t
{
	const char* name;
	int32_t		value;
};

#define MAKE_ENUM_STRING( e ) enumString_t{ #e, e }

typedef int ParseObjectFunc( parseState_t& st, void* object );

struct objectTuple_t
{
	const char* name;
	void* ptr;
	ParseObjectFunc* func;
};


template<class T>
void ParseArray( parseState_t& st, ParseObjectFunc* readFunc, void* object );
void ParseObject( parseState_t& st, const objectTuple_t* objectMap, const uint32_t objectCount );


int ParseStringObject( parseState_t& st, void* value )
{
	std::string* s = reinterpret_cast<std::string*>( value );
	const uint32_t valueLen = ( st.tokens[ st.tx + 1 ].end - st.tokens[ st.tx + 1 ].start );
	*s = std::string( st.file->data() + st.tokens[ st.tx + 1 ].start, valueLen );
	st.tx += 2;
	return 0;
}


int ParseFloatObject( parseState_t& st, void* value )
{
	float* f = reinterpret_cast<float*>( value );
	const uint32_t valueLen = ( st.tokens[ st.tx + 1 ].end - st.tokens[ st.tx + 1 ].start );
	std::string s = std::string( st.file->data() + st.tokens[ st.tx + 1 ].start, valueLen );
	*f = std::stof( s );

	st.tx += 2;
	return 0;
}


int ParseBoolObject( parseState_t& st, void* value )
{
	bool* b = reinterpret_cast<bool*>( value );
	const uint32_t valueLen = ( st.tokens[ st.tx + 1 ].end - st.tokens[ st.tx + 1 ].start );
	std::string s = std::string( st.file->data() + st.tokens[ st.tx + 1 ].start, valueLen );

	std::transform( s.begin(), s.end(), s.begin(),
		[]( unsigned char c ) { return std::tolower( c ); } );

	*b = ( s == "true" || s == "1" ) ? true : false;

	st.tx += 2;
	return 0;
}


int ParseIntObject( parseState_t& st, void* value )
{
	int32_t* i = reinterpret_cast<int32_t*>( value );
	const uint32_t valueLen = ( st.tokens[ st.tx + 1 ].end - st.tokens[ st.tx + 1 ].start );
	std::string s = std::string( st.file->data() + st.tokens[ st.tx + 1 ].start, valueLen );

	*i = std::stoi( s );

	st.tx += 2;
	return 0;
}


int ParseUIntObject( parseState_t& st, void* value )
{
	uint32_t* u = reinterpret_cast<uint32_t*>( value );
	const uint32_t valueLen = ( st.tokens[ st.tx + 1 ].end - st.tokens[ st.tx + 1 ].start );
	std::string s = std::string( st.file->data() + st.tokens[ st.tx + 1 ].start, valueLen );

	*u = std::stoul( s );

	st.tx += 2;
	return 0;
}


int ParseImageObject( parseState_t& st, void* object )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return 0;
	}

	AssetLibImages& textureLib = st.assets->textureLib;

	std::string name;
	std::string type;

	const uint32_t objectCount = 2;
	static objectTuple_t objectMap[ objectCount ] =
	{
		{ "name", &name, &ParseStringObject },
		{ "type", &type, &ParseStringObject },
	};

	ParseObject( st, objectMap, objectCount );

	TextureLoader* loader = new TextureLoader();
	loader->SetBasePath( TexturePath );
	loader->SetTextureFile( name );
	loader->LoadAsCubemap( type == "CUBE" ? true : false );

	textureLib.AddDeferred( name.c_str(), Asset<Texture>::loadHandlerPtr_t( loader ) );

	return st.tx;
}


int ParseMaterialShaderObject( parseState_t& st, void* object )
{
	st.tx += 1;
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return 0;
	}

	Material* material = reinterpret_cast<Material*>( object );

	const uint32_t objectCount = 9;
	std::string s[ objectCount ];

	static const enumString_t enumMap[ objectCount ] =
	{
		MAKE_ENUM_STRING( DRAWPASS_SHADOW ),
		MAKE_ENUM_STRING( DRAWPASS_DEPTH ),
		MAKE_ENUM_STRING( DRAWPASS_OPAQUE ),
		MAKE_ENUM_STRING( DRAWPASS_TRANS ),
		MAKE_ENUM_STRING( DRAWPASS_TERRAIN ),
		MAKE_ENUM_STRING( DRAWPASS_DEBUG_WIREFRAME ),
		MAKE_ENUM_STRING( DRAWPASS_SKYBOX ),
		MAKE_ENUM_STRING( DRAWPASS_POST_2D ),
		MAKE_ENUM_STRING( DRAWPASS_DEBUG_SOLID ),
	};

	static const objectTuple_t objectMap[ objectCount ] =
	{
		{ enumMap[ 0 ].name, &s[ 0 ], &ParseStringObject },
		{ enumMap[ 1 ].name, &s[ 1 ], &ParseStringObject },
		{ enumMap[ 2 ].name, &s[ 2 ], &ParseStringObject },
		{ enumMap[ 3 ].name, &s[ 3 ], &ParseStringObject },
		{ enumMap[ 4 ].name, &s[ 4 ], &ParseStringObject },
		{ enumMap[ 5 ].name, &s[ 5 ], &ParseStringObject },
		{ enumMap[ 6 ].name, &s[ 6 ], &ParseStringObject },
		{ enumMap[ 7 ].name, &s[ 7 ], &ParseStringObject },
		{ enumMap[ 8 ].name, &s[ 8 ], &ParseStringObject },

	};
	static_assert( COUNTARRAY( objectMap ) == DRAWPASS_COUNT, "Size mismatch" );

	ParseObject( st, objectMap, objectCount );

	for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
	{
		if ( s[ mapIx ] == "" ) {
			continue;
		}
		material->AddShader( enumMap[ mapIx ].value, AssetLibGpuProgram::Handle( s[ mapIx ].c_str() ) );
	}
	return 0;
}


int ParseMaterialTextureObject( parseState_t& st, void* object )
{
	st.tx += 1;
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return 0;
	}

	Material* material = reinterpret_cast<Material*>( object );

	const uint32_t objectCount = 12;
	std::string s[ objectCount ];

	static const enumString_t enumMap[ objectCount ] =
	{
		MAKE_ENUM_STRING( GGX_COLOR_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_NORMAL_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_SPEC_MAP_SLOT ),
		MAKE_ENUM_STRING( HGT_COLOR_MAP_SLOT0 ),
		MAKE_ENUM_STRING( HGT_COLOR_MAP_SLOT1 ),
		MAKE_ENUM_STRING( HGT_HEIGHT_MAP_SLOT ),
		MAKE_ENUM_STRING( CUBE_RIGHT_SLOT ),
		MAKE_ENUM_STRING( CUBE_LEFT_SLOT ),
		MAKE_ENUM_STRING( CUBE_TOP_SLOT ),
		MAKE_ENUM_STRING( CUBE_BOTTOM_SLOT ),
		MAKE_ENUM_STRING( CUBE_FRONT_SLOT ),
		MAKE_ENUM_STRING( CUBE_BACK_SLOT ),
	};

	static const objectTuple_t objectMap[ objectCount ] =
	{
		{ enumMap[ 0 ].name, &s[ 0 ], &ParseStringObject },
		{ enumMap[ 1 ].name, &s[ 1 ],&ParseStringObject },
		{ enumMap[ 2 ].name, &s[ 2 ], &ParseStringObject },
		{ enumMap[ 3 ].name, &s[ 3 ], &ParseStringObject },
		{ enumMap[ 4 ].name, &s[ 4 ], &ParseStringObject },
		{ enumMap[ 5 ].name, &s[ 5 ], &ParseStringObject },
		{ enumMap[ 6 ].name, &s[ 6 ], &ParseStringObject },
		{ enumMap[ 7 ].name, &s[ 7 ], &ParseStringObject },
		{ enumMap[ 8 ].name, &s[ 8 ], &ParseStringObject },
		{ enumMap[ 9 ].name, &s[ 9 ], &ParseStringObject },
		{ enumMap[ 10 ].name, &s[ 10 ], &ParseStringObject },
		{ enumMap[ 11 ].name, &s[ 11 ] ,&ParseStringObject },
	};

	ParseObject( st, objectMap, objectCount );

	for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
	{
		if ( s[ mapIx ] == "" ) {
			continue;
		}
		material->AddTexture( enumMap[ mapIx ].value, AssetLibImages::Handle( s[ mapIx ].c_str() ) );
	}
	return 0;
}


int ParseModelObject( parseState_t& st, void* object )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	struct modelObjectData_t
	{
		std::string name;
		std::string modelName;
	};

	modelObjectData_t od;

	const uint32_t objectCount = 2;
	static const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name", &od.name, &ParseStringObject },
		{ "model", &od.modelName, &ParseStringObject }
	};

	ParseObject( st, objectMap, objectCount );

	ModelLoader* loader = new ModelLoader();
	loader->SetModelPath( ModelPath );
	loader->SetTexturePath( TexturePath );
	loader->SetModelName( od.modelName );
	loader->SetAssetRef( st.assets );
	st.assets->modelLib.AddDeferred( od.name.c_str(), loader_t( loader ) );

	return st.tx;
}


int ParseEntityObject( parseState_t& st, void* object )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	std::string name;
	std::string modelName;
	std::string materialName;
	std::vector<Entity*>& entities = st.scene->entities;

	float x = 0.0f, y = 0.0f, z = 0.0f;
	float rx = 0.0f, ry = 0.0f, rz = 0.0f;
	float sx = 1.0f, sy = 1.0f, sz = 1.0f;
	bool hidden = false, wireframe = false;

	const uint32_t objectCount = 14;
	static const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name", &name, &ParseStringObject },
		{ "model", &modelName, &ParseStringObject },
		{ "material", &materialName, &ParseStringObject },
		{ "x", &x, &ParseFloatObject },
		{ "y", &y, &ParseFloatObject },
		{ "z", &z, &ParseFloatObject },
		{ "rx", &rx, &ParseFloatObject },
		{ "ry", &ry, &ParseFloatObject },
		{ "rz", &rz, &ParseFloatObject },
		{ "sx", &sx, &ParseFloatObject },
		{ "sy", &sy, &ParseFloatObject },
		{ "sz", &sz, &ParseFloatObject },
		{ "hidden", &hidden, &ParseBoolObject },
		{ "wireframe", &wireframe, &ParseBoolObject },
	};

	ParseObject( st, objectMap, objectCount );

	Entity* ent = new Entity();
	ent->name = name;
	if ( modelName == "_skybox" ) {
		ent->modelHdl = st.assets->modelLib.AddDeferred( modelName.c_str(), loader_t( new SkyBoxLoader() ) );
	}
	else {
		ModelLoader* loader = new ModelLoader();
		loader->SetModelPath( ModelPath );
		loader->SetTexturePath( TexturePath );
		loader->SetModelName( modelName );
		loader->SetAssetRef( st.assets );
		ent->modelHdl = st.assets->modelLib.AddDeferred( modelName.c_str(), loader_t( loader ) );
	}
	ent->SetOrigin( vec3f( x, y, z ) );
	ent->SetRotation( vec3f( rx, ry, rz ) );
	ent->SetScale( vec3f( sx, sy, sz ) );
	if ( materialName != "" ) {
		ent->materialHdl = AssetLibMaterials::Handle( materialName.c_str() );
	}
	if ( hidden ) {
		ent->SetFlag( ENT_FLAG_NO_DRAW );
	}
	if ( wireframe ) {
		ent->SetFlag( ENT_FLAG_WIREFRAME );
	}
	st.scene->entities.push_back( ent );

	return st.tx;
}


int ParseMaterialObject( parseState_t& st, void* object )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	AssetLibMaterials* materials = reinterpret_cast<AssetLibMaterials*>( object );

	Material m;
	std::string name;

	const uint32_t objectCount = 8;
	static const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name", &name, &ParseStringObject },
		{ "tr", &m.Tr, &ParseFloatObject },
		{ "ns", &m.Ns, &ParseFloatObject },
		{ "ni", &m.Ni, &ParseFloatObject },
		{ "d", &m.d, &ParseFloatObject },
		{ "illum", &m.illum, &ParseFloatObject },
		{ "shaders", &m, &ParseMaterialShaderObject },
		{ "textures", &m, &ParseMaterialTextureObject },
	};

	ParseObject( st, objectMap, objectCount );

	materials->Add( name.c_str(), m );

	return st.tx;
}


int ParseShaderObject( parseState_t& st, void* object )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	std::string name;
	std::string vsShader;
	std::string psShader;
	AssetLibGpuProgram* shaders = reinterpret_cast<AssetLibGpuProgram*>( object );

	const uint32_t objectCount = 3;
	static const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name", &name, &ParseStringObject },
		{ "vs", &vsShader, &ParseStringObject },
		{ "ps", &psShader, &ParseStringObject },
	};

	ParseObject( st, objectMap, objectCount );

	GpuProgramLoader* loader = new GpuProgramLoader();
	loader->SetBasePath( "shaders_bin/" );
	loader->AddRasterPath( vsShader, psShader );
	shaders->AddDeferred( name.c_str(), Asset<GpuProgram>::loadHandlerPtr_t( loader ) );

	return st.tx;
}


void ParseObject( parseState_t& st, const objectTuple_t* objectMap, const uint32_t objectCount )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return;
	}

	int itemsFound = 0;
	int itemsCount = st.tokens[ st.tx ].size;

	st.tx += 1;

	while ( ( itemsFound < itemsCount ) && ( st.tx < st.r ) )
	{
		for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
		{
			if ( jsoneq( st.file->data(), &st.tokens[ st.tx ], objectMap[ mapIx ].name ) != 0 ) {
				continue;
			}
			( *objectMap[ mapIx ].func )( st, objectMap[ mapIx ].ptr );
			++itemsFound;
			break;
		}
	}
}


void ParseArray( parseState_t& st, ParseObjectFunc* readFunc, void* object )
{
	if ( st.tokens[ st.tx ].type != JSMN_ARRAY ) {
		return;
	}
	int itemsFound = 0;
	int itemsCount = st.tokens[ st.tx ].size;

	st.tx += 1;

	while ( ( itemsFound < itemsCount ) && ( st.tx < st.r ) )
	{
		int ret = ( *readFunc )( st, object );
		if ( ret > 0 )
		{
			st.tx = ret;
			++itemsFound;
		}
	}

	st.tx -= 1;
}


void LoadScene( std::string fileName, Scene** scene, AssetManager* assets )
{
	std::vector<char> file = ReadFile( fileName );

	jsmn_init( &p );
	int r = jsmn_parse( &p, file.data(), static_cast<uint32_t>( file.size() ), t,
		sizeof( t ) / sizeof( t[ 0 ] ) );
	if ( r < 0 ) {
		std::cout << "Failed to parse JSON" << std::endl;
	}

	if ( r < 1 || t[ 0 ].type != JSMN_OBJECT ) {
		std::cout << "Object expected" << std::endl;
	}

	for ( int i = 1; i < r; ++i ) {
		if ( jsoneq( file.data(), &t[ i ], "scene" ) == 0 )
		{
			const uint32_t valueLen = ( t[ i + 1 ].end - t[ i + 1 ].start );
			std::string s = std::string( file.data() + t[ i + 1 ].start, valueLen );

			if ( s == "chess" ) {
				*scene = new ChessScene();
			} else {
				*scene = new Scene();
			}
			break;
		}
	}

	parseState_t st;
	st.file = &file;
	st.r = r;
	st.tokens = t;
	st.tx = 1;
	st.assets = assets;
	st.scene = *scene;

	for ( ; st.tx < st.r; ++st.tx ) {
		if ( jsoneq( st.file->data(), &st.tokens[ st.tx ], "shaders" ) == 0 )
		{
			if ( st.tokens[ st.tx + 1 ].type != JSMN_ARRAY ) {
				continue;
			}
			st.tx += 1;
			ParseArray( st, ParseShaderObject, &st.assets->gpuPrograms );
		}
		else if ( jsoneq( st.file->data(), &st.tokens[ st.tx ], "images" ) == 0 )
		{
			if ( st.tokens[ st.tx + 1 ].type != JSMN_ARRAY ) {
				continue;
			}
			st.tx += 1;
			ParseArray( st, ParseImageObject, &st.assets->textureLib );
		}
		else if ( jsoneq( st.file->data(), &st.tokens[ st.tx ], "materials" ) == 0 )
		{
			if ( st.tokens[ st.tx + 1 ].type != JSMN_ARRAY ) {
				continue;
			}
			st.tx += 1;
			ParseArray( st, ParseMaterialObject, &st.assets->materialLib );
		}
		else if ( jsoneq( st.file->data(), &st.tokens[ st.tx ], "models" ) == 0 )
		{
			if ( st.tokens[ st.tx + 1 ].type != JSMN_ARRAY ) {
				continue;
			}
			st.tx += 1;
			ParseArray( st, ParseModelObject, &st.assets->modelLib );
		}
		else if ( jsoneq( st.file->data(), &st.tokens[ st.tx ], "entities" ) == 0 )
		{
			if ( st.tokens[ st.tx + 1 ].type != JSMN_ARRAY ) {
				continue;
			}
			st.tx += 1;
			ParseArray( st, ParseEntityObject, &st.scene->entities );
		}
	}

	bool hasItems = true;
	do
	{
		assets->gpuPrograms.LoadAll();
		assets->textureLib.LoadAll();
		assets->modelLib.LoadAll();

		hasItems =	assets->gpuPrograms.HasPendingLoads()	||
					assets->modelLib.HasPendingLoads()		||
					assets->textureLib.HasPendingLoads();
	} while ( hasItems );
}