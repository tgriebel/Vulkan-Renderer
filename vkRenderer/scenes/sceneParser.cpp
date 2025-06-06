/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "sceneParser.h"

#include <algorithm>
#include <string>

#include "../src/globals/common.h"
#include "../src/globals/render_util.h"
#include "../src/render_core/gpuImage.h"
#include <gfxcore/scene/entity.h>
#include <gfxcore/scene/scene.h>
#include <gfxcore/asset_types/gpuProgram.h>
#include <gfxcore/asset_types/model.h>
#include <gfxcore/io/io.h>

//#define JSMN_PARENT_LINKS
#include <SysCore/jsmn.h>
#include "chessScene.h"
#include "nesScene.h"

static const int TOKEN_LEN = 128;

static uint8_t trashBuffer[ 8192 ];

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
	jsmn_parser*		p;
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


void ParseArray( parseState_t& st, ParseObjectFunc* readFunc, void* object );
void ParseObject( parseState_t& st, const objectTuple_t* objectMap, const uint32_t objectCount );


void InitSceneType( const std::string type, Scene** scene )
{
	if ( type == "chess" ) {
		*scene = new ChessScene();
	}
	else if ( type == "nes" ) {
		*scene = new NesScene();
	}
	else {
		*scene = new Scene();
	}
}


std::string ParseToken( parseState_t& st )
{
	const uint32_t valueLen = ( st.tokens[ st.tx ].end - st.tokens[ st.tx ].start );
	return std::string( st.file->data() + st.tokens[ st.tx ].start, valueLen );
}


int ParseStringObject( parseState_t& st, void* value )
{
	char* s = reinterpret_cast<char*>( value );
	const uint32_t valueLen = ( st.tokens[ st.tx + 1 ].end - st.tokens[ st.tx + 1 ].start );

	assert( valueLen > 0 );
	assert( valueLen < TOKEN_LEN );
	memset( s, '\0', TOKEN_LEN );
	memcpy( s, st.file->data() + st.tokens[ st.tx + 1 ].start, valueLen );
	s[valueLen] = '\0';

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

template <uint32_t BIT>
int ParseFlagObject( parseState_t& st, void* value )
{
	uint32_t* f = reinterpret_cast<uint32_t*>( value );
	const uint32_t valueLen = ( st.tokens[ st.tx + 1 ].end - st.tokens[ st.tx + 1 ].start );
	std::string s = std::string( st.file->data() + st.tokens[ st.tx + 1 ].start, valueLen );

	std::transform( s.begin(), s.end(), s.begin(),
		[]( unsigned char c ) { return std::tolower( c ); } );

	*f |= ( s == "true" || s == "1" ) ? BIT : false;

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

	char name[TOKEN_LEN] = "";
	char type[TOKEN_LEN] = "";
	char uvAddress[TOKEN_LEN] = "";
	char uvFilter[TOKEN_LEN] = "";

	const uint32_t uvAddressEnumCount = 3;
	static const enumString_t enumUvAddressMap[ uvAddressEnumCount ] =
	{
		MAKE_ENUM_STRING( SAMPLER_ADDRESS_WRAP ),
		MAKE_ENUM_STRING( SAMPLER_ADDRESS_CLAMP_EDGE ),
		MAKE_ENUM_STRING( SAMPLER_ADDRESS_CLAMP_BORDER ),
	};

	const uint32_t uvFilterEnumCount = 3;
	static const enumString_t enumUvFilterMap[ uvFilterEnumCount ] =
	{
		MAKE_ENUM_STRING( SAMPLER_FILTER_NEAREST ),
		MAKE_ENUM_STRING( SAMPLER_FILTER_BILINEAR ),
		MAKE_ENUM_STRING( SAMPLER_FILTER_TRILINEAR ),
	};

	const uint32_t objectCount = 4;
	const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name", &name, &ParseStringObject },
		{ "type", &type, &ParseStringObject },
		{ "uvAddress", &uvAddress, &ParseStringObject },
		{ "uvFilter", &uvFilter, &ParseStringObject },
	};

	ParseObject( st, objectMap, objectCount );

	samplerState_t samplerState;
	samplerState.addrMode = SAMPLER_ADDRESS_WRAP;
	samplerState.filter = SAMPLER_FILTER_BILINEAR;

	for( uint32_t i = 0; i < uvAddressEnumCount; ++i )
	{
		if( strcmp( uvAddress, enumUvAddressMap[ i ].name ) == 0 )
		{
			samplerState.addrMode = (samplerAddress_t)enumUvAddressMap[ i ].value;
			break;
		}
	}

	for ( uint32_t i = 0; i < uvFilterEnumCount; ++i )
	{
		if ( strcmp( uvFilter, enumUvFilterMap[ i ].name ) == 0 )
		{
			samplerState.filter = (samplerFilter_t)enumUvFilterMap[ i ].value;
			break;
		}
	}

	ImageLoader* loader = new ImageLoader();
	loader->SetBasePath( TexturePath );
	loader->SetTextureFile( name );
	loader->SetSampler( samplerState );
	loader->LoadAsCubemap( strcmp( type, "CUBE" ) == 0 ? true : false );

	textureLib.AddDeferred( name, Asset<Image>::loadHandlerPtr_t( loader ) );

	return st.tx;
}


