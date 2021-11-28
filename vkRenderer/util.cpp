#include "common.h"
#include "deviceContext.h"
#include "io.h"
#include "GeoBuilder.h"
#include "assetLib.h"

extern AssetLib< material_t > materialLib;

#define STB_IMAGE_IMPLEMENTATION // includes func defs
#include "stb_image.h"

extern 

glm::mat4 MatrixFromVector( const glm::vec3& v )
{
	glm::vec3 up = glm::vec3( 0.0f, 0.0f, 1.0f );
	const glm::vec3 u = glm::normalize( v );
	if ( glm::dot( v, up ) > 0.99999f ) {
		up = glm::vec3( 0.0f, 1.0f, 0.0f );
	}
	const glm::vec3 left = -glm::cross( u, up );

	const glm::mat4 m = glm::mat4( up[ 0 ], up[ 1 ], up[ 2 ], 0.0f,
		left[ 0 ], left[ 1 ], left[ 2 ], 0.0f,
		v[ 0 ], v[ 1 ], v[ 2 ], 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f );

	return m;
}

bool LoadTextureImage( const char * texturePath, texture_t& texture )
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load( texturePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha );

	if ( !pixels ) {
		stbi_image_free( pixels );
		return false;
	}

	texture.bytes		= pixels;
	texture.width		= texWidth;
	texture.height		= texHeight;
	texture.channels	= texChannels;
	texture.sizeBytes	= ( texWidth * texHeight * 4 );

	texture.bytes = new uint8_t[ texture.sizeBytes ];
	memcpy( texture.bytes, pixels, texture.sizeBytes );
	stbi_image_free( pixels );
	return true;
}

void CreateShaders( GpuProgram& prog )
{
	prog.vs = CreateShaderModule( prog.vsBlob );
	prog.ps = CreateShaderModule( prog.psBlob );
}

void CopyGeoBuilderResult( const GeoBuilder& gb, std::vector<VertexInput>& vb, std::vector<uint32_t>& ib )
{
	vb.reserve( gb.vb.size() );
	for ( const GeoBuilder::vertex_t& v : gb.vb )
	{
		VertexInput vert;
		vert.pos = v.pos;
		vert.color = v.color;
		vert.tangent = v.tangent;
		vert.bitangent = v.bitangent;
		vert.texCoord = v.texCoord;

		vb.push_back( vert );
	}

	ib.reserve( gb.ib.size() );
	for ( uint32_t index : gb.ib )
	{
		ib.push_back( index );
	}
}

void CreateSkyBoxSurf( modelSource_t& outModel )
{
	const float gridSize = 1.0f;
	const uint32_t width = 1;
	const uint32_t height = 1;

	const float third = 1.0f / 3.0f;
	const float quarter = 1.0f / 4.0f;

	GeoBuilder::planeInfo_t info[ 6 ];
	info[ 0 ].gridSize = glm::vec2( gridSize );
	info[ 0 ].widthInQuads = width;
	info[ 0 ].heightInQuads = height;
	info[ 0 ].uv[ 0 ] = glm::vec2( quarter, 2.0f * third );
	info[ 0 ].uv[ 1 ] = glm::vec2( quarter, third );
	info[ 0 ].origin = glm::vec3( 0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
	info[ 0 ].color = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 0 ].normalDirection = NORMAL_Z_NEG;

	info[ 1 ].gridSize = glm::vec2( gridSize );
	info[ 1 ].widthInQuads = width;
	info[ 1 ].heightInQuads = height;
	info[ 1 ].uv[ 0 ] = glm::vec2( quarter, 0.0f );
	info[ 1 ].uv[ 1 ] = glm::vec2( quarter, third );
	info[ 1 ].origin = glm::vec3( 0.5f * gridSize * width, 0.5f * gridSize * height, -gridSize );
	info[ 1 ].color = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 1 ].normalDirection = NORMAL_Z_POS;

	info[ 2 ].gridSize = glm::vec2( gridSize );
	info[ 2 ].widthInQuads = width;
	info[ 2 ].heightInQuads = height;
	info[ 2 ].uv[ 0 ] = glm::vec2( 0.0f, third );
	info[ 2 ].uv[ 1 ] = glm::vec2( quarter, third );
	info[ 2 ].origin = glm::vec3( -0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
	info[ 2 ].color = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 2 ].normalDirection = NORMAL_X_POS;

	info[ 3 ].gridSize = glm::vec2( gridSize );
	info[ 3 ].widthInQuads = width;
	info[ 3 ].heightInQuads = height;
	info[ 3 ].uv[ 0 ] = glm::vec2( 2.0f * quarter, third );
	info[ 3 ].uv[ 1 ] = glm::vec2( quarter, third );
	info[ 3 ].origin = glm::vec3( 0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
	info[ 3 ].color = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 3 ].normalDirection = NORMAL_X_NEG;

	info[ 4 ].gridSize = glm::vec2( gridSize );
	info[ 4 ].widthInQuads = width;
	info[ 4 ].heightInQuads = height;
	info[ 4 ].uv[ 0 ] = glm::vec2( 3.0f * quarter, third );
	info[ 4 ].uv[ 1 ] = glm::vec2( quarter, third );
	info[ 4 ].origin = glm::vec3( 0.5f * gridSize * width, -0.5f * gridSize * height, 0.0f );
	info[ 4 ].color = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 4 ].normalDirection = NORMAL_Y_NEG;

	info[ 5 ].gridSize = glm::vec2( gridSize );
	info[ 5 ].widthInQuads = width;
	info[ 5 ].heightInQuads = height;
	info[ 5 ].uv[ 0 ] = glm::vec2( quarter, third );
	info[ 5 ].uv[ 1 ] = glm::vec2( quarter, third );
	info[ 5 ].origin = glm::vec3( 0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
	info[ 5 ].color = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 5 ].normalDirection = NORMAL_Y_POS;

	GeoBuilder gb;
	for ( int i = 0; i < 6; ++i ) {
		gb.AddPlaneSurf( info[ i ] );
	}

	CopyGeoBuilderResult( gb, outModel.vertices, outModel.indices );

	outModel.material = materialLib.Find( "SKY" );
	outModel.materialId = materialLib.FindId( "SKY" );
}

