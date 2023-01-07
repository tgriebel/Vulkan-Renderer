#pragma once

#include <vector>
#include <string>
#include <fstream>

#include "common.h"

#include <syscore/serializer.h>

GpuProgram LoadProgram( const std::string& csFile );
GpuProgram LoadProgram( const std::string& vsFile, const std::string& psFile );