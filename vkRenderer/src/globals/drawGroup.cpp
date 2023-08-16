#include <numeric>
#include "DrawGroup.h"

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

	std::vector<uint32_t> sortIndices( committedModelCnt );
	std::iota( sortIndices.begin(), sortIndices.end(), 0 );

	std::sort( sortIndices.begin(), sortIndices.end(), Comparator( this ) );

	for ( uint32_t srcIndex = 0; srcIndex < committedModelCnt; ++srcIndex )
	{
		const uint32_t dstIndex = sortIndices[ srcIndex ];
		sortedSurfaces[ dstIndex ] = surfaces[ srcIndex ];
		sortedInstances[ dstIndex ] = instances[ srcIndex ];
	}
}


void DrawGroup::Merge()
{
	mergedModelCnt = 0;
	std::unordered_map< uint32_t, uint32_t > uniqueSurfs;
	uniqueSurfs.reserve( committedModelCnt );
	for ( uint32_t i = 0; i < committedModelCnt; ++i ) {
		drawSurfInstance_t& instance = sortedInstances[ i ];
		auto it = uniqueSurfs.find( sortedSurfaces[ i ].hash );
		if ( it == uniqueSurfs.end() ) {
			const uint32_t surfId = mergedModelCnt;
			uniqueSurfs[ sortedSurfaces[ i ].hash ] = surfId;

			instanceCounts[ surfId ] = 1;
			merged[ surfId ] = sortedSurfaces[ i ];

			instance.id = 0;
			instance.surfId = surfId;

			++mergedModelCnt;
		}
		else {
			instance.id = instanceCounts[ it->second ];
			instance.surfId = it->second;
			instanceCounts[ it->second ]++;
		}
	}
	uint32_t totalCount = 0;
	for ( uint32_t i = 0; i < mergedModelCnt; ++i )
	{
		merged[ i ].objectId += totalCount;
		totalCount += instanceCounts[ i ];
	}
}