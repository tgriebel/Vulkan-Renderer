#include "common.h"
#include "deviceContext.h"

#define STB_IMAGE_IMPLEMENTATION // includes func defs
#include "stb_image.h"

glm::mat4 MatrixFromVector( const glm::vec3& v )
{
	glm::vec3 up = glm::vec3( 0.0f, 0.0f, 1.0f );
	const glm::vec3 u = glm::normalize( v );
	if ( glm::dot( v, up ) > 0.99999f ) {
		up = glm::vec3( 0.0f, 1.0f, 0.0f );
	}
	const glm::vec3 left = -glm::cross( u, up );

	const glm::mat4 m = glm::mat4( up[ 0 ], up[ 1 ], up[ 2 ], 0.0f,
		left[ 0 ], left[ 1 ], left[ 2 ], 0.0f,
		v[ 0 ], v[ 1 ], v[ 2 ], 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f );

	return m;
}

bool LoadTextureImage( const char * texturePath, textureSource_t& texture )
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load( texturePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha );

	if ( !pixels ) {
		stbi_image_free( pixels );
		return false;
	}

	texture.name		= "";
	texture.bytes		= pixels;
	texture.width		= texWidth;
	texture.height		= texHeight;
	texture.channels	= texChannels;
	texture.sizeBytes	= ( texWidth * texHeight * 4 );

	texture.bytes = new uint8_t[ texture.sizeBytes ];
	memcpy( texture.bytes, pixels, texture.sizeBytes );
	stbi_image_free( pixels );
	return true;
}
