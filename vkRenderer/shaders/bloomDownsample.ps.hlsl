#include "globals.h"

struct ImageShaderTask
{
    float4 generic0;
    float4 generic1;
    float4 generic2;
};


float3 PowVec3( float3 v, float p )
{
    return float3( pow( v.x, p ), pow( v.y, p ), pow( v.z, p ) );
}


const float invGamma = 1.0 / 2.2;
float3 ToSRGB( float3 v )
{
    return PowVec3( v, invGamma );
}


float RGBToLuminance( float3 col )
{
    return dot( col, float3( 0.2126f, 0.7152f, 0.0722f ) );
}


float KarisAverage( float3 col )
{
    // Formula is 1 / (1 + luma)
    float luma = RGBToLuminance( ToSRGB( col ) ) * 0.25f;
    return 1.0f / ( 1.0f + luma );
}

PS_LAYOUT_IMAGE_PROCESS( Texture2D, ImageShaderTask )

PS_Output PSMain( PS_Input input )
{
    PS_Output output = (PS_Output)0;

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
    float3 a = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x - 2.0f * x, input.uv0.y + 2 * y ) ).rgb;
    float3 b = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x,              input.uv0.y + 2 * y ) ).rgb;
    float3 c = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x + 2.0f * x,   input.uv0.y + 2 * y ) ).rgb;

    float3 d = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x - 2.0f * x,   input.uv0.y ) ).rgb;
    float3 e = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x,              input.uv0.y ) ).rgb;
    float3 f = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x + 2.0f * x,   input.uv0.y ) ).rgb;

    float3 g = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x - 2.0f * x,   input.uv0.y - 2 * y ) ).rgb;
    float3 h = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x,              input.uv0.y - 2 * y ) ).rgb;
    float3 i = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x + 2.0f * x,   input.uv0.y - 2 * y ) ).rgb;

    float3 j = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x - x,          input.uv0.y + y ) ).rgb;
    float3 k = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x + x,          input.uv0.y + y ) ).rgb;
    float3 l = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x - x,          input.uv0.y - y ) ).rgb;
    float3 m = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x + x,          input.uv0.y - y ) ).rgb;

    a = ClampColorFp16( a );
    b = ClampColorFp16( b );
    c = ClampColorFp16( c );
    d = ClampColorFp16( d );
    e = ClampColorFp16( e );
    f = ClampColorFp16( f );
    g = ClampColorFp16( g );
    h = ClampColorFp16( h );
    i = ClampColorFp16( i );
    j = ClampColorFp16( j );
    k = ClampColorFp16( k );
    l = ClampColorFp16( l );
    m = ClampColorFp16( m );
    
    if ( level == 0 )
    {
        float3 groups[ 5 ];
        
        groups[ 0 ] = ( a + b + d + e ) * ( 0.125f / 4.0f );
        groups[ 1 ] = ( b + c + e + f ) * ( 0.125f / 4.0f );
        groups[ 2 ] = ( d + e + g + h ) * ( 0.125f / 4.0f );
        groups[ 3 ] = ( e + f + h + i ) * ( 0.125f / 4.0f );
        groups[ 4 ] = ( j + k + l + m ) * ( 0.5f / 4.0f );
        groups[ 0 ] *= KarisAverage( groups[ 0 ] );
        groups[ 1 ] *= KarisAverage( groups[ 1 ] );
        groups[ 2 ] *= KarisAverage( groups[ 2 ] );
        groups[ 3 ] *= KarisAverage( groups[ 3 ] );
        groups[ 4 ] *= KarisAverage( groups[ 4 ] );
        output.outColor.rgb = groups[ 0 ] + groups[ 1 ] + groups[ 2 ] + groups[ 3 ] + groups[ 4 ];
        output.outColor.a = 1.0f;
    }
    else
    {
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
        output.outColor.rgb = e * 0.125f;
        output.outColor.rgb += ( a + c + g + i ) * 0.03125f;
        output.outColor.rgb += ( b + d + f + h ) * 0.0625f;
        output.outColor.rgb += ( j + k + l + m ) * 0.125f;
        output.outColor.a = 1.0f;
    }
    return output;
}
