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
#include "render_util.h"

#include "common.h"
#include "io.h"
#include "../../GeoBuilder.h"
#include <gfxcore/scene/assetManager.h>
#include <gfxcore/core/assetLib.h>
#include <gfxcore/asset_types/gpuProgram.h>
#include <gfxcore/asset_types/texture.h>
#include <gfxcore/asset_types/model.h>

extern AssetManager g_assets;


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


static void CopyGeoBuilderResult( const GeoBuilder& gb, Surface& surf, AABB& bounds )
{

	surf.vertices.reserve( gb.vb.size() );
	for ( const GeoBuilder::vertex_t& v : gb.vb )
	{
		vertex_t vert;
		vert.pos = vec4f( v.pos[ 0 ], v.pos[ 1 ], v.pos[ 2 ], 0.0f );
		vert.color = Color( v.color[ 0 ], v.color[ 1 ], v.color[ 2 ], v.color[ 3 ] );
		vert.normal = vec3f( v.normal[ 0 ], v.normal[ 1 ], v.normal[ 2 ] );
		vert.tangent = vec3f( v.tangent[ 0 ], v.tangent[ 1 ], v.tangent[ 2 ] );
		vert.bitangent = vec3f( v.bitangent[ 0 ], v.bitangent[ 1 ], v.bitangent[ 2 ] );
		vert.uv = vec2f( v.texCoord[ 0 ], v.texCoord[ 1 ] );
		vert.uv2 = vec2f( 0.0f, 0.0f );

		surf.vertices.push_back( vert );
		bounds.Expand( vert.pos );
	}

	surf.indices.reserve( gb.ib.size() );
	for ( uint32_t index : gb.ib )
	{
		surf.indices.push_back( index );
	}
}


bool SkyBoxLoader::Load( Asset<Model>& modelAsset )
{
	Model& model = modelAsset.Get();

	const float gridSize = 1.0f;
	const uint32_t width = 1;
	const uint32_t height = 1;

	GeoBuilder::planeInfo_t info[ 6 ];
	info[ 0 ].gridSize = vec2f( gridSize );
	info[ 0 ].widthInQuads = width;
	info[ 0 ].heightInQuads = height;
	info[ 0 ].uvScale = 1.0f;
	info[ 0 ].uv[ 0 ] = vec2f( 1.0f, 1.0f );
	info[ 0 ].uv[ 1 ] = vec2f( -1.0f, -1.0f );
	info[ 0 ].origin = vec3f( 0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
	info[ 0 ].color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 0 ].normalDirection = GeoBuilder::NORMAL_Z_NEG;

	info[ 1 ].gridSize = vec2f( gridSize );
	info[ 1 ].widthInQuads = width;
	info[ 1 ].heightInQuads = height;
	info[ 1 ].uvScale = 1.0f;
	info[ 1 ].uv[ 0 ] = vec2f( 1.0f, 1.0f );
	info[ 1 ].uv[ 1 ] = vec2f( -1.0f, -1.0f );
	info[ 1 ].origin = vec3f( 0.5f * gridSize * width, 0.5f * gridSize * height, -gridSize );
	info[ 1 ].color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 1 ].normalDirection = GeoBuilder::NORMAL_Z_POS;

	info[ 2 ].gridSize = vec2f( gridSize );
	info[ 2 ].widthInQuads = width;
	info[ 2 ].heightInQuads = height;
	info[ 2 ].uvScale = 1.0f;
	info[ 2 ].uv[ 0 ] = vec2f( 0.0f, 0.0f );
	info[ 2 ].uv[ 1 ] = vec2f( 1.0f, 1.0f );
	info[ 2 ].origin = vec3f( -0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
	info[ 2 ].color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 2 ].normalDirection = GeoBuilder::NORMAL_X_POS;

	info[ 3 ].gridSize = vec2f( gridSize );
	info[ 3 ].widthInQuads = width;
	info[ 3 ].heightInQuads = height;
	info[ 3 ].uvScale = 1.0f;
	info[ 3 ].uv[ 0 ] = vec2f( 0.0f, 0.0f );
	info[ 3 ].uv[ 1 ] = vec2f( 1.0f, 1.0f );
	info[ 3 ].origin = vec3f( 0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
	info[ 3 ].color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 3 ].normalDirection = GeoBuilder::NORMAL_X_NEG;

	info[ 4 ].gridSize = vec2f( gridSize );
	info[ 4 ].widthInQuads = width;
	info[ 4 ].heightInQuads = height;
	info[ 4 ].uvScale = 1.0f;
	info[ 4 ].uv[ 0 ] = vec2f( 0.0f, 0.0f );
	info[ 4 ].uv[ 1 ] = vec2f( 1.0f, 1.0f );
	info[ 4 ].origin = vec3f( 0.5f * gridSize * width, -0.5f * gridSize * height, 0.0f );
	info[ 4 ].color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 4 ].normalDirection = GeoBuilder::NORMAL_Y_NEG;

	info[ 5 ].gridSize = vec2f( gridSize );
	info[ 5 ].widthInQuads = width;
	info[ 5 ].heightInQuads = height;
	info[ 5 ].uvScale = 1.0f;
	info[ 5 ].uv[ 0 ] = vec2f( 0.0f, 0.0f );
	info[ 5 ].uv[ 1 ] = vec2f( 1.0f, 1.0f );
	info[ 5 ].origin = vec3f( 0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
	info[ 5 ].color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info[ 5 ].normalDirection = GeoBuilder::NORMAL_Y_POS;

	GeoBuilder gb;
	for ( int i = 0; i < 6; ++i ) {
		gb.AddPlaneSurf( info[ i ] );
	}

	model.surfCount = 1;
	model.surfs.resize( model.surfCount );
	CopyGeoBuilderResult( gb, model.surfs[ 0 ], model.bounds );

	model.surfs[ 0 ].materialHdl = AssetLibMaterials::Handle( "SKY" );

	return true;
}


