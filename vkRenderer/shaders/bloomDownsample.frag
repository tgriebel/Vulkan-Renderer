#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_BASIC_IO

struct ImageShaderTask
{
    vec4 generic0;
    vec4 generic1;
    vec4 generic2;
};

PS_LAYOUT_IMAGE_PROCESS( sampler2D, ImageShaderTask )

void main()
{
    // Source: https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

    float x = dimensions.z;
    float y = dimensions.w;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x - 2.0f * x,   fragTexCoord.y + 2 * y ) ).rgb;
    vec3 b = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x,              fragTexCoord.y + 2 * y ) ).rgb;
    vec3 c = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x + 2.0f * x,   fragTexCoord.y + 2 * y ) ).rgb;

    vec3 d = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x - 2.0f * x,   fragTexCoord.y ) ).rgb;
    vec3 e = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x,              fragTexCoord.y ) ).rgb;
    vec3 f = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x + 2.0f * x,   fragTexCoord.y ) ).rgb;

    vec3 g = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x - 2.0f * x,   fragTexCoord.y - 2 * y ) ).rgb;
    vec3 h = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x,              fragTexCoord.y - 2 * y ) ).rgb;
    vec3 i = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x + 2.0f * x,   fragTexCoord.y - 2 * y ) ).rgb;

    vec3 j = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x - x,          fragTexCoord.y + y ) ).rgb;
    vec3 k = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x + x,          fragTexCoord.y + y ) ).rgb;
    vec3 l = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x - x,          fragTexCoord.y - y ) ).rgb;
    vec3 m = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x + x,          fragTexCoord.y - y ) ).rgb;

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    // a,b,d,e * 0.125
    // b,c,e,f * 0.125
    // d,e,g,h * 0.125
    // e,f,h,i * 0.125
    // j,k,l,m * 0.5
    // This shows 5 square areas that are being sampled. But some of them overlap,
    // so to have an energy preserving downsample we need to make some adjustments.
    // The weights are the distributed, so that the sum of j,k,l,m (e.g.)
    // contribute 0.5 to the final color output. The code below is written
    // to effectively yield this sum. We get:
    // 0.125*5 + 0.03125*4 + 0.0625*4 = 1
    outColor.rgb = e * 0.125f;
    outColor.rgb += ( a + c + g + i ) * 0.03125f;
    outColor.rgb += ( b + d + f + h ) * 0.0625f;
    outColor.rgb += ( j + k + l + m ) * 0.125f;
    outColor.a = 1.0f;
}