

#define AMBIENT vec4( 0.03f, 0.03f, 0.03f, 1.0f )

#define MODEL_LAYOUT( S, N )		layout( set = S, binding = N ) uniform UniformBufferObject					\
									{																			\
										mat4        model;														\
										mat4        view;														\
										mat4        proj;														\
									} ubo[];

#define GLOBALS_LAYOUT( S, N )		layout( set = S, binding = N ) uniform GlobalConstants						\
									{																			\
										vec4        time;														\
										vec4        heightMap;													\
										vec4        toneMap;													\
										uint		shadowBaseId;												\
									} globals;

#define SAMPLER_LAYOUT( S, N )		layout( set = S, binding = N ) uniform sampler2D texSampler[];

#define CODE_IMAGE_LAYOUT( S, N )	layout( set = S, binding = N ) uniform sampler2D codeSamplers[];

#define MATERIAL_LAYOUT( S, N )		layout( set = S, binding = N ) uniform MaterialBuffer						\
									{																			\
										uint        textureId0;													\
										uint        textureId1;													\
										uint        textureId2;													\
										uint        textureId3;													\
										uint        textureId4;													\
										uint        textureId5;													\
										uint        textureId6;													\
										uint        textureId7;													\
										vec4		Ka;															\
										vec4		Ke;															\
										vec4		Kd;															\
										vec4		Ks;															\
										vec4		Tf;															\
										float		Tr;															\
										float		Ns;															\
										float		Ni;															\
										float		d;															\
										float		illum;														\
									} materials[];

#define LIGHT_LAYOUT( S, N )		layout( set = S, binding = N ) uniform LightBuffer							\
									{																			\
										vec3	    lightPos;													\
										vec3		intensity;													\
										vec3		lightDir;													\
									} lights[];

#define PUSH_CONSTANTS				layout( push_constant ) uniform fragmentPushConstants						\
									{																			\
										layout( offset = 0 ) uint objectId;										\
										layout( offset = 4 ) uint materialId;									\
									} pushConstants;

#define VS_IN		layout( set = 0, location = 0 ) in vec3 inPosition;											\
					layout( set = 0, location = 1 ) in vec4 inColor;											\
					layout( set = 0, location = 2 ) in vec3 inTangent;											\
					layout( set = 0, location = 3 ) in vec3 inBinorm;											\
					layout( set = 0, location = 4 ) in vec2 inTexCoord;

#define VS_OUT		layout( set = 0, location = 0 ) out vec4 fragColor;											\
					layout( set = 0, location = 1 ) out vec3 fragNormal;										\
					layout( set = 0, location = 2 ) out vec2 fragTexCoord;										\
					layout( set = 0, location = 3 ) out vec4 clipPosition;										\
					layout( set = 0, location = 4 ) out vec4 worldPosition;

#define VS_LAYOUT_BASIC_IO	VS_IN																				\
							VS_OUT

#define VS_LAYOUT_STANDARD	GLOBALS_LAYOUT( 0, 0 )																\
							MODEL_LAYOUT( 0, 1 )																\
							SAMPLER_LAYOUT( 0, 2 )																\
							MATERIAL_LAYOUT( 0, 3 )																\
							LIGHT_LAYOUT( 0, 4 )																\
							CODE_IMAGE_LAYOUT( 0, 5 )															\
							PUSH_CONSTANTS																		\
							VS_IN																				\
							VS_OUT

#define PS_IN		layout( set = 0, location = 0 ) in vec4 fragColor;											\
					layout( set = 0, location = 1 ) in vec3 fragNormal;											\
					layout( set = 0, location = 2 ) in vec2 fragTexCoord;										\
					layout( set = 0, location = 3 ) in vec4 clipPosition;										\
					layout( set = 0, location = 4 ) in vec4 worldPosition;

#define PS_OUT		layout( set = 0, location = 0 ) out vec4 outColor;

#define PS_LAYOUT_BASIC_IO	PS_IN																				\
							PS_OUT

#define PS_LAYOUT_STANDARD	GLOBALS_LAYOUT( 0, 0 )																\
							MODEL_LAYOUT( 0, 1 )																\
							SAMPLER_LAYOUT( 0, 2 )																\
							MATERIAL_LAYOUT( 0, 3 )																\
							LIGHT_LAYOUT( 0, 4 )																\
							CODE_IMAGE_LAYOUT( 0, 5 )															\
							PUSH_CONSTANTS																		\
							PS_IN																				\
							PS_OUT


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//																																					//
//																FUNCTIONS																			//
//																																					//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float SrgbTolinear( float value ) {
	return pow( value, 2.2f );
}

vec3 SrgbTolinear( vec3 sRGB ) {
	return vec3( SrgbTolinear( sRGB.r ), SrgbTolinear( sRGB.g ), SrgbTolinear( sRGB.b ) );
}

vec4 SrgbTolinear( vec4 sRGBA ) {
	return vec4( SrgbTolinear( sRGBA.rgb ), sRGBA.a );
}

float LinearToSrgb( float value ) {
	return pow( value, 1.0f / 2.2f );
}

vec3 LinearToSrgb( vec3 inLinear ) {
	return vec3( LinearToSrgb( inLinear.r ), LinearToSrgb( inLinear.g ), LinearToSrgb( inLinear.b ) );
}

vec4 LinearToSrgb( vec4 inLinear ) {
	return vec4( LinearToSrgb( inLinear.rgb ), inLinear.a );
}