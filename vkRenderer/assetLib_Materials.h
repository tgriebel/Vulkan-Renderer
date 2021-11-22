#pragma once

#include "common.h"
#include "assetLib_GpuProgs.h"
#include <map>

class AssetLibMaterials {
public:
	std::map< std::string, material_t > materials;
	void Create();
};