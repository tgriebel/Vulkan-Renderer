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

#define MaxLights		128
#define MaxMaterials	256
#define MaxViews		6
#define MaxSurfaces		1000

#define PI				3.14159265359f

struct light_t
{
	vec4	lightPos;
	vec4	intensity;
	vec4	lightDir;
	uint	shadowViewId;
	uint	pad0;
	uint	pad1;
	uint	pad2;
};

struct material_t
{
	int		textureId0;
	int		textureId1;
	int		textureId2;
	int		textureId3;
	int		textureId4;
	int		textureId5;
	int		textureId6;
	int		textureId7;
	vec3	Ka;
	float	Tr;
	vec3	Ke;
	float	Ns;
	vec3	Kd;
	float	Ni;
	vec3	Ks;
	float	illum;
	vec3	Tf;
	uint	textured;
	uint	pad0;
	uint	pad1;
	uint	pad2;
	uint	pad3;
};

struct view_t
{
	mat4	viewMat;
	mat4	projMat;
	vec4	dimensions;
	uint	numLights;
	uint	pad0;
	uint	pad1;
	uint	pad2;
};


#define AMBIENT vec4( 0.03f, 0.03f, 0.03f, 1.0f )

#define IMAGE_PROCESS_LAYOUT( S, N )		layout( set = S, binding = N ) uniform ImageProcess						\
											{																		\
												vec4	dimensions;													\
												vec4	generic0;													\
												vec4	generic1;													\
												vec4	generic2;													\
											} imageProcess;

#define MODEL_LAYOUT( S, N )				layout( set = S, binding = N ) buffer UniformBufferObject				\
											{																		\
												mat4        model[MaxSurfaces];										\
											} ubo;

#define GLOBALS_LAYOUT( S, N )				layout( set = S, binding = N ) uniform GlobalConstants					\
											{																		\
												vec4        time;													\
												vec4        generic;												\
												vec4        shadowParms;											\
												vec4        toneMap;												\
												uint		numSamples;												\
											} globals;

#define VIEW_LAYOUT( S, N )					layout( set = S, binding = N ) buffer ViewUniformBuffer					\
											{																		\
												view_t		views[MaxViews];										\
											} viewUbo;

#define READ_BUFFER_LAYOUT( SET, SLOT, TYPE, NAME )																	\
											layout( std140, set = SET, binding = SLOT ) readonly buffer ReadBuffer##SLOT	\
											{																		\
												TYPE		NAME[];													\
											};

#define WRITE_BUFFER_LAYOUT( SET, SLOT, TYPE, NAME )																\
											layout( std140, set = SET, binding = SLOT ) buffer WriteBuffer##SLOT	\
											{																		\
												TYPE	NAME[];														\
											};

#define SAMPLER_2D_LAYOUT( S, N )			layout( set = S, binding = N ) uniform sampler2D texSampler[];

#define SAMPLER_CUBE_LAYOUT( S, N )			layout( set = S, binding = N ) uniform samplerCube cubeSamplers[];

#define CODE_IMAGE_LAYOUT( S, N, SAMPLER )	layout( set = S, binding = N ) uniform SAMPLER codeSamplers[];

#define STENCIL_LAYOUT( S, N, SAMPLER )		layout( set = S, binding = N ) uniform SAMPLER stencilImage;

#define MATERIAL_LAYOUT( S, N )				layout( set = S, binding = N ) buffer MaterialBuffer					\
											{																		\
												material_t materials[MaxMaterials];									\
											} materialUbo;

#define LIGHT_LAYOUT( S, N )				layout( set = S, binding = N ) buffer LightBuffer						\
											{																		\
												light_t lights[MaxLights];											\
											} lightUbo;

#define PUSH_CONSTANTS						layout( push_constant ) uniform fragmentPushConstants					\
											{																		\
												layout( offset = 0 ) uint objectId;									\
												layout( offset = 4 ) uint materialId;								\
												layout( offset = 8 ) uint viewId;									\
											} pushConstants;

