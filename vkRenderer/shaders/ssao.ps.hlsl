#include "globals.h"

struct SsaoProcess
{
    uint dummy;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, SsaoProcess )

psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;

    output.outColor.r = 1.0f;

    return output;
}
