#pragma once
#include <atomic>
#include <chrono>
#include "common.h"
#include "GeoBuilder.h"

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

glm::mat4 MatrixFromVector( const glm::vec3& v );
bool LoadTextureImage( const char* texturePath, texture_t& texture );
bool LoadTextureCubeMapImage( const char* textureBasePath, const char* ext, texture_t& texture );
void CopyGeoBuilderResult( const GeoBuilder& gb, std::vector<VertexInput>& vb, std::vector<uint32_t>& ib );
void CreateSkyBoxSurf( modelSource_t& outModel );
void CreateTerrainSurface( modelSource_t& outModel );
void CreateWaterSurface( modelSource_t& outModel );
void CreateQuadSurface2D( const std::string& materialName, modelSource_t& outModel, glm::vec2& origin, glm::vec2& size );