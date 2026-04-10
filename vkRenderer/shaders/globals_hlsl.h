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

#ifndef GLOBALS_HLSL_H
#define GLOBALS_HLSL_H

// ============================================================
// Constants
// ============================================================

#define MaxLights       128
#define MaxMaterials    256
#define MaxViews        15
#define MaxSurfaces     1000

#define PI              3.14159265359f
#define AMBIENT         float4( 0.03f, 0.03f, 0.03f, 1.0f )

// ============================================================
// Convenience
// ============================================================

#define NUI( x ) NonUniformResourceIndex( x )

#define BIND_SLOT( x )      [[vk::location( x )]]
#define BIND_SET( S, N )    [[vk::binding( N, S )]]
#define BIND_INLINE         [[vk::push_constant]]

// GLSL mat4[col] returns a column. HLSL mat[row] returns a row.
// These extract columns from a float4x4, matching GLSL mat[col] semantics.
float3 MatCol3( float4x4 m, int c ) { return float3( m[0][c], m[1][c], m[2][c] ); }
float4 MatCol4( float4x4 m, int c ) { return float4( m[0][c], m[1][c], m[2][c], m[3][c] ); }

int2 GetTextureSize( Texture2D tex, int mipLevel )
{
    uint w, h, levels;
    tex.GetDimensions( mipLevel, w, h, levels );
    return int2( w, h );
}

uint GetTextureLevels( Texture2D tex )
{
    uint w, h, levels;
    tex.GetDimensions( 0, w, h, levels );
    return levels;
}

uint GetTextureLevelsCube( TextureCube tex )
{
    uint w, h, levels;
    tex.GetDimensions( 0, w, h, levels );
    return levels;
}

// ============================================================
// Structs
// ============================================================

struct light_t
{
    float4 lightPos;
    float4 intensity;
    float4 lightDir;
    uint   shadowViewId;
    uint   pad0;
    uint   pad1;
    uint   pad2;
};

struct material_t
{
    int     textureId0;
    int     textureId1;
    int     textureId2;
    int     textureId3;
    int     textureId4;
    int     textureId5;
    int     textureId6;
    int     textureId7;
    float3  Ka;
    float   Tr;
    float3  Ke;
    float   Ns;
    float3  Kd;
    float   Ni;
    float3  Ks;
    float   illum;
    float3  Tf;
    uint    textured;
    float   roughness;
    float   metalness;
    uint    pad2;
    uint    pad3;
    uint    extraData[64];
};

struct view_t
{
    float4x4 viewMat;
    float4x4 projMat;
    float4   dimensions;
    float3   viewOrigin;
    uint     numLights;
};

struct pass_t
{
    uint codeImageCount;
};

struct surface_t
{
    float4x4 model;
    uint     diffuseIblCubeId;
    uint     envCubeId;
    uint     pad[14];
};

struct Globals_t
{
    float4  time;
    float4  generic;
    float4  shadowParms;
    float4  toneMapTint;
    float4  bloom;
    float4  exposure;
    float4  exposure2;
    float4  dof;
    uint    numSamples;
    uint    whiteId;
    uint    blackId;
    uint    defaultAlbedoId;
    uint    defaultNormalId;
    uint    defaultRoughnessId;
    uint    defaultMetalId;
    uint    defaultImageId;
    uint    brdfLutId;
    uint    isTextured;
    uint    shadow2dCount;
    uint    shadowCubeCount;
    uint    textureCount;
    uint    materialCount;
};

struct PushConstants_t
{
    uint objectId;
    uint materialId;
    uint viewId;
};

// ============================================================
// Resource binding macros
// ============================================================

#define IMAGE_CONSTANT_LAYOUT( S, N, TYPE, NAME )                                                                   \
    BIND_SET( S, N ) cbuffer _ShaderConstants                                                                       \
    {                                                                                                               \
        float4  dimensions;                                                                                         \
        uint    pass;                                                                                               \
        uint    previousImageId;                                                                                    \
        uint    level;                                                                                              \
        uint    layer;                                                                                              \
        uint    mipCount;                                                                                           \
        uint    layerCount;                                                                                         \
        uint    _sc_pad0;                                                                                           \
        uint    _sc_pad1;                                                                                           \
        TYPE    NAME;                                                                                               \
    };

#define CONSTANT_LAYOUT( S, N, TYPE, NAME )                                                                         \
                                                        BIND_SET( S, N ) cbuffer _ShaderConstants { TYPE NAME; };

