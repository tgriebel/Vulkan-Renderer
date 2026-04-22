#ifndef GLOBALS_HLSL_H
#define GLOBALS_HLSL_H

#define SHADER_STRUCTS_HLSL
#include "gpuShared.h"

// ============================================================
// Constants
// ============================================================

#define PI              3.14159265359f
#define AMBIENT         float4( 0.03f, 0.03f, 0.03f, 1.0f )

// ============================================================
// Convenience
// ============================================================

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


float3 DecodeNormal( const float3 normalMapTexel )
{
	return ( 2.0f * normalMapTexel - float3( 1.0f, 1.0f, 1.0f ) );
}


float3 ComputeNormalWS( const float3 tangentNormal, const float3 T, const float3 B, const float3 N )
{
	return normalize( tangentNormal.x * T + tangentNormal.y * B + tangentNormal.z * N );
}


float3 ClampColorFp16( const float3 color )
{
	return min( color, 65504.0f );
}


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

#define CONSTANT_LAYOUT( S, N, TYPE, NAME )				BIND_SET( S, N ) cbuffer _ShaderConstants { TYPE NAME; };

#define MODEL_LAYOUT( S, N )                            BIND_SET( S, N ) StructuredBuffer<gpuSurface_t> surfaces;

#define GLOBALS_LAYOUT( S, N )                          BIND_SET( S, N ) ConstantBuffer<gpuGlobals_t> globals;

#define VIEW_LAYOUT( S, N )                             BIND_SET( S, N ) StructuredBuffer<gpuView_t> views;

#define READ_BUFFER_LAYOUT( S, N, TYPE, NAME )          BIND_SET( S, N ) StructuredBuffer<TYPE> NAME;

#define WRITE_BUFFER_LAYOUT( S, N, TYPE, NAME )         BIND_SET( S, N ) RWStructuredBuffer<TYPE> NAME;

#define SAMPLER( S, N, NAME )							BIND_SET( S, N ) SamplerState NAME;

#define SAMPLER_2D_LAYOUT( S, N )                       BIND_SET( S, N ) Texture2D texSampler[];

#define SAMPLER_CUBE_LAYOUT( S, N )                     BIND_SET( S, N ) TextureCube cubeSamplers[];

#define CODE_IMAGE_LAYOUT( S, N, TEXTYPE )              BIND_SET( S, N ) TEXTYPE codeSamplers[];

#define CODE_IMAGE_CUBE_LAYOUT( S, N )                  BIND_SET( S, N ) TextureCube codeCubeSamplers[];

#define STENCIL_LAYOUT( S, N, TEXTYPE )                 BIND_SET( S, N ) TEXTYPE stencilImage;

#define MATERIAL_LAYOUT( S, N )                         BIND_SET( S, N ) StructuredBuffer<gpuMaterial_t> materials;

#define LIGHT_LAYOUT( S, N )                            BIND_SET( S, N ) StructuredBuffer<gpuLight_t> lights;

#define PASS_LAYOUT( S, N )                             BIND_SET( S, N ) StructuredBuffer<gpuPass_t> passData;

#define MATERIAL_PUSH_CONSTANTS                         BIND_INLINE gpuPushConstants_t pushConstants;

// ============================================================
// Compound bind macros
// ============================================================

#define GLOBAL_BINDS( SET )                                                                                         \
                                                        GLOBALS_LAYOUT( SET, 0 )                                    \
                                                        VIEW_LAYOUT( SET, 1 )                                       \
                                                        SAMPLER_2D_LAYOUT( SET, 2 )                                 \
                                                        SAMPLER_CUBE_LAYOUT( SET, 3 )                               \
                                                        MATERIAL_LAYOUT( SET, 4 )									\
                                                        SAMPLER( SET, 5, bilinearSamplerWrap )						\
														SAMPLER( SET, 6, bilinearSamplerClampEdge )					\
                                                        SAMPLER( SET, 7, bilinearSamplerClampBorder );				\
                                                        SAMPLER( SET, 8, depthShadowSampler );