int ParseMaterialShaderObject( parseState_t& st, void* object )
{
	st.tx += 1;
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return 0;
	}

	Material* material = reinterpret_cast<Material*>( object );

	const uint32_t objectCount = 10;
	char s[ objectCount ][TOKEN_LEN] = {};

	static const enumString_t enumMap[ objectCount ] =
	{
		MAKE_ENUM_STRING( DRAWPASS_SHADOW ),
		MAKE_ENUM_STRING( DRAWPASS_DEPTH ),
		MAKE_ENUM_STRING( DRAWPASS_OPAQUE ),
		MAKE_ENUM_STRING( DRAWPASS_TRANS ),
		MAKE_ENUM_STRING( DRAWPASS_EMISSIVE ),
		MAKE_ENUM_STRING( DRAWPASS_TERRAIN ),
		MAKE_ENUM_STRING( DRAWPASS_DEBUG_WIREFRAME ),
		MAKE_ENUM_STRING( DRAWPASS_SKYBOX ),
		MAKE_ENUM_STRING( DRAWPASS_2D ),
		MAKE_ENUM_STRING( DRAWPASS_DEBUG_3D ),
	};

	const objectTuple_t objectMap[ objectCount ] =
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
		{ enumMap[ 9 ].name, &s[ 9 ], &ParseStringObject },

	};

	ParseObject( st, objectMap, objectCount );

	for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
	{
		if ( s[ mapIx ][0] == '\0' ) {
			continue;
		}
		material->AddShader( drawPass_t( enumMap[ mapIx ].value ), AssetLibGpuProgram::Handle( s[ mapIx ] ) );
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

	const uint32_t objectCount = 20;
	char s[ objectCount ][TOKEN_LEN] = {};

	static const enumString_t enumMap[ objectCount ] =
	{
		MAKE_ENUM_STRING( GGX_COLOR_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_NORMAL_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_SPEC_MAP_SLOT ),
		MAKE_ENUM_STRING( HGT_COLOR_MAP_SLOT0 ),
		MAKE_ENUM_STRING( HGT_COLOR_MAP_SLOT1 ),
		MAKE_ENUM_STRING( HGT_HEIGHT_MAP_SLOT ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_0 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_1 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_2 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_3 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_4 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_5 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_6 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_7 ),
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
		{ enumMap[ 12 ].name, &s[ 12 ] ,&ParseStringObject },
		{ enumMap[ 13 ].name, &s[ 13 ] ,&ParseStringObject },
		{ enumMap[ 14 ].name, &s[ 14 ] ,&ParseStringObject },
		{ enumMap[ 15 ].name, &s[ 15 ] ,&ParseStringObject },
		{ enumMap[ 16 ].name, &s[ 16 ] ,&ParseStringObject },
		{ enumMap[ 17 ].name, &s[ 17 ] ,&ParseStringObject },
		{ enumMap[ 18 ].name, &s[ 18 ] ,&ParseStringObject },
		{ enumMap[ 19 ].name, &s[ 19 ] ,&ParseStringObject },
	};

	ParseObject( st, objectMap, objectCount );

	for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
	{
		if ( s[ mapIx ][0] == '\0' ) {
			continue;
		}
		material->AddTexture( enumMap[ mapIx ].value, AssetLibImages::Handle( s[ mapIx ] ) );
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
		char		name[ TOKEN_LEN ];
		char		modelName[ TOKEN_LEN ];
		char		s[3][ TOKEN_LEN ];
		float		f[3];
		int			i[3];
	};

	modelObjectData_t od;

	const uint32_t objectCount = 11;
	const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name", &od.name, &ParseStringObject },
		{ "model", &od.modelName, &ParseStringObject },
		{ "argStr0", &od.s[0], &ParseStringObject },
		{ "argStr1", &od.s[1], &ParseStringObject },
		{ "argStr2", &od.s[2], &ParseStringObject },
		{ "argInt0", &od.i[0], &ParseIntObject },
		{ "argInt1", &od.i[1], &ParseIntObject },
		{ "argInt2", &od.i[2], &ParseIntObject },
		{ "argFlt0", &od.f[0], &ParseFloatObject },
		{ "argFlt1", &od.f[1], &ParseFloatObject },
		{ "argFlt2", &od.f[2], &ParseFloatObject },
	};

	ParseObject( st, objectMap, objectCount );

	if ( strcmp( od.modelName, "_skybox" ) == 0 ) {
		st.assets->modelLib.AddDeferred( od.name, loader_t( new SkyBoxLoader() ) );
	}
	else if ( strcmp( od.modelName, "_terrain" ) == 0 )
	{
		hdl_t handle = AssetLibMaterials::Handle( od.s[0] );
		st.assets->modelLib.AddDeferred( od.name, loader_t( new TerrainLoader( od.i[0], od.i[1], od.f[0], od.f[1], handle ) ) );
	}
	else 
	{
		ModelLoader* loader = new ModelLoader();
		loader->SetModelPath( ModelPath );
		loader->SetTexturePath( TexturePath );
		loader->SetModelName( od.modelName );
		loader->SetAssetRef( st.assets );
		st.assets->modelLib.AddDeferred( od.name, loader_t( loader ) );
	}
	return st.tx;
}


