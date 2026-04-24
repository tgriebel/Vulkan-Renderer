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


float ClampColorFp16( const float color )
{
	return clamp( color, 0.0f, 65504.0f );
}



float2 ClampColorFp16( const float2 color )
{
	return clamp( color, 0.0f, 65504.0f );
}


float3 ClampColorFp16( const float3 color )
{
	return clamp( color, 0.0f, 65504.0f );
}


float4 ClampColorFp16( const float4 color )
{
	return clamp( color, 0.0f, 65504.0f );
}

#endif // UTIL_HLSL_H
