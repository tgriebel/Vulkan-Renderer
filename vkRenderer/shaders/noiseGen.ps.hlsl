#include "globals.h"
#include "util.h"
#include "lightUtil.h"

struct NoiseGenConstants
{
    float dummy; // TODO: take in type field (e.g. blue noise, IGN, perlin)
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, NoiseGenConstants )

psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;

    output.outColor.r = 1.0f;
    output.outColor.g = 1.0f;
    output.outColor.b = 0.0f;
    output.outColor.a = 1.0f;

    return output;
}
