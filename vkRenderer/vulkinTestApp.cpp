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

#include "stdafx.h"

#include <map>
#include <thread>
#include <chrono>
#include <mutex>
#include "src/globals/common.h"
#include <gfxcore/scene/camera.h>
#include <gfxcore/scene/scene.h>
#include <gfxcore/scene/assetManager.h>
#include <gfxcore/core/assetLib.h>
#include "src/globals/render_util.h"
#include "src/render_state/deviceContext.h"
#include "src/render_binding/pipeline.h"
#include "src/render_core/swapChain.h"
#include "window.h"
#include "src/render_state/FrameState.h"
#include "src/globals/renderConstants.h"
#include "src/render_core/renderer.h"
#include "src/render_binding/allocator.h"
#include "src/render_binding/gpuResources.h"
#include "scenes/sceneParser.h"
#include "scenes/chessScene.h"
#include <gfxcore/asset_types/gpuProgram.h>
#include <SysCore/systemUtils.h>
#include <gfxcore/io/serializeClasses.h>

AssetManager						g_assets;
Scene*								g_scene;
Renderer							g_renderer;
Window								g_window;

static SpinLock						acquireNextFrame;

static std::string sceneFile = "chess.json";

#if defined( USE_IMGUI )
imguiControls_t g_imguiControls;
#endif

void CreateCodeAssets();
void UpdateScene( Scene* scene );
void InitScene( Scene* scene );
void ShutdownScene( Scene* scene );
void DrawSceneDebugMenu();
void DrawAssetDebugMenu();
void DrawManipDebugMenu();
void DrawEntityDebugMenu();
void DrawOutlinerDebugMenu();
void DeviceDebugMenu();

void RenderThread()
{
}

void CheckReloadAssets()
{
	if ( g_imguiControls.rebuildShaders ) {
		system( "glsl_compile.bat" );
		g_assets.gpuPrograms.UnloadAll();
		g_assets.gpuPrograms.LoadAll();
		g_renderer.CreatePipelineObjects();

		g_imguiControls.rebuildShaders = false;
	}
}

#include <chrono>
#include <ctime>

std::vector<bakedAssetInfo_t> assetInfo;
Serializer* s;

template<class T>
void BakeLibraryAssets( AssetLib<T>& lib, const std::string& path, const std::string& ext )
{
	assert( s->GetMode() == serializeMode_t::STORE );

	auto time = std::chrono::system_clock::now();
	std::time_t date = std::chrono::system_clock::to_time_t( time );
	char dateCStr[ 128 ];
	ctime_s( dateCStr, 128, &date );

	const uint32_t count = lib.Count();
	for ( uint32_t i = 0; i < count; ++i )
	{
		Asset<T>* asset = lib.Find( i );

		bakedAssetInfo_t info = {};
		info.name = asset->GetName();
		info.hash = asset->Handle().String();
		info.type = lib.AssetTypeName();
		info.date = std::string( dateCStr );
		info.sizeBytes = s->CurrentSize();

		s->Clear( false );
		s->NextString( info.name );
		s->NextString( info.type );
		s->NextString( info.date );
		asset->Get().Serialize( s );
		s->WriteFile( path + asset->Handle().String() + ext );

		assetInfo.push_back( info );
	}
}


void BakeAssets()
{	
	s = new Serializer( MB( 128 ), serializeMode_t::STORE );
	assetInfo.reserve( 1000 );

	MakeDirectory( BakePath );
	MakeDirectory( BakePath + TexturePath );
	MakeDirectory( BakePath + MaterialPath );
	MakeDirectory( BakePath + ModelPath );

	BakeLibraryAssets( g_assets.textureLib, BakePath + TexturePath, BakedTextureExtension );
	BakeLibraryAssets( g_assets.materialLib, BakePath + MaterialPath, BakedMaterialExtension );
	BakeLibraryAssets( g_assets.modelLib, BakePath + ModelPath, BakedModelExtension );

	std::ofstream assetFile( BakePath + "asset_info.csv", std::ios::out | std::ios::trunc );
	assetFile << "Name,Type,Hash,Data,Size\n";
	for( auto it = assetInfo.begin(); it != assetInfo.end(); ++it )
	{
		const bakedAssetInfo_t& asset = *it;
		assetFile << asset.name << "," << asset.type << "," << asset.hash << "," << asset.sizeBytes << "," << asset.date; // date has an end-line char
	}
	assetFile.close();

	delete s;
	exit(0);
}