int ParseEntityObject( parseState_t& st, void* object )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	char name[ TOKEN_LEN ] = "";
	char modelName[ TOKEN_LEN ] = "";
	char materialName[ TOKEN_LEN ] = "";
	std::vector<Entity*>& entities = st.scene->entities;

	float x = 0.0f, y = 0.0f, z = 0.0f;
	float rx = 0.0f, ry = 0.0f, rz = 0.0f;
	float sx = 1.0f, sy = 1.0f, sz = 1.0f;
	bool hidden = false, wireframe = false;

	const uint32_t objectCount = 14;
	const objectTuple_t objectMap[ objectCount ] =
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
	ent->modelHdl = AssetLibModels::Handle( modelName );
	
	if ( strcmp( modelName, "_skybox" ) == 0 ) {
		ent->modelHdl = st.assets->modelLib.AddDeferred( modelName, loader_t( new SkyBoxLoader() ) );
	}
	else if( st.assets->modelLib.Find( ent->modelHdl ) == st.assets->modelLib.GetDefault() )
	{
		ModelLoader* loader = new ModelLoader();
		loader->SetModelPath( ModelPath );
		loader->SetTexturePath( TexturePath );
		loader->SetModelName( modelName );
		loader->SetAssetRef( st.assets );
		ent->modelHdl = st.assets->modelLib.AddDeferred( modelName, loader_t( loader ) );
	}
	ent->SetOrigin( vec3f( x, y, z ) );
	ent->SetRotation( vec3f( rx, ry, rz ) );
	ent->SetScale( vec3f( sx, sy, sz ) );
	if ( strcmp( materialName, "" ) != 0 ) {
		ent->materialHdl = AssetLibMaterials::Handle( materialName );
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
	materialParms_t mParms;
	char name[ TOKEN_LEN ] = "";

	// TODO: allow parse functions that aren't just primitives. Need objects/array reading
	const uint32_t objectCount = 23;
	const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name", &name, &ParseStringObject },
		{ "KaR", &mParms.Ka.r, &ParseFloatObject },
		{ "KaG", &mParms.Ka.g, &ParseFloatObject },
		{ "KaB", &mParms.Ka.b, &ParseFloatObject },
		{ "KeR", &mParms.Ke.r, &ParseFloatObject },
		{ "KeG", &mParms.Ke.g, &ParseFloatObject },
		{ "KeB", &mParms.Ke.b, &ParseFloatObject },
		{ "KdR", &mParms.Kd.r, &ParseFloatObject },
		{ "KdG", &mParms.Kd.g, &ParseFloatObject },
		{ "KdB", &mParms.Kd.b, &ParseFloatObject },
		{ "KsR", &mParms.Ks.r, &ParseFloatObject },
		{ "KsG", &mParms.Ks.g, &ParseFloatObject },
		{ "KsB", &mParms.Ks.b, &ParseFloatObject },
		{ "TfR", &mParms.Tf.r, &ParseFloatObject },
		{ "TfG", &mParms.Tf.g, &ParseFloatObject },
		{ "TfB", &mParms.Tf.b, &ParseFloatObject },
		{ "tr", &mParms.Tr, &ParseFloatObject },
		{ "ns", &mParms.Ns, &ParseFloatObject },
		{ "ni", &mParms.Ni, &ParseFloatObject },
		{ "d", &mParms.d, &ParseFloatObject },
		{ "illum", &mParms.illum, &ParseFloatObject },
		{ "shaders", &m, &ParseMaterialShaderObject },
		{ "textures", &m, &ParseMaterialTextureObject },
	};

	ParseObject( st, objectMap, objectCount );

	m.SetParms( mParms );

	materials->Add( name, m );

	return st.tx;
}


