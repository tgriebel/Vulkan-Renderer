#pragma once

#include <vector>
#include <string>
#include <fstream>

#include "common.h"

std::vector<char> ReadFile( const std::string& filename );
GpuProgram LoadProgram( const std::string& csFile );
GpuProgram LoadProgram( const std::string& vsFile, const std::string& psFile );
hdl_t LoadModel( const std::string& fileName, const std::string& objectName );