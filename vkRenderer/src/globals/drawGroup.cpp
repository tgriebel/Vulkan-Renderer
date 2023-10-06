#include <numeric>
#include "DrawGroup.h"
#include "../render_core/renderer.h"

void DrawGroup::Sort()
{
	class Comparator
	{
	private:
		DrawGroup* drawgroup;
	public:
		Comparator( DrawGroup* drawgroup ) : drawgroup( drawgroup ) {}
		bool operator()( const uint32_t ix0, const uint32_t ix1 )
		{
			const auto lhs = drawgroup->surfaces[ ix0 ].sortKey.key;
			const auto rhs = drawgroup->surfaces[ ix1 ].sortKey.key;
			return ( lhs < rhs );
		}
	};

	std::vector<uint32_t> sortIndices( committedModelCount );
	std::iota( sortIndices.begin(), sortIndices.end(), 0 );

	std::sort( sortIndices.begin(), sortIndices.end(), Comparator( this ) );

	for ( uint32_t srcIndex = 0; srcIndex < committedModelCount; ++srcIndex )
	{
		const uint32_t dstIndex = sortIndices[ srcIndex ];
		sortedSurfaces[ dstIndex ] = surfaces[ srcIndex ];
		sortedInstances[ dstIndex ] = instances[ srcIndex ];
	}
}


void DrawGroup::Merge()
{
	mergedModelCount = 0;
	std::unordered_map< uint32_t, uint32_t > uniqueSurfs;
	uniqueSurfs.reserve( committedModelCount );

	uint32_t surfaceHashes[ MaxSurfaces ];
	for ( uint32_t i = 0; i < committedModelCount; ++i ) {
		surfaceHashes[ i ] = Hash( sortedSurfaces[ i ] );
	}

	for ( uint32_t i = 0; i < committedModelCount; ++i ) {
		drawSurfInstance_t& instance = sortedInstances[ i ];
		auto it = uniqueSurfs.find( surfaceHashes[ i ] );
		if ( it == uniqueSurfs.end() ) {
			const uint32_t surfId = mergedModelCount;
			uniqueSurfs[ surfaceHashes[ i ] ] = surfId;

			instanceCounts[ surfId ] = 1;
			merged[ surfId ] = sortedSurfaces[ i ];

			instance.id = 0;
			instance.surfId = surfId;

			++mergedModelCount;
		}
		else {
			instance.id = instanceCounts[ it->second ];
			instance.surfId = it->second;
			instanceCounts[ it->second ]++;
		}
	}
	uint32_t totalCount = 0;
	for ( uint32_t i = 0; i < mergedModelCount; ++i )
	{
		merged[ i ].objectOffset += totalCount;
		totalCount += instanceCounts[ i ];
	}
}


void DrawGroup::AssignGeometryResources( const GeometryContext* context )
{
	geo = context;

	// Cache upload records
	for ( uint32_t i = 0; i < mergedModelCount; ++i ) {
		uploads[ i ] = geo->surfUploads[ merged[ i ].uploadId ];
	}
}