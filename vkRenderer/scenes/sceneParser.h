#pragma once
#include <string>

class Scene;
class AssetManager;

void LoadScene( std::string fileName, Scene** scene, AssetManager* assets );