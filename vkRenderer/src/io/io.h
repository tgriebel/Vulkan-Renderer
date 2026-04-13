#pragma once

#include <vector>
#include <string>
#include <fstream>

#include <syscore/serializer.h>

#include "../globals/common.h"
#include "../asset_types/gpuProgram.h"

class Image;
class Model;
class AssetManager;

template<class T>
class Asset;

bool LoadImage( const char* texturePath, const bool isLinearColor, Image& texture );
bool LoadImageHDR( const char* texturePath, Image& texture );
bool LoadCubeMapImage( const char* textureBasePath, const char* ext, Image& texture );
bool WriteImage( const char* path, const Image& image );
bool LoadRawModel( AssetManager& assets, const std::string& fileName, const std::string& modelPath, const std::string& texturePath, Model& model );
bool LoadModel( AssetManager& assets, const hdl_t& hdl, const std::string& bakePath, const std::string& modelPath, const std::string& ext );
bool WriteModel( Asset<Model>* model, const std::string& fileName );
