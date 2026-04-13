#include <external/MikkTSpace/mikktspace.h>
#include <GfxCore/primitives/geom.h>

#include <vector>


struct MikkUserData
{
	std::vector<vertex_t>& vertices;
	std::vector<uint32_t>& indices;
};


static int MikkGetNumFaces( const SMikkTSpaceContext* ctx )
{
	MikkUserData* data = static_cast< MikkUserData* >( ctx->m_pUserData );
	return static_cast< int >( data->indices.size() / 3 );
}


static int MikkGetNumVerticesOfFace( const SMikkTSpaceContext* /*ctx*/, int /*face*/ )
{
	return 3; // Triangulated mesh
}


static void MikkGetPosition( const SMikkTSpaceContext* ctx, float outPos[], int face, int vert )
{
	MikkUserData* data = static_cast< MikkUserData* >( ctx->m_pUserData );
	const vertex_t& v = data->vertices[ data->indices[ face * 3 + vert ] ];
	outPos[ 0 ] = v.pos[ 0 ];
	outPos[ 1 ] = v.pos[ 1 ];
	outPos[ 2 ] = v.pos[ 2 ];
}


static void MikkGetNormal( const SMikkTSpaceContext* ctx, float outNorm[], int face, int vert )
{
	MikkUserData* data = static_cast< MikkUserData* >( ctx->m_pUserData );
	const vertex_t& v = data->vertices[ data->indices[ face * 3 + vert ] ];
	outNorm[ 0 ] = v.normal[ 0 ];
	outNorm[ 1 ] = v.normal[ 1 ];
	outNorm[ 2 ] = v.normal[ 2 ];
}


static void MikkGetTexCoord( const SMikkTSpaceContext* ctx, float outUV[], int face, int vert )
{
	MikkUserData* data = static_cast< MikkUserData* >( ctx->m_pUserData );
	const vertex_t& v = data->vertices[ data->indices[ face * 3 + vert ] ];
	outUV[ 0 ] = v.uv[ 0 ];
	outUV[ 1 ] = v.uv[ 1 ];
}


static void MikkSetTSpaceBasic( const SMikkTSpaceContext* ctx, const float tangent[], float sign, int face, int vert )
{
	MikkUserData* data = static_cast< MikkUserData* >( ctx->m_pUserData );
	vertex_t& v = data->vertices[ data->indices[ face * 3 + vert ] ];

	v.tangent[ 0 ] = tangent[ 0 ];
	v.tangent[ 1 ] = tangent[ 1 ];
	v.tangent[ 2 ] = tangent[ 2 ];

	// Reconstruct bitangent: B = sign * cross( N, T )
	v.bitangent[ 0 ] = sign * ( v.normal[ 1 ] * tangent[ 2 ] - v.normal[ 2 ] * tangent[ 1 ] );
	v.bitangent[ 1 ] = sign * ( v.normal[ 2 ] * tangent[ 0 ] - v.normal[ 0 ] * tangent[ 2 ] );
	v.bitangent[ 2 ] = sign * ( v.normal[ 0 ] * tangent[ 1 ] - v.normal[ 1 ] * tangent[ 0 ] );

	const uint32_t signBit = ( Dot( Cross( v.tangent, v.bitangent ), v.normal ) > 0.0f ) ? 0 : 1;
	union tangentBitPack_t
	{
		struct
		{
			uint32_t signBit : 1;
			uint32_t vecBits : 31;
		};
		float value;
	};
	tangentBitPack_t packed;
	packed.value = v.tangent[ 0 ];
	packed.signBit = ( sign >= 0 ) ? 0x00 : 0x01;
	v.tangent[ 0 ] = packed.value;
}


void GenerateMikkTangents( std::vector<vertex_t>& vertices, std::vector<uint32_t>& indices )
{
	MikkUserData userData = { vertices, indices };

	SMikkTSpaceInterface iface = {};
	iface.m_getNumFaces = MikkGetNumFaces;
	iface.m_getNumVerticesOfFace = MikkGetNumVerticesOfFace;
	iface.m_getPosition = MikkGetPosition;
	iface.m_getNormal = MikkGetNormal;
	iface.m_getTexCoord = MikkGetTexCoord;
	iface.m_setTSpaceBasic = MikkSetTSpaceBasic;

	SMikkTSpaceContext ctx = {};
	ctx.m_pInterface = &iface;
	ctx.m_pUserData = &userData;

	genTangSpaceDefault( &ctx );
}
