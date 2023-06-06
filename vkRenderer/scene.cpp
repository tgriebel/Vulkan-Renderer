#include "common.h"
#include <gfxcore/core/util.h>
#include "io.h"
#include "window.h"
#include <gfxcore/scene/entity.h>
#include <gfxcore/scene/scene.h>
#include "src/globals/render_util.h"

extern AssetManager g_assets;

extern imguiControls_t			g_imguiControls;
extern Window					g_window;

void CreateCodeAssets()
{
	// Textures
	{
		for( uint32_t t = 0; t < 4; ++t )
		{
			const RGBA rgba = Color( Color::Gold ).AsRGBA();

			std::stringstream ss;
			ss << "CODE_COLOR_" << t;
			std::string s = ss.str();

			hdl_t handle = g_assets.textureLib.Add( s.c_str(), Image() );
			Image& texture = g_assets.textureLib.Find( handle )->Get();

			texture.info.width = 256;
			texture.info.height = 240;
			texture.info.mipLevels = 1;
			texture.info.layers = 1;
			texture.info.type = IMAGE_TYPE_2D;
			texture.info.channels = 4;
			texture.info.fmt = IMAGE_FMT_RGBA_8;
			texture.info.tiling = IMAGE_TILING_MORTON;
			texture.sizeBytes = texture.info.channels * texture.info.width * texture.info.height;

			texture.bytes = new uint8_t[ texture.sizeBytes ];

			const uint32_t pixelCount = texture.info.width * texture.info.height;
			for ( uint32_t i = 0; i < pixelCount; ++i )
			{
				texture.bytes[ i * 4 + 0 ] = rgba.r;
				texture.bytes[ i * 4 + 1 ] = rgba.g;
				texture.bytes[ i * 4 + 2 ] = rgba.b;
				texture.bytes[ i * 4 + 3 ] = rgba.a;
			}
			
		}
	}

	// Materials
	{
		{
			Material material;
			material.usage = MATERIAL_USAGE_CODE;
			material.AddShader( DRAWPASS_POST_2D, AssetLibGpuProgram::Handle( "PostProcess" ) );
			for ( uint32_t i = 0; i < Material::MaxMaterialTextures; ++i ) {
				material.AddTexture( i, i );
			}
			g_assets.materialLib.Add( "TONEMAP", material );
		}

		{
			Material material;
			material.usage = MATERIAL_USAGE_CODE;
			material.AddShader( DRAWPASS_POST_2D, AssetLibGpuProgram::Handle( "Image2D" ) );
			g_assets.materialLib.Add( "IMAGE2D", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_WIREFRAME, AssetLibGpuProgram::Handle( "Debug" ) );
			g_assets.materialLib.Add( "DEBUG_WIRE", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_SOLID, AssetLibGpuProgram::Handle( "DebugSolid" ) );
			g_assets.materialLib.Add( "DEBUG_SOLID", material );
		}
	}

	// Models
	{
		{
			Model model;
			CreateQuadSurface2D( "TONEMAP", model, vec2f( 1.0f, 1.0f ), vec2f( 2.0f ) );
			g_assets.modelLib.Add( "_postProcessQuad", model );
		}
		{
			Model model;
			CreateQuadSurface2D( "IMAGE2D", model, vec2f( 1.0f, 1.0f ), vec2f( 1.0f * ( 9.0 / 16.0f ), 1.0f ) );
			g_assets.modelLib.Add( "_quadTexDebug", model );
		}
	}
}


void InitScene( Scene* scene )
{
	// FIXME: weird left-over stuff from refactors
	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	for ( uint32_t i = 0; i < entCount; ++i ) {
		scene->CreateEntityBounds( scene->entities[ i ]->modelHdl, *scene->entities[ i ] );
	}

	scene->Init();

	{
		Entity* ent = new Entity();
		scene->CreateEntityBounds( g_assets.modelLib.RetrieveHdl( "_postProcessQuad" ), *ent );
		ent->name = "_postProcessQuad";
		scene->entities.push_back( ent );
	}

	{
		Entity* ent = new Entity();
		scene->CreateEntityBounds( g_assets.modelLib.RetrieveHdl( "_quadTexDebug" ), *ent );
		ent->name = "_quadTexDebug";
		scene->entities.push_back( ent );
	}

	{
		scene->lights.resize( 3 );
		scene->lights[ 0 ].lightPos = vec4f( 0.0f, 0.0f, 6.0f, 0.0f );
		scene->lights[ 0 ].intensity = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
		scene->lights[ 0 ].lightDir = vec4f( 0.0f, 0.0f, -1.0f, 0.0f );
		scene->lights[ 0 ].flags = LIGHT_FLAGS_SHADOW;

		scene->lights[ 1 ].lightPos = vec4f( 0.0f, 10.0f, 5.0f, 0.0f );
		scene->lights[ 1 ].intensity = vec4f( 0.5f, 0.5f, 0.5f, 1.0f );
		scene->lights[ 1 ].lightDir = vec4f( 0.0f, 0.0f, 1.0f, 0.0f );
		scene->lights[ 1 ].flags = LIGHT_FLAGS_NONE;

		scene->lights[ 2 ].lightPos = vec4f( 0.0f, -10.0f, 5.0f, 0.0f );
		scene->lights[ 2 ].intensity = vec4f( 0.5f, 0.5f, 0.5f, 1.0f );
		scene->lights[ 2 ].lightDir = vec4f( 0.0f, 0.0f, 1.0f, 0.0f );
		scene->lights[ 2 ].flags = LIGHT_FLAGS_NONE;
	}
}


