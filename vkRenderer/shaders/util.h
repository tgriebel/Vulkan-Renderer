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


// https://therealmjp.github.io/posts/position-from-depth-3/
float LinearDepth( const float zDepth, const float4x4 proj )
{
	float a = proj[ 2 ][ 2 ];
	float b = proj[ 3 ][ 2 ];
	return b / ( zDepth + a );
}


float3 ReconstructViewPos( const float2 uv, const float zDepth, const float4x4 proj )
{
	float depth = LinearDepth( zDepth, proj );

	float2 ndc = float2( uv.x * 2.0f - 1.0f, uv.y * 2.0f - 1.0f );

	float2 viewPos = ndc * depth * float2( 1.0f / proj[ 0 ][ 0 ], 1.0f / proj[ 1 ][ 1 ] );

	return float3( viewPos, -depth );
}


// https://atyuwen.github.io/posts/normal-reconstruction/
float3 ReconstructNormal( Texture2D depthBufer, const float2 uv, const float4x4 proj )
{
	const float2 ts = float2( dimensions.z, dimensions.w );

	const float dR = depthBufer.SampleLevel( bilinearSamplerClampEdge, uv + float2( ts.x, 0.0f ), 0 ).r;
	const float dL = depthBufer.SampleLevel( bilinearSamplerClampEdge, uv + float2( -ts.x, 0.0f ), 0 ).r;
	const float dU = depthBufer.SampleLevel( bilinearSamplerClampEdge, uv + float2( 0.0f, ts.y ), 0 ).r;
	const float dD = depthBufer.SampleLevel( bilinearSamplerClampEdge, uv + float2( 0.0f, -ts.y ), 0 ).r;

	const float3 pR = ReconstructViewPos( uv + float2( ts.x, 0.0f ), dR, proj );
	const float3 pL = ReconstructViewPos( uv + float2( -ts.x, 0.0f ), dL, proj );
	const float3 pU = ReconstructViewPos( uv + float2( 0.0f, ts.y ), dU, proj );
	const float3 pD = ReconstructViewPos( uv + float2( 0.0f, -ts.y ), dD, proj );

	// Pick the horizontal and vertical pair with the smaller depth discontinuity
	// so normals stay sharp at silhouette edges.
	const float3 dX = ( abs( dR - dL ) < abs( dU - dD ) ) ? ( pR - pL ) : ( pL - pR );
	const float3 dY = ( abs( dU - dD ) < abs( dR - dL ) ) ? ( pU - pD ) : ( pD - pU );

	return normalize( cross( dX, dY ) );
}


void BuildTBN( in float3 N, out float3 T, out float3 B )
{
	float3 up = ( abs( N.z ) < 0.999f ) ? float3( 0.0f, 0.0f, 1.0f ) : float3( 1.0f, 0.0f, 0.0f );
	T = normalize( cross( up, N ) );
	B = cross( N, T );
}


// http://advances.realtimerendering.com/s2014/index.html, CoD:AW slide 123
float InterleavedGradientNoise( float2 screenPos )
{
	const float3 k = float3( 0.06711056f, 0.00583715f, 52.9829189f );
	return frac( k.z * frac( dot( screenPos, k.xy ) ) );
}

#endif // UTIL_HLSL_H
