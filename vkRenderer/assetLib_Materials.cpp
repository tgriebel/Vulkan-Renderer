#include <algorithm>
#include "assetLib.h"

typedef AssetLib< texture_t > AssetLibImages;
extern AssetLib< RenderProgram > gpuPrograms;
extern AssetLibImages textureLib;

void AssetLib< material_t >::Create()
{
	{
		material_t material;
		material.shaders[ DRAWPASS_SHADOW ] = &gpuPrograms.assets[ RENDER_PROGRAM_TERRAIN_SHADOW ];
		material.shaders[ DRAWPASS_DEPTH ] = &gpuPrograms.assets[ RENDER_PROGRAM_TERRAIN_DEPTH ];
		material.shaders[ DRAWPASS_TERRAIN ] = &gpuPrograms.assets[ RENDER_PROGRAM_TERRAIN ];
		material.shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.assets[ RENDER_PROGRAM_TERRAIN_DEPTH ];
		material.texture0 = textureLib.FindId( "heightmap.png" );
		material.texture1 = textureLib.FindId( "grass.jpg" );
		material.texture2 = textureLib.FindId( "desert.jpg" );
		Add( "TERRAIN", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_SKYBOX ] = &gpuPrograms.assets[ RENDER_PROGRAM_SKY ];
		material.texture0 = textureLib.FindId( "skybox.jpg" );
		Add( "SKY", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_SHADOW ] = &gpuPrograms.assets[ RENDER_PROGRAM_SHADOW ];
		material.shaders[ DRAWPASS_DEPTH ] = &gpuPrograms.assets[ RENDER_PROGRAM_DEPTH_PREPASS ];
		material.shaders[ DRAWPASS_OPAQUE ] = &gpuPrograms.assets[ RENDER_PROGRAM_LIT_OPAQUE ];
		material.shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.assets[ RENDER_PROGRAM_DEPTH_PREPASS ];
		material.texture0 = textureLib.FindId( "viking_room.png" );
		Add( "VIKING", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_TRANS ] = &gpuPrograms.assets[ RENDER_PROGRAM_LIT_TRANS ];
		material.shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.assets[ RENDER_PROGRAM_DEPTH_PREPASS ];
		material.texture0 = 0;
		Add( "WATER", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_POST_2D ] = &gpuPrograms.assets[ RENDER_PROGRAM_POST_PROCESS ];
		material.texture0 = 0;
		material.texture1 = 1;
		Add( "TONEMAP", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_POST_2D ] = &gpuPrograms.assets[ RENDER_PROGRAM_IMAGE_2D ];
		material.texture0 = 2;
		Add( "IMAGE2D", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_SHADOW ] = &gpuPrograms.assets[ RENDER_PROGRAM_SHADOW ];
		material.shaders[ DRAWPASS_DEPTH ] = &gpuPrograms.assets[ RENDER_PROGRAM_DEPTH_PREPASS ];
		material.shaders[ DRAWPASS_OPAQUE ] = &gpuPrograms.assets[ RENDER_PROGRAM_LIT_OPAQUE ];
		material.shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.assets[ RENDER_PROGRAM_DEPTH_PREPASS ];
		material.texture0 = textureLib.FindId( "palm_tree_diffuse.jpg" );
		Add( "PALM", material );
	}
}