#define MODEL_LAYOUT( S, N )                                                                                        \
                                                        BIND_SET( S, N ) StructuredBuffer<surface_t> surfaces;

#define GLOBALS_LAYOUT( S, N )                                                                                      \
                                                        BIND_SET( S, N ) ConstantBuffer<Globals_t> globals;

#define VIEW_LAYOUT( S, N )                                                                                         \
                                                        BIND_SET( S, N ) StructuredBuffer<view_t> views;

#define READ_BUFFER_LAYOUT( S, N, TYPE, NAME )                                                                      \
                                                        BIND_SET( S, N ) StructuredBuffer<TYPE> NAME;

#define WRITE_BUFFER_LAYOUT( S, N, TYPE, NAME )                                                                     \
                                                        BIND_SET( S, N ) RWStructuredBuffer<TYPE> NAME;

#define SAMPLER_2D( S, N )								BIND_SET( S, N ) SamplerState bilinearSampler;

#define SAMPLER_2D_LAYOUT( S, N )                                                                                   \
                                                        BIND_SET( S, N ) Texture2D texSampler[];                    \
                                                        BIND_SET( S, N ) SamplerState texSamplerSt;

#define SAMPLER_CUBE_LAYOUT( S, N )                                                                                 \
                                                        BIND_SET( S, N ) TextureCube cubeSamplers[];                \
                                                        BIND_SET( S, N ) SamplerState cubeSamplersSt;

#define CODE_IMAGE_LAYOUT( S, N, TEXTYPE )                                                                          \
                                                        BIND_SET( S, N ) TEXTYPE codeSamplers[];                    \
                                                        BIND_SET( S, N ) SamplerState codeSamplersSt;

#define CODE_IMAGE_CUBE_LAYOUT( S, N )                                                                              \
                                                        BIND_SET( S, N ) TextureCube codeCubeSamplers[];            \
                                                        BIND_SET( S, N ) SamplerState codeCubeSamplersSt;

#define STENCIL_LAYOUT( S, N, TEXTYPE )                                                                             \
                                                        BIND_SET( S, N ) TEXTYPE stencilImage;                      \
                                                        BIND_SET( S, N ) SamplerState stencilImageSt;

#define MATERIAL_LAYOUT( S, N )                                                                                     \
                                                        BIND_SET( S, N ) StructuredBuffer<material_t> materials;

#define LIGHT_LAYOUT( S, N )                                                                                        \
                                                        BIND_SET( S, N ) StructuredBuffer<light_t> lights;

#define PASS_LAYOUT( S, N )                                                                                         \
                                                        BIND_SET( S, N ) StructuredBuffer<pass_t> passData;

#define MATERIAL_PUSH_CONSTANTS                                                                                     \
                                                        BIND_INLINE PushConstants_t pushConstants;

// ============================================================
// Compound bind macros
// ============================================================

#define GLOBAL_BINDS( SET )                                                                                         \
                                                        GLOBALS_LAYOUT( SET, 0 )                                    \
                                                        VIEW_LAYOUT( SET, 1 )                                       \
                                                        SAMPLER_2D_LAYOUT( SET, 2 )                                 \
                                                        SAMPLER_CUBE_LAYOUT( SET, 3 )                               \
                                                        MATERIAL_LAYOUT( SET, 4 )									\
                                                        BIND_SET( SET, 5 ) SamplerState bilinearSamplerWrap;		\
                                                        BIND_SET( SET, 6 ) SamplerState bilinearSamplerClamp;

#define VIEW_BINDS( SET )                               MODEL_LAYOUT( SET, 0 )

#define PASS_BINDS( SET, TEXTYPE )                                                                                  \
                                                        LIGHT_LAYOUT( SET, 0 )                                      \
                                                        CODE_IMAGE_LAYOUT( SET, 1, TEXTYPE )                        \
                                                        CODE_IMAGE_CUBE_LAYOUT( SET, 2 )                            \
                                                        STENCIL_LAYOUT( SET, 3, TEXTYPE )

// ============================================================
// Vertex shader I/O
// ============================================================

#define VS_IN                                                                                                       \
    struct VS_Input                                                                                                 \
    {                                                                                                               \
        BIND_SLOT(0) float3 inPosition                  : POSITION;                                                 \
        BIND_SLOT(1) float4 inColor                     : COLOR0;                                                   \
        BIND_SLOT(2) float3 inNormal                    : NORMAL;                                                   \
        BIND_SLOT(3) float3 inTangent                   : TANGENT;                                                  \
        BIND_SLOT(4) float3 inBitangent                 : BINORMAL;                                                 \
        BIND_SLOT(5) float4 inTexCoord                  : TEXCOORD0;                                                \
    };

