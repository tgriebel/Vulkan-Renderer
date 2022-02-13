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

AABB Entity::GetBounds() const {
	const modelSource_t* model = modelLib.Find( modelId );
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

	const mouse_t& mouse = window.input.GetMouse();
	if ( mouse.centered )
	{
		const float maxSpeed = mouse.speed;
		const float yawDelta = maxSpeed * mouse.dx;
		const float pitchDelta = -maxSpeed * mouse.dy;
		scene.camera.SetYaw( yawDelta );
		scene.camera.SetPitch( pitchDelta );
	} else {
		const vec2f screenPoint = vec2f( mouse.x, mouse.y );
		int width, height;
		window.GetWindowFrameBufferSize( width, height );
		const vec2f ndc = vec2f( 2.0f * screenPoint[ 0 ] / width, 2.0f * screenPoint[ 1 ] / height ) - vec2f( 1.0f );

		Ray ray = scene.camera.GetViewRay( vec2f( 0.5f * ndc[ 0 ] + 0.5f, 0.5f * ndc[ 1 ] + 0.5f ) );

		const int entityNum = static_cast<int>( scene.entities.Count() );
		for ( int i = 0; i < entityNum; ++i )
		{
			Entity* ent = scene.FindEntity( i );
			if ( !ent->HasFlag( ENT_FLAG_SELECTABLE ) ) {
				continue;
			}
			ent->outline = false;
		//	ent->ClearRenderFlag( WIREFRAME );
		}
		for ( int i = 0; i < entityNum; ++i )
		{
			Entity* ent = scene.FindEntity( i );
			if ( !ent->HasFlag( ENT_FLAG_SELECTABLE ) ) {
				continue;
			}
			const modelSource_t* model = modelLib.Find( ent->modelId );

			float t0, t1;
			if ( ent->GetBounds().Intersect( ray, t0, t1 ) ) {
				const vec3f outPt = ray.GetOrigin() + t1 * ray.GetVector();
				ent->outline = true;
				//ent->SetRenderFlag( WIREFRAME );
				break;
			}
		}
	}

	// Skybox
	vec3f skyBoxOrigin;
	skyBoxOrigin[ 0 ] = scene.camera.GetOrigin()[ 0 ];
	skyBoxOrigin[ 1 ] = scene.camera.GetOrigin()[ 1 ];
	skyBoxOrigin[ 2 ] = scene.camera.GetOrigin()[ 2 ] - 0.5f;
	( scene.FindEntity( "_skybox" ) )->SetOrigin( skyBoxOrigin );

	UpdateSceneLocal();

	if ( imguiControls.dbgImageId >= 0 )
	{
		scene.FindEntity( "_quadTexDebug" )->ClearRenderFlag( HIDDEN );
		materialLib.Find( "IMAGE2D" )->textures[ 0 ] = ( imguiControls.dbgImageId % textureLib.Count() );
	}
	else {
		scene.FindEntity( "_quadTexDebug" )->SetRenderFlag( HIDDEN );
	}
}