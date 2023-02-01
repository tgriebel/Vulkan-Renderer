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
#include <scene/camera.h>
#include <scene/scene.h>
#include <scene/assetManager.h>
#include <core/assetLib.h>
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
#include <resource_types/gpuProgram.h>
#include <SysCore/systemUtils.h>

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
void InitScene( Scene* scene );

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
		gAssets.gpuPrograms.UnloadAll();
		gAssets.gpuPrograms.LoadAll();
		Renderer::GenerateGpuPrograms( gAssets.gpuPrograms );
		gRenderer.CreatePipelineObjects();

		gImguiControls.rebuildShaders = false;
	}
}

int main( int argc, char* argv[] )
{
	CreateCodeAssets();
	if( argc == 2 ) {
		LoadScene( argv[1], &gScene, &gAssets );
	} else {
		LoadScene( "chess.json", &gScene, &gAssets );
	}

	std::thread renderThread( RenderThread );

	gWindow.Init();
	InitScene( gScene );

	try
	{
		gRenderer.Init();
		gRenderer.UploadAssets( gAssets );

		while ( gWindow.IsOpen() )
		{
			CheckReloadAssets();

			gWindow.PumpMessages();

			if ( gImguiControls.openFileDialog ) {
				std::string path = gWindow.OpenFileDialog();
				std::string dir;
				std::string file;

				SplitPath( path, dir, file );

				std::string modelName = path;

				ModelLoader* loader = new ModelLoader();
				loader->SetModelPath( dir );
				loader->SetTexturePath( dir );
				loader->SetModelName( file );
				loader->SetAssetRef( &gAssets );
				gAssets.modelLib.AddDeferred( modelName.c_str(), loader_t( loader ) );

				gAssets.RunLoadLoop();
				Entity* ent = new Entity();
				ent->name = path;
				//ent->SetFlag( ENT_FLAG_DEBUG );
				gScene->entities.push_back( ent );
				gScene->CreateEntityBounds( gAssets.modelLib.RetrieveHdl( modelName.c_str() ), *ent );

				gImguiControls.openFileDialog = false;
			}

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