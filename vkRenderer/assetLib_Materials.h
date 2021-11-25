#pragma once

#include "common.h"
#include "assetLib_GpuProgs.h"
#include <map>

class AssetLibMaterials {
public:
	std::vector< material_t > materials;
	std::vector< std::string > names;
	void				Create();
	uint32_t			Count() { return static_cast< uint32_t >( materials.size() ); }
	void				Add( const char* name, const material_t& material );
	material_t*			Find( const char* name );
	inline material_t*	Find( const int id ) { return ( id < materials.size() && id >= 0 ) ? &materials[ id ] : NULL; }
	int					FindId( const char* name );
};