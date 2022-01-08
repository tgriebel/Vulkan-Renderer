#include "io.h"
#include "assetLib.h"
#include "util.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

extern AssetLib< Material >			materialLib;
extern AssetLib< texture_t >		textureLib;
extern AssetLib< modelSource_t >	modelLib;
extern AssetLib< GpuProgram >		gpuPrograms;

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

GpuProgram LoadProgram( const std::string& vsFile, const std::string& psFile )
{
	GpuProgram program;
	program.shaders[ 0 ].name = vsFile;
	program.shaders[ 0 ].blob = ReadFile( vsFile );
	program.shaders[ 1 ].name = psFile;
	program.shaders[ 1 ].blob = ReadFile( psFile );
	program.shaderCount = 2;
	program.isCompute = false;
	return program;
}


GpuProgram LoadProgram( const std::string& csFile )
{
	GpuProgram program;
	program.shaders[ 0 ].name = csFile;
	program.shaders[ 0 ].blob = ReadFile( csFile );
	program.shaderCount = 1;
	program.isCompute = true;
	return program;
}


void LoadModel( const std::string& fileName, const std::string& objectName )
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if ( !tinyobj::LoadObj( &attrib, &shapes, &materials, &warn, &err, ( ModelPath + fileName ).c_str(), ModelPath.c_str() ) )
	{
		throw std::runtime_error( warn + err );
	}

	for ( const auto& material : materials )
	{
		texture_t texture;
		if ( LoadTextureImage( material.diffuse_texname.c_str(), texture ) ) {
			texture.uploaded = false;
			texture.mipLevels = static_cast<uint32_t>( std::floor( std::log2( std::max( texture.width, texture.height ) ) ) ) + 1;
			textureLib.Add( material.diffuse_texname.c_str(), texture );
		}

		Material mat;
		mat.shaders[ DRAWPASS_SHADOW ] = gpuPrograms.RetrieveHdl( "Shadow" );
		mat.shaders[ DRAWPASS_DEPTH ] = gpuPrograms.RetrieveHdl( "LitDepth" );
		mat.shaders[ DRAWPASS_OPAQUE ] = gpuPrograms.RetrieveHdl( "LitOpaque" );
		mat.shaders[ DRAWPASS_WIREFRAME ] = gpuPrograms.RetrieveHdl( "LitDepth" );
		mat.textures[ 0 ] = textureLib.RetrieveHdl( material.diffuse_texname.c_str() );
		mat.Kd = rgbTuplef_t( material.diffuse[ 0 ], material.diffuse[ 1 ], material.diffuse[ 2 ] );
		mat.Ks = rgbTuplef_t( material.specular[ 0 ], material.specular[ 1 ], material.specular[ 2 ] );
		mat.Ka = rgbTuplef_t( material.ambient[ 0 ], material.ambient[ 1 ], material.ambient[ 2 ] );
		mat.Ke = rgbTuplef_t( material.emission[ 0 ], material.emission[ 1 ], material.emission[ 2 ] );
		mat.Tf = rgbTuplef_t( material.transmittance[ 0 ], material.transmittance[ 1 ], material.transmittance[ 2 ] );
		mat.Ni = material.ior;
		mat.Ns = material.shininess;
		mat.d = material.dissolve;
		mat.Tr = ( 1.0f - material.dissolve );
		mat.illum = static_cast<float>( material.illum );
		materialLib.Add( material.name.c_str(), mat );
	}

	std::unordered_map<VertexInput, uint32_t> uniqueVertices{};

	modelSource_t model;

	uint32_t vertexCnt = 0;
	//model.surfs[ 0 ].vertices.reserve( attrib.vertices.size() );
	model.surfCount = 0;

	for ( const auto& shape : shapes )
	{
		for ( const auto& index : shape.mesh.indices )
		{
			VertexInput vertex{ };

			vertex.pos[ 0 ] = attrib.vertices[ 3 * index.vertex_index + 0 ];
			vertex.pos[ 1 ] = attrib.vertices[ 3 * index.vertex_index + 1 ];
			vertex.pos[ 2 ] = attrib.vertices[ 3 * index.vertex_index + 2 ];

			model.bounds.Expand( vec3d( vertex.pos[ 0 ], vertex.pos[ 1 ], vertex.pos[ 2 ] ) );

			vertex.texCoord[ 0 ] = attrib.texcoords[ 2 * index.texcoord_index + 0 ];
			vertex.texCoord[ 1 ] = 1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ];

			// vertex.normal[0] = attrib.normals[3 * index.normal_index + 0];
			// vertex.normal[1] = attrib.normals[3 * index.normal_index + 1];
			// vertex.normal[2] = attrib.normals[3 * index.normal_index + 2];

			vertex.color = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );

			if ( uniqueVertices.count( vertex ) == 0 )
			{
				model.surfs[ model.surfCount ].vertices.push_back( vertex );
				uniqueVertices[ vertex ] = static_cast<uint32_t>( model.surfs[ model.surfCount ].vertices.size() - 1 );
			}

			model.surfs[ model.surfCount ].indices.push_back( uniqueVertices[ vertex ] );
		}

		model.surfs[ model.surfCount ].materialId = 0;
		if ( ( materials.size() > 0 ) && ( shape.mesh.material_ids.size() > 0 ) ) {
			const int shapeMaterial = shape.mesh.material_ids[ 0 ];
			const int materialId = materialLib.FindId( materials[ shapeMaterial ].name.c_str() );
			if ( materialId >= 0 ) {
				model.surfs[ model.surfCount ].materialId = materialId;
			}
		}
		++model.surfCount;
	}
	modelLib.Add( objectName.c_str(), model );
}