int main( int argc, char* argv[] )
{
	CreateCodeAssets();
	if( argc == 2 ) {
		LoadScene( argv[1], &g_scene, &g_assets );
	} else {
		LoadScene( sceneFile, &g_scene, &g_assets );
	}

	std::thread renderThread( RenderThread );

	g_window.Init();
	InitScene( g_scene );

	const bool bake = false;
	if( bake ) {
		BakeAssets();
	}

	try
	{
		g_renderer.Init();
		g_renderer.AttachDebugMenu( &DrawSceneDebugMenu );
		g_renderer.AttachDebugMenu( &DrawAssetDebugMenu );
		g_renderer.AttachDebugMenu( &DrawManipDebugMenu );
		g_renderer.AttachDebugMenu( &DrawEntityDebugMenu );
		g_renderer.AttachDebugMenu( &DrawOutlinerDebugMenu );
		g_renderer.AttachDebugMenu( &DeviceDebugMenu );

		while ( g_window.IsOpen() )
		{
			CheckReloadAssets();

			g_window.PumpMessages();

			if( g_window.IsResizeRequested() )
			{
				g_renderer.Resize();
				g_window.CompleteImageResize();
			}

			if ( g_imguiControls.openModelImportFileDialog )
			{
				std::vector<const char*> filters;
				filters.push_back( "*.obj" );
				std::string path = g_window.OpenFileDialog( "Import Obj", filters, "Model files (*.obj)" );
				std::string dir;
				std::string file;

				SplitPath( path, dir, file );

				std::string modelName = path;

				ModelLoader* loader = new ModelLoader();
				loader->SetModelPath( dir );
				loader->SetTexturePath( dir );
				loader->SetModelName( file );
				loader->SetAssetRef( &g_assets );
				g_assets.modelLib.AddDeferred( modelName.c_str(), loader_t( loader ) );

				g_assets.RunLoadLoop();
				Entity* ent = new Entity();
				ent->name = path;
				//ent->SetFlag( ENT_FLAG_DEBUG );
				g_scene->entities.push_back( ent );
				g_scene->CreateEntityBounds( g_assets.modelLib.RetrieveHdl( modelName.c_str() ), *ent );

				g_imguiControls.openModelImportFileDialog = false;
			}

			if ( g_imguiControls.openSceneFileDialog )
			{
				std::vector<const char*> filters;
				filters.push_back( "*.json" );
				std::string path = g_window.OpenFileDialog( "Open Scene", filters, "Scene files" );
				
				std::string dir;
				std::string file;
				SplitPath( path, dir, file );
		
				ShutdownScene( g_scene );
				delete g_scene;
				g_scene = nullptr;
				g_renderer.ShutdownGPU();
				g_assets.Clear();

				CreateCodeAssets();
				LoadScene( file, &g_scene, &g_assets );
				InitScene( g_scene );
		
				g_renderer.InitGPU();

				g_imguiControls.openSceneFileDialog = false;
			}

			if( g_imguiControls.reloadScene )
			{
				g_imguiControls.reloadScene = true;
			}

			UpdateScene( g_scene );
			g_window.input.NewFrame();
			g_renderer.RenderScene( g_scene );
#if defined( USE_IMGUI )
			ImGui_ImplGlfw_NewFrame();
#endif
			g_scene->AdvanceFrame();
		}
		g_renderer.Shutdown();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		renderThread.join();
		return EXIT_FAILURE;
	}
	renderThread.join();
	return EXIT_SUCCESS;
}