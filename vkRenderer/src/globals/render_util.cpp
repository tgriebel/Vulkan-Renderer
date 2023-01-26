#include "common.h"
#include "../render_state/deviceContext.h"
#include "io.h"
#include "../../GeoBuilder.h"
#include <scene/assetManager.h>
#include <core/assetLib.h>
#include <resource_types/gpuProgram.h>
#include "render_util.h"

extern AssetManager gAssets;


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


void MatrixToEulerZYX( const mat4x4f& m, float& xDegrees, float& yDegrees, float& zDegrees )
{
	if( m[2][0] < 1.0f )
	{
		if( m[2][0] >= -1.0f )
		{
			yDegrees = asin( -m[2][0] );
			zDegrees = atan2( m[1][0], m[0][0] );
			xDegrees = atan2( m[2][1], m[2][2] );
		}
		else
		{
			yDegrees = 1.570795f;
			zDegrees = -atan2( -m[1][2], m[1][1] );
			xDegrees = 0.0f;
		}
	}
	else
	{
		yDegrees = -1.570795f / 2.0f;
		zDegrees = atan2( -m[1][2], m[1][1] );
		xDegrees = 0.0f;
	}
	xDegrees = Degrees( xDegrees );
	yDegrees = Degrees( yDegrees );
	zDegrees = Degrees( zDegrees );
}


static void CopyGeoBuilderResult( const GeoBuilder& gb, std::vector<vertex_t>& vb, std::vector<uint32_t>& ib )
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


bool SkyBoxLoader::Load( Model& model )
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

	CopyGeoBuilderResult( gb, model.surfs[ 0 ].vertices, model.surfs[ 0 ].indices );

	model.surfCount = 1;
	model.surfs[ 0 ].materialHdl = gAssets.materialLib.RetrieveHdl( "SKY" );

	return true;
}


bool TerrainLoader::Load( Model& model )
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

	CopyGeoBuilderResult( gb, model.surfs[ 0 ].vertices, model.surfs[ 0 ].indices );

	model.surfCount = 1;
	model.surfs[ 0 ].materialHdl = gAssets.materialLib.RetrieveHdl( "TERRAIN" );

	return true;
}


bool WaterLoader::Load( Model& model )
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

	CopyGeoBuilderResult( gb, model.surfs[ 0 ].vertices, model.surfs[ 0 ].indices );

	model.surfCount = 1;
	model.surfs[ 0 ].materialHdl = gAssets.materialLib.RetrieveHdl( "WATER" );

	return true;
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
	outModel.surfs[ 0 ].materialHdl = gAssets.materialLib.RetrieveHdl( materialName.c_str() );
}


bool QuadLoader::Load( Model& model )
{
	//CreateQuadSurface2D();
	return false;
}