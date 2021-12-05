#pragma once
#include "common.h"
#include <map>

template< class Asset >
class AssetLib {
private:
	std::vector< Asset >		assets;
	std::vector< std::string >	names;
public:
	void					Create();
	const Asset*			GetDefault() const { return ( assets.size() > 0 ) ? &assets[ 0 ] : nullptr; };
	uint32_t				Count() { return static_cast<uint32_t>( assets.size() ); }
	void					Add( const char* name, const Asset& asset );
	Asset*					Find( const char* name );
	inline Asset*			Find( const int id ) { return ( id < assets.size() && id >= 0 ) ? &assets[ id ] : nullptr; }
	int						FindId( const char* name );
	const char*				FindName( const int id ) { return ( id < names.size() && id >= 0 ) ? names[ id ].c_str() : ""; }
};

template< class Asset >
void AssetLib< Asset >::Add( const char* name, const Asset& asset )
{
	assets.push_back( asset );
	names.push_back( name );
}

template< class Asset >
int AssetLib< Asset >::FindId( const char* name )
{
	auto it = find( names.begin(), names.end(), name );
	const int idx = static_cast<int>( std::distance( names.begin(), it ) );
	return ( it != names.end() ) ? idx : -1;
}

template< class Asset >
Asset* AssetLib< Asset >::Find( const char* name )
{
	const int id = FindId( name );
	return ( id >= 0 ) ? &assets[ id ] : nullptr;
}