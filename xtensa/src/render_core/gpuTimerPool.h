#pragma once

#include "../globals/common.h"

class CommandList;

// ---------------------------------------------------------------------------
// GpuTimerPool
//
// Manages a VkQueryPool partitioned into per-frame slabs so timestamps
// written in frame N can be safely read back once frame N's fence signals.
//
// Layout (MaxScopes=8, MaxFrameStates=3):
//   Frame 0 : queries  0 .. 15   (scope K: begin=K*2, end=K*2+1)
//   Frame 1 : queries 16 .. 31
//   Frame 2 : queries 32 .. 47
// ---------------------------------------------------------------------------
class GpuTimerPool
{
public:
	static const uint32_t MaxScopes		= 8;
	static const uint32_t TotalQueries	= MaxScopes * 2 * MaxFrameStates;

	struct ScopeResult
	{
		const char* name	= nullptr;
		float ms			= 0.0f;
	};

	void				Create();
	void				Destroy();

	// Call inside the command buffer at the start of each frame
	// (resets this frame's query slab so queries can be written again)
	void				FrameBegin( CommandList* cmd, uint32_t bufferId );

	// Call after the frame fence has signalled — reads back completed timings
	void				FrameReadback( uint32_t frameIndex );

	void				BeginScope( CommandList* cmd, uint32_t bufferId, const char* name );
	void				EndScope  ( CommandList* cmd, uint32_t bufferId, const char* name );

	const ScopeResult*  Results() const { return m_results; }

	void				DrawDebugMenu() const;

private:
	uint32_t			QueryBase( uint32_t bufferId ) const { return bufferId * MaxScopes * 2; }
	int					FindScope( const char* name ) const;
	int					FindOrRegisterScope( const char* name );

#ifdef USE_VULKAN
	VkQueryPool		m_pool = VK_NULL_HANDLE;
#endif
	float			m_periodMs = 0.0f;
	bool			m_slabReset[ MaxFrameStates ] = {};
	const char*		m_scopeNames[ MaxScopes ] = {};
	ScopeResult		m_results[ MaxScopes ] = {};
};


// ---------------------------------------------------------------------------
// GpuScopedTimer
// ---------------------------------------------------------------------------
struct GpuScopedTimer
{
	GpuScopedTimer( CommandList* ctx, const char* name );
	~GpuScopedTimer();

private:
	CommandList*	m_ctx;
	const char*		m_name;
};


#define GPU_SCOPED_TIMER( cmdContext, label ) GpuScopedTimer gpuScopedTimer_##label( cmdContext, #label );

extern GpuTimerPool g_gpuTimerPool;
