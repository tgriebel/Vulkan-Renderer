#pragma once

#include <SysCore/timer.h>

#include "../globals/common.h"
#include "../globals/renderConstants.h"

#include "../render_state/cmdContext.h"
#include "../render_core/renderview.h"
#include "../render_core/renderResource.h"

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

public:
	ShaderBindParms*		globalParms;

	// Memory
	AllocatorMemory			scratchMemory;
	AllocatorMemory			localMemory;
	AllocatorMemory			frameBufferMemory;
	AllocatorMemory			sharedMemory;

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
