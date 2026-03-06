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

// ============================================================
// Expanded from: PS_LAYOUT_BASIC_IO
//   -> PS_IN
//   -> PS_OUT
// ============================================================

struct PS_Input
{
    float4 pos            : SV_Position;
    float4 fragColor      : TEXCOORD0;
    float3 fragNormal     : TEXCOORD1;
    float3 fragTangent    : TEXCOORD2;  // fragTangentBasis row 0
    float3 fragBitangent  : TEXCOORD3;  // fragTangentBasis row 1
    float3 fragTBN2       : TEXCOORD4;  // fragTangentBasis row 2
    float4 fragTexCoord   : TEXCOORD5;
    float3 objectPosition : TEXCOORD6;
    float4 clipPosition   : TEXCOORD7;
    float4 worldPosition  : TEXCOORD8;
    uint   objectId       : TEXCOORD9;
};

// ============================================================
// Expanded from: PS_LAYOUT_IMAGE_PROCESS( sampler2D, GaussianProcess )
//   -> GLOBALS_LAYOUT( 0, 0 )
//   -> VIEW_LAYOUT( 0, 1 )
//   -> SAMPLER_2D_LAYOUT( 0, 2 )
//   -> SAMPLER_CUBE_LAYOUT( 0, 3 )
//   -> MATERIAL_LAYOUT( 0, 4 )
//   -> CODE_IMAGE_LAYOUT( 1, 0, sampler2D )
//   -> CODE_IMAGE_CUBE_LAYOUT( 1, 1 )
//   -> STENCIL_LAYOUT( 1, 2, sampler2D )
//   -> IMAGE_CONSTANT_LAYOUT( 1, 3, GaussianProcess, imageProcess )
// ============================================================

// GLOBALS_LAYOUT( 0, 0 )
cbuffer GlobalConstants : register(b0, space0)
{
    float4 time;
    float4 generic;
    float4 shadowParms;
    float4 toneMap;
    float4 dof;
    uint   numSamples;
    uint   whiteId;
    uint   blackId;
    uint   defaultAlbedoId;
    uint   defaultNormalId;
    uint   defaultRoughnessId;
    uint   defaultMetalId;
    uint   defaultImageId;
    uint   brdfLutId;
    uint   isTextured;
    uint   shadow2dCount;
    uint   shadowCubeCount;
    uint   textureCount;
    uint   materialCount;
};

// VIEW_LAYOUT( 0, 1 )
StructuredBuffer<view_t> viewUbo : register(t1, space0);

// SAMPLER_2D_LAYOUT( 0, 2 )
Texture2D texSampler[] : register(t2, space0);

// SAMPLER_CUBE_LAYOUT( 0, 3 )
TextureCube cubeSamplers[] : register(t3, space0);

// MATERIAL_LAYOUT( 0, 4 )
StructuredBuffer<material_t> materialUbo : register(t4, space0);

// CODE_IMAGE_LAYOUT( 1, 0, sampler2D )
Texture2D codeSamplers[] : register(t0, space1);

// CODE_IMAGE_CUBE_LAYOUT( 1, 1 )
TextureCube codeCubeSamplers[] : register(t1, space1);

// STENCIL_LAYOUT( 1, 2, sampler2D )
Texture2D stencilImage : register(t2, space1);

// IMAGE_CONSTANT_LAYOUT( 1, 3, GaussianProcess, imageProcess )
struct GaussianProcess
{
    uint dummy;
};

cbuffer ShaderConstants : register(b3, space1)
{
    float4          dimensions;
    uint            pass;
    uint            previousImageId;
    uint            level;
    uint            layer;
    uint            mipCount;
    uint            layerCount;
    GaussianProcess imageProcess;
};

// Shared sampler state
SamplerState linearSampler : register(s0);

// ============================================================
// Gaussian blur weights
// ============================================================
static const uint  weightCount = 5;
static const float weights[5]  = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

// ============================================================
// Main
// ============================================================
float4 main( PS_Input input ) : SV_Target
{
    const bool  horizontal = ( pass == 0 );
    const float lodBias    = -16.0f;

    float2 offset = dimensions.zw;

    float4 outColor = float4(
        codeSamplers[ 0 ].Sample( linearSampler, input.fragTexCoord.xy ).rgb * weights[ 0 ],
        1.0f
    );

    if ( horizontal )
    {
        for ( uint i = 1; i < weightCount; ++i )
        {
            outColor.rgb += codeSamplers[ 0 ].SampleBias( linearSampler, input.fragTexCoord.xy + float2( offset.x * i, 0.0f ), lodBias ).rgb * weights[ i ];
            outColor.rgb += codeSamplers[ 0 ].SampleBias( linearSampler, input.fragTexCoord.xy - float2( offset.x * i, 0.0f ), lodBias ).rgb * weights[ i ];
        }
    }
    else
    {
		//[[vk::nonuniformEXT]]
		uint nonUniformId = previousImageId;
		
        for ( uint i = 1; i < weightCount; ++i )
        {
			outColor.rgb += codeSamplers[ nonUniformId ].SampleBias( linearSampler, input.fragTexCoord.xy + float2( 0.0f, offset.y * i ), lodBias ).rgb * weights[ i ];
			outColor.rgb += codeSamplers[ nonUniformId ].SampleBias( linearSampler, input.fragTexCoord.xy - float2( 0.0f, offset.y * i ), lodBias ).rgb * weights[ i ];
        }
    }

    outColor.a = 1.0f;
    return outColor;
}