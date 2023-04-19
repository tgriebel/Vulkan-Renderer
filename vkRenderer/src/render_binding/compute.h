#pragma once

#include "../globals/common.h"
#include "gpuResources.h"
#include "pipeline.h"

class ShaderDispatch
{
private:
	Asset<GpuProgram>*						program;
	std::vector<ShaderBinding>				bindSlots;
	std::vector<const GpuBuffer*>			attachments;
	uint32_t								x;
	uint32_t								y;
	uint32_t								z;

	void AddBind( const ShaderBinding& binding )
	{
		bindSlots[ binding.GetSlot() ] = binding;
	}
public:

	ShaderDispatch()
	{}

	ShaderDispatch( Asset<GpuProgram>* prog )
	{
		program = prog;
	}

	ShaderDispatch( Asset<GpuProgram>* prog, const ShaderBinding bindings[], const uint32_t bindCount )
	{
		program = prog;

		bindSlots.resize( bindCount );
		attachments.resize( bindCount );

		for( uint32_t i = 0; i < bindCount; ++i )
		{
			AddBind( bindings[i] );
		}
	}

	Asset<GpuProgram>* GetProgram()
	{
		return program;
	}

	bool Bind( const ShaderBinding& bindings, const GpuBuffer& buffer )
	{
		const uint32_t slot = bindings.GetSlot();
		if( bindSlots[ slot ].GetHash() == bindings.GetHash() )
		{
			attachments[ slot ] = &buffer;
			return true;
		}
		else {
			return false;
		}
	}

	const uint32_t SetDispatchSize( const uint32_t _x, const uint32_t _y, const uint32_t _z )
	{
		x = _x;
		y = _y;
		z = _z;
	}

	const uint32_t GetBindCount()
	{
		return static_cast<uint32_t>( bindSlots.size() );
	}

	const ShaderBinding& GetBinding( uint32_t slot )
	{
		return bindSlots[ slot ];
	}
};