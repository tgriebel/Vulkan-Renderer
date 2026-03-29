/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

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
