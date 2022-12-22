#include "common.h"
#include "deviceContext.h"
#include "io.h"
#include "GeoBuilder.h"
#include "assetLib.h"
#include "scene.h"

extern Scene						scene;

#define STB_IMAGE_IMPLEMENTATION // includes func defs
#include "stb_image.h"

mat4x4f MatrixFromVector( const vec3f& v )
{
	vec3f up = vec3f( 0.0f, 0.0f, 1.0f );
	const vec3f u = v.Normalize();
	if ( Dot( v, up ) > 0.99999f ) {
		up = vec3f( 0.0f, 1.0f, 0.0f );
	}
	const vec3f left = Cross( u, up ).Reverse();

	const float values[ 16 ] = {	up[ 0 ], up[ 1 ], up[ 2 ], 0.0f,
									left[ 0 ], left[ 1 ], left[ 2 ], 0.0f,
									v[ 0 ], v[ 1 ], v[ 2 ], 0.0f,
									0.0f, 0.0f, 0.0f, 1.0f };

	return mat4x4f( values );
}

bool LoadTextureImage( const char * texturePath, texture_t& texture )
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load( texturePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha );

	if ( !pixels ) {
		stbi_image_free( pixels );
		return false;
	}

	texture.info.width		= texWidth;
	texture.info.height		= texHeight;
	texture.info.channels	= texChannels;
	texture.info.layers		= 1;
	texture.info.type		= TEXTURE_TYPE_2D;
	texture.uploadId		= -1;
	texture.info.mipLevels	= static_cast<uint32_t>( std::floor( std::log2( std::max( texture.info.width, texture.info.height ) ) ) ) + 1;
	texture.sizeBytes		= ( texWidth * texHeight * 4 );
	texture.bytes = new uint8_t[ texture.sizeBytes ];
	memcpy( texture.bytes, pixels, texture.sizeBytes );
	stbi_image_free( pixels );
	return true;
}

bool LoadTextureCubeMapImage( const char* textureBasePath, const char* ext, texture_t& texture )
{
	std::string paths[ 6 ] = {
		( std::string( textureBasePath ) + "_right." + ext ),
		( std::string( textureBasePath ) + "_left." + ext ),			
		( std::string( textureBasePath ) + "_top." + ext ),
		( std::string( textureBasePath ) + "_bottom." + ext ),
		( std::string( textureBasePath ) + "_front." + ext ),
		( std::string( textureBasePath ) + "_back." + ext ),
	};

	int sizeBytes = 0;
	texture_t textures2D[ 6 ];
	for ( int i = 0; i < 6; ++i )
	{
		if ( LoadTextureImage( paths[ i ].c_str(), textures2D[ i ] ) == false ) {
			sizeBytes = 0;
			break;
		}
		assert( textures2D[ i ].sizeBytes > 0 );
		sizeBytes += textures2D[ i ].sizeBytes;
	}

	if ( sizeBytes == 0 ) {
		for ( int i = 0; i < 6; ++i ) {
			if ( textures2D[ i ].bytes == nullptr ) {
				delete[] textures2D[ i ].bytes;
			}
		}
		return false;
	}

	const int texWidth = textures2D[ 0 ].info.width;
	const int texHeight = textures2D[ 0 ].info.height;
	const int texChannels = textures2D[ 0 ].info.channels;

	uint8_t* bytes = new uint8_t[ sizeBytes ];

	int byteOffset = 0;
	for ( int i = 0; i < 6; ++i )
	{
		if( ( texWidth != textures2D[ i ].info.width ) ||
			( texHeight != textures2D[ i ].info.height ) ||
			( texChannels != textures2D[ i ].info.channels ) )
		{
			if ( bytes != nullptr ) {
				delete[] bytes;
			}
			for ( int j = 0; j < 6; ++j ) {
				if ( textures2D[ j ].bytes == nullptr ) {
					delete[] textures2D[ j ].bytes;
				}
			}
			return false;
		}
		
		memcpy( bytes + byteOffset, textures2D[ i ].bytes, textures2D[ i ].sizeBytes );
		byteOffset += textures2D[ i ].sizeBytes;
	}

	assert( sizeBytes == byteOffset );
	texture.info.width		= texWidth;
	texture.info.height		= texHeight;
	texture.info.channels	= texChannels;
	texture.info.layers		= 6;
	texture.info.type		= TEXTURE_TYPE_CUBE;
	texture.uploadId		= -1;
	texture.info.mipLevels	= static_cast<uint32_t>( std::floor( std::log2( std::max( texture.info.width, texture.info.height ) ) ) ) + 1;
	texture.bytes			= bytes;
	texture.sizeBytes		= sizeBytes;
	return true;
}

