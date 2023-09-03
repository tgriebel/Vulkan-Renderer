float SrgbToLinear( float value ) {
	return( value <= 0.04045f ) ? value / 12.92f : pow( ( value + 0.055f ) / 1.055f, 2.4f );
}

vec3 SrgbToLinear( vec3 sRGB ) {
	return vec3( SrgbToLinear( sRGB.r ), SrgbToLinear( sRGB.g ), SrgbToLinear( sRGB.b ) );
}

vec4 SrgbToLinear( vec4 sRGBA ) {
	return vec4( SrgbToLinear( sRGBA.rgb ), sRGBA.a );
}

float LinearToSrgb( float value ) {
	return( value < 0.0031308f ? value * 12.92f : 1.055f * pow( value, 0.41666f ) - 0.055f );
}

vec3 LinearToSrgb( vec3 inLinear ) {
	return vec3( LinearToSrgb( inLinear.r ), LinearToSrgb( inLinear.g ), LinearToSrgb( inLinear.b ) );
}

vec4 LinearToSrgb( vec4 inLinear ) {
	return vec4( LinearToSrgb( inLinear.rgb ), inLinear.a );
}