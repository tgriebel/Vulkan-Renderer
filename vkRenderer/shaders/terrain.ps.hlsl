#include "globals.h"
#include "color.h"

PS_LAYOUT_STANDARD( Texture2D )

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;
    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

    const view_t view = views[ viewlId ];
	const gpuMaterial_t material = materials[materialId];

    const uint blendId = material.textureId[ 0 ];
    const uint textureId0 = material.textureId[ 1 ];
    const uint textureId1 = material.textureId[ 2 ];

    const float maxHeight = globals.generic.x;
    const float4 blendValue = maxHeight * texSampler[ blendId ].Sample( bilinearSamplerWrap, input.uv0.xy );
    const float4 texColor0 = SrgbToLinear( texSampler[ textureId0 ].Sample( bilinearSamplerWrap, input.uv0.xy ) );
    const float4 texColor1 = SrgbToLinear( texSampler[ textureId1 ].Sample( bilinearSamplerWrap, input.uv0.xy ) );
    const float4 texColor = lerp( texColor1, texColor0, smoothstep( 0.0f, 0.4f, blendValue ) );
    output.outColor = AMBIENT * texColor;

    for( int i = 0; i < (int)view.numLights; ++i )
    {
	    const float3 lightDist = -normalize( lights[ i ].lightPos.xyz - input.worldPosition.xyz );
        const float spotAngle = dot( lightDist, lights[ i ].lightDir.xyz );
        const float spotFov = 0.5f;
        float4 color = texColor;
        // color.rgb *= lights[ i ].intensity * max( 0.0f, dot( lightDist, normalize( input.normal ) ) );
	    color.rgb *= lights[ i ].intensity.rgb * smoothstep( 0.5f, 0.8f, spotAngle );
        output.outColor += color;
    }
   // output.outColor.rgb = input.normal;

    float visibility = 1.0f;
    const uint shadowMapTexId = 0;
    const uint shadowId = (uint)( globals.shadowParms.x );
    float4 lsPosition = mul( mul( view.projMat, view.viewMat ), float4( input.worldPosition.xyz, 1.0f ) );
    lsPosition.xyz /= lsPosition.w;
    float2 ndc = 0.5f * ( ( lsPosition.xy ) + 1.0f );
    float bias = 0.0001f;
    float depth = ( lsPosition.z );

    const int2 pixelLocation = int2( globals.shadowParms.yz * ndc.xy );
    const float shadowValue = codeSamplers[ shadowMapTexId ].Load( int3( pixelLocation, 0 ) ).r;
    if( shadowValue < ( depth - bias ) ) // shadowed
    {
        visibility = globals.shadowParms.w;
    }
    output.outColor.rgb *= visibility;
	return output;
}
