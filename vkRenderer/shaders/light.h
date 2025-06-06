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

float D_GGX( const float NoH, const float roughness )
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NoH2 = NoH * NoH;

    float num = a2;
    float denom = ( NoH2 * ( a2 - 1.0 ) + 1.0 );
    denom = PI * denom * denom;

    return num / denom;
}

float G_SchlickGGX( const float cosAngle, const float roughness )
{
    const float r = ( roughness + 1.0 );
    const float k = ( r * r ) / 8.0f;

    const float num = cosAngle;
    const float denom = cosAngle * ( 1.0f - k ) + k;

    return ( num / denom );
}

float G_Smith( const float NoV, const float NoL, const float roughness )
{
    const float ggx2 = G_SchlickGGX( NoV, roughness );
    const float ggx1 = G_SchlickGGX( NoL, roughness );
    return ( ggx1 * ggx2 );
}

vec3 F_Schlick( float cosTheta, vec3 f0 ) {
    return f0 + ( 1.0 - f0 ) * pow( 1.0 - cosTheta, 5.0 );
}

vec3 F_SchlickRoughness( float cosTheta, vec3 f0, float roughness )
{
    return f0 + ( max( vec3( 1.0 - roughness ), f0 ) - f0 ) * pow( clamp( 1.0 - cosTheta, 0.0, 1.0 ), 5.0 );
}

float Fd_Lambert() {
    return 1.0 / PI;
}

// https://learnopengl.com/PBR/IBL/Specular-IBL
float RadicalInverse_VdC( uint bits )
{
    bits = ( bits << 16u ) | ( bits >> 16u );
    bits = ( ( bits & 0x55555555u ) << 1u ) | ( ( bits & 0xAAAAAAAAu ) >> 1u );
    bits = ( ( bits & 0x33333333u ) << 2u ) | ( ( bits & 0xCCCCCCCCu ) >> 2u );
    bits = ( ( bits & 0x0F0F0F0Fu ) << 4u ) | ( ( bits & 0xF0F0F0F0u ) >> 4u );
    bits = ( ( bits & 0x00FF00FFu ) << 8u ) | ( ( bits & 0xFF00FF00u ) >> 8u );
    return float( bits ) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
vec2 Hammersley( uint i, uint N )
{
    return vec2( float( i ) / float( N ), RadicalInverse_VdC( i ) );
}

vec3 ImportanceSampleGGX( vec2 Xi, vec3 N, float roughness )
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt( ( 1.0 - Xi.y ) / ( 1.0 + ( a * a - 1.0 ) * Xi.y ) );
    float sinTheta = sqrt( 1.0 - cosTheta * cosTheta );

    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos( phi ) * sinTheta;
    H.y = sin( phi ) * sinTheta;
    H.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    vec3 up = abs( N.z ) < 0.999 ? vec3( 0.0, 0.0, 1.0 ) : vec3( 1.0, 0.0, 0.0 );
    vec3 tangent = normalize( cross( up, N ) );
    vec3 bitangent = cross( N, tangent );

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize( sampleVec );
}
