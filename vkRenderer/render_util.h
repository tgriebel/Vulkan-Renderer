#pragma once
#include <atomic>
#include <chrono>
#include "common.h"
#include "GeoBuilder.h"
#include <resource_types/texture.h>

class SpinLock
{
public:
	SpinLock() {
		lock.store( false );
	}

	void Lock() {
		bool expected = false;
		while ( lock.compare_exchange_strong( expected, true ) == false ) {
			std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
		}
	}
	void Unlock() {
		if( lock.load() ) {
			lock.store( false );
		}
	}
private:
	std::atomic<bool> lock;
};

mat4x4f MatrixFromVector( const vec3f& v );
bool LoadTextureImage( const char* texturePath, texture_t& texture );
bool LoadTextureCubeMapImage( const char* textureBasePath, const char* ext, texture_t& texture );
void CopyGeoBuilderResult( const GeoBuilder& gb, std::vector<vsInput_t>& vb, std::vector<uint32_t>& ib );
void CreateSkyBoxSurf( Model& outModel );
void CreateTerrainSurface( Model& outModel );
void CreateWaterSurface( Model& outModel );
void CreateQuadSurface2D( const std::string& materialName, Model& outModel, vec2f& origin, vec2f& size );