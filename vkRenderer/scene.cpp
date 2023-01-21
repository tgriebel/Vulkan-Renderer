#include "common.h"
#include <core/util.h>
#include "io.h"
#include "window.h"
#include <scene/entity.h>
#include <scene/scene.h>

//#define BEACH
#if defined( BEACH )
#include "scenes/beachScene.h"
#else
#include "scenes/chessScene.h"
#endif

extern Scene scene;

extern imguiControls_t			imguiControls;
extern Window					window;

void CreateCodeAssets( Scene& scene )
{
	// Materials
	{
		{
			Material material;
			material.AddShader( DRAWPASS_POST_2D, AssetLibGpuProgram::Handle( "PostProcess" ) );
			scene.materialLib.Add( "TONEMAP", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_POST_2D, AssetLibGpuProgram::Handle( "Image2D" ) );
			scene.materialLib.Add( "IMAGE2D", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_WIREFRAME, AssetLibGpuProgram::Handle( "Debug" ) );
			scene.materialLib.Add( "DEBUG_WIRE", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_SOLID, AssetLibGpuProgram::Handle( "Debug_Solid" ) );
			scene.materialLib.Add( "DEBUG_SOLID", material );
		}
	}

	// Models
	{
		{
			Model model;
			CreateQuadSurface2D( "TONEMAP", model, vec2f( 1.0f, 1.0f ), vec2f( 2.0f ) );
			scene.modelLib.Add( "_postProcessQuad", model );
		}
		{
			Model model;
			CreateQuadSurface2D( "IMAGE2D", model, vec2f( 1.0f, 1.0f ), vec2f( 1.0f * ( 9.0 / 16.0f ), 1.0f ) );
			scene.modelLib.Add( "_quadTexDebug", model );
		}
	}
}

void UpdateScene( const float dt )
{
	// FIXME: race conditions
	// Need to do a ping-pong update
	if ( window.input.IsKeyPressed( 'D' ) ) {
		scene.camera.MoveRight( dt * 0.01f );
	}
	if ( window.input.IsKeyPressed( 'A' ) ) {
		scene.camera.MoveRight( dt * -0.01f );
	}
	if ( window.input.IsKeyPressed( 'W' ) ) {
		scene.camera.MoveForward( dt * 0.01f );
	}
	if ( window.input.IsKeyPressed( 'S' ) ) {
		scene.camera.MoveForward( dt * -0.01f );
	}
	if ( window.input.IsKeyPressed( '8' ) ) {
		scene.camera.AdjustPitch( -dt * 0.01f );
	}
	if ( window.input.IsKeyPressed( '2' ) ) {
		scene.camera.AdjustPitch( dt * 0.01f );
	}
	if ( window.input.IsKeyPressed( '4' ) ) {
		scene.camera.AdjustYaw( dt * 0.01f );
	}
	if ( window.input.IsKeyPressed( '6' ) ) {
		scene.camera.AdjustYaw( -dt * 0.01f );
	}
	if ( window.input.IsKeyPressed( '+' ) ) {
		scene.camera.SetFov( scene.camera.GetFov() + Radians( 0.1f ) );
	}
	if ( window.input.IsKeyPressed( '-' ) ) {
		scene.camera.SetFov( scene.camera.GetFov() - Radians( 0.1f ) );
	}
	scene.camera.SetAspectRatio( window.GetWindowFrameBufferAspect() );

	// Skybox
	vec3f skyBoxOrigin;
	skyBoxOrigin[ 0 ] = scene.camera.GetOrigin()[ 0 ];
	skyBoxOrigin[ 1 ] = scene.camera.GetOrigin()[ 1 ];
	skyBoxOrigin[ 2 ] = scene.camera.GetOrigin()[ 2 ] - 0.5f;
	( scene.FindEntity( "_skybox" ) )->SetOrigin( skyBoxOrigin );

	UpdateSceneLocal( dt );

	if ( imguiControls.dbgImageId >= 0 )
	{
		scene.FindEntity( "_quadTexDebug" )->ClearFlag( ENT_FLAG_NO_DRAW );
		scene.materialLib.Find( "IMAGE2D" )->Get().AddTexture( 0, imguiControls.dbgImageId % scene.textureLib.Count() );
	}
	else {
		scene.FindEntity( "_quadTexDebug" )->SetFlag( ENT_FLAG_NO_DRAW );
	}
}