#pragma once

#include "../asset_types/asset.h"
#include "../asset_types/assetLib.h"
#include <cassert>

class AssetManager
{
private:
	std::vector<Library*>	libraries;

	static inline uint32_t NextTypeId()
	{
		static uint32_t counter = 0;
		return counter++;
	}

	template< class T >
	static uint32_t TypeId()
	{
		static uint32_t id = NextTypeId();
		return id;
	}

public:
	~AssetManager()
	{
		for ( Library* lib : libraries )
		{
			delete lib;
		}
		libraries.clear();
	}

	template< class T >
	AssetLib<T>& RegisterLib( const char* name = "" )
	{
		const uint32_t id = TypeId<T>();
		if ( id >= libraries.size() ) {
			libraries.resize( id + 1, nullptr );
		}
		assert( libraries[ id ] == nullptr );
		AssetLib<T>* lib = new AssetLib<T>( name );
		libraries[ id ] = lib;
		return *lib;
	}

	template< class T >
	AssetLib<T>* GetLib()
	{
		const uint32_t id = TypeId<T>();
		assert( id < libraries.size() && libraries[ id ] != nullptr );
		return static_cast< AssetLib<T>* >( libraries[ id ] );
	}

	template< class T >
	const AssetLib<T>* GetLib() const
	{
		const uint32_t id = TypeId<T>();
		assert( id < libraries.size() && libraries[ id ] != nullptr );
		return static_cast< const AssetLib<T>* >( libraries[ id ] );
	}

	Library* FindLibrary( const uint32_t index )
	{
		return ( index < libraries.size() ) ? libraries[ index ] : nullptr;
	}

	uint32_t LibraryCount() const
	{
		return static_cast<uint32_t>( libraries.size() );
	}

	void Clear()
	{
		for ( Library* lib : libraries )
		{
			if ( lib != nullptr ) {
				lib->Clear();
			}
		}
	}

	inline bool HasPendingLoads()
	{
		for ( Library* lib : libraries )
		{
			if ( lib != nullptr && lib->HasPendingLoads() ) {
				return true;
			}
		}
		return false;
	}

	void RunLoadLoop( const uint32_t limit = 12 )
	{
		uint32_t i = 0;
		while ( i < limit )
		{
			bool hasLoads = false;
			for ( Library* lib : libraries )
			{
				if ( lib != nullptr && lib->HasPendingLoads() )
				{
					lib->LoadAll();
					hasLoads = true;
				}
			}
			if ( hasLoads == false ) {
				break;
			}
			++i;
		}
	}
};
