

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

float Fd_Lambert() {
    return 1.0 / PI;
}

float Shadow( in light_t light, in vec3 pos )
{
    const uint shadowViewId = light.shadowViewId;
    if ( shadowViewId != 0xFF )
    {
        const view_t shadowView = viewUbo.views[ shadowViewId ];

        const uint shadowMapTexId = shadowViewId;
        vec4 lsPosition = shadowView.projMat * shadowView.viewMat * vec4( pos.xyz, 1.0f );
        lsPosition.xyz /= lsPosition.w;

        const vec2 ndc = 0.5f * ( ( lsPosition.xy ) + 1.0f );
        const float bias = 0.001f;
        const float depth = ( lsPosition.z );

        if ( length( ndc.xy - vec2( 0.5f ) ) < 0.5f )
        {
            const ivec2 shadowPixelLocation = ivec2( globals.shadowParms.yz * ndc.xy );
            const float shadowValue = texelFetch( codeSamplers[ shadowMapTexId ], shadowPixelLocation, 0 ).r;
            if ( shadowValue < ( depth - bias ) ) {
                return 1.0f - min( 1.0f, globals.shadowParms.w );
            }
        }
        else {
            return 1.0f - min( 1.0f, globals.shadowParms.w );
        }
    }
    return 1.0f;
}