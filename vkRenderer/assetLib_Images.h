#pragma once
#include "common.h"
#include <map>



class AssetLibImages {
public:
	std::map< std::string, texture_t > textures;
	void Create();
};