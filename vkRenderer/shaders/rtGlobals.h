#include "globals.h"

struct hitPayload_t
{
	float3	color;
	float	weight;
	int		depth;
};

// Push-constant layout — must match RayTracingTask::baseConstants_t (C++ side).
// Occupies the first 16 bytes of the 128-byte push-constant budget.
// User constants (if any) follow at offset 16.
struct rtBaseConstants_t
{
	uint	viewId;
	uint	width;
	uint	height;
	uint	pad;
};

#define RT_PUSH_CONSTANTS	BIND_INLINE rtBaseConstants_t rtConstants;

