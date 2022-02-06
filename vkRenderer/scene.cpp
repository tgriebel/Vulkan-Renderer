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

typedef AssetLib< texture_t > AssetLibImages;
typedef AssetLib< Material > AssetLibMaterials;

extern AssetLib< GpuProgram >	gpuPrograms;
extern AssetLibImages			textureLib;
extern AssetLibMaterials		materialLib;
extern imguiControls_t			imguiControls;
extern Window					window;

AABB entity_t::GetBounds() const {
	const modelSource_t* model = modelLib.Find( modelId );
	const AABB& bounds = model->bounds;
	vec3f origin = GetOrigin();
	return AABB( bounds.GetMin() + origin, bounds.GetMax() + origin );
}

vec3f entity_t::GetOrigin() const {
	return vec3f( matrix[ 3 ][ 0 ], matrix[ 3 ][ 1 ], matrix[ 3 ][ 2 ] );
}

void entity_t::SetOrigin( const vec3f& origin ) {
	matrix[ 3 ][ 0 ] = origin[ 0 ];
	matrix[ 3 ][ 1 ] = origin[ 1 ];
	matrix[ 3 ][ 2 ] = origin[ 2 ];
}

void entity_t::SetScale( const vec3f& scale ) {
	matrix[ 0 ][ 0 ] = scale[ 0 ];
	matrix[ 1 ][ 1 ] = scale[ 1 ];
	matrix[ 2 ][ 2 ] = scale[ 2 ];
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

	const mouse_t& mouse = window.input.GetMouse();
	if ( mouse.centered )
	{
		const float maxSpeed = mouse.speed;
		const float yawDelta = maxSpeed * mouse.dx;
		const float pitchDelta = -maxSpeed * mouse.dy;
		scene.camera.SetYaw( yawDelta );
		scene.camera.SetPitch( pitchDelta );
	}

	// Skybox
	mat4x4f skyBoxMatrix = mat4x4f( 1.0f );
	skyBoxMatrix[ 3 ][ 0 ] = scene.camera.GetOrigin()[ 0 ];
	skyBoxMatrix[ 3 ][ 1 ] = scene.camera.GetOrigin()[ 1 ];
	skyBoxMatrix[ 3 ][ 2 ] = scene.camera.GetOrigin()[ 2 ] - 0.5f;
	//skyBoxMatrix[ 3 ][ 3 ] = 0.0f;
	scene.entities.Find( 0 )->matrix = skyBoxMatrix;

	UpdateSceneLocal();

	materialLib.Find( "IMAGE2D" )->textures[ 0 ] = ( imguiControls.dbgImageId % textureLib.Count() );
}