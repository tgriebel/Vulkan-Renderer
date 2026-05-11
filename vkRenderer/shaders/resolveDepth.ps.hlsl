#include "globals.h"
#include "util.h"

struct ImageShaderTask
{
    float4 generic0;
    float4 generic1;
    float4 generic2;
};

PS_LAYOUT_IMAGE_SHADER( Texture2DMS<float4>, ImageShaderTask )

psOutput_t PSMain( vsToPsInterpolators input )
{
    const int2 pixelLocation = int2( dimensions.xy * input.uv0.xy );

    psOutput_t output = (psOutput_t)0;

    float4 outColor = float4( 0.0f, 0.0f, 0.0f, 1.0f );

    for ( int j = 0; j < int( globals.numSamples ); ++j )
    {
        outColor.r = max( outColor.r, localTextures[ 0 ].Load( pixelLocation, j ).r );
        outColor.g += asuint( stencilImage.Load( pixelLocation, j ).r ) == 0x01 ? 1.0f : 0.0f;
    }
    outColor.g /= globals.numSamples;

    output.outColor = outColor;

    return output;
}
