#pragma once

#include <initializer_list>

#include <SysCore/timer.h>

#include "../globals/common.h"

#include "../render_state/cmdContext.h"
#include "../render_core/renderview.h"
#include "../render_core/renderResource.h"

class ShaderBinding;
using bindSetMap_t = std::unordered_map<uint64_t, ShaderBindSet>;

// Renderer system and resources accessible to sub-systems
class RenderContext
{
private:
	using bindParmArray_t = Array<ShaderBindParms, DescriptorPoolMaxSets>;
	using pendingArray_t = Array<uint32_t, DescriptorPoolMaxSets>;

	// Shader binding
	bindParmArray_t			bindParmsList;
	pendingArray_t			pendingIndices;
	bindSetMap_t			bindSets;
	uint64_t				frameNumber = 0;
	float					deltaTimeMs = 0.0f;

	uint32_t				displayWidth;
	uint32_t				displayHeight;

public:
	ShaderBindParms*		globalParms;

	// Memory
	AllocatorMemory			scratchMemory;
	AllocatorMemory			localMemory;
	AllocatorMemory			frameBufferMemory;
	AllocatorMemory			sharedMemory;

	inline const uint32_t	GetDisplayWidth() const { return displayWidth; }
	inline const uint32_t	GetDisplayHeight() const { return displayHeight; }

	uint64_t				CreateBindSet( const char* name, const ShaderBinding bindings[], const uint32_t bindCount );
	uint64_t				CreateBindSet( const char* name, std::initializer_list<ShaderBinding> bindings )
							{
								return CreateBindSet( name, bindings.begin(), static_cast<uint32_t>( bindings.size() ) );
							}

	ShaderBindParms*		RegisterBindParm( const ShaderBindSet* set );
	ShaderBindParms*		RegisterBindParm( const uint64_t setId );
	ShaderBindParms*		RegisterBindParm( const char* setName );
	const ShaderBindSet*	LookupBindSet( const uint64_t setId ) const;
	const ShaderBindSet*	LookupBindSet( const char* name ) const;
	void					UpdateBindParms();
	void					AllocRegisteredBindParms();
	void					FreeRegisteredBindParms();
	void					RefreshRegisteredBindParms();

	inline uint64_t			FrameNumber() const { return frameNumber; }

	friend class Renderer; // TODO: Need an interface for bind sets
};
