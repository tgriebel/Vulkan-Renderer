#pragma once

#include "common.h"

class Camera
{
private:
	glm::mat4 axis;
	glm::vec4 origin;

public:

	float	fov;
	float	near;
	float	far;
	float	aspect;
	float	yaw;
	float	pitch;

	float	focalLength;
	float	viewportWidth;
	float	viewportHeight;
	float	halfFovX;
	float	halfFovY;

	void Init( const glm::vec4& _origin, const glm::mat4& _axis, const float _fov = 90.0f, const float _near = 1.0f, const float _far = 1000.0f )
	{
		origin = _origin;
		axis = _axis;

		fov = glm::radians( _fov );
		near = _near;
		far = _far;
		aspect = 1.0f;
		yaw = 0.0f;
		pitch = 0.0f;
	}

	Camera()
	{
		glm::vec4 origin = glm::vec4( 0.0f, 0.0f, 0.0f, 0.0f );
		glm::mat4 axis = glm::mat4( glm::vec4( 0.0f, -1.0f, 0.0f, 0.0f ), glm::vec4( 0.0f, 0.0f, -1.0f, 0.0f ), glm::vec4( -1.0f, 0.0f, 0.0f, 0.0f ), glm::vec4( 0.0f, 0.0f, 0.0f, 1.0f ) );
		Init( origin, axis );
	}

	Camera( const glm::vec4& _origin )
	{
		glm::mat4 axis = glm::mat4( glm::vec4( 0.0f, -1.0f, 0.0f, 0.0f ), glm::vec4( 0.0f, 0.0f, -1.0f, 0.0f ), glm::vec4( -1.0f, 0.0f, 0.0f, 0.0f ), glm::vec4( 0.0f, 0.0f, 0.0f, 1.0f ) );
		Init( _origin, axis );
	}

	Camera( const glm::vec4& _origin, const glm::mat4& _axis )
	{
		Init( _origin, _axis );
	}

	struct plane_t
	{
		glm::vec4 halfWidth;
		glm::vec4 halfHeight;
		glm::vec4 origin;
	};

	plane_t GetFocalPlane() const
	{
		plane_t plane;
		plane.origin = origin + focalLength * GetForward();
		plane.halfWidth = 0.5f * focalLength * viewportWidth * GetRight();
		plane.halfHeight = 0.5f * focalLength * viewportHeight * GetUp();
		return plane;
	}

	Ray GetViewRay( const vec2f& uv ) const
	{
		// TODO: clip by near plane
		plane_t plane = GetFocalPlane();
		glm::vec4 corner = plane.origin - plane.halfWidth - plane.halfHeight;
		glm::vec4 viewPt = corner + glm::vec4( 2.0f * uv[ 0 ] * plane.halfWidth + 2.0f * uv[ 1 ] * plane.halfHeight );

		vec3d rayOrigin = vec3d( origin.x, origin.y, origin.z );
		vec3d rayTarget = vec3d( viewPt.x, viewPt.y, viewPt.z );
		return Ray( rayOrigin, rayTarget );
	}

	glm::vec4 GetOrigin()
	{
		return origin;
	}

	glm::mat4 GetAxis() const
	{
		glm::mat4 view = glm::rotate( axis, yaw, glm::vec3( 0.0f, 1.0f, 0.0f ) );
		view = glm::rotate( view, pitch, glm::vec3( 1.0f, 0.0f, 0.0f ) );
		return view;
	}

	glm::mat4 GetViewMatrix() const
	{
		glm::mat4 view = GetAxis();
		glm::vec4 localOrigin = glm::vec4( -glm::dot( view[ 0 ], origin ), -glm::dot( view[ 1 ], origin ), -glm::dot( view[ 2 ], origin ), 0.0f );

		// Column-major
		return glm::mat4(	view[ 0 ][ 0 ], view[ 1 ][ 0 ], view[ 2 ][ 0 ], 0.0f,	// X
							view[ 0 ][ 1 ], view[ 1 ][ 1 ], view[ 2 ][ 1 ], 0.0f,	// Y
							view[ 0 ][ 2 ], view[ 1 ][ 2 ], view[ 2 ][ 2 ], 0.0f,	// Z
							localOrigin[ 0 ], localOrigin[ 1 ], localOrigin[ 2 ], 1.0f );
	}

	glm::mat4 GetPerspectiveMatrix() const
	{
		const float halfFovX = glm::tan( 0.5f * fov );
		const float halfFovY = glm::tan( 0.5f * ( fov / aspect ) );

		glm::mat4 proj = glm::mat4( 0.0f );
		proj[ 0 ][ 0 ] = 1.0f / halfFovX;
		proj[ 1 ][ 1 ] = 1.0f / halfFovY;
		proj[ 2 ][ 2 ] = far / ( near - far );
		proj[ 2 ][ 3 ] = -1.0f;
		proj[ 3 ][ 2 ] = -( far * near ) / ( far - near );
		return proj;
	}

	glm::mat4 GetOrthogonalMatrix( const float left, const float right, const float top, const float bottom )
	{
		glm::mat4 proj = glm::mat4( 0.0f );
		proj[ 0 ][ 0 ] = 2.0f / ( right - left );
		proj[ 1 ][ 1 ] = 2.0f / ( top - bottom );
		proj[ 2 ][ 2 ] = -2.0f / ( far - near );
		proj[ 3 ][ 3 ] = 1.0f;
		proj[ 3 ][ 0 ] = -( right + left ) / ( right - left );
		proj[ 3 ][ 1 ] = -( top + bottom ) / ( top - bottom );
		proj[ 3 ][ 2 ] = -( far + near ) / ( far - near );
		return proj;
	}

	glm::vec4 GetForward() const
	{
		glm::mat4 view = GetAxis();
		return -view[ 2 ];
	}

	glm::vec4 GetRight() const
	{
		glm::mat4 view = GetAxis();
		return view[ 0 ];
	}

	glm::vec4 GetUp() const
	{
		glm::mat4 view = GetAxis();
		return view[ 1 ];
	}

	void Translate( glm::vec4 offset )
	{
		origin += offset;
	}

	void SetYaw( const float delta )
	{
		yaw += delta;
	}

	void SetPitch( const float delta )
	{
		pitch += delta;
	}

	void MoveForward( const float delta )
	{
		origin += delta * GetForward();
	}

	void MoveRight( const float delta )
	{
		origin += delta * GetRight();
	}

	void MoveUp( const float delta )
	{
		origin += delta * GetUp();
	}
};