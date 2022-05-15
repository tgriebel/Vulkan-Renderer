#include "stdafx.h"

#include <map>
#include <thread>
#include <chrono>
#include <mutex>

#include "common.h"
#include "render_util.h"
#include "deviceContext.h"
#include "camera.h"
#include "scene.h"
#include "pipeline.h"
#include "swapChain.h"
#include "assetLib.h"
#include "window.h"
#include "FrameState.h"
#include "renderConstants.h"
#include "renderer.h"
#include "allocator.h"
#include "gpuResources.h"

typedef AssetLib< texture_t >		AssetLibImages;
typedef AssetLib< Material >		AssetLibMaterials;
typedef AssetLib< GpuProgram >		AssetLibGpuProgram;
typedef AssetLib< modelSource_t >	AssetLibModels;

AssetLibGpuProgram					gpuPrograms;
AssetLibMaterials					materialLib;
AssetLibImages						textureLib;
AssetLibModels						modelLib;
Scene								scene;
Renderer							renderer;
Window								window;

static SpinLock						acquireNextFrame;

#if defined( USE_IMGUI )
imguiControls_t imguiControls;
#endif

void MakeScene();
void UpdateScene( const float dt );

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
	if ( imguiControls.rebuildShaders ) {
		system( "glsl_compile.bat" );
		gpuPrograms.Destroy();
		gpuPrograms.Create();
		Renderer::GenerateGpuPrograms( gpuPrograms );
		renderer.CreatePipelineObjects();

		imguiControls.rebuildShaders = false;
	}
}

int main()
{
	{
		// Create assets
		// TODO: enforce ordering somehow. This comes after programs and textures
		gpuPrograms.Create();
		textureLib.Create();
		materialLib.Create();
		modelLib.Create();
	}
	MakeScene();

	std::thread renderThread( RenderThread );

	window.Init();

	try
	{
		renderer.Init();
		while ( window.IsOpen() )
		{
			CheckReloadAssets();

			window.PumpMessages();
			UpdateScene( AdvanceTime() );
			window.input.NewFrame();
			renderer.RenderScene( scene );
#if defined( USE_IMGUI )
			ImGui_ImplGlfw_NewFrame();
#endif
		}
		renderer.Destroy();
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