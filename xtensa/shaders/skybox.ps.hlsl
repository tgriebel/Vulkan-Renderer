#include "globals.h"
#include "util.h"

PS_LAYOUT_STANDARD( Texture2D )

psOutput_t PSMain( vsToPsInterpolators input )
{
	psOutput_t output = (psOutput_t)0;
    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

	const gpuMaterial_t material = materials[materialId];
	const gpuView_t view = views[viewlId];

#ifdef USE_CUBE_SAMPLER
    const float3 viewVector = normalize( input.objectPosition );
    const float3 skyColor = globalCubemaps[ material.textureId[ 0 ] ].Sample( bilinearSamplerWrap, CubeVector( viewVector ) ).rgb;
    output.outColor.rgb = SrgbToLinear( skyColor );
#else
    const float xm = abs( input.normal.x );
    const float ym = abs( input.normal.y );
    const float zm = abs( input.normal.z );
    const float majorAxis = max( max( xm, ym ), zm );

    uint textureId = 0;

    if( majorAxis == xm ) {
        textureId = ( sign( input.normal.x ) > 0.0f ) ? material.textureId[ CUBE_FRONT_MAP_SLOT ] : material.textureId[ CUBE_BACK_MAP_SLOT ];
    } else if( majorAxis == ym ) {
        textureId = ( sign( input.normal.y ) > 0.0f ) ? material.textureId[ CUBE_LEFT_MAP_SLOT ] : material.textureId[ CUBE_RIGHT_MAP_SLOT ];
    } else if( majorAxis == zm ) {
        textureId = ( sign( input.normal.z ) > 0.0f ) ? material.textureId[ CUBE_TOP_MAP_SLOT ] : material.textureId[ CUBE_BOTTOM_MAP_SLOT ];
    }
	output.outColor = SrgbToLinear( globalTextures[ textureId ].Sample( bilinearSamplerWrap, input.uv0.xy ) );
#endif
    output.outColor.a = 1.0f;
	return output;
}
