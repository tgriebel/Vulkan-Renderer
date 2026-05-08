// Depth-of-Field Circle-of-Confusion
// Thin-lens CoC in pixels from focal length / aperture / focus distance.
// Sign convention: coc > 0 = far blur, coc < 0 = near blur, 0 = in focus.
//
//     A        = focalLengthM / fstop                                 (aperture diameter)
//     cocWorld = A * (Z - S) / Z * focalLengthM / (S - focalLengthM)  (signed, meters)
//     cocPx    = cocWorld * (imageHeightPx / sensorHeightM)
//
// Output is half-res (matches FB_dofCoc image). Format is R11G11B10 — no alpha,
// so signed CoC is encoded as a debug-friendly split:
//     R = saturate(  coc / maxCoCPixels )   far blur intensity
//     B = saturate( -coc / maxCoCPixels )   near blur intensity
// Swap to a signed single-channel format (R16F) once the prefilter consumer
// expects a packed CoC scalar instead of this debug visualization.

#include "globals.h"
#include "util.h"

struct DofCocConstants
{
    float focalLengthMM;     // 14 .. 200 mm
    float fstop;             // f-number ; 1.0 .. 22.0
    float focusDistanceM;    // metres
    float maxCoCPixels;      // clamp on |coc| ; ~24 px is a good ceiling
    float sensorHeightM;     // 0.024 = full-frame ; could move to globals later
    float pad0;
    float pad1;
    float pad2;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, DofCocConstants )

// localTextures[ 0 ] = (unused — the IMAGE_PROCESS layout reserves slot 0 for the
//                       output ping-pong source ; CoC has no ping-pong)
// localTextures[ 1 ] = resolved depth buffer (full-res; we sample at half-res UV)


float ComputeCocPixels( float linearZ, float focusDistM, float focalLenM, float fstopVal,
                        float imageHeightPx, float sensorHeightM, float maxCoCPx )
{
    // Avoid div-by-zero at the focal plane and behind the lens.
    const float Z       = max( linearZ,        focalLenM + 1e-4f );
    const float S       = max( focusDistM,     focalLenM + 1e-4f );
    const float A       = focalLenM / max( fstopVal, 1e-3f );

    // Thin-lens signed circle-of-confusion (metres on the sensor).
    const float cocWorld = A * ( Z - S ) / Z * ( focalLenM / ( S - focalLenM ) );

    // Project to pixels and clamp.
    const float pxPerMeter = imageHeightPx / max( sensorHeightM, 1e-6f );
    return clamp( cocWorld * pxPerMeter, -maxCoCPx, maxCoCPx );
}


psOutput_t PSMain( vsToPsInterpolators input )
{
    const float2 uv = input.uv0.xy;

    psOutput_t output = (psOutput_t)0;

    // dimensions.xy is the half-res CoC target ; depth is full-res. Sampling at uv
    // works for both because uv is in [0,1].
    const float zDepth = localTextures[ 1 ].SampleLevel( bilinearSamplerClampEdge, uv, 0 ).r;

    // Sky / background — treat as fully out-of-focus far field.
    if ( zDepth <= 0.0f )
    {
        output.outColor = float4( 1.0f, 0.0f, 0.0f, 1.0f );
        return output;
    }

    const uint           viewId  = 0;
    const float4x4       invProj = views[ viewId ].invProjMat;
    const float          linearZ = -ReconstructViewPos( uv, zDepth, invProj ).z;

    const float focalLenM = imageProcess.focalLengthMM * 0.001f;

    // dimensions.xy is the half-res output extent ; the "image height in pixels"
    // for the px-per-metre conversion should be the *full-res* height. dimensions.y
    // is half-res so multiply by 2. Cheap to do here ; refactor if the task ever
    // exposes the source resolution explicitly.
    const float imageHeightPx = dimensions.y * 2.0f;

    const float coc = ComputeCocPixels( linearZ,
                                        imageProcess.focusDistanceM,
                                        focalLenM,
                                        imageProcess.fstop,
                                        imageHeightPx,
                                        imageProcess.sensorHeightM,
                                        imageProcess.maxCoCPixels );

    const float invMax = 1.0f / max( imageProcess.maxCoCPixels, 1e-3f );
    const float farW   = saturate(  coc * invMax );
    const float nearW  = saturate( -coc * invMax );

    output.outColor = float4( farW, 0.0f, nearW, 1.0f );
    return output;
}
