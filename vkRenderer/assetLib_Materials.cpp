#include <algorithm>

#include "assetLib_Materials.h"
#include "assetLib_GpuProgs.h"
#include "assetLib_Images.h"

extern AssetLibGpuProgram gpuPrograms;
extern AssetLibImages textureLib;

void AssetLibMaterials::Create()
{
	{
		material_t material;
		material.shaders[ DRAWPASS_SHADOW ] = &gpuPrograms.programs[ RENDER_PROGRAM_TERRAIN_SHADOW ];
		material.shaders[ DRAWPASS_DEPTH ] = &gpuPrograms.programs[ RENDER_PROGRAM_TERRAIN_DEPTH ];
		material.shaders[ DRAWPASS_TERRAIN ] = &gpuPrograms.programs[ RENDER_PROGRAM_TERRAIN ];
		material.shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.programs[ RENDER_PROGRAM_TERRAIN_DEPTH ];
		material.texture0 = textureLib.FindId( "heightmap.png" );
		material.texture1 = textureLib.FindId( "grass.jpg" );
		material.texture2 = textureLib.FindId( "desert.jpg" );
		Add( "TERRAIN", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_SKYBOX ] = &gpuPrograms.programs[ RENDER_PROGRAM_SKY ];
		material.texture0 = textureLib.FindId( "skybox.jpg" );
		Add( "SKY", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_SHADOW ] = &gpuPrograms.programs[ RENDER_PROGRAM_SHADOW ];
		material.shaders[ DRAWPASS_DEPTH ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
		material.shaders[ DRAWPASS_OPAQUE ] = &gpuPrograms.programs[ RENDER_PROGRAM_LIT_OPAQUE ];
		material.shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
		material.texture0 = textureLib.FindId( "viking_room.png" );
		Add( "VIKING", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_TRANS ] = &gpuPrograms.programs[ RENDER_PROGRAM_LIT_TRANS ];
		material.shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
		material.texture0 = textureLib.FindId( "checker.png" );
		Add( "WATER", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_POST_2D ] = &gpuPrograms.programs[ RENDER_PROGRAM_POST_PROCESS ];
		material.texture0 = 0;
		material.texture1 = 1;
		Add( "TONEMAP", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_POST_2D ] = &gpuPrograms.programs[ RENDER_PROGRAM_IMAGE_2D ];
		material.texture0 = 2;
		Add( "IMAGE2D", material );
	}

	{
		material_t material;
		material.shaders[ DRAWPASS_SHADOW ] = &gpuPrograms.programs[ RENDER_PROGRAM_SHADOW ];
		material.shaders[ DRAWPASS_DEPTH ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
		material.shaders[ DRAWPASS_OPAQUE ] = &gpuPrograms.programs[ RENDER_PROGRAM_LIT_OPAQUE ];
		material.shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
		material.texture0 = textureLib.FindId( "palm_tree_diffuse.jpg" );
		Add( "PALM", material );
	}
}

void AssetLibMaterials::Add( const char* name, const material_t& material )
{
	materials.push_back( material );
	names.push_back( name );
}

int AssetLibMaterials::FindId( const char* name )
{
	auto it = find( names.begin(), names.end(), name );
	const size_t idx = static_cast<uint32_t>( std::distance( names.begin(), it ) );
	return ( it != names.end() ) ? idx : -1;
}

material_t* AssetLibMaterials::Find( const char* name )
{
	const int id = FindId( name );
	return ( id >= 0 ) ? &materials[ id ] : NULL;
}