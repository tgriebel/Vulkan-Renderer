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

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

VS_LAYOUT_STANDARD( sampler2D )

void main()
{
	objectId = pushConstants.objectId + gl_InstanceIndex;
	const uint materialId = pushConstants.materialId;
	const uint viewlId = pushConstants.viewId;

	vec3 position = inPosition;
	worldPosition = ubo.model[ objectId ] * vec4( position, 1.0f );
    gl_Position = viewUbo.views[ viewlId ].proj * viewUbo.views[ viewlId ].view * worldPosition;

	// Tangent-space matrix
	{
		const float normalSign = ( floatBitsToUint( inTangent.x ) & 0x1 ) > 0 ? -1.0f : 1.0f;
		vec3 T = normalize( vec3( uintBitsToFloat( floatBitsToUint( inTangent.x ) & ~0x1 ), inTangent.yz ) );
		vec3 N = normalize( normalSign * cross( inTangent, inBitangent ) );
		vec3 B = normalize( inBitangent );
		T = ( ubo.model[ objectId ] * vec4( T, 0.0f ) ).xyz;
		N = ( ubo.model[ objectId ] * vec4( N, 0.0f ) ).xyz;
		B = ( ubo.model[ objectId ] * vec4( B, 0.0f ) ).xyz;
		fragTangentBasis = mat3( T, B, N );
	}

    fragColor = inColor;
    fragTexCoord = inTexCoord;
	clipPosition = gl_Position;
}