void CreateTerrainSurface( modelSource_t& outModel )
{
	GeoBuilder::planeInfo_t info;
	info.gridSize = glm::vec2( 0.1f );
	info.widthInQuads = 100;
	info.heightInQuads = 100;
	info.uv[ 0 ] = glm::vec2( 0.0f, 0.0f );
	info.uv[ 1 ] = glm::vec2( 1.0f, 1.0f );
	info.origin = glm::vec3( 0.5f * info.gridSize.x * info.widthInQuads, 0.5f * info.gridSize.y * info.heightInQuads, 0.0f );
	info.color = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
	info.normalDirection = NORMAL_Z_NEG;

	GeoBuilder gb;
	gb.AddPlaneSurf( info );

	CopyGeoBuilderResult( gb, outModel.vertices, outModel.indices );

	outModel.material = materialLib.Find( "TERRAIN" );
	outModel.materialId = materialLib.FindId( "TERRAIN" );
}

void CreateWaterSurface( modelSource_t& outModel )
{
	const float gridSize = 10.f;
	const uint32_t width = 1;
	const uint32_t height = 1;
	const glm::vec2 uvs[] = { glm::vec2( 0.0f, 0.0f ), glm::vec2( 1.0f, 1.0f ) };

	GeoBuilder::planeInfo_t info;
	info.gridSize = glm::vec2( 10.0f );
	info.widthInQuads = 1;
	info.heightInQuads = 1;
	info.uv[ 0 ] = glm::vec2( 0.0f, 0.0f );
	info.uv[ 1 ] = glm::vec2( 1.0f, 1.0f );
	info.origin = glm::vec3( 0.5f * info.gridSize.x * info.widthInQuads, 0.5f * info.gridSize.y * info.heightInQuads, -0.15f );
	info.color = glm::vec4( 0.0f, 0.0f, 1.0f, 0.2f );
	info.normalDirection = NORMAL_Z_NEG;

	GeoBuilder gb;
	gb.AddPlaneSurf( info );

	CopyGeoBuilderResult( gb, outModel.vertices, outModel.indices );

	outModel.material = materialLib.Find( "WATER" );
	outModel.materialId = materialLib.FindId( "WATER" );
}

void CreateQuadSurface2D( const std::string& materialName, modelSource_t& outModel, glm::vec2& origin, glm::vec2& size )
{
	GeoBuilder::planeInfo_t info;
	info.gridSize = size;
	info.widthInQuads = 1;
	info.heightInQuads = 1;
	info.uv[ 0 ] = glm::vec2( 0.0f, 0.0f );
	info.uv[ 1 ] = glm::vec2( -1.0f, 1.0f );
	info.origin = glm::vec3( origin.x, origin.y, 0.0f );
	info.normalDirection = NORMAL_Z_NEG;

	GeoBuilder gb;
	gb.AddPlaneSurf( info );

	CopyGeoBuilderResult( gb, outModel.vertices, outModel.indices );

	outModel.material = materialLib.Find( materialName.c_str() );
	outModel.materialId = materialLib.FindId( materialName.c_str() );
}

void CreateStaticModel( const std::string& modelName, const std::string& materialName, modelSource_t& outModel )
{
	LoadModel( modelName, outModel );

	outModel.material = materialLib.Find( materialName.c_str() );
	outModel.materialId = materialLib.FindId( materialName.c_str() );
}