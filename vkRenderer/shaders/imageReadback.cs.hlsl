#include "globals.h"
#include "util.h"

struct ReadbackParms
{
    float4 dimensions;
};

GLOBALS_LAYOUT( 0, 0 )
CODE_IMAGE_LAYOUT( 0, 1, Texture2D )
CONSTANT_LAYOUT( 0, 2, ReadbackParms, readbackParms )
WRITE_BUFFER_LAYOUT( 0, 3, float4, imageReadback )

struct ReadbackPush_t
{
    float4 dimensions;
    uint imageId;
    int lod;
    uint baseOffset;
};

BIND_INLINE ReadbackPush_t wb;

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

    const float4 pixel = localTextures[ wb.imageId + z ].Load( int3( x, y, wb.lod ) );

    const uint offset = x + ( y * width ) + z * ( width * height );

    //const float4 sRgb = LinearToSrgb( pixel.rgba );
    imageReadback[ wb.baseOffset + offset ].xyzw = pixel.rgba;
}
