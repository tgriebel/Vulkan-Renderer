#include "io.h"
#include "assetLib.h"
#include "render_util.h"
#include "scene.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

extern Scene	scene;

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


hdl_t LoadModel( const std::string& fileName, const std::string& objectName )
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
		const std::string supportedTextures[ 3 ] = {
			TexturePath + material.diffuse_texname,
			TexturePath + material.bump_texname,
			TexturePath + material.specular_texname,
		};
		for ( int i = 0; i < 3; ++i )
		{
			if ( LoadTextureImage( supportedTextures[ i ].c_str(), texture ) ) {
				texture.uploadId = -1;
				texture.info.mipLevels = static_cast<uint32_t>( std::floor( std::log2( std::max( texture.info.width, texture.info.height ) ) ) ) + 1;
				scene.textureLib.Add( supportedTextures[ i ].c_str(), texture );
			}
		}

		Material mat;
		mat.shaders[ DRAWPASS_SHADOW ] = scene.gpuPrograms.RetrieveHdl( "Shadow" );
		mat.shaders[ DRAWPASS_DEPTH ] = scene.gpuPrograms.RetrieveHdl( "LitDepth" );
		mat.shaders[ DRAWPASS_OPAQUE ] = scene.gpuPrograms.RetrieveHdl( "LitOpaque" );
		mat.shaders[ DRAWPASS_DEBUG_WIREFRAME ] = scene.gpuPrograms.RetrieveHdl( "Debug" ); // FIXME: do this with an override
		mat.shaders[ DRAWPASS_DEBUG_SOLID ] = scene.gpuPrograms.RetrieveHdl( "Debug_Solid" );
		mat.textures[ 0 ] = scene.textureLib.RetrieveHdl( supportedTextures[ 0 ].c_str() );
		mat.textures[ 1 ] = scene.textureLib.RetrieveHdl( supportedTextures[ 1 ].c_str() );
		mat.textures[ 2 ] = scene.textureLib.RetrieveHdl( supportedTextures[ 2 ].c_str() );

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
		scene.materialLib.Add( material.name.c_str(), mat );
	}

	Model model;

	uint32_t vertexCnt = 0;
	//model.surfs[ 0 ].vertices.reserve( attrib.vertices.size() );
	model.surfCount = 0;

	for ( const auto& shape : shapes )
	{
		std::unordered_map<VertexInput, uint32_t> uniqueVertices{};
		std::unordered_map< uint32_t, uint32_t > indexFaceCount{};
		for ( const auto& index : shape.mesh.indices )
		{
			VertexInput vertex{ };

			vertex.pos[ 0 ] = attrib.vertices[ 3 * index.vertex_index + 0 ];
			vertex.pos[ 1 ] = attrib.vertices[ 3 * index.vertex_index + 1 ];
			vertex.pos[ 2 ] = attrib.vertices[ 3 * index.vertex_index + 2 ];

			model.bounds.Expand( vec3f( vertex.pos[ 0 ], vertex.pos[ 1 ], vertex.pos[ 2 ] ) );

			vertex.texCoord[ 0 ] = attrib.texcoords[ 2 * index.texcoord_index + 0 ];
			vertex.texCoord[ 1 ] = 1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ];
			vertex.texCoord[ 2 ] = 0.0f;
			vertex.texCoord[ 3 ] = 0.0f;

			vertex.normal[0] = attrib.normals[ 3 * index.normal_index + 0 ];
			vertex.normal[1] = attrib.normals[ 3 * index.normal_index + 1 ];
			vertex.normal[2] = attrib.normals[ 3 * index.normal_index + 2 ];

			vertex.color[ 0 ] = attrib.colors[ 3 * index.vertex_index + 0 ];
			vertex.color[ 1 ] = attrib.colors[ 3 * index.vertex_index + 1 ];
			vertex.color[ 2 ] = attrib.colors[ 3 * index.vertex_index + 2 ];
			vertex.color[ 3 ] = 1.0f;

			if ( uniqueVertices.count( vertex ) == 0 )
			{
				model.surfs[ model.surfCount ].vertices.push_back( vertex );
				uniqueVertices[ vertex ] = static_cast<uint32_t>( model.surfs[ model.surfCount ].vertices.size() - 1 );
			}

			model.surfs[ model.surfCount ].indices.push_back( uniqueVertices[ vertex ] );
			indexFaceCount[ uniqueVertices[ vertex ] ]++;
		}

		const int indexCount = static_cast<int>( model.surfs[ model.surfCount ].indices.size() );
		assert( ( indexCount % 3 ) == 0 );

		for ( int i = 0; i < indexCount; i += 3 ) {
			int indices[ 3 ];
			float weights[ 3 ];
			indices[ 0 ] = model.surfs[ model.surfCount ].indices[ i + 0 ];
			indices[ 1 ] = model.surfs[ model.surfCount ].indices[ i + 1 ];
			indices[ 2 ] = model.surfs[ model.surfCount ].indices[ i + 2 ];

			assert( indexFaceCount[ indices[ 0 ] ] > 0 );
			assert( indexFaceCount[ indices[ 1 ] ] > 0 );
			assert( indexFaceCount[ indices[ 2 ] ] > 0 );

			weights[ 0 ] = 1.0f;// ( 1.0f / indexFaceCount[ indices[ 0 ] ] );
			weights[ 1 ] = 1.0f;//( 1.0f / indexFaceCount[ indices[ 1 ] ] );
			weights[ 2 ] = 1.0f;//( 1.0f / indexFaceCount[ indices[ 2 ] ] );

			VertexInput& v0 = model.surfs[ model.surfCount ].vertices[ indices[ 0 ] ];
			VertexInput& v1 = model.surfs[ model.surfCount ].vertices[ indices[ 1 ] ];
			VertexInput& v2 = model.surfs[ model.surfCount ].vertices[ indices[ 2 ] ];

			const vec4f uvEdgeDt0 = ( v1.texCoord - v0.texCoord );
			const vec4f uvEdgeDt1 = ( v2.texCoord - v0.texCoord );
			float uDt = ( v1.texCoord[ 0 ] - v0.texCoord[ 0 ] );

			const float r = 1.0f / ( uvEdgeDt0[ 0 ] * uvEdgeDt1[ 1 ] - uvEdgeDt1[ 0 ] * uvEdgeDt0[ 1 ] );

			const vec3f edge0 = ( v1.pos - v0.pos );
			const vec3f edge1 = ( v2.pos - v0.pos );

			const vec3f faceNormal = Cross( edge0, edge1 ).Normalize();
			if ( faceNormal.Length() < 0.001f ) {
				continue; // TODO: remove?
			}

			const vec3f faceTangent = ( edge0 * uvEdgeDt1[ 1 ] - edge1 * uvEdgeDt0[ 1 ] ) * r;
			const vec3f faceBitangent = ( edge1 * uvEdgeDt0[ 0 ] - edge0 * uvEdgeDt1[ 0 ] ) * r;

			//assert( Dot( faceNormal, faceTangent ) < 0.001f );
			//assert( Dot( faceNormal, faceBitangent ) < 0.001f );
			//assert( Dot( faceTangent, faceBitangent ) < 0.001f );
			//assert( faceTangent.Length() > 0.001 );
			//assert( faceBitangent.Length() > 0.001 );

			v0.tangent += weights[ 0 ] * faceTangent;
			v0.bitangent += weights[ 0 ] * faceBitangent;
			v0.normal += weights[ 0 ] * faceNormal;

			v1.tangent += weights[ 1 ] * faceTangent;
			v1.bitangent += weights[ 1 ] * faceBitangent;
			v1.normal += weights[ 1 ] * faceNormal;

			v2.tangent += weights[ 2 ] * faceTangent;
			v2.bitangent += weights[ 2 ] * faceBitangent;
			v2.normal += weights[ 2 ] * faceNormal;
		}

		const int vertexCount = static_cast<int>( model.surfs[ model.surfCount ].vertices.size() );
		for ( int i = 0; i < vertexCount; ++i ) {
			VertexInput& v = model.surfs[ model.surfCount ].vertices[ i ];
			v.tangent.FlushDenorms();
			v.bitangent.FlushDenorms();
			v.normal.FlushDenorms();
			// Re-orthonormalize
			// TODO: use graham-schmidt?
			v.tangent = v.tangent.Normalize();
			v.bitangent = Cross( v.tangent, v.normal ).Normalize();
			v.normal = Cross( v.tangent, v.bitangent ).Normalize();

			const uint32_t signBit = ( Dot( Cross( v.tangent, v.bitangent ), v.normal ) > 0.0f ) ? 1 : 0;
			union tangentBitPack_t {
				struct {
					uint32_t signBit : 1;
					uint32_t vecBits : 31;
				};
				float value;
			};
			tangentBitPack_t packed;
			packed.value = v.tangent[ 0 ];
			packed.signBit = signBit;
			v.tangent[ 0 ] = packed.value;

			//assert( fabs( v.normal.Length() - 1.0f ) < 0.001f );
			//assert( fabs( v.tangent.Length() - 1.0f ) < 0.001f );
			//assert( fabs( v.bitangent.Length() - 1.0f ) < 0.001f );

			float tsValues[ 9 ] = { v.tangent[ 0 ], v.tangent[ 1 ], v.tangent[ 2 ],
									v.bitangent[ 0 ], v.bitangent[ 1 ], v.bitangent[ 2 ],
									v.normal[ 0 ], v.normal[ 1 ], v.normal[ 2 ] };
			mat3x3f tsMatrix = mat3x3f( tsValues );
			//assert( tsMatrix.IsOrthonormal( 0.01f ) );
		}

		model.surfs[ model.surfCount ].materialHdl = INVALID_HDL;
		if ( ( materials.size() > 0 ) && ( shape.mesh.material_ids.size() > 0 ) ) {
			const int shapeMaterial = shape.mesh.material_ids[ 0 ];
			const hdl_t materialHdl = scene.materialLib.RetrieveHdl( materials[ shapeMaterial ].name.c_str() );
			if ( materialHdl.IsValid() ) {
				model.surfs[ model.surfCount ].materialHdl = materialHdl;
			}
		}
		++model.surfCount;
	}
	return scene.modelLib.Add( objectName.c_str(), model );
}

bool WriteModel( const std::string& fileName, hdl_t modelHdl ) {
	Model* model = scene.modelLib.Find( modelHdl );
	if( model == nullptr ) {
		return false;
	}
	
	Serializer* s = new Serializer( MB( 8 ), serializeMode_t::STORE );
	s->Next( Ref( model->bounds.max[0] ) );
	s->Next( Ref( model->bounds.max[1] ) );
	s->Next( Ref( model->bounds.max[2] ) );
	return true;
}