int ParseShaderObject( parseState_t& st, void* object )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	char name[ TOKEN_LEN ] = "";
	char vsShader[ TOKEN_LEN ] = "";
	char psShader[ TOKEN_LEN ] = "";
	char csShader[ TOKEN_LEN ] = "";
	char bindSet[ TOKEN_LEN ] = "";
	char perm[ TOKEN_LEN ] = "";
	shaderFlags_t shaderFlags = shaderFlags_t::NONE;
	AssetLibGpuProgram* shaders = reinterpret_cast<AssetLibGpuProgram*>( object );

	const uint32_t objectCount = 8;
	const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name", reinterpret_cast<void*>( name ), &ParseStringObject },
		{ "vs", reinterpret_cast<void*>( vsShader ), &ParseStringObject },
		{ "ps", reinterpret_cast<void*>( psShader ), &ParseStringObject },
		{ "cs", reinterpret_cast<void*>( csShader ), &ParseStringObject },
		{ "bindset", reinterpret_cast<void*>( &bindSet ), &ParseStringObject },
		{ "perm", reinterpret_cast<void*>( &perm ), &ParseStringObject },
		{ "sampling_ms", reinterpret_cast<void*>( &shaderFlags ), &ParseFlagObject<(uint32_t)shaderFlags_t::USE_MSAA> },
		{ "image_shader", reinterpret_cast<void*>( &shaderFlags ), &ParseFlagObject<(uint32_t)shaderFlags_t::IMAGE_SHADER> },
	};

	ParseObject( st, objectMap, objectCount );

	GpuProgramLoader* loader = new GpuProgramLoader();
	loader->SetSourcePath( "shaders/" );
	loader->SetBinPath( "shaders_bin/" );
	loader->AddFilePaths( vsShader, psShader, csShader );
	loader->SetBindSet( bindSet );
	loader->SetPerm( perm );
	loader->SetFlags( shaderFlags );
	shaders->AddDeferred( name, Asset<GpuProgram>::loadHandlerPtr_t( loader ) );

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
		bool found = false;
		for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
		{
			if ( jsoneq( st.file->data(), &st.tokens[ st.tx ], objectMap[ mapIx ].name ) != 0 ) {
				continue;
			}

			if ( st.tokens[ st.tx + 1 ].type == JSMN_ARRAY )
			{
				st.tx += 1;
				ParseArray( st, objectMap[ mapIx ].func, objectMap[ mapIx ].ptr );
			}
			else
			{
				( *objectMap[ mapIx ].func )( st, objectMap[ mapIx ].ptr );
			}

			++itemsFound;
			found = true;
			break;
		}

		if( found == false )
		{
			const std::string s = ParseToken( st );
			std::cout << "No matching parse function for '" << s << ";" << std::endl;
			st.tx += 1;
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

	std::string arrayName = ParseToken( st );

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
}


