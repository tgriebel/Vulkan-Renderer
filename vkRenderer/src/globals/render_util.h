/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once
#include <atomic>
#include <chrono>
#include "common.h"
#include "../../GeoBuilder.h"
#include <gfxcore/asset_types/texture.h>
#include <gfxcore/asset_types/model.h>

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
	int width;
	int height;
	float cellSize;
	float uvScale;
	hdl_t handle;
	bool Load( Model& model );
public:
	TerrainLoader( const int _width, const int _height, const float _cellSize, const float _uvScale, const hdl_t _handle )
	{
		width = _width;
		height= _height;
		cellSize = _cellSize;
		handle = _handle;
		uvScale = _uvScale;
	}
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
void MatrixToEulerZYX( const mat4x4f& m, float& a, float& b, float& c );
void CreateQuadSurface2D( const std::string& materialName, Model& outModel, vec2f& origin, vec2f& size );