#define VS_IN								layout( location = 0 ) in vec3 inPosition;								\
											layout( location = 1 ) in vec4 inColor;									\
											layout( location = 2 ) in vec3 inNormal;								\
											layout( location = 3 ) in vec3 inTangent;								\
											layout( location = 4 ) in vec3 inBitangent;								\
											layout( location = 5 ) in vec4 inTexCoord;

#define VS_OUT								layout( location = 0 ) out vec4 fragColor;								\
											layout( location = 1 ) out vec3 fragNormal;								\
											layout( location = 2 ) out mat3 fragTangentBasis;						\
											layout( location = 5 ) out vec4 fragTexCoord;							\
											layout( location = 6 ) out vec4 clipPosition;							\
											layout( location = 7 ) out vec4 worldPosition;							\
											layout( location = 8 ) out flat uint objectId;

#define VS_LAYOUT_BASIC_IO					VS_IN																	\
											VS_OUT

#define VS_LAYOUT_STANDARD( SAMPLER )		GLOBALS_LAYOUT( 0, 0 )													\
											VIEW_LAYOUT( 0, 1)														\
											SAMPLER_2D_LAYOUT( 0, 2 )												\
											SAMPLER_CUBE_LAYOUT( 0, 3 )												\
											MATERIAL_LAYOUT( 0, 4 )													\
											MODEL_LAYOUT( 1, 0 )													\
											LIGHT_LAYOUT( 2, 0 )													\
											CODE_IMAGE_LAYOUT( 2, 1, SAMPLER )										\
											STENCIL_LAYOUT( 2, 2, SAMPLER )											\
											PUSH_CONSTANTS															\
											VS_IN																	\
											VS_OUT

#define PS_IN								layout( location = 0 ) in vec4 fragColor;								\
											layout( location = 1 ) in vec3 fragNormal;								\
											layout( location = 2 ) in mat3 fragTangentBasis;						\
											layout( location = 5 ) in vec4 fragTexCoord;							\
											layout( location = 6 ) in vec4 clipPosition;							\
											layout( location = 7 ) in vec4 worldPosition;							\
											layout( location = 8 ) in flat uint objectId;

#define PS_OUT								layout( location = 0 ) out vec4 outColor;
#define PS_LAYOUT_MRT_1_OUT					layout( location = 1 ) out vec4 outColor1;

#define PS_LAYOUT_BASIC_IO					PS_IN																	\
											PS_OUT

#define PS_LAYOUT_STANDARD( SAMPLER )		GLOBALS_LAYOUT( 0, 0 )													\
											VIEW_LAYOUT( 0, 1)														\
											SAMPLER_2D_LAYOUT( 0, 2 )												\
											SAMPLER_CUBE_LAYOUT( 0, 3 )												\
											MATERIAL_LAYOUT( 0, 4 )													\
											MODEL_LAYOUT( 1, 0 )													\
											LIGHT_LAYOUT( 2, 0 )													\
											CODE_IMAGE_LAYOUT( 2, 1, SAMPLER )										\
											STENCIL_LAYOUT( 2, 2, SAMPLER )											\
											PUSH_CONSTANTS															\
											PS_IN																	\
											PS_OUT

#define PS_LAYOUT_IMAGE_PROCESS( SAMPLER )	GLOBALS_LAYOUT( 0, 0 )													\
											VIEW_LAYOUT( 0, 1)														\
											SAMPLER_2D_LAYOUT( 0, 2 )												\
											SAMPLER_CUBE_LAYOUT( 0, 3 )												\
											MATERIAL_LAYOUT( 0, 4 )													\
											CODE_IMAGE_LAYOUT( 1, 0, SAMPLER )										\
											STENCIL_LAYOUT( 1, 1, SAMPLER )											\
											IMAGE_PROCESS_LAYOUT( 1, 2 )