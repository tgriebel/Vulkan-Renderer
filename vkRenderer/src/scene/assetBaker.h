#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>
#include <chrono>

#include <SysCore/common.h>
#include <SysCore/serializer.h>
#include <SysCore/systemUtils.h>

#include "../asset_types/assetLib.h"

class Model;
class Material;
class Image;
class GpuProgram;

struct bakedAssetInfo_t
{
	std::string		name;
	std::string		hash;
	std::string		type;
	std::string		date;		// Unix epoch seconds as string
	uint32_t		sizeBytes;
	uint64_t		dataHash;
};

struct sourceFile_t
{
	std::string		path;
	std::string		name;
	std::string		type;
};

void ToggleBakedLoading( const bool enabled );

bool AreBakedAssetsEnabled();

bool IsBakedAssetFresh( const sourceFile_t& source, const bakedAssetInfo_t& bakedInfo );

class AssetBaker
{
private:
	std::string				m_bakePath;
	std::string				m_modelPath;	
	std::string				m_modelExt;
	std::string				m_materialPath;
	std::string				m_materialExt;
	std::string				m_imagePath;
	std::string				m_imageExt;
	AssetLib<Model>*		m_modelLib;
	AssetLib<Material>*		m_materialLib;
	AssetLib<Image>*		m_imageLib;
public:
	AssetBaker() {}

	void AddAssetLib( AssetLib<Model>* lib, const std::string path, const std::string ext );
	void AddAssetLib( AssetLib<Material>* lib, const std::string path, const std::string ext );
	void AddAssetLib( AssetLib<Image>* lib, const std::string path, const std::string ext );
	void AddBakeDirectory( const std::string path );
	void Bake();
};


template<class T>
bool StoreBaked( Asset<T>& asset, bakedAssetInfo_t& info, const std::string& path, const std::string& ext )
{
	if( asset.CanBake() == false ) {
		return false;
	}

	info.name = asset.GetName();
	info.hash = asset.Handle().String();

	s->Clear( false );
	s->SetPosition( 0 );
	s->NextString( info.name );
	s->NextString( info.type );
	s->NextString( info.date );
	asset.Serialize( s );

	info.sizeBytes = s->CurrentSize();

	uint32_t byteCount = info.sizeBytes;
	uint64_t dataHash = s->Hash();
	s->Next( byteCount );
	s->Next( dataHash );

	info.dataHash = dataHash;

	s->WriteFile( path + asset.Handle().String() + ext );

	return true;
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

	if( SysCore::FileExists( bakedPath ) )
	{
		const uint32_t fileSize = static_cast< uint32_t >( std::filesystem::file_size( bakedPath ) );
		Serializer s( fileSize, serializeMode_t::LOAD );
		s.ReadFile( bakedPath );

		s.SetPosition( 0 );
		s.NextString( info.name );
		s.NextString( info.type );
		s.NextString( info.date );

		if( IsBakedAssetFresh( source, info ) == false ) {
			std::cout << "Baked file out-of-date: " << info.name << " source is newer.\n";
			return false;
		}

		asset.Serialize( &s );

		assert( info.name.length() > 0 );

		asset.Rename( info.name );

		info.sizeBytes = s.CurrentSize();
		info.hash = Library::Handle( info.name.c_str() ).String();

		const uint64_t currentHash = s.Hash();

		uint32_t byteCount;
		uint64_t dataHash;
		s.Next( byteCount );
		s.Next( dataHash );

		if( currentHash != dataHash ) {
			std::cout << "Baked hash mismatch: " << currentHash << " != " << dataHash << "\n";
			return false;
		}

		if( info.sizeBytes != byteCount ) {
			std::cout << "Baked byte size mismatch: " << info.sizeBytes << " != " << byteCount << "\n";
			return false;
		}

		const bool loaded = ( s.Status() == serializeStatus_t::OK );
		assert( loaded );
		return loaded;
	}
	else {
		std::cout << "Baked file not found: " << bakedPath << " for asset " << asset.GetName() << "\n";
	}
	return false;
}
