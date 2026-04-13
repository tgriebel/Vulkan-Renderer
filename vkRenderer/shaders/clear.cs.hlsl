#include "globals_hlsl.h"

struct Particle
{
    float2 position;
    float2 velocity;
    float4 color;
};

GLOBALS_LAYOUT( 0, 0 )
//READ_BUFFER_LAYOUT( 0, 1, Particle, particlesIn)
WRITE_BUFFER_LAYOUT( 0, 1, Particle, particlesOut)

[numthreads(256, 1, 1)]
void CSMain( uint3 dtid : SV_DispatchThreadID )
{
    uint index = dtid.x;

   // Particle particleIn = particlesIn[index];

    particlesOut[index].position = float2( 1.0f, 2.0f );//particleIn.position + particleIn.velocity.xy * globals.time.x;
    particlesOut[index].velocity = float2( 3.0f, 4.0f );//particleIn.velocity;
}
