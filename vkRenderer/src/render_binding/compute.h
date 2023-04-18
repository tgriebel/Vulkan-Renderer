#pragma once

#include "../globals/common.h"
#include "gpuResources.h"
#include "pipeline.h"

class ComputeShader
{
private:
	Asset<GpuProgram>*			program;
	std::vector<ShaderBinding>	bindings;
public:

	ComputeShader()
	{}

	ComputeShader( Asset<GpuProgram>* prog )
	{
		program = prog;
	}

	ComputeShader( Asset<GpuProgram>* prog, const ShaderBinding bindings[], const uint32_t bindCount )
	{
		program = prog;

		for( uint32_t i = 0; i < bindCount; ++i )
		{
			AddBind( bindings[i] );
		}
	}

	Asset<GpuProgram>* GetProgram()
	{
		return program;
	}

	void AddBind( const ShaderBinding& binding )
	{
		// TODO: hash, check for duplicate slots
		bindings.push_back( binding );
	}

	void Bind( const ShaderBinding& bindings, GpuBuffer& buffer );

	const uint32_t GetBindCount()
	{
		return static_cast<uint32_t>( bindings.size() );
	}

	const ShaderBinding& GetBinding( uint32_t slot )
	{
		return bindings[ slot ];
	}
};