void CreateShaders( GpuProgram& prog )
{

}

void CopyGeoBuilderResult( const GeoBuilder& gb, std::vector<vertex_t>& vb, std::vector<uint32_t>& ib )
{
	vb.reserve( gb.vb.size() );
	for ( const GeoBuilder::vertex_t& v : gb.vb )
	{
		vertex_t vert;
		vert.pos = vec4f( v.pos[ 0 ], v.pos[ 1 ], v.pos[ 2 ], 0.0f );
		vert.color = Color( v.color[ 0 ], v.color[ 1 ], v.color[ 2 ], v.color[ 3 ] );
		vert.normal = vec3f( v.normal[ 0 ], v.normal[ 1 ], v.normal[ 2 ] );
		vert.uv = vec2f( v.texCoord[ 0 ], v.texCoord[ 1 ] );
		vert.uv2 = vec2f( 0.0f, 0.0f );

		vb.push_back( vert );
	}

	ib.reserve( gb.ib.size() );
	for ( uint32_t index : gb.ib )
	{
		ib.push_back( index );
	}
}

void CreateSkyBoxSurf( Model& outModel )
{
	const float gridSize = 1.0f;
	const uint32_t width = 1;
	const uint32_t height = 1;

	GeoBuilder::planeInfo_t info[ 6 ];
	info[ 0 ].gridSize = vec2f( gridSize );
	info[ 0 ].widthInQuads = width;
	info[ 0 ].heightInQuads = height;
	info[ 0 ].uv[ 0 ] = vec2f( 1.0f, 1.0f );
	info[ 0 ].uv[ 1 ] = vec2f( -1.0f, -1.0f );
	info[ 0 ].origin = vec3f( 0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
	info[ 0 ].color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 0 ].normalDirection = NORMAL_Z_NEG;

	info[ 1 ].gridSize = vec2f( gridSize );
	info[ 1 ].widthInQuads = width;
	info[ 1 ].heightInQuads = height;
	info[ 1 ].uv[ 0 ] = vec2f( 1.0f, 1.0f );
	info[ 1 ].uv[ 1 ] = vec2f( -1.0f, -1.0f );
	info[ 1 ].origin = vec3f( 0.5f * gridSize * width, 0.5f * gridSize * height, -gridSize );
	info[ 1 ].color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 1 ].normalDirection = NORMAL_Z_POS;

	info[ 2 ].gridSize = vec2f( gridSize );
	info[ 2 ].widthInQuads = width;
	info[ 2 ].heightInQuads = height;
	info[ 2 ].uv[ 0 ] = vec2f( 0.0f, 0.0f );
	info[ 2 ].uv[ 1 ] = vec2f( 1.0f, 1.0f );
	info[ 2 ].origin = vec3f( -0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
	info[ 2 ].color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 2 ].normalDirection = NORMAL_X_POS;

	info[ 3 ].gridSize = vec2f( gridSize );
	info[ 3 ].widthInQuads = width;
	info[ 3 ].heightInQuads = height;
	info[ 3 ].uv[ 0 ] = vec2f( 0.0f, 0.0f );
	info[ 3 ].uv[ 1 ] = vec2f( 1.0f, 1.0f );
	info[ 3 ].origin = vec3f( 0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
	info[ 3 ].color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 3 ].normalDirection = NORMAL_X_NEG;

	info[ 4 ].gridSize = vec2f( gridSize );
	info[ 4 ].widthInQuads = width;
	info[ 4 ].heightInQuads = height;
	info[ 4 ].uv[ 0 ] = vec2f( 0.0f, 0.0f );
	info[ 4 ].uv[ 1 ] = vec2f( 1.0f, 1.0f );
	info[ 4 ].origin = vec3f( 0.5f * gridSize * width, -0.5f * gridSize * height, 0.0f );
	info[ 4 ].color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 4 ].normalDirection = NORMAL_Y_NEG;

	info[ 5 ].gridSize = vec2f( gridSize );
	info[ 5 ].widthInQuads = width;
	info[ 5 ].heightInQuads = height;
	info[ 5 ].uv[ 0 ] = vec2f( 0.0f, 0.0f );
	info[ 5 ].uv[ 1 ] = vec2f( 1.0f, 1.0f );
	info[ 5 ].origin = vec3f( 0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
	info[ 5 ].color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 5 ].normalDirection = NORMAL_Y_POS;

	GeoBuilder gb;
	for ( int i = 0; i < 6; ++i ) {
		gb.AddPlaneSurf( info[ i ] );
	}

	CopyGeoBuilderResult( gb, outModel.surfs[ 0 ].vertices, outModel.surfs[ 0 ].indices );

	outModel.surfCount = 1;
	outModel.surfs[ 0 ].materialHdl = scene.materialLib.RetrieveHdl( "SKY" );
}

