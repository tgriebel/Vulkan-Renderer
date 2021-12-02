#include "stdafx.h"

#include <map>
#include <thread>
#include <chrono>
#include <mutex>

#include "common.h"
#include "util.h"
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

typedef AssetLib< texture_t >		AssetLibImages;
typedef AssetLib< material_t >		AssetLibMaterials;
typedef AssetLib< GpuProgram >		AssetLibGpuProgram;
typedef AssetLib< modelSource_t >	AssetLibModels;

AssetLibGpuProgram					gpuPrograms;
AssetLibMaterials					materialLib;
AssetLibImages						textureLib;
AssetLibModels						modelLib;
Scene								scene;
Renderer							renderer;

static SpinLock						windowReady;

void MakeBeachScene();
void UpdateScene( const input_t& input, const float dt );

static float AdvanceTime()
{
	static auto prevTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	const float dt = std::chrono::duration<float, std::chrono::milliseconds::period>( currentTime - prevTime ).count();
	prevTime = currentTime;
	return dt;
}

void WindowThread()
{
	context.window.Init();
	windowReady.Unlock();

	while ( context.window.IsOpen() )
	{
		context.window.PumpMessages();
		if( renderer.IsReady() ) {
			ImGui_ImplGlfw_NewFrame();
		}
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
	MakeBeachScene();

	windowReady.Lock();
	std::thread winThread( WindowThread );
	windowReady.Lock();

	try
	{
		renderer.Init();
		while ( context.window.IsOpen() )
		{
			UpdateScene( context.window.input, AdvanceTime() );
			renderer.RenderScene( scene );
		}
		renderer.Destroy();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		winThread.join();
		return EXIT_FAILURE;
	}
	winThread.join();
	return EXIT_SUCCESS;
}