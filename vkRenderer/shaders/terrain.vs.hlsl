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

float3x3 GetTerrainTangent( float2 uv )
{
	int2 texDim = GetTextureSize( texSampler[2], 0 );

	const float maxHeight = 1.0f;
	float gridSize = 0.01f;
	int cx = int( uv.x * texDim.x );
	int cy = int( uv.y * texDim.y );
	int x0 = cx - 1;
	int x1 = cx + 1;
	int y0 = cy - 1;
	int y1 = cy + 1;

	float dzdx = texSampler[2].Load( int3( x1, cy, 0 ) ).r - texSampler[2].Load( int3( x0, cy, 0 ) ).r;
	float dzdy = texSampler[2].Load( int3( cx, y1, 0 ) ).r - texSampler[2].Load( int3( cx, y0, 0 ) ).r;

	dzdx *= maxHeight / gridSize;
	dzdy *= maxHeight / gridSize;

	float3 bx = normalize( float3( 2.0f, 0.0f, dzdx ) );
	float3 by = normalize( float3( 0.0f, 2.0f, dzdy ) );

	return float3x3( bx, by, float3( 0.0f, 0.0f, 1.0f ) );
}

VS_Output VSMain( VS_Input input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	VS_Output output = (VS_Output)0;

	output.objectId = pushConstants.objectId + instanceIndex;
	const uint materialId = pushConstants.materialId;
	const uint viewlId = pushConstants.viewId;

	const view_t view = views[ viewlId ];

	const uint heightMapId = materials[ materialId ].textureId0;
	const float heightMapValue = texSampler[ heightMapId ].SampleLevel( texSamplerSt, input.inTexCoord.xy, 0 ).r;

	const float maxHeight = globals.generic.x;
	float3 position = input.inPosition;
	position.z += maxHeight * heightMapValue;
	output.objectPosition = position;
	output.worldPosition = mul( surfaces[ output.objectId ].model, float4( position, 1.0f ) );
	output.pos = mul( view.projMat, mul( view.viewMat, output.worldPosition ) );
	output.color = input.inColor;
	output.uv0 = input.inTexCoord;

	float3x3 modelMat3 = (float3x3)surfaces[ output.objectId ].model;
	float3x3 tangentMat = GetTerrainTangent( input.inTexCoord.xy );
	float3 wt0 = mul( modelMat3, tangentMat[0] );
	float3 wt1 = mul( modelMat3, tangentMat[1] );
	output.normal = normalize( cross( wt0, wt1 ) );
	output.clipPosition = output.pos;

	return output;
}
