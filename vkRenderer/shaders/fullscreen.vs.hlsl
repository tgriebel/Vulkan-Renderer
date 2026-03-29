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

VS_LAYOUT_BASIC_IO

static const float2 positions[ 3 ] = {
	float2( -1.0f, -1.0f ),
	float2( 3.0f, -1.0f ),
	float2( -1.0f, 3.0f )
};

static const float2 uvs[ 3 ] = {
	float2( 0.0f, 0.0f ),
	float2( 2.0f, 0.0f ),
	float2( 0.0f, 2.0f )
};

// TODO: replace with this
// float2( ( vertexId << 1 ) & 2, vertexId & 2 );
// float4( output.fragTexCoord.xy * 2.0f + -1.0f, 0.0f, 1.0f );

VS_Output VSMain( VS_Input input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	VS_Output output = (VS_Output)0;

	output.objectPosition	= float3( positions[ vertexId ].xy, 0.0f );
	output.worldPosition	= float4( positions[ vertexId ].xy, 0.0, 1.0 );
	output.pos				= output.worldPosition;
	output.fragColor		= float4( 1.0f, 1.0f, 1.0f, 1.0f );
	output.fragTexCoord		= float4( uvs[ vertexId ], 0.0, 0.0 );
	output.fragNormal		= float3( 0.0f, 0.0f, 1.0f );
	output.clipPosition		= output.pos;

	return output;
}
