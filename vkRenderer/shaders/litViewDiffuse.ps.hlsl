#include "globals.h"

PS_LAYOUT_STANDARD(Texture2D)

#ifdef USE_MRT
PS_LAYOUT_MRT_1_OUT
#endif

PS_Output PSMain(PS_Input input)
{
	const uint materialId = pushConstants.materialId;
	const uint viewlId = pushConstants.viewId;

	const view_t view = views[viewlId];
	const material_t material = materials[materialId];

	const float4x4 modelMat = surfaces[input.objectId].model;
	const float4x4 viewMat = view.viewMat;
	const float3 cameraOrigin = view.viewOrigin;
	const float3 modelOrigin = float3(modelMat[0][3], modelMat[1][3], modelMat[2][3]);

	const float3 normalSample = float3( 0.0f, 0.0f, 1.0f );

	const float3 normal = normalize( normalSample.x * input.tangent + normalSample.y * input.bitangent + normalSample.z * input.TBN2 );
	
	const float3 V = normalize( cameraOrigin.xyz - input.worldPosition.xyz );
	const float3 N = normalize( normal );
	float NoV = saturate( dot( N, V ) );

	float4 outColor;
	outColor.rgb = NoV * float3( 1.0f, 1.0f, 1.0f );
	outColor.a = 1.0f;

#ifdef USE_MRT
    float4 outColor1;
    outColor1.rgb = 0.5f * ( N + float3( 1.0f, 1.0f, 1.0f ) );
    outColor1.a = 1.0f;

    PS_Output_MRT output = (PS_Output_MRT)0;

    output.outColor = outColor;
    output.outColor1 = outColor1;
#else
	PS_Output output = (PS_Output) 0;

	output.outColor = outColor;
#endif
	return output;
}
