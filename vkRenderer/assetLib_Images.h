#pragma once
#include "common.h"
#include <map>

class AssetLibImages {
public:
	std::vector< texture_t > textures;
	std::vector< std::string > names;
	void				Create();
	const texture_t*	GetDefault() const { return ( textures.size() > 0 ) ? &textures[ 0 ] : NULL; };
	uint32_t			Count() { return static_cast<uint32_t>( textures.size() ); }
	void				Add( const char* name, const texture_t& texture );
	texture_t*			Find( const char* name );
	inline texture_t*	Find( const int id ) { return ( id < textures.size() && id >= 0 ) ? &textures[ id ] : NULL; }
	int					FindId( const char* name );
};