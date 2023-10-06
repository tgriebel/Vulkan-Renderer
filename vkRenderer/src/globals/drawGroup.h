#pragma once

#include <cstdint>
#include <gfxcore/scene/scene.h>
#include "common.h"

class GpuBuffer;
class ShaderBindParms;
class GeometryContext;

const static uint32_t KeyMaterialBits = 32;
const static uint32_t KeyStencilBits = 8;

union sortKey_t
{
	struct
	{
		uint64_t	materialId : KeyMaterialBits;
		uint64_t	stencilBit : KeyStencilBits;
	};
	uint64_t	key;
};


struct drawSurf_t
{
	sortKey_t			sortKey;
	uint32_t			uploadId;
	uint32_t			objectOffset;
	renderFlags_t		flags;
	uint8_t				stencilBit;

	const char*			dbgName;

	hdl_t				pipelineObject;
};
static_assert( sizeof( drawSurf_t ) == 40, "Informative" );


inline uint32_t Hash( const drawSurf_t& surf ) {
	uint64_t shaderIds;
	shaderIds = surf.pipelineObject.Get();
	uint32_t shaderHash = Hash( reinterpret_cast<const uint8_t*>( &shaderIds ), sizeof( shaderIds ) );
	uint32_t stateHash = Hash( reinterpret_cast<const uint8_t*>( &surf ), offsetof( drawSurf_t, dbgName ) );
	return ( shaderHash ^ stateHash );
}


struct drawSurfInstance_t
{
	mat4x4f		modelMatrix;
	uint16_t	surfId;
	uint16_t	id;
};
static_assert( sizeof( drawSurfInstance_t ) == 68, "Informative" );


inline bool operator==( const drawSurf_t& lhs, const drawSurf_t& rhs )
{
	bool isEqual = true;
	const uint32_t sizeBytes = sizeof( drawSurf_t );
	const uint8_t* lhsBytes = reinterpret_cast<const uint8_t*>( &lhs );
	const uint8_t* rhsBytes = reinterpret_cast<const uint8_t*>( &rhs );
	for ( int i = 0; i < sizeBytes; ++i ) {
		isEqual = isEqual && ( lhsBytes[ i ] == rhsBytes[ i ] );
	}
	return isEqual;
}


inline bool operator<( const drawSurf_t& surf0, const drawSurf_t& surf1 )
{
	if ( surf0.sortKey.materialId == surf1.sortKey.materialId ) {
		return ( surf0.objectOffset < surf1.objectOffset );
	}
	else {
		return ( surf0.sortKey.materialId < surf1.sortKey.materialId );
	}
}


template<> struct std::hash<drawSurf_t> {
	size_t operator()( drawSurf_t const& surf ) const {
		return Hash( reinterpret_cast<const uint8_t*>( &surf ), sizeof( surf ) );
	}
};


class DrawGroup
{
private:
	const GeometryContext*	geo;

	uint32_t				committedModelCount;
	uint32_t				mergedModelCount;
	uint32_t				instanceCounts[ MaxSurfaces ];
	drawSurf_t				surfaces[ MaxSurfaces ];
	drawSurf_t				sortedSurfaces[ MaxSurfaces ];
	drawSurf_t				merged[ MaxSurfaces ];
	surfaceUpload_t			uploads[ MaxSurfaces ];
	drawSurfInstance_t		instances[ MaxSurfaces ];
	drawSurfInstance_t		sortedInstances[ MaxSurfaces ];

public:

	DrawGroup()
	{
		committedModelCount = 0;
		mergedModelCount = 0;

		geo = nullptr;

		memset( surfaces,			0, MaxSurfaces );
		memset( sortedSurfaces,		0, MaxSurfaces );
		memset( merged,				0, MaxSurfaces );
		memset( uploads,			0, MaxSurfaces );
		memset( sortedInstances,	0, MaxSurfaces );
		memset( instances,			0, MaxSurfaces );
		memset( instanceCounts,		0, MaxSurfaces );
	}

	void			Sort();
	void			Merge();

	inline void	Reset()
	{
		committedModelCount = 0;
		mergedModelCount = 0;
	}

	inline uint32_t	InstanceCount() const
	{
		return committedModelCount;
	}

	inline uint32_t	Count() const
	{
		return mergedModelCount;
	}

	inline const drawSurfInstance_t* Instances() const
	{
		assert( instanceModelCount > 0 );
		return &sortedInstances[ 0 ];
	}

	inline const GeometryContext* Geometry() const
	{
		assert( geo != nullptr );
		return geo;
	}

	inline uint32_t InstanceId( const uint32_t instanceIx ) const
	{
		const drawSurf_t& surf = merged[ sortedInstances[ instanceIx ].surfId ];
		const uint32_t objectId = ( sortedInstances[ instanceIx ].id + surf.objectOffset );
		return objectId;
	}

	inline const drawSurf_t& DrawSurf( const uint32_t surfIx ) const
	{
		return merged[ surfIx ];
	}

	inline const surfaceUpload_t& SurfUpload( const uint32_t surfIx ) const
	{
		return uploads[ surfIx ];
	}

	inline const uint32_t InstanceCount( const uint32_t surfIx ) const
	{
		return instanceCounts[ surfIx ];
	}

	inline void Add( const drawSurf_t& surf, const drawSurfInstance_t& instance )
	{
		assert( committedModelCount < MaxSurfaces );

		surfaces[ committedModelCount ] = surf;
		instances[ committedModelCount ] = instance;

		++committedModelCount;
	}

	void AssignGeometryResources( const GeometryContext* context );

	//ShaderBindParms*		parms;	// A draw group has coherent raster state so share parms. Anything that doesn't goes to a new group
};