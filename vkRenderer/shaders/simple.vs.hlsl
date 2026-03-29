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

VS_LAYOUT_STANDARD( Texture2D )

VS_Output VSMain( VS_Input input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	VS_Output output = (VS_Output)0;

	output.objectId = pushConstants.objectId + instanceIndex;
	const uint materialId = pushConstants.materialId;
	const uint viewlId = pushConstants.viewId;

	const view_t view = views[ viewlId ];
	const float4x4 modelMatrix = surfaces[ output.objectId ].model;

	float3 position = input.inPosition;
	output.objectPosition = position;
	output.worldPosition = mul( modelMatrix, float4( position, 1.0f ) );
	output.pos = mul( view.projMat, mul( view.viewMat, output.worldPosition ) );

	// Tangent-space matrix
	{
		const float normalSign = ( asuint( input.inTangent.x ) & 0x1 ) > 0 ? -1.0f : 1.0f;
		float3 T = normalize( float3( asfloat( asuint( input.inTangent.x ) & ~0x1 ), input.inTangent.yz ) );
		float3 N = normalize( normalSign * cross( input.inTangent, input.inBitangent ) );
		float3 B = normalize( input.inBitangent );
		T = mul( modelMatrix, float4( T, 0.0f ) ).xyz;
		N = mul( modelMatrix, float4( N, 0.0f ) ).xyz;
		B = mul( modelMatrix, float4( B, 0.0f ) ).xyz;
		output.tangent = T;
		output.bitangent = B;
		output.TBN2 = N;
	}

	output.color = input.inColor;
	output.uv0 = input.inTexCoord;
	output.clipPosition = output.pos;

	return output;
}
