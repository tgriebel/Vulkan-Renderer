#include "globals.h"
#include "color.h"
#include "util.h"

PS_LAYOUT_STANDARD( Texture2D )

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;
    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

	const gpuMaterial_t material = materials[materialId];
	const gpuView_t view = views[viewlId];

#ifdef USE_CUBE_SAMPLER
    const float3 viewVector = normalize( input.objectPosition );
    const float3 skyColor = cubeSamplers[ material.textureId[ 0 ] ].Sample( bilinearSamplerWrap, CubeVector( viewVector ) ).rgb;
    output.outColor.rgb = SrgbToLinear( skyColor );
#else
    const float xm = abs( input.normal.x );
    const float ym = abs( input.normal.y );
    const float zm = abs( input.normal.z );
    const float majorAxis = max( max( xm, ym ), zm );

    uint textureId = 0;

    if( majorAxis == xm ) {
        textureId = ( sign( input.normal.x ) > 0.0f ) ? material.textureId[ CUBE_FRONT_SLOT ] : material.textureId[ CUBE_BACK_SLOT ];
    } else if( majorAxis == ym ) {
        textureId = ( sign( input.normal.y ) > 0.0f ) ? material.textureId[ CUBE_LEFT_SLOT ] : material.textureId[ CUBE_RIGHT_SLOT ];
    } else if( majorAxis == zm ) {
        textureId = ( sign( input.normal.z ) > 0.0f ) ? material.textureId[ CUBE_TOP_SLOT ] : material.textureId[ CUBE_BOTTOM_SLOT ];
    }
	output.outColor = SrgbToLinear( texSampler[ textureId ].Sample( bilinearSamplerWrap, input.uv0.xy ) );
#endif
    output.outColor.a = 1.0f;
	return output;
}
