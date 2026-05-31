#include "rtGlobals.h"

GLOBALS_LAYOUT( 0, 0 )
VIEW_LAYOUT( 0, 1 )
RT_ACCELERATION_STRUCTURE( 1, 0, tlas )
RT_OUTPUT( 1, 1, rtOutput )

[shader( "miss" )]
void miss_main( inout hitPayload_t payload )
{
    payload.color = float3( 0.5, 0.5, 0.5 );
}