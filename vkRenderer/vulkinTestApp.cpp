#include "stdafx.h"

#include <map>
#include <thread>
#include <chrono>
#include <mutex>
#include "common.h"
#include <scene/camera.h>
#include <scene/scene.h>
#include <scene/assetManager.h>
#include <core/assetLib.h>
#include "render_util.h"
#include "deviceContext.h"
#include "pipeline.h"
#include "swapChain.h"
#include "window.h"
#include "FrameState.h"
#include "renderConstants.h"
#include "renderer.h"
#include "allocator.h"
#include "gpuResources.h"
#include "scenes/sceneParser.h"
#include "scenes/chessScene.h"
#include <resource_types/gpuProgram.h>

typedef AssetLib< Texture >			AssetLibImages;
typedef AssetLib< Material >		AssetLibMaterials;
typedef AssetLib< GpuProgram >		AssetLibGpuProgram;
typedef AssetLib< Model >			AssetLibModels;

AssetManager						gAssets;
Scene*								gScene;
Renderer							gRenderer;
Window								gWindow;

static SpinLock						acquireNextFrame;

#if defined( USE_IMGUI )
imguiControls_t gImguiControls;
#endif

void CreateCodeAssets();
void UpdateScene( Scene* scene, const float dt );

static float AdvanceTime()
{
	static auto prevTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	const float dt = std::chrono::duration<float, std::chrono::milliseconds::period>( currentTime - prevTime ).count();
	prevTime = currentTime;
	return dt;
}

void RenderThread()
{
}

void CheckReloadAssets() {
	if ( gImguiControls.rebuildShaders ) {
		system( "glsl_compile.bat" );
		gAssets.gpuPrograms.Clear();
		assert( false ); // FIXME
		//LoadShaders( scene.gpuPrograms );
		Renderer::GenerateGpuPrograms( gAssets.gpuPrograms );
		gRenderer.CreatePipelineObjects();

		gImguiControls.rebuildShaders = false;
	}
}

int main()
{
	gScene = new ChessScene();

	CreateCodeAssets();
	LoadScene();

	gScene->Init();

	std::thread renderThread( RenderThread );

	gWindow.Init();

	try
	{
		gRenderer.Init();
		while ( gWindow.IsOpen() )
		{
			CheckReloadAssets();

			gWindow.PumpMessages();
			UpdateScene( gScene, AdvanceTime() );
			gWindow.input.NewFrame();
			gRenderer.RenderScene( gScene );
#if defined( USE_IMGUI )
			ImGui_ImplGlfw_NewFrame();
#endif
		}
		gRenderer.Destroy();
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