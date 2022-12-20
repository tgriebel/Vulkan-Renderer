#include "common.h"
#include "scene.h"
#include "util.h"
#include "io.h"
#include "window.h"

//#define BEACH
#if defined( BEACH )
#include "scenes/beachScene.h"
#else
#include "scenes/chessScene.h"
#endif

extern Scene scene;

extern imguiControls_t			imguiControls;
extern Window					window;

AABB Entity::GetBounds() const {
	const modelSource_t* model = scene.modelLib.Find( modelHdl );
	const AABB& bounds = model->bounds;
	vec3f origin = GetOrigin();
	return AABB( bounds.GetMin() + origin, bounds.GetMax() + origin );
}

vec3f Entity::GetOrigin() const {
	return vec3f( matrix[ 3 ][ 0 ], matrix[ 3 ][ 1 ], matrix[ 3 ][ 2 ] );
}

void Entity::SetOrigin( const vec3f& origin ) {
	matrix[ 3 ][ 0 ] = origin[ 0 ];
	matrix[ 3 ][ 1 ] = origin[ 1 ];
	matrix[ 3 ][ 2 ] = origin[ 2 ];
}

void Entity::SetScale( const vec3f& scale ) {
	matrix[ 0 ][ 0 ] = scale[ 0 ];
	matrix[ 1 ][ 1 ] = scale[ 1 ];
	matrix[ 2 ][ 2 ] = scale[ 2 ];
}

void Entity::SetRotation( const vec3f& xyzDegrees ) {
	const mat4x4f rotationMatrix = ComputeRotationZYX( xyzDegrees[ 0 ], xyzDegrees[ 1 ], xyzDegrees[ 2 ] );
	matrix = rotationMatrix * matrix;
}

mat4x4f Entity::GetMatrix() const {
	return matrix;
}

void Entity::SetFlag( const entityFlags_t flag ) {
	flags = static_cast<entityFlags_t>( flags | flag );
}

void Entity::ClearFlag( const entityFlags_t flag ) {
	flags = static_cast<entityFlags_t>( flags & ~flag );
}

bool Entity::HasFlag( const entityFlags_t flag ) const {
	return ( ( flags & flag ) != 0 );
}

void Entity::SetRenderFlag( const renderFlags_t flag ) {
	renderFlags = static_cast<renderFlags_t>( renderFlags | flag );
}

void Entity::ClearRenderFlag( const renderFlags_t flag ) {
	renderFlags = static_cast<renderFlags_t>( renderFlags & ~flag );
}

bool Entity::HasRenderFlag( const renderFlags_t flag ) const {
	return ( ( renderFlags & flag ) != 0 );
}

renderFlags_t Entity::GetRenderFlags() const {
	return renderFlags;
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
		scene.FindEntity( "_quadTexDebug" )->ClearRenderFlag( HIDDEN );
		scene.materialLib.Find( "IMAGE2D" )->textures[ 0 ] = ( imguiControls.dbgImageId % scene.textureLib.Count() );
	}
	else {
		scene.FindEntity( "_quadTexDebug" )->SetRenderFlag( HIDDEN );
	}
}