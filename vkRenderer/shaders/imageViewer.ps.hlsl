#include "globals.h"
#include "util.h"

struct ImageViewer
{
	float4	scissorRectUv;
	float4	tint;
	uint	flags;
	uint	sampleIndex;  // ~0u = average all samples
};

#ifdef USE_MSAA
PS_LAYOUT_IMAGE_PROCESS( Texture2DMS<float4>, ImageViewer )
#else
PS_LAYOUT_IMAGE_PROCESS( Texture2D, ImageViewer )
#endif

psOutput_t PSMain( vsToPsInterpolators input )
{
	psOutput_t output = (psOutput_t)0;

	const bool isCubeImage = ( ( imageProcess.flags >> 0 ) & 1 ) != 0;
	const bool applySrgbCurve = ( ( imageProcess.flags >> 1 ) & 1 ) != 0;

	// The shader uses a clip fullscreen quad so the sample UV needs to be computed from the scissor subsection of the fullscreen quad
	const float2 uv = ( input.uv0.xy - imageProcess.scissorRectUv.xy ) / imageProcess.scissorRectUv.zw;

	float4 color;
	if ( isCubeImage )
	{
		const float s = 2.0f * uv.x - 1.0f;
		const float t = 2.0f * uv.y - 1.0f;
		float3 dir;
		switch ( layer )
		{
			case 0:  dir = float3(  1.0f,    -t,    -s ); break;  // +X
			case 1:  dir = float3( -1.0f,    -t,     s ); break;  // -X
			case 2:  dir = float3(     s,  1.0f,     t ); break;  // +Y
			case 3:  dir = float3(     s, -1.0f,    -t ); break;  // -Y
			case 4:  dir = float3(     s,    -t,  1.0f ); break;  // +Z
			default: dir = float3(    -s,    -t, -1.0f ); break;  // -Z
		}
		color = localCubemaps[ 0 ].SampleLevel( bilinearSamplerClampEdge, dir, (float)level );
	}
	else
	{
#ifdef USE_MSAA
		const int2 pixelLocation = int2( uv * dimensions.xy );
		if ( imageProcess.sampleIndex < globals.numSamples )
		{
			color = localTextures[ 0 ].Load( pixelLocation, int( imageProcess.sampleIndex ) );
		}
		else
		{
			color = float4( 0.0f, 0.0f, 0.0f, 0.0f );
			for ( int i = 0; i < int( globals.numSamples ); ++i ) {
				color += localTextures[ 0 ].Load( pixelLocation, i );
			}
			color /= float( globals.numSamples );
		}
#else
		uint mipW, mipH, mipLevels;
		localTextures[ 0 ].GetDimensions( (uint)level, mipW, mipH, mipLevels );
		const int3 pixelLocation = int3( uv * int2( mipW, mipH ), level );
		color = localTextures[ 0 ].Load( pixelLocation );
#endif
	}

	const float4 tint = imageProcess.tint;

	if ( !any( tint ) )
	{
		output.outColor = float4( 0.0f, 0.0f, 0.0f, 1.0f );
		return output;
	}
	
	// Mark each channel 0/1, then check if only a single 1 value is in the mask
    const float4 mask = step( 0.00001f, abs( tint ) );	
    const bool isMonochrome = abs( dot( mask, 1.0.xxxx ) - 1.0f ) < 0.00001f;

	// Single channel selection displays in black and white
    if ( isMonochrome )
	{
        const float channelColor = dot( color, tint );
        output.outColor = float4( channelColor.rrr, 1.0f );
    }
	else
	{
		output.outColor   = color * tint;
		output.outColor.a = tint.a > 0.0f ? color.a : 1.0f;
	}

    if ( applySrgbCurve )
	{
		output.outColor.rgb = LinearToSrgb( output.outColor.rgb );
	}

	return output;
}
