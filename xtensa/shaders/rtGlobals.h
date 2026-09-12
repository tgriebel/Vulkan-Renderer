#define USE_RT

#include "globals.h"

// ============================================================
// Shared payload — must be identical in rgen, rchit, and rmiss
// ============================================================

struct hitPayload_t
{
	float4	color;
};


// ============================================================
// Push-constant layout — must match RayTracingTask::baseConstants_t
// ============================================================

struct rtBaseConstants_t
{
	uint	viewId;
	uint	width;
	uint	height;
	uint	pad;
};

#define RT_PUSH_CONSTANTS			BIND_INLINE rtBaseConstants_t rtConstants;


// ============================================================
// Binding macros
// ============================================================

#define RT_ACCELERATION_STRUCTURE( S, N, NAME )		BIND_SET( S, N ) RaytracingAccelerationStructure NAME;
#define RT_OUTPUT( S, N, NAME )						BIND_SET( S, N ) RWTexture2D<float4> NAME;
#define RT_VERTEX_BUFFER( S, N, NAME )				BIND_SET( S, N ) StructuredBuffer<rtVertex_t> NAME;
#define RT_INDEX_BUFFER( S, N, NAME )				BIND_SET( S, N ) StructuredBuffer<uint> NAME;
#define RT_SURFACE_INFO( S, N, NAME )				BIND_SET( S, N ) StructuredBuffer<gpuRtSurface_t> NAME;
