#include "gpuTimerPool.h"
#include "../render_state/deviceContext.h"
#include "../render_state/cmdContext.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <cstdio>
#include <vector>

GpuTimerPool g_gpuTimerPool;


// ---------------------------------------------------------------------------
// GpuTimerPool
// ---------------------------------------------------------------------------

void GpuTimerPool::Create()
{
#ifdef USE_VULKAN
	if ( context.deviceProperties.limits.timestampComputeAndGraphics == VK_FALSE ) {
		return;
	}

	VkQueryPoolCreateInfo info = {};
	info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	info.queryType  = VK_QUERY_TYPE_TIMESTAMP;
	info.queryCount = TotalQueries;
	VK_CHECK_RESULT( vkCreateQueryPool( context.device, &info, nullptr, &m_pool ) );

	m_periodMs = context.deviceProperties.limits.timestampPeriod / 1e6f;
#endif
}


void GpuTimerPool::Destroy()
{
#ifdef USE_VULKAN
	if ( m_pool != VK_NULL_HANDLE )
	{
		vkDestroyQueryPool( context.device, m_pool, nullptr );
		m_pool = VK_NULL_HANDLE;
	}
#endif
}


void GpuTimerPool::FrameBegin( CommandList* cmd, uint32_t bufferId )
{
#ifdef USE_VULKAN
	if ( m_pool == VK_NULL_HANDLE ) {
		return;
	}
	vkCmdResetQueryPool( cmd->CommandBuffer(), m_pool, QueryBase(bufferId), MaxScopes * 2);
	m_slabReset[ bufferId ] = true;
#endif
}


void GpuTimerPool::FrameReadback( uint32_t bufferId )
{
#ifdef USE_VULKAN
	if ( m_pool == VK_NULL_HANDLE ) {
		return;
	}
	// First MaxFrameStates frames: this slab has not been reset yet, so reading
	// it would hit "query not reset" validation. Skip until FrameBegin runs once.
	if ( m_slabReset[ bufferId ] == false ) {
		return;
	}

	const uint32_t base = QueryBase( bufferId );
	const uint32_t queryCount = MaxScopes * 2;

	// PARTIAL_BIT is illegal on timestamp queries, so use WITH_AVAILABILITY_BIT
	// to detect queries that were reset but never written this frame.
	// Layout per query: [ uint64 timestamp, uint64 availability ].
	uint64_t data[ MaxScopes * 2 * 2 ] = {};

	vkGetQueryPoolResults(
		context.device,
		m_pool,
		base,
		queryCount,
		sizeof( data ),
		data,
		2 * sizeof( uint64_t ),
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
	);

	for ( uint32_t i = 0; i < MaxScopes; ++i )
	{
		m_results[ i ].name = m_scopeNames[ i ];

		const uint64_t beginTs    = data[ ( i * 2 + 0 ) * 2 + 0 ];
		const uint64_t beginAvail = data[ ( i * 2 + 0 ) * 2 + 1 ];
		const uint64_t endTs      = data[ ( i * 2 + 1 ) * 2 + 0 ];
		const uint64_t endAvail   = data[ ( i * 2 + 1 ) * 2 + 1 ];

		if ( beginAvail != 0 && endAvail != 0 ) {
			m_results[ i ].ms = static_cast<float>( endTs - beginTs ) * m_periodMs;
		}
	}
#endif
}


int GpuTimerPool::FindScope( const char* name ) const
{
	// Callers always pass string literals so pointer identity is stable
	for ( uint32_t i = 0; i < MaxScopes; ++i ) {
		if ( m_scopeNames[ i ] == name ) {
			return static_cast<int>( i );
		}
	}
	return -1;
}


int GpuTimerPool::FindOrRegisterScope( const char* name )
{
	const int idx = FindScope( name );
	if ( idx >= 0 ) {
		return idx;
	}
	// Find the first empty slot
	for ( uint32_t i = 0; i < MaxScopes; ++i )
	{
		if ( m_scopeNames[ i ] == nullptr )
		{
			m_scopeNames[ i ] = name;
			return static_cast<int>( i );
		}
	}
	return -1;   // pool exhausted
}


void GpuTimerPool::BeginScope( CommandList* cmd, uint32_t bufferId, const char* name )
{
#ifdef USE_VULKAN
	if ( m_pool == VK_NULL_HANDLE ) {
		return;
	}
	const int idx = FindOrRegisterScope( name );
	if ( idx < 0 ) {
		return;
	}
	vkCmdWriteTimestamp( cmd->CommandBuffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_pool, QueryBase( bufferId ) + static_cast<uint32_t>( idx ) * 2 );
#endif
}


void GpuTimerPool::EndScope( CommandList* cmd, uint32_t bufferId, const char* name )
{
#ifdef USE_VULKAN
	if ( m_pool == VK_NULL_HANDLE ) {
		return;
	}
	const int idx = FindScope( name );
	if ( idx < 0 ) {
		return;
	}
	vkCmdWriteTimestamp( cmd->CommandBuffer(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_pool, QueryBase( bufferId ) + static_cast<uint32_t>( idx ) * 2 + 1 );
#endif
}


void GpuTimerPool::DrawDebugMenu() const
{
#ifdef USE_IMGUI
	float maxMs = 0.001f;
	for ( uint32_t i = 0; i < MaxScopes; ++i ) {
		if ( m_results[ i ].name != nullptr ) {
			maxMs = std::max( maxMs, m_results[ i ].ms );
		}
	}

	bool anyResults = ( maxMs > 0.001f );
	if ( !anyResults ) {
		return;
	}

	ImGui::Separator();
	ImGui::Text( "GPU Timings" );

	for ( uint32_t i = 0; i < MaxScopes; ++i )
	{
		const ScopeResult& r = m_results[ i ];
		if ( r.name == nullptr ) {
			continue;
		}
		char label[ 64 ];
		snprintf( label, sizeof( label ), "%-24s %6.3f ms", r.name, r.ms );
		ImGui::ProgressBar( r.ms / maxMs, ImVec2( -1.0f, 0.0f ), label );
	}
#endif
}


// ---------------------------------------------------------------------------
// GpuScopedTimer
// ---------------------------------------------------------------------------

GpuScopedTimer::GpuScopedTimer( CommandList* ctx, const char* name )
	: m_ctx( ctx ), m_name( name )
{
	ctx->BeginTimestamp( name );
}


GpuScopedTimer::~GpuScopedTimer()
{
	m_ctx->EndTimestamp( m_name );
}
