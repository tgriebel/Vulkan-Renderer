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
#include <syscore/systemUtils.h>
#include <gfxcore/scene/scene.h>
#include <gfxcore/scene/assetManager.h>
#include "window.h"
#include "src/globals/renderConstants.h"
#include "src/render_core/renderer.h"
#include "scenes/sceneParser.h"
#include <SysCore/systemUtils.h>
#include <gfxcore/scene/assetBaker.h>
#include "raytracerInterface.h"
#include "src/app/cvar.h"

AssetManager						g_assets;
Scene*								g_scene;
Renderer							g_renderer;
Window								g_window;

static std::string sceneFile = "chess.json";

#if defined( USE_IMGUI )
imguiControls_t g_imguiControls;
#endif

void CreateCodeAssets();
void UpdateScene( Scene* scene );
void InitScene( Scene* scene );
void ShutdownScene( Scene* scene );

void RenderThread()
{
}

void CheckReloadAssets()
{
#if defined( USE_IMGUI )
	if ( g_imguiControls.rebuildShaders )
	{
		g_assets.gpuPrograms.UnloadAll();
		g_assets.gpuPrograms.LoadAll( true );

		g_imguiControls.rebuildShaders = false;
	}

	if( g_imguiControls.shaderHdl != INVALID_HDL )
	{
		Asset<GpuProgram>* prog = g_assets.gpuPrograms.Find( g_imguiControls.shaderHdl );
		prog->Reload( true );
		g_imguiControls.shaderHdl = INVALID_HDL;
	}
#endif
}

void BakeAssets()
{	
	AssetBaker baker;
	baker.AddBakeDirectory( BakePath );
	baker.AddAssetLib( &g_assets.modelLib, ModelPath, BakedModelExtension );
	baker.AddAssetLib( &g_assets.materialLib, MaterialPath, BakedMaterialExtension );
	baker.AddAssetLib( &g_assets.textureLib, TexturePath, BakedTextureExtension );

	baker.Bake();
}

MakeCVar( bool,		r_cubeCapture );
MakeCVar( bool,		r_computeDiffuseIbl );
MakeCVar( char*,	c_scene );
MakeCVar( bool,		c_bakeAssets );
MakeCVar( bool,		r_shadows );
MakeCVar( bool,		r_downsampleScene );
 
void ParseCmdArgs( const int argc, char* argv[] )
{
	for ( int32_t i = 1; i < argc; ++i ) {
		CVar::ParseCommand( argv[ i ] );
	}
}

int main( int argc, char* argv[] )
{
	CreateCodeAssets(); // TODO: Check render dependencies, may need to move into render init?

	ParseCmdArgs( argc, argv );

	if( c_scene.IsValid() ) {
		LoadScene( c_scene.GetString(), &g_scene, &g_assets );
	} else {
		LoadScene( sceneFile, &g_scene, &g_assets );
	}

	renderConfig_t config {};
	config.useCubeViews = r_cubeCapture.GetBool();
	config.computeDiffuseIbl = r_computeDiffuseIbl.GetBool();
	config.shadows = r_shadows.GetBool();
	config.downsampleScene = r_downsampleScene.GetBool();

	std::thread renderThread( RenderThread );

	InitScene( g_scene );

	if( c_bakeAssets.GetBool() ) {
		BakeAssets();
		exit( 0 );
	}

	g_window.Init();

	try
	{
		g_renderer.Init( config );

		while ( g_window.IsOpen() )
		{
			CheckReloadAssets();

			g_window.PumpMessages();

			if( g_window.IsResizeRequested() )
			{
				g_renderer.Resize();
				g_window.CompleteImageResize();
			}

#if defined( USE_IMGUI )
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
#endif

			UpdateScene( g_scene );

#if defined( USE_IMGUI )
			if ( g_imguiControls.rebuildRaytraceScene ) {
				BuildRayTraceScene( g_scene );
			}

			if ( g_imguiControls.raytraceScene ) {
				TraceScene( false );
			}

			if ( g_imguiControls.rasterizeScene ) {
				TraceScene( true );
			}
#endif
			
			g_window.BeginFrame();

			g_renderer.Commit( g_scene );
			g_renderer.Render();

			g_scene->AdvanceFrame();
			g_window.EndFrame();
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