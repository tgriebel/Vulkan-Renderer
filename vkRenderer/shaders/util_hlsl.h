#ifndef UTIL_HLSL_H
#define UTIL_HLSL_H

// GLSL mat4() fills columns-first; HLSL float4x4() fills rows-first.
// This is the transposed form so it matches the GLSL glslSpace matrix.
static const float4x4 glslSpace = float4x4(
	 0.0f, -1.0f, 0.0f, 0.0f,
	 0.0f,  0.0f, 1.0f, 0.0f,
	 1.0f,  0.0f, 0.0f, 0.0f,
	 0.0f,  0.0f, 0.0f, 0.0f
);

float3 CubeVector( const float3 v )
{
	return float3( -v.y, v.z, v.x ); // to glsl coordinate space
}

// Note: HLSL has built-in saturate() so this is just for reference
// float Saturate( const float v ) { return saturate( v ); }
// float3 Saturate( const float3 v ) { return saturate( v ); }

#endif // UTIL_HLSL_H
