/*
vec2f PackFloat32( const float unpacked )
{
	packFp32_t full;
	packFp16_t half;

	full.f = unpacked;

	half.fp.sign = full.fp.sign;
	if ( full.fp.exp > 0x70 ) {
		half.fp.exp = ( full.fp.exp - 0x70 ); // Implicitly clamps to INF
	}
	else {
		half.fp.exp = 0; // Flush denormals
	}
	half.fp.mantissa = full.fp.mantissa >> 13; // Don't round up

	return half.u;
}
*/

vec3 CubeVector( const vec3 v )
{
	return vec3( -v.y, v.z, v.x ); // to glsl coordinate space
}