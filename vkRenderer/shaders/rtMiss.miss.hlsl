#include "rtGlobals.h"

[shader( "miss" )]
void miss_main( inout hitPayload_t payload )
{
    payload.color = float3( 0.5, 0.5, 0.5 );
}