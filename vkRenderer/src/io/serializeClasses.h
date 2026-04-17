#pragma once

#include <filesystem>
#include <chrono>
#include <syscore/common.h>


inline bool IsBakedAssetFresh( const sourceFile_t& source, const bakedAssetInfo_t& bakedInfo )
{
	// Check if the name mismtaches
	// If there is no source path, the baked asset is referenced and loaded directly
	const std::string bakedStem = std::filesystem::path( bakedInfo.name ).stem().string();

	if ( ( source.name.empty() == false ) && ( bakedStem != source.name ) ) {
		return false;
	}

	if ( source.path.empty() ) {
		return true;
	}

	std::error_code ec;
	const auto srcFileTime = std::filesystem::last_write_time( source.path, ec );
	if ( ec ) {
		return false;
	}

	int64_t bakeEpoch = 0;
	try {
		bakeEpoch = std::stoll( bakedInfo.date );
	} catch ( ... ) {
		return false;
	}

	const auto clockDelta = srcFileTime - std::filesystem::file_time_type::clock::now();
	const auto srcSysTime = std::chrono::system_clock::now()
		+ std::chrono::duration_cast<std::chrono::system_clock::duration>( clockDelta );
	const int64_t srcEpoch = std::chrono::duration_cast<std::chrono::seconds>(
		srcSysTime.time_since_epoch() ).count();

	return ( srcEpoch <= bakeEpoch );
}


template<class T>
bool LoadBaked( Asset<T>& asset, bakedAssetInfo_t& info, const sourceFile_t& source, const std::string& dir, const std::string& ext )
{
	if( AreBakedAssetsEnabled() == false ) {
		return false;
	}

	const hdl_t handle = asset.Handle();
	const std::string hash = handle.String();
	const std::string bakedPath = dir + hash + "." + ext;
	if( FileExists( bakedPath ) )
	{
		const uint32_t fileSize = static_cast<uint32_t>( std::filesystem::file_size( bakedPath ) );
		Serializer s( fileSize, serializeMode_t::LOAD );
		s.ReadFile( bakedPath );

		s.SetPosition( 0 );
		s.NextString( info.name );
		s.NextString( info.type );
		s.NextString( info.date );

		if( IsBakedAssetFresh( source, info ) == false )
		{
			std::cout << "Baked file out-of-date: " << info.name << " source is newer.\n";
			return false;
		}

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

		if( currentHash != dataHash )
		{
			std::cout << "Baked hash mismatch: " << currentHash << " != " << dataHash << "\n";
			return false;
		}

		if( info.sizeBytes != byteCount )
		{
			std::cout << "Baked byte size mismatch: " << info.sizeBytes << " != " << byteCount << "\n";
			return false;
		}

		const bool loaded = ( s.Status() == serializeStatus_t::OK );
		assert( loaded );
		return loaded;
	}
	else
	{
		std::cout << "Baked file not found: " << bakedPath << " for asset " << asset.GetName() << "\n";		
	}
	return false;
}
