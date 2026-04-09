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
#include "color_hlsl.h"

struct WriteBackParms
{
    float4 dimensions;
};

GLOBALS_LAYOUT( 0, 0 )
CODE_IMAGE_LAYOUT( 0, 1, Texture2D )
CONSTANT_LAYOUT( 0, 2, WriteBackParms, writebackParms )
WRITE_BUFFER_LAYOUT( 0, 3, float4, imageWriteback )

struct WritebackPush_t
{
    float4 dimensions;
    uint imageId;
    int lod;
    uint baseOffset;
};

[[vk::push_constant]] WritebackPush_t wb;

uint PackF16( const float unpacked )
{
    const uint element = asuint( unpacked );

    const uint signBit = ( element >> 16 ) & 0x8000;
    uint exp = ( element >> 23 ) & 0xFF;
    uint mantissa = element & 0x7FFFFF;

    if ( exp > 0x70 ) {
        exp = ( exp - 0x70 ); // Implicitly clamps to INF
    } else {
        exp = 0; // Flush denormals
    }
    mantissa >>= 13; // Don't round up

    const uint packed = signBit | exp | mantissa;

    return packed;
}

float2 PackVectorF16( const float4 unpacked )
{
    const uint x0 = PackF16( unpacked.x );
    const uint x1 = PackF16( unpacked.y );
    const uint x2 = PackF16( unpacked.z );
    const uint x3 = PackF16( unpacked.w );

    return float2( asfloat( x0 << 16 | x1 ), asfloat( x2 << 16 | x3 ) );
}

[numthreads(8, 8, 8)]
void CSMain( uint3 dtid : SV_DispatchThreadID )
{
    const uint x = dtid.x;
    const uint y = dtid.y;
    const uint z = dtid.z;

    const uint width = uint( wb.dimensions.x );
    const uint height = uint( wb.dimensions.y );
    const uint layers = uint( wb.dimensions.z );

    if( x >= width || y >= height || z >= layers ) {
        return;
    }

    const float4 pixel = codeSamplers[ wb.imageId + z ].Load( int3( x, y, wb.lod ) );

    const uint offset = x + ( y * width ) + z * ( width * height );

    //const float4 sRgb = LinearToSrgb( pixel.rgba );
    imageWriteback[ wb.baseOffset + offset ].xyzw = pixel.rgba;
}
