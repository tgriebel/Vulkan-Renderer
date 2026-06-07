#include "rtGlobals.h"

GLOBALS_LAYOUT( 0, 0 )
VIEW_LAYOUT( 0, 1 )
RT_ACCELERATION_STRUCTURE( 1, 0, tlas )
RT_OUTPUT( 1, 1, rtOutput )
RT_PUSH_CONSTANTS

[shader( "closesthit" )]
void closesthit_main( inout hitPayload_t payload, in BuiltInTriangleIntersectionAttributes attribs )
{
    payload.color = float3( 1.0f, 1.0f, 0.0f );
}