void CreateTerrainSurface( Model& outModel )
{
	GeoBuilder::planeInfo_t info;
	info.gridSize = vec2f( 0.1f );
	info.widthInQuads = 100;
	info.heightInQuads = 100;
	info.uv[ 0 ] = vec2f( 0.0f, 0.0f );
	info.uv[ 1 ] = vec2f( 1.0f, 1.0f );
	info.origin = vec3f( 0.5f * info.gridSize[ 0 ] * info.widthInQuads, 0.5f * info.gridSize[ 1 ] * info.heightInQuads, 0.0f );
	info.color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info.normalDirection = NORMAL_Z_NEG;

	GeoBuilder gb;
	gb.AddPlaneSurf( info );

	CopyGeoBuilderResult( gb, outModel.surfs[ 0 ].vertices, outModel.surfs[ 0 ].indices );

	outModel.surfCount = 1;
	outModel.surfs[ 0 ].materialHdl = scene.materialLib.RetrieveHdl( "TERRAIN" );
}

void CreateWaterSurface( Model& outModel )
{
	const float gridSize = 10.f;
	const uint32_t width = 1;
	const uint32_t height = 1;
	const vec2f uvs[] = { vec2f( 0.0f, 0.0f ), vec2f( 1.0f, 1.0f ) };

	GeoBuilder::planeInfo_t info;
	info.gridSize = vec2f( 10.0f );
	info.widthInQuads = 1;
	info.heightInQuads = 1;
	info.uv[ 0 ] = vec2f( 0.0f, 0.0f );
	info.uv[ 1 ] = vec2f( 1.0f, 1.0f );
	info.origin = vec3f( 0.5f * info.gridSize[ 0 ] * info.widthInQuads, 0.5f * info.gridSize[ 1 ] * info.heightInQuads, -0.15f );
	info.color = vec4f( 0.0f, 0.0f, 1.0f, 0.2f );
	info.normalDirection = NORMAL_Z_NEG;

	GeoBuilder gb;
	gb.AddPlaneSurf( info );

	CopyGeoBuilderResult( gb, outModel.surfs[ 0 ].vertices, outModel.surfs[ 0 ].indices );

	outModel.surfCount = 1;
	outModel.surfs[ 0 ].materialHdl = scene.materialLib.RetrieveHdl( "WATER" );
}

void CreateQuadSurface2D( const std::string& materialName, Model& outModel, vec2f& origin, vec2f& size )
{
	GeoBuilder::planeInfo_t info;
	info.gridSize = size;
	info.widthInQuads = 1;
	info.heightInQuads = 1;
	info.uv[ 0 ] = vec2f( 1.0f, 0.0f );
	info.uv[ 1 ] = vec2f( -1.0f, 1.0f );
	info.origin = vec3f( origin[ 0 ], origin[ 1 ], 0.0f );
	info.normalDirection = NORMAL_Z_NEG;

	GeoBuilder gb;
	gb.AddPlaneSurf( info );

	CopyGeoBuilderResult( gb, outModel.surfs[ 0 ].vertices, outModel.surfs[ 0 ].indices );

	outModel.surfs[ 0 ].vertices[ 0 ].uv = vec2f( 0.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 0 ].uv2 = vec2f( 0.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 1 ].uv = vec2f( 1.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 1 ].uv2 = vec2f( 0.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 2 ].uv = vec2f( 0.0f, 1.0f );
	outModel.surfs[ 0 ].vertices[ 2 ].uv2 = vec2f( 0.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 3 ].uv = vec2f( 1.0f, 1.0f );
	outModel.surfs[ 0 ].vertices[ 3 ].uv2 = vec2f( 0.0f, 0.0f );

	outModel.surfCount = 1;
	outModel.surfs[ 0 ].materialHdl = scene.materialLib.RetrieveHdl( materialName.c_str() );
}