void ParseJson( const std::string& fileName, Scene** scene, AssetManager* assets, const bool isRoot )
{
	assert( assets != nullptr );
	assert( scene != nullptr );

	std::vector<char> file = ReadTextFile( fileName );

	const int maxTokens = 1024;

	parseState_t st;
	st.file = &file;
	st.p = new jsmn_parser;
	st.tokens = new jsmntok_t[ maxTokens ];
	st.tx = 1;
	st.assets = assets;

	jsmn_init( st.p );
	st.r = jsmn_parse( st.p, st.file->data(), static_cast<uint32_t>( st.file->size() ), st.tokens, maxTokens );
	if ( st.r < 0 ) {
		std::cout << "Failed to parse JSON" << std::endl;
	}

	if ( st.r < 1 || st.tokens[ 0 ].type != JSMN_OBJECT ) {
		std::cout << "Object expected" << std::endl;
	}

	if( isRoot )
	{
		while( st.tx < st.r )
		{
			const int items = st.tokens[ st.tx ].size;
			if ( jsoneq( file.data(), &st.tokens[ st.tx ], "sceneClass" ) == 0 )
			{
				st.tx += 1;
				InitSceneType( ParseToken( st ), scene );
			}
			else if ( jsoneq( file.data(), &st.tokens[ st.tx ], "reflink" ) == 0 )
			{
				st.tx += 1;
				ParseJson( ParseToken( st ), scene, assets, false );
			} else {
				st.tx += 1;
			}
			st.tx += items;
		}
	}

	st.tx = 0;
	st.scene = *scene;

	assert( COUNTARRAY( trashBuffer ) >= file.size() );

	const uint32_t objectCount = 8;
	const objectTuple_t objectMap[ objectCount ] =
	{
		{ "sceneClass", &trashBuffer, &ParseStringObject },
		{ "type", &trashBuffer, &ParseStringObject },
		{ "reflink", &trashBuffer, &ParseStringObject },
		{ "shaders", &st.assets->gpuPrograms, &ParseShaderObject },
		{ "images", &st.assets->textureLib, &ParseImageObject },
		{ "materials", &st.assets->materialLib, &ParseMaterialObject },
		{ "models", &st.assets->modelLib, &ParseModelObject },
		{ "entities", &st.scene->entities, &ParseEntityObject },
	};

	ParseObject( st, objectMap, objectCount );

	delete st.p;
	delete[] st.tokens;
}


void LoadScene( std::string fileName, Scene** scene, AssetManager* assets )
{
	{
		SCOPED_TIMER_PRINT( ParseScene );
		ParseJson( fileName, scene, assets, true );
	}

	{
		SCOPED_TIMER_PRINT( LoadAssets );
		g_assets.RunLoadLoop();
	}
}