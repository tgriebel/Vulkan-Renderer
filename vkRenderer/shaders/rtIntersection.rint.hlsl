#include "rtGlobals.h"

GLOBALS_LAYOUT( 0, 0 )
VIEW_LAYOUT( 0, 1 )
RT_ACCELERATION_STRUCTURE( 1, 0, tlas )
RT_OUTPUT( 1, 1, rtOutput )

// Custom attribute struct for procedural geometry — expand when intersection logic is implemented
struct intersectionAttribs_t
{
    float2 barycentrics;
};

[shader( "intersection" )]
void intersection_main()
{
    intersectionAttribs_t attribs;
    attribs.barycentrics = float2( 0.0f, 0.0f );
    ReportHit( RayTCurrent(), 0, attribs );
}
