#pragma once

#include "common.h"

#include <vector>

// template<typename Vec>
class GeoBuilder
{
public:

	struct vertex_t
	{
		vec3f	pos;
		vec4f	color;
		vec3f	normal;
		vec3f	bitangent;
		vec3f	tangent;
		vec2f	texCoord;
	};

	struct planeInfo_t
	{
		vec3f				origin;
		vec4f				color;
		uint32_t			widthInQuads;
		uint32_t			heightInQuads;
		vec2f				gridSize;
		normalDirection_t	normalDirection;
		vec2f				uv[ 2 ];
	};

	std::vector<vertex_t>	vb;
	std::vector<uint32_t>	ib;

	GeoBuilder()
	{
	
	}

	void AddPlaneSurf( const planeInfo_t& info )
	{
		std::pair<size_t, size_t> vertexDim = std::pair<size_t, size_t>( info.widthInQuads + 1, info.heightInQuads + 1 );

		const size_t verticesPerQuad = 6;

		const size_t firstIndex = vb.size();
		size_t indicesCnt = ib.size();
		size_t vbIx = firstIndex;
		vb.resize( vbIx + vertexDim.first * vertexDim.second );
		ib.resize( indicesCnt + verticesPerQuad * info.widthInQuads * info.heightInQuads );

		for ( size_t j = 0; j < vertexDim.second; ++j )
		{
			for ( size_t i = 0; i < vertexDim.first; ++i )
			{
				const float u = ( i / static_cast<float>( info.widthInQuads ) );
				const float v = ( j / static_cast<float>( info.heightInQuads ) );

				vertex_t& vert = vb[ vbIx ];
				switch ( info.normalDirection )
				{
				default:
				case NORMAL_X_POS:	vert.pos			= { 0.0f, i * info.gridSize[ 0 ], j * info.gridSize[ 1 ] };
									vert.color			= info.color;
									vert.tangent		= vec3f( 0.0f, 1.0f, 0.0f );
									vert.bitangent		= vec3f( 0.0f, 0.0f, 1.0f );
									vert.normal			= vec3f( 1.0f, 0.0f, 0.0f );
									vert.texCoord[ 0 ]	= info.uv[ 0 ][ 0 ] + ( 1.0f - u ) * info.uv[ 1 ][ 0 ];
									vert.texCoord[ 1 ]	= info.uv[ 0 ][ 1 ] + ( 1.0f - v ) * info.uv[ 1 ][ 1 ];
								 break;

				case NORMAL_X_NEG:	vert.pos			= { 0.0f, i * info.gridSize[ 0 ], j * info.gridSize[ 1 ] };
									vert.color			= info.color;
									vert.tangent		= vec3f( 0.0f, -1.0f, 0.0f );
									vert.bitangent		= vec3f( 0.0f, 0.0f, 1.0f );
									vert.normal			= vec3f( -1.0f, 0.0f, 0.0f );
									vert.texCoord[ 0 ]	= info.uv[ 0 ][ 0 ] + u * info.uv[ 1 ][ 0 ];
									vert.texCoord[ 1 ]	= info.uv[ 0 ][ 1 ] + ( 1.0f - v ) * info.uv[ 1 ][ 1 ];
								 break;

				case NORMAL_Y_POS:	vert.pos			= { i * info.gridSize[ 0 ], 0.0f, j * info.gridSize[ 1 ] };
									vert.color			= info.color;
									vert.tangent		= vec3f( -1.0f, 0.0f, 0.0f );
									vert.bitangent		= vec3f( 0.0f, 0.0f,1.0f );
									vert.normal			= vec3f( 0.0f, 1.0f, 0.0f );
									vert.texCoord[ 0 ]	= info.uv[ 0 ][ 0 ] + ( 1.0f - u ) * info.uv[ 1 ][ 0 ];
									vert.texCoord[ 1 ]	= info.uv[ 0 ][ 1 ] + ( 1.0f - v ) * info.uv[ 1 ][ 1 ];
								 break;

				case NORMAL_Y_NEG:	vert.pos			= { i * info.gridSize[ 0 ], 0.0f, j * info.gridSize[ 1 ] };
									vert.color			= info.color;
									vert.tangent		= vec3f( 0.0f, 0.0f, 1.0f );
									vert.bitangent		= vec3f( -1.0f, 0.0f, 0.0f );
									vert.normal			= vec3f( 0.0f, -1.0f, 0.0f );
									vert.texCoord[ 0 ]	= info.uv[ 0 ][ 0 ] + u * info.uv[ 1 ][ 0 ];
									vert.texCoord[ 1 ]	= info.uv[ 0 ][ 1 ] + ( 1.0f - v ) * info.uv[ 1 ][ 1 ];
								 break;

				case NORMAL_Z_POS:	vert.pos			= { i * info.gridSize[ 0 ], j * info.gridSize[ 1 ], 0.0f };
									vert.color			= info.color;
									vert.tangent		= vec3f( 0.0f, 1.0f, 0.0f );
									vert.bitangent		= vec3f( -1.0f, 0.0f, 0.0f );
									vert.normal			= vec3f( 0.0f, 0.0f, 1.0f );
									vert.texCoord[ 0 ]	= info.uv[ 0 ][ 0 ] + ( 1.0f - u ) * info.uv[ 1 ][ 0 ];
									vert.texCoord[ 1 ]	= info.uv[ 0 ][ 1 ] + ( 1.0f - v ) * info.uv[ 1 ][ 1 ];
								 break;

				case NORMAL_Z_NEG:	vert.pos			= { i * info.gridSize[ 0 ],	j * info.gridSize[ 1 ],	0.0f };
									vert.color			= info.color;
									vert.tangent		= vec3f( 0.0f, -1.0f, 0.0f );
									vert.bitangent		= vec3f( 1.0f, 0.0f, 0.0f );
									vert.normal			= vec3f( 0.0f, 0.0f, -1.0f );
									vert.texCoord[ 0 ]	= info.uv[ 0 ][ 0 ] + ( 1.0f - u ) * info.uv[ 1 ][ 0 ];
									vert.texCoord[ 1 ]	= info.uv[ 0 ][ 1 ] + v * info.uv[ 1 ][ 1 ];
								 break;
				}
				vert.pos -= info.origin;

				++vbIx;
			}
		}

		for ( uint32_t j = 0; j < info.heightInQuads; ++j )
		{
			for ( uint32_t i = 0; i < info.widthInQuads; ++i )
			{
				uint32_t vIx[ 4 ];
				vIx[ 0 ] = static_cast<uint32_t>( firstIndex + ( i + 0 ) + ( j + 0 ) * ( info.heightInQuads + 1 ) );
				vIx[ 1 ] = static_cast<uint32_t>( firstIndex + ( i + 1 ) + ( j + 0 ) * ( info.heightInQuads + 1 ) );
				vIx[ 2 ] = static_cast<uint32_t>( firstIndex + ( i + 0 ) + ( j + 1 ) * ( info.heightInQuads + 1 ) );
				vIx[ 3 ] = static_cast<uint32_t>( firstIndex + ( i + 1 ) + ( j + 1 ) * ( info.heightInQuads + 1 ) );

				switch ( info.normalDirection )
				{
				case NORMAL_X_POS:
				case NORMAL_Y_POS:
				case NORMAL_Z_POS:
					ib[ indicesCnt++ ] = vIx[ 2 ];
					ib[ indicesCnt++ ] = vIx[ 1 ];
					ib[ indicesCnt++ ] = vIx[ 0 ];

					ib[ indicesCnt++ ] = vIx[ 2 ];
					ib[ indicesCnt++ ] = vIx[ 3 ];
					ib[ indicesCnt++ ] = vIx[ 1 ];
					break;

				case NORMAL_X_NEG:
				case NORMAL_Y_NEG:
				case NORMAL_Z_NEG:
					ib[ indicesCnt++ ] = vIx[ 2 ];
					ib[ indicesCnt++ ] = vIx[ 0 ];
					ib[ indicesCnt++ ] = vIx[ 1 ];

					ib[ indicesCnt++ ] = vIx[ 2 ];
					ib[ indicesCnt++ ] = vIx[ 1 ];
					ib[ indicesCnt++ ] = vIx[ 3 ];
					break;
				}
			}
		}
	}

