#pragma once

#include "renderer.h"
#include <scene/scene.h>
#include <scene/entity.h>

static RtView rtview;
static RtScene rtScene;

static void BuildRayTraceScene( Scene* scene )
{
	rtScene.scene = scene;
	rtScene.assets = &g_assets;

	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	rtScene.lights.clear();
	rtScene.models.clear();
	rtScene.models.reserve( entCount );
	rtScene.aabb = AABB();

	for ( uint32_t i = 0; i < entCount; ++i )
	{
		RtModel rtModel;
		CreateRayTraceModel( g_assets, scene->entities[ i ], &rtModel );
		rtScene.models.push_back( rtModel );

		AABB& aabb = rtModel.octree.GetAABB();
		rtScene.aabb.Expand( aabb.GetMin() );
		rtScene.aabb.Expand( aabb.GetMax() );
	}

	for ( uint32_t i = 0; i < MaxLights; ++i )
	{
		rtScene.lights.push_back( scene->lights[ i ] );
	}

	const uint32_t texCount = g_assets.textureLib.Count();
	for ( uint32_t i = 0; i < texCount; ++i )
	{
		Asset<Texture>* texAsset = g_assets.textureLib.Find( i );
		Texture& texture = texAsset->Get();

		texture.cpuImage.Init( texture.info.width, texture.info.height );

		for ( uint32_t py = 0; py < texture.info.height; ++py ) {
			for ( uint32_t px = 0; px < texture.info.width; ++px ) {
				RGBA rgba;
				rgba.r = texture.bytes[ ( py * texture.info.width + px ) * 4 + 0 ];
				rgba.g = texture.bytes[ ( py * texture.info.width + px ) * 4 + 1 ];
				rgba.b = texture.bytes[ ( py * texture.info.width + px ) * 4 + 2 ];
				rgba.a = texture.bytes[ ( py * texture.info.width + px ) * 4 + 3 ];
				texture.cpuImage.SetPixel( px, py, rgba );
			}
		}
	}
}


static void TraceScene( const bool rasterize = false )
{
	g_imguiControls.raytraceScene = false;

	rtview.targetSize[ 0 ] = 320;
	rtview.targetSize[ 1 ] = 180;
	//g_window.GetWindowSize( rtview.targetSize[0], rtview.targetSize[1] );
	CpuImage<Color> rtimage( rtview.targetSize[ 0 ], rtview.targetSize[ 1 ], Color::Black, "testRayTrace" );
	{
		rtview.camera = rtScene.scene->camera;
		rtview.viewTransform = rtview.camera.GetViewMatrix();
		rtview.projTransform = rtview.camera.GetPerspectiveMatrix();
		rtview.projView = rtview.projTransform * rtview.viewTransform;
	}

	if ( rasterize ) {
		RasterScene( rtimage, rtview, rtScene, true );
	}
	else {
		TraceScene( rtview, rtScene, rtimage );
	}

	{
		std::stringstream ss;
		const char* name = rtimage.GetName();
		ss << "output/";
		if ( ( name == nullptr ) || ( name == "" ) )
		{
			ss << reinterpret_cast<uint64_t>( &rtimage );
		}
		else
		{
			ss << name;
		}

		ss << ".bmp";

		Bitmap bitmap = Bitmap( rtimage.GetWidth(), rtimage.GetHeight() );
		ImageToBitmap( rtimage, bitmap );
		bitmap.Write( ss.str() );
	}
}