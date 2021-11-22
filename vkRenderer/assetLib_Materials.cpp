#include "assetLib_Materials.h"
#include "assetLib_GpuProgs.h"

extern AssetLibGpuProgram gpuPrograms;

void AssetLibMaterials::Create()
{
	materials[ "TERRAIN" ].shaders[ DRAWPASS_SHADOW ] = &gpuPrograms.programs[ RENDER_PROGRAM_TERRAIN_SHADOW ];
	materials[ "TERRAIN" ].shaders[ DRAWPASS_DEPTH ] = &gpuPrograms.programs[ RENDER_PROGRAM_TERRAIN_DEPTH ];
	materials[ "TERRAIN" ].shaders[ DRAWPASS_TERRAIN ] = &gpuPrograms.programs[ RENDER_PROGRAM_TERRAIN ];
	materials[ "TERRAIN" ].shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.programs[ RENDER_PROGRAM_TERRAIN_DEPTH ];
	//materials[ "TERRAIN" ].texture0 = GetTextureId( "heightmap.png" );
	//materials[ "TERRAIN" ].texture1 = GetTextureId( "grass.jpg" );
	//materials[ "TERRAIN" ].texture2 = GetTextureId( "desert.jpg" );

	materials[ "SKY" ].shaders[ DRAWPASS_SKYBOX ] = &gpuPrograms.programs[ RENDER_PROGRAM_SKY ];
	//materials[ "SKY" ].texture0 = GetTextureId( "skybox.jpg" );

	materials[ "VIKING" ].shaders[ DRAWPASS_SHADOW ] = &gpuPrograms.programs[ RENDER_PROGRAM_SHADOW ];
	materials[ "VIKING" ].shaders[ DRAWPASS_DEPTH ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
	materials[ "VIKING" ].shaders[ DRAWPASS_OPAQUE ] = &gpuPrograms.programs[ RENDER_PROGRAM_LIT_OPAQUE ];
	materials[ "VIKING" ].shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
	//materials[ "VIKING" ].texture0 = GetTextureId( "viking_room.png" );

	materials[ "WATER" ].shaders[ DRAWPASS_TRANS ] = &gpuPrograms.programs[ RENDER_PROGRAM_LIT_TRANS ];
	materials[ "WATER" ].shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
	//materials[ "WATER" ].texture0 = GetTextureId( "checker.png" );

	materials[ "TONEMAP" ].shaders[ DRAWPASS_POST_2D ] = &gpuPrograms.programs[ RENDER_PROGRAM_POST_PROCESS ];
	materials[ "TONEMAP" ].texture0 = 0;
	materials[ "TONEMAP" ].texture1 = 1;

	materials[ "IMAGE2D" ].shaders[ DRAWPASS_POST_2D ] = &gpuPrograms.programs[ RENDER_PROGRAM_IMAGE_2D ];
	materials[ "IMAGE2D" ].texture0 = 2;

	materials[ "PALM" ].shaders[ DRAWPASS_SHADOW ] = &gpuPrograms.programs[ RENDER_PROGRAM_SHADOW ];
	materials[ "PALM" ].shaders[ DRAWPASS_DEPTH ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
	materials[ "PALM" ].shaders[ DRAWPASS_OPAQUE ] = &gpuPrograms.programs[ RENDER_PROGRAM_LIT_OPAQUE ];
	materials[ "PALM" ].shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
	//materials[ "PALM" ].texture0 = GetTextureId( "palm_tree_diffuse.jpg" );
}
