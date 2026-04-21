#include "stdafx.h"

#include <map>
#include <thread>
#include <chrono>
#include <mutex>
#include "src/globals/common.h"
#include "src/globals/assetDefs.h"
#include <syscore/systemUtils.h>
#include "src/scene/sceneBase.h"
#include "src/app/window.h"
#include "src/globals/renderConstants.h"
#include "src/render_core/renderer.h"
#include "scenes/sceneParser.h"
#include <SysCore/systemUtils.h>
#include "src/scene/assetBaker.h"
#include "src/scene/codeAssets.h"
#include <gfxcore/io/serializeClasses.h>
#include "src/app/raytracerInterface.h"
#include "src/app/cvar.h"

#include "scenes/chessScene.h"
#include "scenes/nesScene.h"

#include "src/app/imguiInterface.h"

AssetManager						g_assets;
Scene*								g_scene;
Renderer							g_renderer;
Window								g_window;

using namespace SysCore;


static const char* sceneFile = "chess.json";

#if defined( USE_IMGUI )
imguiControls_t g_imguiControls;
#endif

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
		GpuProgramLib().UnloadAll();
		GpuProgramLib().LoadAll( true );

		g_imguiControls.rebuildShaders = false;
	}

	if( g_imguiControls.shaderHdl != INVALID_HDL )
	{
		Asset<GpuProgram>* prog = GpuProgramLib().Find( g_imguiControls.shaderHdl );
		prog->Reload( true );
		g_imguiControls.shaderHdl = INVALID_HDL;
	}
#endif
}


MakeCVar( BOOL,		r_cubeCapture, false );
MakeCVar( BOOL,		r_writeCubeCapture, false );
MakeCVar( BOOL,		r_computeDiffuseIbl, false );
MakeCVar( BOOL,		r_computeSpecularIbl, false );
MakeCVar( BOOL,		r_computeBrdfLut, false );
MakeCVar( STRING,	c_scene, sceneFile );
MakeCVar( STRING,	r_cubemapName, "chess" );
MakeCVar( BOOL,		c_bakeAssets, false );
MakeCVar( BOOL,		c_loadBakedAssets, true );
MakeCVar( BOOL,		s_threadedLoad, true );
MakeCVar( BOOL,		r_shadows, true );
MakeCVar( BOOL,		r_downsampleScene, true );
MakeCVar( BOOL,		r_bloom, true );
MakeCVar( BOOL,		r_autoExposure, true );
MakeCVar( BOOL,		r_screenshot, true );
MakeCVar( BOOL,		r_gaussianBlur, true );
MakeCVar( INT,		r_fullscreenMode, 0 );
MakeCVar( INT,		r_windowWidth, -1 );
MakeCVar( INT,		r_windowHeight, -1 );
 

void ParseCmdArgs( const int argc, char* argv[] )
{
	for ( int32_t i = 1; i < argc; ++i ) {
		CVar::ParseCommand( argv[ i ] );
	}
}


void ParseConfig( std::string& fileName )
{
	std::ifstream file;

	file.open( fileName );

	if ( !file.is_open() ) {
		throw std::runtime_error( "Failed to open config file!" );
	}

	while ( file.good() )
	{
		std::string line;
		getline( file, line );

		CVar::ParseCommand( line );
	}
	file.close();
}


void InitSceneType( const std::string type, Scene** scene )
{
	if ( type == "chess" ) {
		*scene = new ChessScene();
	}
	else if ( type == "nes" ) {
		*scene = new NesScene();
	}
	else {
		*scene = new Scene();
	}
}


int main( int argc, char* argv[] )
{
	g_assets.RegisterLib<Model>( "Model" );
	g_assets.RegisterLib<Image>( "Image" );
	g_assets.RegisterLib<Material>( "Material" );
	g_assets.RegisterLib<GpuProgram>( "Gpu Program" );

	CreateCodeAssets(); // TODO: Check render dependencies, may need to move into render init?

	if( ( argc > 1 ) && HasSuffix( argv[ 1 ], ".ini" ) )
	{
		std::string fileName = argv[ 1 ];
		ParseConfig( fileName );
	}

	if ( c_bakeAssets.GetBool() || c_loadBakedAssets.GetBool() == false ) {
		ToggleBakedLoading( false );
	}

	if( c_scene.IsValid() ) {
		LoadScene( c_scene.GetString(), &g_scene, &g_assets, InitSceneType );
	} else {
		LoadScene( sceneFile, &g_scene, &g_assets, InitSceneType );
	}

	renderConfig_t config {};
	config.useCubeViews = r_cubeCapture.GetBool() || r_computeDiffuseIbl.GetBool() || r_computeSpecularIbl.GetBool();
	config.writeCubeViews = r_writeCubeCapture.GetBool() || r_computeDiffuseIbl.GetBool() || r_computeSpecularIbl.GetBool();
	config.computeDiffuseIbl = r_computeDiffuseIbl.GetBool();
	config.computeSpecularIBL = r_computeSpecularIbl.GetBool();
	config.shadows = r_shadows.GetBool();
	config.downsampleScene = r_downsampleScene.GetBool();
	config.bloom = r_bloom.GetBool();
	config.autoExposure = r_autoExposure.GetBool();
	config.screenshot = r_screenshot.GetBool();
	config.cubemapName = r_cubemapName.GetString();
	config.computeBrdfLut = r_computeBrdfLut.GetBool();
	config.gaussianBlur = r_gaussianBlur.GetBool();

	std::thread renderThread( RenderThread );

	InitScene( g_scene );

	if( c_bakeAssets.GetBool() )
	{
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

				const hdl_t modelHdl = ModelLib().AddDeferred( file.c_str(), loader_t( loader ) );

				Entity* ent = new Entity();
				ent->name = file;
				

				g_assets.RunLoadLoop();
				//ent->materialHdl = 
				//ent->SetFlag( ENT_FLAG_DEBUG );
				g_scene->entities.push_back( ent );
				g_scene->CreateEntityBounds( modelHdl, *ent );

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
				LoadScene( file, &g_scene, &g_assets, InitSceneType );
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
			//	BuildRayTraceScene( g_scene );
			}

			if ( g_imguiControls.raytraceScene ) {
			//	TraceScene( false );
			}

			if ( g_imguiControls.rasterizeScene ) {
			//	TraceScene( true );
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
