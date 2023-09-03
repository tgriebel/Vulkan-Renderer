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
#include "color.h"

PS_LAYOUT_STANDARD( sampler2D )

void main()
{
    const uint materialId = pushConstants.materialId;

    const material_t material = materialUbo.materials[ materialId ];
    
    uint textureId = 0;

    const float xm = abs( fragNormal.x );
    const float ym = abs( fragNormal.y );
    const float zm = abs( fragNormal.z );
    const float majorAxis = max( max( xm, ym ), zm );

    if( majorAxis == xm ) {
        textureId = ( sign( fragNormal.x ) > 0.0f ) ? material.textureId0 : material.textureId1;
    } else if( majorAxis == ym ) {
        textureId = ( sign( fragNormal.y ) > 0.0f ) ? material.textureId5 : material.textureId4;
    } else if( majorAxis == zm ) {
        textureId = ( sign( fragNormal.z ) > 0.0f ) ? material.textureId2 : material.textureId3;
    }
	outColor = SrgbToLinear( texture( texSampler[textureId], fragTexCoord.xy ) );
}