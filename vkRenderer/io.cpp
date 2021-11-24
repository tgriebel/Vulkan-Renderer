#include "io.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

std::vector<char> ReadFile( const std::string& filename )
{
	std::ifstream file( filename, std::ios::ate | std::ios::binary );

	if ( !file.is_open() )
	{
		throw std::runtime_error( "Failed to open file!" );
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer( fileSize );

	file.seekg( 0 );
	file.read( buffer.data(), fileSize );
	file.close();

	return buffer;
}


void LoadModel( const std::string& fileName, modelSource_t& model )
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if ( !tinyobj::LoadObj( &attrib, &shapes, &materials, &warn, &err, ( ModelPath + fileName ).c_str() ) )
	{
		throw std::runtime_error( warn + err );
	}

	std::unordered_map<VertexInput, uint32_t> uniqueVertices{};

	uint32_t vertexCnt = 0;
	model.vertices.reserve( attrib.vertices.size() );

	for ( const auto& shape : shapes )
	{
		for ( const auto& index : shape.mesh.indices )
		{
			VertexInput vertex{ };

			vertex.pos[ 0 ] = attrib.vertices[ 3 * index.vertex_index + 0 ];
			vertex.pos[ 1 ] = attrib.vertices[ 3 * index.vertex_index + 1 ];
			vertex.pos[ 2 ] = attrib.vertices[ 3 * index.vertex_index + 2 ];

			vertex.texCoord[ 0 ] = attrib.texcoords[ 2 * index.texcoord_index + 0 ];
			vertex.texCoord[ 1 ] = 1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ];

			// vertex.normal[0] = attrib.normals[3 * index.normal_index + 0];
			// vertex.normal[1] = attrib.normals[3 * index.normal_index + 1];
			// vertex.normal[2] = attrib.normals[3 * index.normal_index + 2];

			vertex.color = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );

			if ( uniqueVertices.count( vertex ) == 0 )
			{
				model.vertices.push_back( vertex );
				uniqueVertices[ vertex ] = static_cast<uint32_t>( model.vertices.size() - 1 );
			}

			model.indices.push_back( uniqueVertices[ vertex ] );
		}
	}
}