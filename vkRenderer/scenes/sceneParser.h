#pragma once
#include <string>

class Scene;
class AssetManager;

typedef void sceneInitializerCallback_t( const std::string type, Scene** scene );

void LoadScene( std::string fileName, Scene** scene, AssetManager* assets, sceneInitializerCallback_t* sceneInitializer );