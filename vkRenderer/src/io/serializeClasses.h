#pragma once

#include <syscore/common.h>

template<class T>
bool LoadBaked( Asset<T>& asset, bakedAssetInfo_t& info, const std::string& dir, const std::string& ext )
{
	if( AreBakedAssetsEnabled() == false ) {
		return false;
	}

	const hdl_t handle = asset.Handle();
	const std::string hash = handle.String();
	const std::string bakedPath = dir + hash + "." + ext;
	if ( FileExists( bakedPath ) )
	{
		Serializer s( MB( 32 ), serializeMode_t::LOAD );
		s.ReadFile( bakedPath );

		s.SetPosition( 0 );
		s.NextString( info.name );
		s.NextString( info.type );
		s.NextString( info.date );

		asset.Serialize( &s );

		assert( info.name.length() > 0 );

		asset.Rename( info.name );

		info.sizeBytes = s.CurrentSize();
		info.hash = Library::Handle( info.name.c_str() ).String();

		const uint32_t currentHash = s.Hash();

		uint32_t byteCount;
		uint32_t dataHash;
		s.Next( byteCount );
		s.Next( dataHash );

		if( currentHash != dataHash ) {
			return false;
		}

		if ( info.sizeBytes != byteCount ) {
			return false;
		}

		const bool loaded = ( s.Status() == serializeStatus_t::OK );
		assert( loaded );
		return loaded;
	}
	else
	{
		std::stringstream ss;
		ss << "Baked file not found: " << bakedPath << " for asset " << asset.GetName() << "\n";
		std::cout << ss.str();
	}
	return false;
}
