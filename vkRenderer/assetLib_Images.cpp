#include "assetLib_Images.h"

void AssetLibImages::Create()
{
}

void AssetLibImages::Add( const char* name, const texture_t& texture )
{
	textures.push_back( texture );
	names.push_back( name );
}

int AssetLibImages::FindId( const char* name )
{
	auto it = find( names.begin(), names.end(), name );
	const size_t idx = static_cast<uint32_t>( std::distance( names.begin(), it ) );
	return ( it != names.end() ) ? idx : -1;
}

texture_t* AssetLibImages::Find( const char* name )
{
	const int id = FindId( name );
	return ( id >= 0 ) ? &textures[ id ] : NULL;
}