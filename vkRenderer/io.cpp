#include "io.h"
#include <core/assetlib.h>
#include "render_util.h"
#include <scene/scene.h>
#include <resource_types/gpuProgram.h>
#include <resource_types/model.h>
#include <io/io.h>

void SerializeStruct( Serializer* s, vertex_t& v );

extern Scene scene;

GpuProgram LoadProgram( const std::string& vsFile, const std::string& psFile )
{
	GpuProgram program;
	program.shaders[ 0 ].name = vsFile;
	program.shaders[ 0 ].blob = ReadFile( vsFile );
	program.shaders[ 1 ].name = psFile;
	program.shaders[ 1 ].blob = ReadFile( psFile );
	program.shaderCount = 2;
	program.isCompute = false;
	return program;
}


GpuProgram LoadProgram( const std::string& csFile )
{
	GpuProgram program;
	program.shaders[ 0 ].name = csFile;
	program.shaders[ 0 ].blob = ReadFile( csFile );
	program.shaderCount = 1;
	program.isCompute = true;
	return program;
}