bool TerrainLoader::Load( Asset<Model>& modelAsset )
{
	Model& model = modelAsset.Get();

	GeoBuilder::planeInfo_t info;
	info.gridSize = vec2f( cellSize );
	info.widthInQuads = width;
	info.heightInQuads = height;
	info.uvScale = uvScale;
	info.uv[ 0 ] = vec2f( 0.0f, 0.0f );
	info.uv[ 1 ] = vec2f( 1.0f, 1.0f );
	info.origin = vec3f( 0.5f * info.gridSize[ 0 ] * info.widthInQuads, 0.5f * info.gridSize[ 1 ] * info.heightInQuads, 0.0f );
	info.color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
	info.normalDirection = GeoBuilder::NORMAL_Z_NEG;

	GeoBuilder gb;
	gb.AddPlaneSurf( info );

	model.surfCount = 1;
	model.surfs.resize( model.surfCount );
	CopyGeoBuilderResult( gb, model.surfs[ 0 ], model.bounds );

	model.surfs[ 0 ].materialHdl = handle;

	return true;
}


bool WaterLoader::Load( Asset<Model>& modelAsset )
{
	Model& model = modelAsset.Get();

	const float gridSize = 10.f;
	const uint32_t width = 1;
	const uint32_t height = 1;
	const vec2f uvs[] = { vec2f( 0.0f, 0.0f ), vec2f( 1.0f, 1.0f ) };

	GeoBuilder::planeInfo_t info;
	info.gridSize = vec2f( 10.0f );
	info.widthInQuads = 1;
	info.heightInQuads = 1;
	info.uvScale = 1.0f;
	info.uv[ 0 ] = vec2f( 0.0f, 0.0f );
	info.uv[ 1 ] = vec2f( 1.0f, 1.0f );
	info.origin = vec3f( 0.5f * info.gridSize[ 0 ] * info.widthInQuads, 0.5f * info.gridSize[ 1 ] * info.heightInQuads, -0.15f );
	info.color = vec4f( 0.0f, 0.0f, 1.0f, 0.2f );
	info.normalDirection = GeoBuilder::NORMAL_Z_NEG;

	GeoBuilder gb;
	gb.AddPlaneSurf( info );

	model.surfCount = 1;
	model.surfs.resize( model.surfCount );
	CopyGeoBuilderResult( gb, model.surfs[ 0 ], model.bounds );

	model.surfs[ 0 ].materialHdl = AssetLibMaterials::Handle( "WATER" );

	return true;
}


void CreateQuadSurface2D( const std::string& materialName, Model& outModel, vec2f& origin, vec2f& size )
{
	GeoBuilder::planeInfo_t info;
	info.gridSize = size;
	info.widthInQuads = 1;
	info.heightInQuads = 1;
	info.uvScale = 1.0f;
	info.uv[ 0 ] = vec2f( 1.0f, 0.0f );
	info.uv[ 1 ] = vec2f( -1.0f, 1.0f );
	info.origin = vec3f( origin[ 0 ], origin[ 1 ], 0.0f );
	info.normalDirection = GeoBuilder::NORMAL_Z_NEG;

	GeoBuilder gb;
	gb.AddPlaneSurf( info );

	outModel.surfCount = 1;
	outModel.surfs.resize( outModel.surfCount );
	CopyGeoBuilderResult( gb, outModel.surfs[ 0 ], outModel.bounds );

	outModel.surfs[ 0 ].vertices[ 0 ].uv = vec2f( 0.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 0 ].uv2 = vec2f( 0.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 1 ].uv = vec2f( 1.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 1 ].uv2 = vec2f( 0.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 2 ].uv = vec2f( 0.0f, 1.0f );
	outModel.surfs[ 0 ].vertices[ 2 ].uv2 = vec2f( 0.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 3 ].uv = vec2f( 1.0f, 1.0f );
	outModel.surfs[ 0 ].vertices[ 3 ].uv2 = vec2f( 0.0f, 0.0f );

	outModel.surfs[ 0 ].materialHdl = AssetLibMaterials::Handle( materialName.c_str() );
}


bool QuadLoader::Load( Asset<Model>& model )
{
	//CreateQuadSurface2D();
	return false;
}