void ShutdownScene( Scene* scene )
{
	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	for( uint32_t i = 0; i < entCount; ++i )
	{
		delete scene->entities[i];
	}
}


void UpdateScene( Scene* scene )
{
	const float dt = scene->DeltaTime();
	const float cameraSpeed = 5.0f;

	if( g_window.IsFocused() == false )
	{
		// FIXME: race conditions
		// Need to do a ping-pong update
		if ( g_window.input.IsKeyPressed( 'D' ) ) {
			scene->camera.MoveRight( cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( 'A' ) ) {
			scene->camera.MoveRight( -cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( 'W' ) ) {
			scene->camera.MoveForward( cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( 'S' ) ) {
			scene->camera.MoveForward( -cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( '8' ) ) {
			scene->camera.AdjustPitch( -cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( '2' ) ) {
			scene->camera.AdjustPitch( cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( '4' ) ) {
			scene->camera.AdjustYaw( cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( '6' ) ) {
			scene->camera.AdjustYaw( -cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( '+' ) ) {
			scene->camera.SetFov( scene->camera.GetFov() + Radians( 0.1f ) );
		}
		if ( g_window.input.IsKeyPressed( '-' ) ) {
			scene->camera.SetFov( scene->camera.GetFov() - Radians( 0.1f ) );
		}
	}
	scene->camera.SetAspectRatio( g_window.GetWindowFrameBufferAspect() );

	const mouse_t& mouse = g_window.input.GetMouse();
	if ( mouse.centered )
	{
		const float maxSpeed = mouse.speed;
		const float yawDelta = maxSpeed * mouse.dx;
		const float pitchDelta = -maxSpeed * mouse.dy;
		scene->camera.AdjustYaw( yawDelta );
		scene->camera.AdjustPitch( pitchDelta );
	}
	else if ( mouse.leftDown )
	{
		Ray ray = scene->camera.GetViewRay( vec2f( 0.5f * mouse.x + 0.5f, 0.5f * mouse.y + 0.5f ) );
		scene->selectedEntity = scene->GetTracedEntity( ray );
	}

	// Skybox
	vec3f skyBoxOrigin;
	skyBoxOrigin[ 0 ] = scene->camera.GetOrigin()[ 0 ];
	skyBoxOrigin[ 1 ] = scene->camera.GetOrigin()[ 1 ];
	skyBoxOrigin[ 2 ] = scene->camera.GetOrigin()[ 2 ] - 0.5f;
	( scene->FindEntity( "_skybox" ) )->SetOrigin( skyBoxOrigin );

	
	if(0)
	{
		Color randomColor( Random(), Random(), Random(), 1.0f );
		const RGBA rgba = Color( randomColor ).AsRGBA();
		Image& texture = g_assets.textureLib.Find( "CODE_COLOR_0" )->Get();

		const uint32_t pixelCount = texture.info.width * texture.info.height;
		for ( uint32_t i = 0; i < pixelCount; ++i )
		{
			texture.bytes[ i * 4 + 0 ] = rgba.r;
			texture.bytes[ i * 4 + 1 ] = rgba.g;
			texture.bytes[ i * 4 + 2 ] = rgba.b;
			texture.bytes[ i * 4 + 3 ] = rgba.a;
		}
		texture.dirty = true;
	}
	

	if ( g_imguiControls.dbgImageId >= 0 )
	{
		Entity* ent = scene->FindEntity( "_quadTexDebug" );
		if( ent != nullptr )
		{
			ent->ClearFlag( ENT_FLAG_NO_DRAW );
			Asset<Material>* matAsset = g_assets.materialLib.Find( "IMAGE2D" );
			if( matAsset != nullptr )
			{
				Material& mat = matAsset->Get();
				mat.AddTexture( 0, g_imguiControls.dbgImageId );
			}
		}
	}
	else {
		Entity* ent = scene->FindEntity( "_quadTexDebug" );
		if ( ent != nullptr ) {
			ent->SetFlag( ENT_FLAG_NO_DRAW );
		}
	}

	scene->Update();
}