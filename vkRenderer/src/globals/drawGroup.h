#pragma once

#include <cstdint>
#include <gfxcore/scene/scene.h>
#include "common.h"

class GpuBuffer;

const static uint32_t KeyMaterialBits = 32;
const static uint32_t KeyStencilBits = 8;

union sortKey_t
{
	uint64_t	materialId : KeyMaterialBits;
	uint64_t	stencilBit : KeyStencilBits;
	uint64_t	key;
};


struct drawSurf_t
{
	uint32_t			uploadId;
	uint32_t			objectId;
	sortKey_t			sortKey;
	renderFlags_t		flags;
	uint8_t				stencilBit;
	uint32_t			hash;

	const char*			dbgName;

	hdl_t				pipelineObject[ DRAWPASS_COUNT ];
};


inline uint32_t Hash( const drawSurf_t& surf ) {
	uint64_t shaderIds[ DRAWPASS_COUNT ];
	for ( uint32_t i = 0; i < DRAWPASS_COUNT; ++i ) {
		shaderIds[ i ] = surf.pipelineObject[ i ].Get();
	}
	uint32_t shaderHash = Hash( reinterpret_cast<const uint8_t*>( &shaderIds ), sizeof( shaderIds[ 0 ] ) * DRAWPASS_COUNT );
	uint32_t stateHash = Hash( reinterpret_cast<const uint8_t*>( &surf ), offsetof( drawSurf_t, hash ) );
	return ( shaderHash ^ stateHash );
}


struct drawSurfInstance_t
{
	mat4x4f		modelMatrix;
	uint32_t	surfId;
	uint32_t	id;
};


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
		return ( surf0.objectId < surf1.objectId );
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
public:

	DrawGroup()
	{
		committedModelCnt = 0;
		mergedModelCnt = 0;

		ib = nullptr;
		vb = nullptr;

		memset( surfaces,			0, MaxSurfaces );
		memset( sortedSurfaces,		0, MaxSurfaces );
		memset( merged,				0, MaxSurfaces );
		memset( uploads,			0, MaxSurfaces );
		memset( sortedInstances,	0, MaxSurfaces );
		memset( instances,			0, MaxSurfaces );
		memset( instanceCounts,		0, MaxSurfaces );
	}

	void	Sort();
	void	Merge();

	GpuBuffer*				ib;	// FIXME: don't use a pointer
	GpuBuffer*				vb;

	uint32_t				committedModelCnt;
	uint32_t				mergedModelCnt;
	drawSurf_t				surfaces[ MaxSurfaces ];
	drawSurf_t				sortedSurfaces[ MaxSurfaces ];
	drawSurfInstance_t		instances[ MaxSurfaces ];
	drawSurfInstance_t		sortedInstances[ MaxSurfaces ];
	drawSurf_t				merged[ MaxSurfaces ];
	surfaceUpload_t			uploads[ MaxSurfaces ];
	uint32_t				instanceCounts[ MaxSurfaces ];
};