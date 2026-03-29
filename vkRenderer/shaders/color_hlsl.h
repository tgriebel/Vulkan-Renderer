#ifndef COLOR_HLSL_H
#define COLOR_HLSL_H

float SrgbToLinear( float value )
{
	return ( value <= 0.04045f ) ? value / 12.92f : pow( ( value + 0.055f ) / 1.055f, 2.4f );
}

float3 SrgbToLinear( float3 sRGB )
{
	return float3( SrgbToLinear( sRGB.r ), SrgbToLinear( sRGB.g ), SrgbToLinear( sRGB.b ) );
}

float4 SrgbToLinear( float4 sRGBA )
{
	return float4( SrgbToLinear( sRGBA.rgb ), sRGBA.a );
}

float LinearToSrgb( float value )
{
	return ( value < 0.0031308f ? value * 12.92f : 1.055f * pow( value, 0.41666f ) - 0.055f );
}

float3 LinearToSrgb( float3 inLinear )
{
	return float3( LinearToSrgb( inLinear.r ), LinearToSrgb( inLinear.g ), LinearToSrgb( inLinear.b ) );
}

float4 LinearToSrgb( float4 inLinear )
{
	return float4( LinearToSrgb( inLinear.rgb ), inLinear.a );
}

float3 VectorDebugColor( const float3 vector )
{
	return 0.5f * ( vector + float3( 1.0f, 1.0f, 1.0f ) );
}

#endif // COLOR_HLSL_H
