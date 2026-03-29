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

#include "globals_hlsl.h"
#include "color_hlsl.h"

PS_LAYOUT_STANDARD( Texture2D )

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;
    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

    const view_t view = views[ viewlId ];
    const material_t material = materials[ materialId ];

    const uint blendId = material.textureId0;
    const uint textureId0 = material.textureId1;
    const uint textureId1 = material.textureId2;

    const float maxHeight = globals.generic.x;
    const float4 blendValue = maxHeight * texSampler[NUI(blendId)].Sample( texSamplerSt, input.fragTexCoord.xy );
    const float4 texColor0 = SrgbToLinear( texSampler[NUI(textureId0)].Sample( texSamplerSt, input.fragTexCoord.xy ) );
    const float4 texColor1 = SrgbToLinear( texSampler[NUI(textureId1)].Sample( texSamplerSt, input.fragTexCoord.xy ) );
    const float4 texColor = lerp( texColor1, texColor0, smoothstep( 0.0f, 0.4f, blendValue ) );
    output.outColor = AMBIENT * texColor;

    for( int i = 0; i < (int)view.numLights; ++i )
    {
	    const float3 lightDist = -normalize( lights[ i ].lightPos.xyz - input.worldPosition.xyz );
        const float spotAngle = dot( lightDist, lights[ i ].lightDir.xyz );
        const float spotFov = 0.5f;
        float4 color = texColor;
        // color.rgb *= lights[ i ].intensity * max( 0.0f, dot( lightDist, normalize( input.fragNormal ) ) );
	    color.rgb *= lights[ i ].intensity.rgb * smoothstep( 0.5f, 0.8f, spotAngle );
        output.outColor += color;
    }
   // output.outColor.rgb = input.fragNormal;

    float visibility = 1.0f;
    const uint shadowMapTexId = 0;
    const uint shadowId = (uint)( globals.shadowParms.x );
    float4 lsPosition = mul( mul( view.projMat, view.viewMat ), float4( input.worldPosition.xyz, 1.0f ) );
    lsPosition.xyz /= lsPosition.w;
    float2 ndc = 0.5f * ( ( lsPosition.xy ) + 1.0f );
    float bias = 0.0001f;
    float depth = ( lsPosition.z );

    const int2 pixelLocation = int2( globals.shadowParms.yz * ndc.xy );
    const float shadowValue = codeSamplers[NUI(shadowMapTexId)].Load( int3( pixelLocation, 0 ) ).r;
    if( shadowValue < ( depth - bias ) ) // shadowed
    {
        visibility = globals.shadowParms.w;
    }
    output.outColor.rgb *= visibility;
	return output;
}
