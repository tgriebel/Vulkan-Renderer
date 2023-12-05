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

#pragma once

#include "src/globals/common.h"

#include <vector>

// template<typename Vec>
class GeoBuilder
{
public:

	enum normalDirection_t
	{
		NORMAL_X_POS = 0,
		NORMAL_X_NEG = 1,
		NORMAL_Y_POS = 2,
		NORMAL_Y_NEG = 3,
		NORMAL_Z_POS = 4,
		NORMAL_Z_NEG = 5,
	};

	enum winding_t
	{
		WINDING_CLOCKWISE = 0,
		WINDING_COUNTER_CLOCKWISE = 1,
	};

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
		uint32_t			subDivisionsX;
		uint32_t			subDivisionsY;
		vec2f				gridSize;
		normalDirection_t	normalDirection;
		winding_t			winding;
		vec2f				uvOffset;
		vec2f				uvScale;
		bool				flipUv;

		planeInfo_t()
		{
			gridSize = vec2f( 1.0f, 1.0f );
			subDivisionsX = 1;
			subDivisionsY = 1;
			uvOffset = vec2f( 0.0f, 0.0f );
			uvScale = vec2f( 1.0f, 1.0f );
			origin = vec3f( 0.0f, 0.0f, 0.0f );
			color = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
			normalDirection = NORMAL_Z_POS;
			winding = WINDING_COUNTER_CLOCKWISE;
			flipUv = false;
		}
	};

	std::vector<vertex_t>	vb;
	std::vector<uint32_t>	ib;

	GeoBuilder()
	{
	
	}

	static float WrapU( const float u )
	{
		float s = u;
		s = ( s > 1.0 ) ? ( s - floor( s ) ) : s;
		s = Saturate( s );

		return s;
	}

	static vec2f WrapUV( const vec2f& uv )
	{
		vec2f st = uv;
		st[0] = WrapU( st[0] );
		st[1] = WrapU( st[1] );
		return st;
	}

	void AddPlaneSurf( const planeInfo_t& info )
	{
		const std::pair<size_t, size_t> sizeInVertices = std::pair<size_t, size_t>( info.subDivisionsX + 1, info.subDivisionsY + 1 );

		const size_t indicesPerQuad = 6;

		const size_t firstIndex = vb.size();
		size_t indicesCnt = ib.size();
		size_t vbIx = firstIndex;
		vb.resize( vbIx + sizeInVertices.first * sizeInVertices.second );
		ib.resize( indicesCnt + indicesPerQuad * info.subDivisionsX * info.subDivisionsY );

		const vec2f uvCorner = info.uvOffset;
		const vec2f uvDir = info.uvScale;

		const vec2f gridSizeWs = vec2f( info.gridSize[ 0 ], info.gridSize[ 1 ] );
		const vec2f cellSizeWs = vec2f( gridSizeWs[ 0 ] / info.subDivisionsX, gridSizeWs[ 1 ] / info.subDivisionsY );

		// Create vertices
		for ( size_t j = 0; j < sizeInVertices.second; ++j )
		{
			for ( size_t i = 0; i < sizeInVertices.first; ++i )
			{
				const float u = ( i / static_cast<float>( info.subDivisionsX ) );
				const float v = ( j / static_cast<float>( info.subDivisionsY ) );

				vertex_t& vert = vb[ vbIx ];

				switch ( info.normalDirection )
				{
				default:
				case NORMAL_X_POS:
				{										
					vert.pos[ 0 ]		= 0.0f;
					vert.pos[ 1 ]		= i * cellSizeWs[ 0 ] - 0.5f * gridSizeWs[ 0 ];
					vert.pos[ 2 ]		= j * cellSizeWs[ 1 ] - 0.5f * gridSizeWs[ 1 ];
				
					vert.tangent		= vec3f( 0.0f, 1.0f, 0.0f );
					vert.bitangent		= vec3f( 0.0f, 0.0f, 1.0f );
					vert.normal			= vec3f( 1.0f, 0.0f, 0.0f );			
				}
				break;

				case NORMAL_X_NEG:
				{					
					vert.pos[ 0 ]		= 0.0f;
					vert.pos[ 1 ]		= i * cellSizeWs[ 0 ] - 0.5f * gridSizeWs[ 0 ];
					vert.pos[ 2 ]		= j * cellSizeWs[ 1 ] - 0.5f * gridSizeWs[ 1 ];

					vert.tangent		= vec3f( 0.0f, -1.0f, 0.0f );
					vert.bitangent		= vec3f( 0.0f, 0.0f, 1.0f );
					vert.normal			= vec3f( -1.0f, 0.0f, 0.0f );
				
				} break;

				case NORMAL_Y_POS:
				{
					vert.pos[ 0 ]		= i * cellSizeWs[ 0 ] - 0.5f * gridSizeWs[ 0 ];
					vert.pos[ 1 ]		= 0.0f;
					vert.pos[ 2 ]		= j * cellSizeWs[ 1 ] - 0.5f * gridSizeWs[ 1 ];

					vert.tangent		= vec3f( -1.0f, 0.0f, 0.0f );
					vert.bitangent		= vec3f( 0.0f, 0.0f,1.0f );
					vert.normal			= vec3f( 0.0f, 1.0f, 0.0f );
				} break;

				case NORMAL_Y_NEG:
				{
					vert.pos[ 0 ]		= i * cellSizeWs[ 0 ] - 0.5f * gridSizeWs[ 0 ];
					vert.pos[ 1 ]		= 0.0f;
					vert.pos[ 2 ]		= j * cellSizeWs[ 1 ] - 0.5f * gridSizeWs[ 1 ];

					vert.tangent		= vec3f( 0.0f, 0.0f, 1.0f );
					vert.bitangent		= vec3f( -1.0f, 0.0f, 0.0f );
					vert.normal			= vec3f( 0.0f, -1.0f, 0.0f );
				} break;

				case NORMAL_Z_POS:
				{
					vert.pos[ 0 ]		= i * cellSizeWs[ 0 ] - 0.5f * gridSizeWs[ 0 ];
					vert.pos[ 1 ]		= j * cellSizeWs[ 1 ] - 0.5f * gridSizeWs[ 1 ];
					vert.pos[ 2 ]		= 0.0f;

					vert.tangent		= vec3f( 0.0f, 1.0f, 0.0f );
					vert.bitangent		= vec3f( -1.0f, 0.0f, 0.0f );
					vert.normal			= vec3f( 0.0f, 0.0f, 1.0f );
				} break;

				case NORMAL_Z_NEG:
				{
					vert.pos[ 0 ]		= i * cellSizeWs[ 0 ] - 0.5f * gridSizeWs[ 0 ];
					vert.pos[ 1 ]		= j * cellSizeWs[ 1 ] - 0.5f * gridSizeWs[ 1 ];
					vert.pos[ 2 ]		= 0.0f;

					vert.tangent		= vec3f( 0.0f, -1.0f, 0.0f );
					vert.bitangent		= vec3f( 1.0f, 0.0f, 0.0f );
					vert.normal			= vec3f( 0.0f, 0.0f, -1.0f );
				} break;
				}

				vert.color = info.color;
				if( info.flipUv )
				{
					vert.texCoord[ 0 ] = ( uvCorner[0] + v * uvDir[1] );
					vert.texCoord[ 1 ] = ( uvCorner[1] + u * uvDir[0] );
				}
				else 
				{
					vert.texCoord[ 0 ] = ( uvCorner[ 0 ] + u * uvDir[ 0 ] );
					vert.texCoord[ 1 ] = ( uvCorner[ 1 ] + v * uvDir[ 1 ] );
				}

				vert.pos += info.origin;

				++vbIx;
			}
		}

		// Create indices
		for ( uint32_t j = 0; j < info.subDivisionsY; ++j )
		{
			for ( uint32_t i = 0; i < info.subDivisionsX; ++i )
			{
				// Generate two triangles per quad
				uint32_t vIx[ 4 ];
				vIx[ 0 ] = static_cast<uint32_t>( firstIndex + ( i + 0 ) + ( j + 0 ) * sizeInVertices.second );
				vIx[ 1 ] = static_cast<uint32_t>( firstIndex + ( i + 1 ) + ( j + 0 ) * sizeInVertices.second );
				vIx[ 2 ] = static_cast<uint32_t>( firstIndex + ( i + 0 ) + ( j + 1 ) * sizeInVertices.second );
				vIx[ 3 ] = static_cast<uint32_t>( firstIndex + ( i + 1 ) + ( j + 1 ) * sizeInVertices.second );

				winding_t winding;

				switch ( info.normalDirection )
				{
					case NORMAL_X_POS:
					case NORMAL_Y_NEG:	
					case NORMAL_Z_POS:
						winding = info.winding == WINDING_COUNTER_CLOCKWISE ? WINDING_COUNTER_CLOCKWISE : WINDING_CLOCKWISE;
						break;
					case NORMAL_X_NEG:
					case NORMAL_Y_POS:
					case NORMAL_Z_NEG:
						winding = info.winding == WINDING_COUNTER_CLOCKWISE ? WINDING_CLOCKWISE : WINDING_COUNTER_CLOCKWISE;
						break;
				}

				if( winding == WINDING_CLOCKWISE )
				{
					ib[ indicesCnt++ ] = vIx[ 0 ];
					ib[ indicesCnt++ ] = vIx[ 2 ];
					ib[ indicesCnt++ ] = vIx[ 1 ];

					ib[ indicesCnt++ ] = vIx[ 1 ];
					ib[ indicesCnt++ ] = vIx[ 2 ];
					ib[ indicesCnt++ ] = vIx[ 3 ];
				}
				else
				{
					ib[ indicesCnt++ ] = vIx[ 0 ];
					ib[ indicesCnt++ ] = vIx[ 1 ];
					ib[ indicesCnt++ ] = vIx[ 2 ];

					ib[ indicesCnt++ ] = vIx[ 2 ];
					ib[ indicesCnt++ ] = vIx[ 1 ];
					ib[ indicesCnt++ ] = vIx[ 3 ];
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