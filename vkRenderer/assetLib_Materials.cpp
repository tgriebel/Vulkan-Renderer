#include <algorithm>

#include "assetLib_Materials.h"
#include "assetLib_GpuProgs.h"

extern AssetLibGpuProgram gpuPrograms;

void AssetLibMaterials::Add( const char* name, const material_t& material )
{
	materials.push_back( material );
	names.push_back( name );
}

int AssetLibMaterials::FindId( const char* name )
{
	auto it = find( names.begin(), names.end(), name );
	const size_t idx = static_cast<uint32_t>( std::distance( names.begin(), it ) );
	return ( it != names.end() ) ? idx : -1;
}

material_t* AssetLibMaterials::Find( const char* name )
{
	const int id = FindId( name );
	return ( id >= 0 ) ? &materials[ id ] : NULL;
}

void AssetLibMaterials::Create()
{
}
