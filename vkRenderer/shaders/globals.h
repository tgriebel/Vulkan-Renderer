#define MaxLights		128
#define MaxMaterials	256
#define MaxViews		15
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
	uint	extraData[ 64 ]; // 256 bytes, aligned on 64 byte boundary
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

struct pass_t
{
	uint	codeImageCount;
};

struct surface_t
{
	mat4	model;
	uint	diffuseIblCubeId;
	uint	envCubeId;
	uint	pad[14];
};


#define AMBIENT vec4( 0.03f, 0.03f, 0.03f, 1.0f )

#define IMAGE_CONSTANT_LAYOUT( S, N, TYPE, NAME )																	\
											layout( set = S, binding = N ) uniform ShaderConstants					\
											{																		\
												vec4	dimensions;													\
												uint	pass;														\
												uint	previousImageId;											\
												uint	level;														\
												uint	layer;														\
												uint	mipCount;													\
												uint	layerCount;													\
												uint	pad0;														\
												uint	pad1; /* 16byte aligned */									\
												TYPE	NAME;														\
											};

#define CONSTANT_LAYOUT( S, N, TYPE, NAME )																			\
											layout( set = S, binding = N ) uniform ShaderConstants					\
											{																		\
												TYPE	NAME;														\
											};

#define MODEL_LAYOUT( S, N )				layout( set = S, binding = N ) buffer UniformBufferObject				\
											{																		\
												surface_t	surface[MaxSurfaces];									\
											} ubo;

#define GLOBALS_LAYOUT( S, N )				layout( set = S, binding = N ) uniform GlobalConstants					\
											{																		\
												vec4        time;													\
												vec4        generic; /* Debugging or prototyping */					\
												vec4        shadowParms;											\
												vec4        toneMapTint;											\
												vec4        exposure;												\
												vec4        dof;													\
												uint		numSamples;												\
												uint		whiteId;												\
												uint		blackId;												\
												uint		defaultAlbedoId;										\
												uint		defaultNormalId;										\
												uint		defaultRoughnessId;										\
												uint		defaultMetalId;											\
												uint		defaultImageId;											\
												uint		brdfLutId;												\
												uint		isTextured;												\
												uint		shadow2dCount;											\
												uint		shadowCubeCount;										\
												uint		textureCount;											\
												uint		materialCount;											\
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

#define CODE_IMAGE_CUBE_LAYOUT( S, N )		layout( set = S, binding = N ) uniform samplerCube codeCubeSamplers[];

#define STENCIL_LAYOUT( S, N, SAMPLER )		layout( set = S, binding = N ) uniform SAMPLER stencilImage;

#define MATERIAL_LAYOUT( S, N )				layout( set = S, binding = N ) buffer MaterialBuffer					\
											{																		\
												material_t materials[MaxMaterials];									\
											} materialUbo;

#define LIGHT_LAYOUT( S, N )				layout( set = S, binding = N ) buffer LightBuffer						\
											{																		\
												light_t lights[MaxLights];											\
											} lightUbo;

#define PASS_LAYOUT( S, N )					layout( set = S, binding = N ) buffer PassBuffer						\
											{																		\
												pass_t pass;														\
											} passUbo;

#define GLOBAL_BINDS( SET )					GLOBALS_LAYOUT( SET, 0 )												\
											VIEW_LAYOUT( SET, 1)													\
											SAMPLER_2D_LAYOUT( SET, 2 )												\
											SAMPLER_CUBE_LAYOUT( SET, 3 )											\
											MATERIAL_LAYOUT( SET, 4 )

#define VIEW_BINDS( SET )					MODEL_LAYOUT( SET, 0 )

#define PASS_BINDS( SET, SAMPLER )			LIGHT_LAYOUT( SET, 0 )													\
											CODE_IMAGE_LAYOUT( SET, 1, SAMPLER )									\
											CODE_IMAGE_CUBE_LAYOUT( SET, 2 )										\
											STENCIL_LAYOUT( SET, 3, SAMPLER )

#define MATERIAL_PUSH_CONSTANTS				layout( push_constant ) uniform fragmentPushConstants					\
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
											layout( location = 6 ) out vec3 objectPosition;							\
											layout( location = 7 ) out vec4 clipPosition;							\
											layout( location = 8 ) out vec4 worldPosition;							\
											layout( location = 9 ) out flat uint objectId;

#define VS_LAYOUT_BASIC_IO					VS_IN																	\
											VS_OUT

#define VS_LAYOUT_STANDARD( SAMPLER )		GLOBAL_BINDS( 0 )														\
											VIEW_BINDS( 1 )															\
											PASS_BINDS( 2, SAMPLER )												\
											MATERIAL_PUSH_CONSTANTS													\
											VS_IN																	\
											VS_OUT

#define PS_IN								layout( location = 0 ) in vec4 fragColor;								\
											layout( location = 1 ) in vec3 fragNormal;								\
											layout( location = 2 ) in mat3 fragTangentBasis;						\
											layout( location = 5 ) in vec4 fragTexCoord;							\
											layout( location = 6 ) in vec3 objectPosition;							\
											layout( location = 7 ) in vec4 clipPosition;							\
											layout( location = 8 ) in vec4 worldPosition;							\
											layout( location = 9 ) in flat uint objectId;

#define PS_OUT								layout( location = 0 ) out vec4 outColor;
#define PS_LAYOUT_MRT_1_OUT					layout( location = 1 ) out vec4 outColor1;

#define PS_LAYOUT_BASIC_IO					PS_IN																	\
											PS_OUT

#define PS_LAYOUT_STANDARD( SAMPLER )		GLOBAL_BINDS( 0 )														\
											VIEW_BINDS( 1 )															\
											PASS_BINDS( 2, SAMPLER )												\
											MATERIAL_PUSH_CONSTANTS													\
											PS_IN																	\
											PS_OUT

#define PS_LAYOUT_IMAGE_PROCESS( SAMPLER, TYPE )																	\
											GLOBAL_BINDS( 0 )														\
											CODE_IMAGE_LAYOUT( 1, 0, SAMPLER )										\
											CODE_IMAGE_CUBE_LAYOUT( 1, 1 )											\
											STENCIL_LAYOUT( 1, 2, SAMPLER )											\
											IMAGE_CONSTANT_LAYOUT( 1, 3, TYPE, imageProcess )