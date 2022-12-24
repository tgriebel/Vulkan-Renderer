#pragma once

#include <vector>
#include <string>
#include <fstream>

#include "common.h"

#include <serializer.h>

std::vector<char> ReadFile( const std::string& filename );
GpuProgram LoadProgram( const std::string& csFile );
GpuProgram LoadProgram( const std::string& vsFile, const std::string& psFile );
hdl_t LoadRawModel( const std::string& fileName, const std::string& objectName );
bool WriteModel( const std::string& fileName, hdl_t modelHdl );
bool LoadModel( const hdl_t& hdl );