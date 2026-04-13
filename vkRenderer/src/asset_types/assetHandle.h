#pragma once

#include "asset.h"
#include "assetLib.h"
#include <GfxCore/core/handle.h>

template< class AssetType >
class Asset;

template< class AssetType >
class AssetHandle
{
private:
	AssetLib<AssetType>*	lib;
	hdl_t					handle
public:
	static AssetHandle<AssetType> Invalid()
	{
		return AssetHandle<AssetType>();
	}

	AssetHandle()
	{
		lib = nullptr;
		handle = INVALID_HDL;
	}

	AssetHandle( hdl_t _handle, AssetLib<AssetType>& _lib ) : handle( _handle ), lib( &lib )
	{}

	AssetHandle( const AssetHandle<AssetType>& asset )
	{
		lib = asset.lib;
		handle = asset.handle;
	}

	AssetHandle<AssetType>& operator=( const AssetHandle<AssetType>& rhs )
	{
		if ( this != &rhs )
		{
			lib = rhs.lib;
			handle = rhs.handle;
		}
		return *this;
	}

	hdl_t GetHandle() const
	{
		return handle;
	}

	Asset<AssetType>* Resolve()
	{
		return ( lib != nullptr ) ? lib.Find( handle ) : nullptr;
	}
};
