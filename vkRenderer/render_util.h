#pragma once
#include <atomic>
#include <chrono>
#include "common.h"
#include "GeoBuilder.h"
#include <resource_types/texture.h>
#include <resource_types/model.h>

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


class SkyBoxLoader : public LoadHandler<Model>
{
private:
	bool Load( Model& model );
public:
};


class TerrainLoader : public LoadHandler<Model>
{
private:
	bool Load( Model& model );
public:
};


class WaterLoader : public LoadHandler<Model>
{
private:
	bool Load( Model& model );
public:
};


class QuadLoader : public LoadHandler<Model>
{
private:
	bool Load( Model& model );
public:
};

mat4x4f MatrixFromVector( const vec3f& v );
void CreateQuadSurface2D( const std::string& materialName, Model& outModel, vec2f& origin, vec2f& size );