#define VS_OUT                                                                                                      \
    struct VS_Output                                                                                                \
    {                                                                                                               \
                     float4 pos                         : SV_Position;                                              \
        BIND_SLOT(0) float4 color                       : COLOR0;                                                   \
        BIND_SLOT(1) float3 normal                      : NORMAL;                                                   \
        BIND_SLOT(2) float3 tangent                     : TEXCOORD2;                                                \
        BIND_SLOT(3) float3 bitangent                   : TEXCOORD3;                                                \
        BIND_SLOT(4) float3 TBN2                        : TEXCOORD4;                                                \
        BIND_SLOT(5) float4 uv0                         : TEXCOORD5;                                                \
        BIND_SLOT(6) float3 objectPosition              : TEXCOORD6;                                                \
        BIND_SLOT(7) float4 clipPosition                : TEXCOORD7;                                                \
        BIND_SLOT(8) float4 worldPosition               : TEXCOORD8;                                                \
        BIND_SLOT(9) nointerpolation uint objectId      : TEXCOORD9;                                                \
    };

#define VS_LAYOUT_BASIC_IO                              VS_IN VS_OUT

#define VS_LAYOUT_STANDARD( TEXTYPE )                                                                               \
                                                        GLOBAL_BINDS( 0 )                                           \
                                                        VIEW_BINDS( 1 )                                             \
                                                        PASS_BINDS( 2, TEXTYPE )                                    \
                                                        MATERIAL_PUSH_CONSTANTS                                     \
                                                        VS_IN                                                       \
                                                        VS_OUT

// ============================================================
// Pixel shader I/O
// ============================================================

#define PS_IN                                                                                                       \
    struct PS_Input                                                                                                 \
    {                                                                                                               \
                     float4 pos                         : SV_Position;                                              \
        BIND_SLOT(0) float4 color                       : COLOR0;                                                   \
        BIND_SLOT(1) float3 normal                      : NORMAL;                                                   \
        BIND_SLOT(2) float3 tangent                     : TEXCOORD2;                                                \
        BIND_SLOT(3) float3 bitangent                   : TEXCOORD3;                                                \
        BIND_SLOT(4) float3 TBN2                        : TEXCOORD4;                                                \
        BIND_SLOT(5) float4 uv0                         : TEXCOORD5;                                                \
        BIND_SLOT(6) float3 objectPosition              : TEXCOORD6;                                                \
        BIND_SLOT(7) float4 clipPosition                : TEXCOORD7;                                                \
        BIND_SLOT(8) float4 worldPosition               : TEXCOORD8;                                                \
        BIND_SLOT(9) nointerpolation uint objectId      : TEXCOORD9;                                                \
    };

#define PS_OUT                                                                                                      \
    struct PS_Output                                                                                                \
    {                                                                                                               \
        float4 outColor : SV_Target0;                                                                               \
    };

#define PS_LAYOUT_MRT_1_OUT                                                                                         \
    struct PS_Output_MRT                                                                                            \
    {                                                                                                               \
        float4 outColor  : SV_Target0;                                                                              \
        float4 outColor1 : SV_Target1;                                                                              \
    };

#define PS_LAYOUT_BASIC_IO                              PS_IN PS_OUT

#define PS_LAYOUT_STANDARD( TEXTYPE )                                                                               \
                                                        GLOBAL_BINDS( 0 )                                           \
                                                        VIEW_BINDS( 1 )                                             \
                                                        PASS_BINDS( 2, TEXTYPE )                                    \
                                                        MATERIAL_PUSH_CONSTANTS                                     \
                                                        PS_IN                                                       \
                                                        PS_OUT

#define PS_LAYOUT_IMAGE_PROCESS( TEXTYPE, USERTYPE )                                                                \
                                                        GLOBAL_BINDS( 0 )                                           \
                                                        CODE_IMAGE_LAYOUT( 1, 0, TEXTYPE )                          \
                                                        CODE_IMAGE_CUBE_LAYOUT( 1, 1 )                              \
                                                        STENCIL_LAYOUT( 1, 2, TEXTYPE )                             \
                                                        IMAGE_CONSTANT_LAYOUT( 1, 3, USERTYPE, imageProcess )

#endif // GLOBALS_HLSL_H
