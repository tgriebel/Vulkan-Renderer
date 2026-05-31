#include "rtGlobals.h"

GLOBALS_LAYOUT( 0, 0 )
VIEW_LAYOUT( 0, 1 )
RT_ACCELERATION_STRUCTURE( 1, 0, tlas )
RT_OUTPUT( 1, 1, rtOutput )

// Any-hit placeholder — accepts all candidates and defers to closest-hit selection.
// Implement alpha/transparency rejection here when needed (IgnoreHit() to discard).
[shader( "anyhit" )]
void anyhit_main( inout hitPayload_t payload, in BuiltInTriangleIntersectionAttributes attribs )
{
}