	void AddBoxSurf( const vec3f origin, const float size )
	{
		const size_t verticesCnt = 8;
		const size_t triPerSide = 2;
		const size_t sideCnt = 6;

		size_t vbIx = vb.size();
		vb.resize( vb.size() + verticesCnt );
		ib.resize( ib.size() + 3 * triPerSide * sideCnt );

		vec3f positions[ verticesCnt ] =
		{ vec3f( 0.0f,	0.0f,	0.0f ),
			vec3f( size,	0.0f,	0.0f ),
			vec3f( 0.0f,	size,	0.0f ),
			vec3f( size,	size,	0.0f ),
			vec3f( 0.0f,	0.0f,	size ),
			vec3f( size,	0.0f,	size ),
			vec3f( 0.0f,	size,	size ),
			vec3f( size,	size,	size ),
		};

		for ( size_t i = 0; i < verticesCnt; ++i )
		{
			vertex_t& vert = vb[ vbIx ];
			vert.pos = positions[ i ];
			vert.pos -= origin;
			vert.color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
			vert.tangent = { 1.0f, 0.0f, 0.0f };
			vert.bitangent = { 0.0f, 1.0f, 0.0f };
			vert.texCoord = { 0.0f, 0.0f };

			++vbIx;
		}

		// TODO: need to offset by starting index
		uint32_t sideIndices[ sideCnt ][ 4 ] =
		{ { 2, 0, 1, 3 },
			{ 6, 4, 5, 7 },
			{ 0, 4, 5, 1 },
			{ 1, 5, 7, 3 },
			{ 3, 7, 6, 2 },
			{ 2, 6, 4, 0 },
		};

		uint32_t indicesCnt = 0;
		for ( size_t i = 0; i < sideCnt; ++i )
		{
			uint32_t vIx[ 4 ];
			vIx[ 0 ] = static_cast<uint32_t>( sideIndices[ i ][ 0 ] );
			vIx[ 1 ] = static_cast<uint32_t>( sideIndices[ i ][ 1 ] );
			vIx[ 2 ] = static_cast<uint32_t>( sideIndices[ i ][ 2 ] );
			vIx[ 3 ] = static_cast<uint32_t>( sideIndices[ i ][ 3 ] );

			ib[ indicesCnt++ ] = vIx[ 0 ];
			ib[ indicesCnt++ ] = vIx[ 2 ];
			ib[ indicesCnt++ ] = vIx[ 1 ];

			ib[ indicesCnt++ ] = vIx[ 2 ];
			ib[ indicesCnt++ ] = vIx[ 0 ];
			ib[ indicesCnt++ ] = vIx[ 3 ];
		}
	}

};