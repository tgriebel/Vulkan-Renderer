#include "common.h"
#include "scene.h"
#include <core/util.h>
#include "io.h"
#include "window.h"
#include <scene/entity.h>

//#define BEACH
#if defined( BEACH )
#include "scenes/beachScene.h"
#else
#include "scenes/chessScene.h"
#endif

extern Scene scene;

extern imguiControls_t			imguiControls;
extern Window					window;

// FIXME: move. here due to a compile dependency
AABB Entity::GetBounds() const {
	const Model* model = scene.modelLib.Find( modelHdl );
	const AABB& bounds = model->bounds;
	vec3f origin = GetOrigin();
	return AABB( bounds.GetMin() + origin, bounds.GetMax() + origin );
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
		scene.camera.SetPitch( -dt * 0.01f );
	}
	if ( window.input.IsKeyPressed( '2' ) ) {
		scene.camera.SetPitch( dt * 0.01f );
	}
	if ( window.input.IsKeyPressed( '4' ) ) {
		scene.camera.SetYaw( dt * 0.01f );
	}
	if ( window.input.IsKeyPressed( '6' ) ) {
		scene.camera.SetYaw( -dt * 0.01f );
	}
	if ( window.input.IsKeyPressed( '+' ) ) {
		scene.camera.fov += dt;
	}
	if ( window.input.IsKeyPressed( '-' ) ) {
		scene.camera.fov -= dt;
	}
	scene.camera.aspect = window.GetWindowFrameBufferAspect();

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
		scene.materialLib.Find( "IMAGE2D" )->textures[ 0 ] = ( imguiControls.dbgImageId % scene.textureLib.Count() );
	}
	else {
		scene.FindEntity( "_quadTexDebug" )->SetFlag( ENT_FLAG_NO_DRAW );
	}
}