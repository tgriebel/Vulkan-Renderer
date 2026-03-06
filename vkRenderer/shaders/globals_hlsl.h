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

#ifndef GLOBALS_H
#define GLOBALS_H

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
    int    textureId0;
    int    textureId1;
    int    textureId2;
    int    textureId3;
    int    textureId4;
    int    textureId5;
    int    textureId6;
    int    textureId7;
    float3 Ka;
    float  Tr;
    float3 Ke;
    float  Ns;
    float3 Kd;
    float  Ni;
    float3 Ks;
    float  illum;
    float3 Tf;
    uint   textured;
    uint   pad0;
    uint   pad1;
    uint   pad2;
    uint   pad3;
    uint   extraData[64];
};

struct view_t
{
    float4x4 viewMat;
    float4x4 projMat;
    float4   dimensions;
    uint     numLights;
    uint     pad0;
    uint     pad1;
    uint     pad2;
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

// ============================================================
// Layout macros
// ============================================================

#define IMAGE_CONSTANT_LAYOUT( S, N, TYPE, NAME )                                   \
    cbuffer ShaderConstants : register(b##N, space##S)                              \
    {                                                                               \
        float4  dimensions;                                                         \
        uint    pass;                                                               \
        uint    previousImageId;                                                    \
        uint    level;                                                              \
        uint    layer;                                                              \
        uint    mipCount;                                                           \
        uint    layerCount;                                                         \
        TYPE    NAME;                                                               \
    };

#define CONSTANT_LAYOUT( S, N, TYPE, NAME )                                         \
    cbuffer ShaderConstants : register(b##N, space##S)                              \
    {                                                                               \
        TYPE    NAME;                                                               \
    };

#define MODEL_LAYOUT( S, N )                                                        \
    StructuredBuffer<surface_t> ubo : register(t##N, space##S);

#define GLOBALS_LAYOUT( S, N )                                                      \
    cbuffer GlobalConstants : register(b##N, space##S)                              \
    {                                                                               \
        float4  time;                                                               \
        float4  generic;                                                            \
        float4  shadowParms;                                                        \
        float4  toneMap;                                                            \
        float4  dof;                                                                \
        uint    numSamples;                                                         \
        uint    whiteId;                                                            \
        uint    blackId;                                                            \
        uint    defaultAlbedoId;                                                    \
        uint    defaultNormalId;                                                    \
        uint    defaultRoughnessId;                                                 \
        uint    defaultMetalId;                                                     \
        uint    defaultImageId;                                                     \
        uint    brdfLutId;                                                          \
        uint    isTextured;                                                         \
        uint    shadow2dCount;                                                      \
        uint    shadowCubeCount;                                                    \
        uint    textureCount;                                                       \
        uint    materialCount;                                                      \
    };

#define VIEW_LAYOUT( S, N )                                                         \
    StructuredBuffer<view_t> viewUbo : register(t##N, space##S);

#define READ_BUFFER_LAYOUT( S, N, TYPE, NAME )                                      \
    StructuredBuffer<TYPE> NAME : register(t##N, space##S);

#define WRITE_BUFFER_LAYOUT( S, N, TYPE, NAME )                                     \
    RWStructuredBuffer<TYPE> NAME : register(u##N, space##S);

#define SAMPLER_2D_LAYOUT( S, N )                                                   \
    Texture2D texSampler[] : register(t##N, space##S);

#define SAMPLER_CUBE_LAYOUT( S, N )                                                 \
    TextureCube cubeSamplers[] : register(t##N, space##S);

#define CODE_IMAGE_LAYOUT( S, N, SAMPLER )                                          \
    Texture2D codeSamplers[] : register(t##N, space##S);

#define CODE_IMAGE_CUBE_LAYOUT( S, N )                                              \
    TextureCube codeCubeSamplers[] : register(t##N, space##S);

#define STENCIL_LAYOUT( S, N, SAMPLER )                                             \
    Texture2D stencilImage : register(t##N, space##S);

#define MATERIAL_LAYOUT( S, N )                                                     \
    StructuredBuffer<material_t> materialUbo : register(t##N, space##S);

#define LIGHT_LAYOUT( S, N )                                                        \
    StructuredBuffer<light_t> lightUbo : register(t##N, space##S);

#define PASS_LAYOUT( S, N )                                                         \
    StructuredBuffer<pass_t> passUbo : register(t##N, space##S);

#define MATERIAL_PUSH_CONSTANTS                                                     \
    [[vk::push_constant]]                                                           \
    cbuffer fragmentPushConstants                                                   \
    {                                                                               \
        uint objectId;                                                              \
        uint materialId;                                                            \
        uint viewId;                                                                \
    };

// ============================================================
// Vertex shader I/O macros
// ============================================================

#define VS_IN                                                                       \
    struct VS_Input                                                                 \
    {                                                                               \
        float3 inPosition  : POSITION;                                             \
        float4 inColor     : COLOR0;                                               \
        float3 inNormal    : NORMAL;                                               \
        float3 inTangent   : TANGENT;                                              \
        float3 inBitangent : BINORMAL;                                             \
        float4 inTexCoord  : TEXCOORD0;                                            \
    };

#define VS_OUT                                                                      \
    struct VS_Output                                                                \
    {                                                                               \
        float4 pos            : SV_Position;                                       \
        float4 fragColor      : TEXCOORD0;                                         \
        float3 fragNormal     : TEXCOORD1;                                         \
        float3 fragTangent    : TEXCOORD2;                                         \
        float3 fragBitangent  : TEXCOORD3;                                         \
        float3 fragTBN2       : TEXCOORD4;                                         \
        float4 fragTexCoord   : TEXCOORD5;                                         \
        float3 objectPosition : TEXCOORD6;                                         \
        float4 clipPosition   : TEXCOORD7;                                         \
        float4 worldPosition  : TEXCOORD8;                                         \
        uint   objectId       : TEXCOORD9;                                         \
    };

#define VS_LAYOUT_BASIC_IO  VS_IN VS_OUT

#define VS_LAYOUT_STANDARD( SAMPLER )                                               \
    GLOBALS_LAYOUT( 0, 0 )                                                          \
    VIEW_LAYOUT( 0, 1 )                                                             \
    SAMPLER_2D_LAYOUT( 0, 2 )                                                       \
    SAMPLER_CUBE_LAYOUT( 0, 3 )                                                     \
    MATERIAL_LAYOUT( 0, 4 )                                                         \
    MODEL_LAYOUT( 1, 0 )                                                            \
    LIGHT_LAYOUT( 2, 0 )                                                            \
    CODE_IMAGE_LAYOUT( 2, 1, SAMPLER )                                              \
    STENCIL_LAYOUT( 2, 2, SAMPLER )                                                 \
    MATERIAL_PUSH_CONSTANTS                                                         \
    VS_IN                                                                           \
    VS_OUT

// ============================================================
// Pixel shader I/O macros
// ============================================================

#define PS_IN                                                                       \
    struct PS_Input                                                                 \
    {                                                                               \
        float4 pos            : SV_Position;                                       \
        float4 fragColor      : TEXCOORD0;                                         \
        float3 fragNormal     : TEXCOORD1;                                         \
        float3 fragTangent    : TEXCOORD2;                                         \
        float3 fragBitangent  : TEXCOORD3;                                         \
        float3 fragTBN2       : TEXCOORD4;                                         \
        float4 fragTexCoord   : TEXCOORD5;                                         \
        float3 objectPosition : TEXCOORD6;                                         \
        float4 clipPosition   : TEXCOORD7;                                         \
        float4 worldPosition  : TEXCOORD8;                                         \
        uint   objectId       : TEXCOORD9;                                         \
    };

#define PS_OUT                                                                      \
    struct PS_Output                                                                \
    {                                                                               \
        float4 outColor : SV_Target0;                                              \
    };

#define PS_LAYOUT_MRT_1_OUT                                                         \
    float4 outColor1 : SV_Target1;

#define PS_LAYOUT_BASIC_IO  PS_IN PS_OUT

#define PS_LAYOUT_STANDARD( SAMPLER )                                               \
    GLOBALS_LAYOUT( 0, 0 )                                                          \
    VIEW_LAYOUT( 0, 1 )                                                             \
    SAMPLER_2D_LAYOUT( 0, 2 )                                                       \
    SAMPLER_CUBE_LAYOUT( 0, 3 )                                                     \
    MATERIAL_LAYOUT( 0, 4 )                                                         \
    MODEL_LAYOUT( 1, 0 )                                                            \
    LIGHT_LAYOUT( 2, 0 )                                                            \
    CODE_IMAGE_LAYOUT( 2, 1, SAMPLER )                                              \
    STENCIL_LAYOUT( 2, 2, SAMPLER )                                                 \
    MATERIAL_PUSH_CONSTANTS                                                         \
    PS_IN                                                                           \
    PS_OUT

#define PS_LAYOUT_IMAGE_PROCESS( SAMPLER, TYPE )                                    \
    GLOBALS_LAYOUT( 0, 0 )                                                          \
    VIEW_LAYOUT( 0, 1 )                                                             \
    SAMPLER_2D_LAYOUT( 0, 2 )                                                       \
    SAMPLER_CUBE_LAYOUT( 0, 3 )                                                     \
    MATERIAL_LAYOUT( 0, 4 )                                                         \
    CODE_IMAGE_LAYOUT( 1, 0, SAMPLER )                                              \
    CODE_IMAGE_CUBE_LAYOUT( 1, 1 )                                                  \
    STENCIL_LAYOUT( 1, 2, SAMPLER )                                                 \
    IMAGE_CONSTANT_LAYOUT( 1, 3, TYPE, imageProcess )

#endif // GLOBALS_H