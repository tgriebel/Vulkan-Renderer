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

mat3 GetTerrainTangent( vec2 uv )
{
	ivec2 texDim = textureSize( texSampler[2], 0 );
	
	const float maxHeight = 1.0f;
	float gridSize = 0.01f;
	int cx = int( uv.x * texDim.x );
	int cy = int( uv.y * texDim.y );
	int x0 = cx - 1;
	int x1 = cx + 1;
	int y0 = cy - 1;
	int y1 = cy + 1;

	float dzdx = texelFetch( texSampler[2], ivec2( x1, cy ), 0 ).r - texelFetch( texSampler[2], ivec2( x0, cy ), 0 ).r;
	float dzdy = texelFetch( texSampler[2], ivec2( cx, y1 ), 0 ).r - texelFetch( texSampler[2], ivec2( cx, y0 ), 0 ).r;

	dzdx *= maxHeight / gridSize;
	dzdy *= maxHeight / gridSize;

	vec3 bx = normalize( vec3( 2.0f, 0.0f, dzdx ) );
	vec3 by = normalize( vec3( 0.0f, 2.0f, dzdy ) );

	return mat3( bx, by, vec3( 0.0f, 0.0f, 1.0f ) );
}

void main()
{
	objectId = pushConstants.objectId + gl_InstanceIndex;
	const uint materialId = pushConstants.materialId;

	const uint heightMapId = materialUbo.materials[ materialId ].textureId0;
	const float heightMapValue = texture( texSampler[ heightMapId ], inTexCoord.xy ).r;

	const float maxHeight = globals.generic.x;
	vec3 position = inPosition;
	position.z += maxHeight * heightMapValue;
	worldPosition = ubo.model[ objectId ] * vec4( position, 1.0f );
    gl_Position = viewUbo.views[0].proj * viewUbo.views[0].view * worldPosition;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
	mat3 worldTangent = ( mat3( ubo.model[ objectId ] ) * GetTerrainTangent( inTexCoord.xy ) );
	fragNormal = normalize( cross( worldTangent[0], worldTangent[1] ) );
	clipPosition = gl_Position;
}