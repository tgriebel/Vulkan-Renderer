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
	glm::vec3 origin = GetOrigin();
	const vec3d entOrigin = vec3d( origin.x, origin.y, origin.z );
	return AABB( bounds.GetMin() + entOrigin, bounds.GetMax() + entOrigin );
}

glm::vec3 entity_t::GetOrigin() const {
	return glm::vec3( matrix[ 3 ][ 0 ], matrix[ 3 ][ 1 ], matrix[ 3 ][ 2 ] );
}

void entity_t::SetOrigin( const glm::vec3& origin ) {
	matrix[ 3 ][ 0 ] = origin.x;
	matrix[ 3 ][ 1 ] = origin.y;
	matrix[ 3 ][ 2 ] = origin.z;
}

void entity_t::SetScale( const glm::vec3& scale ) {
	matrix[ 0 ][ 0 ] = scale.x;
	matrix[ 1 ][ 1 ] = scale.y;
	matrix[ 2 ][ 2 ] = scale.z;
}

void UpdateScene( const float dt )
{
	// FIXME: race conditions
	// Need to do a ping-pong update

	if ( window.input.IsKeyPressed( 'D' ) ) {
		scene.camera.MoveForward( dt * 0.01f );
	}
	if ( window.input.IsKeyPressed( 'A' ) ) {
		scene.camera.MoveForward( dt * -0.01f );
	}
	if ( window.input.IsKeyPressed( 'W' ) ) {
		scene.camera.MoveVertical( dt * -0.01f );
	}
	if ( window.input.IsKeyPressed( 'S' ) ) {
		scene.camera.MoveVertical( dt * 0.01f );
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
	glm::mat4 skyBoxMatrix = glm::identity<glm::mat4>();
	skyBoxMatrix = glm::identity<glm::mat4>();
	skyBoxMatrix[ 3 ][ 0 ] = scene.camera.GetOrigin()[ 0 ];
	skyBoxMatrix[ 3 ][ 1 ] = scene.camera.GetOrigin()[ 1 ];
	skyBoxMatrix[ 3 ][ 2 ] = scene.camera.GetOrigin()[ 2 ] - 0.5f;
	scene.entities.Find( 0 )->matrix = skyBoxMatrix;

	//entity_t* pawn = scene.entities.Find( "white_pawn_0" );
	//entity_t* debugBox = scene.entities.Find( "white_pawn_0_cube" );
	//AABB bounds = pawn->GetBounds();
	//vec3d size = bounds.GetSize();
	//vec3d center = bounds.GetCenter();
	//debugBox->SetOrigin( glm::vec3( center[ 0 ], center[ 1 ], center[ 2 ] ) );
	//debugBox->SetScale( 0.5f * glm::vec3( size[ 0 ], size[ 1 ], size[ 2 ] ) );
	//GetCenter

	materialLib.Find( "IMAGE2D" )->textures[ 0 ] = ( imguiControls.dbgImageId % textureLib.Count() );
}