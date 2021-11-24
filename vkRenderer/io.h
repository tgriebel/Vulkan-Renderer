#pragma once

#include <vector>
#include <string>
#include <fstream>

#include "common.h"

std::vector<char> ReadFile( const std::string& filename );
void LoadModel( const std::string& fileName, modelSource_t& model );