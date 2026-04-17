// AssetConverter - standalone command-line baker.
//
// Loads a scene JSON the same way the main renderer does, then bakes every
// loaded asset to disk. No Vulkan, no window, no config file.
//
//   Usage:
//       AssetConverter.exe <scene.json>
//

#include <iostream>
#include <string>

#include "../vkRenderer/src/scene/assetManager.h"
#include "../vkRenderer/src/scene/sceneBase.h"
#include "../vkRenderer/src/scene/assetBaker.h"
#include "../vkRenderer/src/scene/codeAssets.h"
#include "../vkRenderer/src/app/cvar.h"
#include "../vkRenderer/src/asset_types/model.h"
#include "../vkRenderer/src/asset_types/material.h"
#include "../vkRenderer/src/asset_types/texture.h"
#include "../vkRenderer/src/asset_types/gpuProgram.h"
#include "../vkRenderer/scenes/sceneParser.h"

// Globals expected by shared translation units (asset libraries, scene parser, etc.)
AssetManager	g_assets;
Scene*			g_scene = nullptr;

// CVars referenced by shared translation units.
MakeCVar( BOOL, s_threadedLoad, true );


static void InitSceneType( const std::string type, Scene** scene )
{
	// AssetConverter doesn't need scene subclass behavior; a base Scene is enough
	// to hold entities long enough for asset loading to resolve.
	*scene = new Scene();
}


int main( int argc, char* argv[] )
{
	if ( argc < 2 )
	{
		std::cerr << "Usage: AssetConverter <scene.json>\n";
		return 1;
	}

	const std::string sceneFile = argv[ 1 ];

	g_assets.RegisterLib<Model>( "Model" );
	g_assets.RegisterLib<Image>( "Image" );
	g_assets.RegisterLib<Material>( "Material" );
	g_assets.RegisterLib<GpuProgram>( "Gpu Program" );

	CreateCodeAssets();

	// Always bake from source — skip any pre-baked cache.
	ToggleBakedLoading( false );

	LoadScene( sceneFile, &g_scene, &g_assets, InitSceneType );

	BakeAssets();

	std::cout << "AssetConverter: baked scene '" << sceneFile << "'\n";
	return 0;
}
