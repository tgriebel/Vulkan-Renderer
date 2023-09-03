//
// PUBLIC DOMAIN CRT STYLED SCAN-LINE SHADER
//
//   by Timothy Lottes
//
// This is more along the style of a really good CGA arcade monitor.
// With RGB inputs instead of NTSC.
// The shadow mask example has the mask rotated 90 degrees for less chromatic aberration.
//
// Left it unoptimized to show the theory behind the algorithm.
//
// It is an example what I personally would want as a display option for pixel art games.
// Please take and use, change, or whatever.
//

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"
#include "color.h"

PS_LAYOUT_STANDARD( sampler2D )

vec2 dim = vec2( 256.0f, 240.0f );
// Emulated input resolution.
#if 1
  // Fix resolution to set amount.
#define res (vec2(256.0/1.0,240.0/1.0))
#else
  // Optimize for resize.
#define res (dim.xy/6.0)
#endif

// Hardness of scanline.
//  -8.0 = soft
// -16.0 = medium
float hardScan = -8.0;

// Hardness of pixels in scanline.
// -2.0 = soft
// -4.0 = hard
float hardPix = -3.0;

// Display warp.
// 0.0 = none
// 1.0/8.0 = extreme
vec2 warp = vec2( 1.0 / 32.0, 1.0 / 24.0 );

// Amount of shadow mask.
float maskDark = 0.5;
float maskLight = 1.5;

//------------------------------------------------------------------------

// Nearest emulated sample given floating point position and texel offset.
// Also zero's off screen.
vec3 Fetch( vec2 pos, vec2 off )
{
    pos = floor( pos * res + off ) / res;
    if ( max( abs( pos.x - 0.5 ), abs( pos.y - 0.5 ) ) > 0.5 )return vec3( 0.0, 0.0, 0.0 );

    const uint materialId = pushConstants.materialId;
    const uint textureId0 = materialUbo.materials[ materialId ].textureId0;
    return SrgbToLinear( texture( texSampler[ textureId0 ], pos.xy, -16.0 ).rgb );
}

// Distance in emulated pixels to nearest texel.
vec2 Dist( vec2 pos )
{
    pos = pos * res;
    return -( ( pos - floor( pos ) ) - vec2( 0.5 ) );
}

// 1D Gaussian.
float Gaus( float pos, float scale )
{
    return exp2( scale * pos * pos );
}

// 3-tap Gaussian filter along horz line.
vec3 Horz3( vec2 pos, float off )
{
    vec3 b = Fetch( pos, vec2( -1.0, off ) );
    vec3 c = Fetch( pos, vec2( 0.0, off ) );
    vec3 d = Fetch( pos, vec2( 1.0, off ) );
    float dst = Dist( pos ).x;
    // Convert distance to weight.
    float scale = hardPix;
    float wb = Gaus( dst - 1.0, scale );
    float wc = Gaus( dst + 0.0, scale );
    float wd = Gaus( dst + 1.0, scale );
    // Return filtered sample.
    return ( b * wb + c * wc + d * wd ) / ( wb + wc + wd );
}

// 5-tap Gaussian filter along horz line.
vec3 Horz5( vec2 pos, float off )
{
    vec3 a = Fetch( pos, vec2( -2.0, off ) );
    vec3 b = Fetch( pos, vec2( -1.0, off ) );
    vec3 c = Fetch( pos, vec2( 0.0, off ) );
    vec3 d = Fetch( pos, vec2( 1.0, off ) );
    vec3 e = Fetch( pos, vec2( 2.0, off ) );
    float dst = Dist( pos ).x;

    // Convert distance to weight.
    float scale = hardPix;
    float wa = Gaus( dst - 2.0, scale );
    float wb = Gaus( dst - 1.0, scale );
    float wc = Gaus( dst + 0.0, scale );
    float wd = Gaus( dst + 1.0, scale );
    float we = Gaus( dst + 2.0, scale );
    // Return filtered sample.
    return ( a * wa + b * wb + c * wc + d * wd + e * we ) / ( wa + wb + wc + wd + we );
}

// Return scanline weight.
float Scan( vec2 pos, float off ) {
    float dst = Dist( pos ).y;
    return Gaus( dst + off, hardScan );
}

// Allow nearest three lines to effect pixel.
vec3 Tri( vec2 pos )
{
    const vec3 a = Horz3( pos, -1.0f );
    const vec3 b = Horz5( pos, 0.0f );
    const vec3 c = Horz3( pos, 1.0f );
    const float wa = Scan( pos, -1.0f );
    const float wb = Scan( pos, 0.0f );
    const float wc = Scan( pos, 1.0f );

    return ( a * wa ) + ( b * wb ) + ( c * wc );
}

// Distortion of scanlines, and end of screen alpha.
vec2 Warp( vec2 pos )
{
    vec2 distorted = pos * 2.0f - 1.0f;
    distorted *= vec2( 1.0f + ( distorted.y * distorted.y ) * warp.x, 1.0f + ( distorted.x * distorted.x ) * warp.y );
    return ( distorted * 0.5f + 0.5f );
}

// Shadow mask.
vec3 Mask( vec2 pos )
{
    vec3 mask = vec3( maskDark, maskDark, maskDark );
    pos.x += pos.y * 3.0f;
    pos.x = fract( pos.x / 6.0f );

    if ( pos.x < 0.333f )
        mask.r = maskLight;
    else if ( pos.x < 0.666f )
        mask.g = maskLight;
    else
        mask.b = maskLight;

    return mask;
}

// Draw dividing bars.
float Bar( float pos, float bar ) { pos -= bar; return pos * pos < 4.0 ? 0.0 : 1.0; }

// Entry.
void main() {
    vec2 pos = Warp( fragTexCoord.xy );
    outColor.rgb = Tri( pos ) * Mask( fragTexCoord.xy / res );
    outColor.a = 1.0;
    outColor.rgb = outColor.rgb;
}