#include "common.h"
#include <core/util.h>
#include "io.h"
#include "window.h"
#include <scene/entity.h>
#include <scene/scene.h>
#include "src/globals/render_util.h"

extern AssetManager gAssets;

extern imguiControls_t			gImguiControls;
extern Window					gWindow;

void CreateCodeAssets()
{
	// Materials
	{
		{
			Material material;
			material.AddShader( DRAWPASS_POST_2D, AssetLibGpuProgram::Handle( "PostProcess" ) );
			gAssets.materialLib.Add( "TONEMAP", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_POST_2D, AssetLibGpuProgram::Handle( "Image2D" ) );
			gAssets.materialLib.Add( "IMAGE2D", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_WIREFRAME, AssetLibGpuProgram::Handle( "Debug" ) );
			gAssets.materialLib.Add( "DEBUG_WIRE", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_SOLID, AssetLibGpuProgram::Handle( "DebugSolid" ) );
			gAssets.materialLib.Add( "DEBUG_SOLID", material );
		}
	}

	// Models
	{
		{
			Model model;
			CreateQuadSurface2D( "TONEMAP", model, vec2f( 1.0f, 1.0f ), vec2f( 2.0f ) );
			gAssets.modelLib.Add( "_postProcessQuad", model );
		}
		{
			Model model;
			CreateQuadSurface2D( "IMAGE2D", model, vec2f( 1.0f, 1.0f ), vec2f( 1.0f * ( 9.0 / 16.0f ), 1.0f ) );
			gAssets.modelLib.Add( "_quadTexDebug", model );
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
		scene->CreateEntityBounds( gAssets.modelLib.RetrieveHdl( "_postProcessQuad" ), *ent );
		ent->name = "_postProcessQuad";
		scene->entities.push_back( ent );
	}

	{
		Entity* ent = new Entity();
		scene->CreateEntityBounds( gAssets.modelLib.RetrieveHdl( "_quadTexDebug" ), *ent );
		ent->name = "_quadTexDebug";
		scene->entities.push_back( ent );
	}

	{
		scene->lights[ 0 ].lightPos = vec4f( 0.0f, 0.0f, 6.0f, 0.0f );
		scene->lights[ 0 ].intensity = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
		scene->lights[ 0 ].lightDir = vec4f( 0.0f, 0.0f, -1.0f, 0.0f );

		scene->lights[ 1 ].lightPos = vec4f( 0.0f, 10.0f, 5.0f, 0.0f );
		scene->lights[ 1 ].intensity = vec4f( 0.5f, 0.5f, 0.5f, 1.0f );
		scene->lights[ 1 ].lightDir = vec4f( 0.0f, 0.0f, 1.0f, 0.0f );

		scene->lights[ 2 ].lightPos = vec4f( 0.0f, -10.0f, 5.0f, 0.0f );
		scene->lights[ 2 ].intensity = vec4f( 0.5f, 0.5f, 0.5f, 1.0f );
		scene->lights[ 2 ].lightDir = vec4f( 0.0f, 0.0f, 1.0f, 0.0f );
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


void UpdateScene( Scene* scene, const float dt )
{
	// FIXME: race conditions
	// Need to do a ping-pong update
	if ( gWindow.input.IsKeyPressed( 'D' ) ) {
		scene->camera.MoveRight( dt * 0.01f );
	}
	if ( gWindow.input.IsKeyPressed( 'A' ) ) {
		scene->camera.MoveRight( dt * -0.01f );
	}
	if ( gWindow.input.IsKeyPressed( 'W' ) ) {
		scene->camera.MoveForward( dt * 0.01f );
	}
	if ( gWindow.input.IsKeyPressed( 'S' ) ) {
		scene->camera.MoveForward( dt * -0.01f );
	}
	if ( gWindow.input.IsKeyPressed( '8' ) ) {
		scene->camera.AdjustPitch( -dt * 0.01f );
	}
	if ( gWindow.input.IsKeyPressed( '2' ) ) {
		scene->camera.AdjustPitch( dt * 0.01f );
	}
	if ( gWindow.input.IsKeyPressed( '4' ) ) {
		scene->camera.AdjustYaw( dt * 0.01f );
	}
	if ( gWindow.input.IsKeyPressed( '6' ) ) {
		scene->camera.AdjustYaw( -dt * 0.01f );
	}
	if ( gWindow.input.IsKeyPressed( '+' ) ) {
		scene->camera.SetFov( scene->camera.GetFov() + Radians( 0.1f ) );
	}
	if ( gWindow.input.IsKeyPressed( '-' ) ) {
		scene->camera.SetFov( scene->camera.GetFov() - Radians( 0.1f ) );
	}
	scene->camera.SetAspectRatio( gWindow.GetWindowFrameBufferAspect() );

	const mouse_t& mouse = gWindow.input.GetMouse();
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

	if ( gImguiControls.dbgImageId >= 0 )
	{
		Entity* ent = scene->FindEntity( "_quadTexDebug" );
		if( ent != nullptr )
		{
			ent->ClearFlag( ENT_FLAG_NO_DRAW );
			gAssets.materialLib.Find( "IMAGE2D" )->Get().AddTexture( 0, gImguiControls.dbgImageId % gAssets.textureLib.Count() );
		}
	}
	else {
		Entity* ent = scene->FindEntity( "_quadTexDebug" );
		if ( ent != nullptr ) {
			ent->SetFlag( ENT_FLAG_NO_DRAW );
		}
	}

	scene->Update( dt );
}