#define VIEW_BINDS( SET )                               MODEL_LAYOUT( SET, 0 )

#define PASS_BINDS( SET, TEXTYPE )                                                                                  \
                                                        LIGHT_LAYOUT( SET, 0 )                                      \
                                                        CODE_IMAGE_LAYOUT( SET, 1, TEXTYPE )                        \
                                                        CODE_IMAGE_CUBE_LAYOUT( SET, 2 )                            \
                                                        STENCIL_LAYOUT( SET, 3, TEXTYPE )

// ============================================================
// Vertex shader I/O
// ============================================================

struct vsOutput_t
{
	float4 pos : SV_Position;
	BIND_SLOT( 0 ) float4 color						: COLOR0;
	BIND_SLOT( 1 ) float3 normal					: NORMAL;
	BIND_SLOT( 2 ) float3 tangent					: TEXCOORD2;
	BIND_SLOT( 3 ) float3 bitangent					: TEXCOORD3;
	BIND_SLOT( 4 ) float3 TBN2						: TEXCOORD4;
	BIND_SLOT( 5 ) float4 uv0						: TEXCOORD5;
	BIND_SLOT( 6 ) float3 objectPosition			: TEXCOORD6;
	BIND_SLOT( 7 ) float4 clipPosition				: TEXCOORD7;
	BIND_SLOT( 8 ) float4 worldPosition				: TEXCOORD8;
	BIND_SLOT( 9 ) nointerpolation uint objectId	: TEXCOORD9;
};

#define VS_LAYOUT_STANDARD( TEXTYPE )                                                                               \
                                                        GLOBAL_BINDS( 0 )                                           \
                                                        VIEW_BINDS( 1 )                                             \
                                                        PASS_BINDS( 2, TEXTYPE )                                    \
                                                        MATERIAL_PUSH_CONSTANTS

// ============================================================
// Pixel shader I/O
// ============================================================

struct PS_Input
{
				 float4 pos                         : SV_Position;
    BIND_SLOT(0) float4 color                       : COLOR0;
    BIND_SLOT(1) float3 normal                      : NORMAL;
    BIND_SLOT(2) float3 tangent                     : TEXCOORD2;
    BIND_SLOT(3) float3 bitangent                   : TEXCOORD3;
    BIND_SLOT(4) float3 TBN2                        : TEXCOORD4;
    BIND_SLOT(5) float4 uv0                         : TEXCOORD5;
    BIND_SLOT(6) float3 objectPosition              : TEXCOORD6;
    BIND_SLOT(7) float4 clipPosition                : TEXCOORD7;
    BIND_SLOT(8) float4 worldPosition               : TEXCOORD8;
    BIND_SLOT(9) nointerpolation uint objectId      : TEXCOORD9;
};

struct PS_Output
{
    float4 outColor : SV_Target0;
};

#define PS_LAYOUT_MRT_1_OUT                                                                                         \
    struct PS_Output_MRT                                                                                            \
    {                                                                                                               \
        float4 outColor  : SV_Target0;                                                                              \
        float4 outColor1 : SV_Target1;                                                                              \
    };

#define PS_LAYOUT_STANDARD( TEXTYPE )                                                                               \
                                                        GLOBAL_BINDS( 0 )                                           \
                                                        VIEW_BINDS( 1 )                                             \
                                                        PASS_BINDS( 2, TEXTYPE )                                    \
                                                        MATERIAL_PUSH_CONSTANTS

#define PS_LAYOUT_IMAGE_PROCESS( TEXTYPE, USERTYPE )                                                                \
                                                        GLOBAL_BINDS( 0 )                                           \
                                                        CODE_IMAGE_LAYOUT( 1, 0, TEXTYPE )                          \
                                                        CODE_IMAGE_CUBE_LAYOUT( 1, 1 )                              \
                                                        STENCIL_LAYOUT( 1, 2, TEXTYPE )                             \
                                                        IMAGE_CONSTANT_LAYOUT( 1, 3, USERTYPE, imageProcess )

#endif // GLOBALS_HLSL_H
