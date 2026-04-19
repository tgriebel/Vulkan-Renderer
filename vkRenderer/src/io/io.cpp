#include "io.h"

#include <map>
#include <deque>
#include <string>
#include <sstream>
#include <unordered_map>

#include <syscore/serializer.h>
#include <syscore/common.h>

#include <GfxCore/image/image.h>

#include "../scene/sceneBase.h"
#include "../scene/assetManager.h"
#include "../asset_types/model.h"
#include "../asset_types/texture.h"
#include "../asset_types/material.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../../external/tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb_image.h"

#define CGLTF_IMPLEMENTATION
#include "../../external/cgltf.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4996) // sprintf
#include "../../external/stb_image_write.h"
#pragma warning(pop)

#define USE_MIKKT

#ifdef USE_MIKKT
#include "../scene/mtInterface.h"
#endif


bool LoadImage( const char* texturePath, const bool isLinearColor, Image& texture )
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load( texturePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha );

	if( !pixels )
	{
		stbi_image_free( pixels );
		return false;
	}

	//assert( texChannels == 4 );
	imageInfo_t info = DefaultImage2dInfo( texWidth, texHeight );
	if( isLinearColor ) {
		info.fmt = imageFmt_t::IMAGE_FMT_RGBA_8_UNORM;
	}
	texture.Create( info, pixels, info.width * info.height * 4 );

	stbi_image_free( pixels );
	return true;
}


bool LoadImageHDR( const char* texturePath, Image& texture )
{
	int texWidth, texHeight, texChannels;
	float* elements = stbi_loadf( texturePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha );

	if( !elements )
	{
		stbi_image_free( elements );
		return false;
	}

	//assert( texChannels == 4 );
	imageInfo_t info = DefaultImage2dInfo( texWidth, texHeight );
	info.fmt = IMAGE_FMT_RGBA_16;

	assert( texture.cpuImage == nullptr );

	ImageBuffer<rgba16_t>* imageBuffer = new ImageBuffer<rgba16_t>( info.width, info.height, info.layers );

	const uint32_t elementCount = 4 * imageBuffer->GetPixelCount();

	uint16_t* buffer = reinterpret_cast< uint16_t* >( imageBuffer->Ptr() );
	for( uint32_t i = 0; i < elementCount; ++i ) {
		buffer[ i ] = PackFloat32( elements[ i ] );
	}

	texture.Create( info, imageBuffer, nullptr );

	stbi_image_free( elements );
	return true;
}


bool LoadCubeMapImage( const char* textureBasePath, const char* ext, Image& texture )
{
	std::string paths[ 6 ] = {
		( std::string( textureBasePath ) + "_right." + ext ),
		( std::string( textureBasePath ) + "_left." + ext ),
		( std::string( textureBasePath ) + "_top." + ext ),
		( std::string( textureBasePath ) + "_bottom." + ext ),
		( std::string( textureBasePath ) + "_front." + ext ),
		( std::string( textureBasePath ) + "_back." + ext ),
	};

	int sizeBytes = 0;
	Image textures2D[ 6 ];
	for( int i = 0; i < 6; ++i )
	{
		if( LoadImage( paths[ i ].c_str(), false, textures2D[ i ] ) == false )
		{
			sizeBytes = 0;
			break;
		}
		assert( textures2D[ i ].cpuImage != nullptr );
		assert( textures2D[ i ].cpuImage->GetByteCount() > 0 );
		sizeBytes += textures2D[ i ].cpuImage->GetByteCount();
	}

	if( sizeBytes == 0 )
	{
		for( int i = 0; i < 6; ++i )
		{
			delete textures2D[ i ].cpuImage;
			textures2D[ i ].cpuImage = nullptr;
		}
		return false;
	}

	const int texWidth = textures2D[ 0 ].info.width;
	const int texHeight = textures2D[ 0 ].info.height;
	const int texChannels = textures2D[ 0 ].info.channels;

	uint8_t* bytes = new uint8_t[ sizeBytes ];

	int byteOffset = 0;
	for( int i = 0; i < 6; ++i )
	{
		if( ( texWidth != textures2D[ i ].info.width ) ||
			( texHeight != textures2D[ i ].info.height ) ||
			( texChannels != textures2D[ i ].info.channels ) )
		{
			if( bytes != nullptr ) {
				delete[] bytes;
			}
			for( int j = 0; j < 6; ++j )
			{
				delete textures2D[ i ].cpuImage;
				textures2D[ i ].cpuImage = nullptr;
			}
			return false;
		}

		memcpy( bytes + byteOffset, textures2D[ i ].cpuImage->Ptr(), textures2D[ i ].cpuImage->GetByteCount() );
		byteOffset += textures2D[ i ].cpuImage->GetByteCount();
	}

	assert( sizeBytes == byteOffset );
	imageInfo_t info = DefaultImage2dInfo( texWidth, texHeight );
	info.channels = texChannels;
	info.layers = 6;
	info.type = IMAGE_TYPE_CUBE;

	texture.Create( info, bytes, sizeBytes );

	delete[] bytes;

	return true;
}


bool WriteImage( const char* path, const Image& image )
{
	std::string fileName;
	std::string ext;
	SysCore::SplitFileName( path, fileName, ext );

	if( ext == "png" )
	{
		const int ret = stbi_write_png( path, image.info.width, image.info.height, image.info.channels, image.cpuImage->Ptr(), image.info.width * image.cpuImage->GetBpp() );
		return ret == 1;
	}
	else if( ext == "jpg" )
	{
		const int ret = stbi_write_jpg( path, image.info.width, image.info.height, image.info.channels, image.cpuImage->Ptr(), 100 );
		return ret == 1;
	}
	else if( ext == "bmp" )
	{
		const int ret = stbi_write_bmp( path, image.info.width, image.info.height, image.info.channels, image.cpuImage->Ptr() );
		return ret == 1;
	}
	else if( ext == "hdr" )
{
		const int ret = stbi_write_hdr( path, image.info.width, image.info.height, image.info.channels, ( float* )image.cpuImage->Ptr() );
		return ret == 1;
	}
	return false;
}


static Material TranslateObjMaterial( AssetManager& assets, const tinyobj::material_t& material, const std::string& texturePath )
{
	Material outMaterial;

	const bool isPbr = material.roughness || material.metallic || !material.roughness_texname.empty() || !material.metallic_texname.empty();

	struct loadInfo_t
	{
		const std::string& name;
		bool isLinear;
	};

	std::vector<loadInfo_t> supportedTextures;

	if ( isPbr )
	{
		supportedTextures.push_back( loadInfo_t{ material.diffuse_texname, false } );
		supportedTextures.push_back( loadInfo_t{ material.normal_texname, true } );
		supportedTextures.push_back( loadInfo_t{ material.roughness_texname, true } );
		supportedTextures.push_back( loadInfo_t{ material.metallic_texname, true } );
	}
	else
	{
		supportedTextures.push_back( loadInfo_t{ material.diffuse_texname, false } );
		supportedTextures.push_back( loadInfo_t{ material.bump_texname, true } );
		supportedTextures.push_back( loadInfo_t{ material.specular_texname, true } );
	}

	const uint32_t textureCount = static_cast<uint32_t>( supportedTextures.size() );
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		const std::string& name = supportedTextures[ i ].name;
		if ( name.length() == 0 ) {
			continue;
		}
		assets.GetLib<Image>()->AddDeferred( name.c_str(), pImgLoader_t( new ImageLoader( texturePath, name, supportedTextures[ i ].isLinear ) ) );
	}

	if ( material.dissolve == 1.0f )
	{
		outMaterial.AddShader( DRAWPASS_SHADOW, AssetLib<GpuProgram>::Handle( "Shadow" ) );
		outMaterial.AddShader( DRAWPASS_DEPTH, AssetLib<GpuProgram>::Handle( "LitDepth" ) );
		outMaterial.AddShader( DRAWPASS_OPAQUE, AssetLib<GpuProgram>::Handle( "LitOpaque" ) );
	}
	else
	{
		outMaterial.AddShader( DRAWPASS_TRANS, AssetLib<GpuProgram>::Handle( "LitTrans" ) );
	}
	outMaterial.AddShader( DRAWPASS_DEBUG_WIREFRAME, AssetLib<GpuProgram>::Handle( "Debug" ) );
	outMaterial.AddShader( DRAWPASS_DEBUG_3D, AssetLib<GpuProgram>::Handle( "DebugSolid" ) );

	if ( isPbr )
	{
		outMaterial.usage = materialUsage_t::MATERIAL_USAGE_GGX;
		outMaterial.AddTexture( GGX_ALBEDO_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 0 ].name.c_str() ) );
		outMaterial.AddTexture( GGX_NORMAL_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 1 ].name.c_str() ) );
		outMaterial.AddTexture( GGX_ROUGHNESS_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 2 ].name.c_str() ) );
		outMaterial.AddTexture( GGX_METALLIC_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 3 ].name.c_str() ) );
	}
	else
	{
		outMaterial.usage = materialUsage_t::MATERIAL_USAGE_GGX;
		outMaterial.AddTexture( GGX_ALBEDO_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 0 ].name.c_str() ) );
		outMaterial.AddTexture( GGX_NORMAL_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 1 ].name.c_str() ) );
		outMaterial.AddTexture( GGX_ROUGHNESS_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 2 ].name.c_str() ) );
	}

	materialParms_t& parms = outMaterial.GetParms();
	parms.Kd = rgb32_t( material.diffuse[ 0 ], material.diffuse[ 1 ], material.diffuse[ 2 ] );
	parms.Ks = rgb32_t( material.specular[ 0 ], material.specular[ 1 ], material.specular[ 2 ] );
	parms.Ka = rgb32_t( material.ambient[ 0 ], material.ambient[ 1 ], material.ambient[ 2 ] );
	parms.Ke = rgb32_t( material.emission[ 0 ], material.emission[ 1 ], material.emission[ 2 ] );
	parms.Tf = rgb32_t( material.transmittance[ 0 ], material.transmittance[ 1 ], material.transmittance[ 2 ] );
	parms.Ni = material.ior;
	parms.Ns = material.shininess;
	parms.Tr = 1.0f - material.dissolve;
	parms.illum = static_cast<float>( material.illum );

	if ( isPbr )
	{
		parms.roughness          = material.roughness;
		parms.metalness          = material.metallic;
		parms.sheen              = material.sheen;
		parms.clearcoatThickness = material.clearcoat_thickness;
		parms.clearcoatRoughness = material.clearcoat_roughness;
		parms.anisotropy         = material.anisotropy;
		parms.anisotropyRotation = material.anisotropy_rotation;
	}
	return outMaterial;
}


bool LoadMaterial( AssetManager& assets, const std::string& fileName, const std::string& materialPath, const std::string& texturePath, Material& material )
{
	std::ifstream matStream( materialPath + fileName );
	if ( matStream.fail() == true ) {
		std::cout << "LoadMaterialFile: failed to open " << materialPath + fileName << "\n";
		return false;
	}

	std::map<std::string, int> matMap;
	std::vector<tinyobj::material_t> materials;
	std::string warn;

	tinyobj::LoadMtl( &matMap, &materials, &matStream, nullptr, &warn );
	if ( warn.empty() == false ) {
		std::cout << "LoadMaterialFile warning: " << warn << "\n";
	}

	// TODO: Load all materials. There's currently just a one-to-one relationship since adding to the asset lib only adds a single material
	material = TranslateObjMaterial( assets, materials[ 0 ], texturePath );

	return true;
}


bool LoadRawModel( AssetManager& assets, const std::string& fileName, const std::string& modelPath, const std::string& texturePath, Model& model )
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if( !tinyobj::LoadObj( &attrib, &shapes, &materials, &warn, &err, ( modelPath + fileName ).c_str(), modelPath.c_str() ) ) {
		throw std::runtime_error( warn + err );
	}

	// Add Materials
	for( const auto& objMaterial : materials )
	{
		const Material material = TranslateObjMaterial( assets, objMaterial, texturePath );
		assets.GetLib<Material>()->Add( objMaterial.name.c_str(), material );
	}

	uint32_t vertexCnt = 0;
	//model.surfs[ 0 ].vertices.reserve( attrib.vertices.size() );
	model.surfCount = 0;
	model.surfs.resize( shapes.size() );
	for( const auto& shape : shapes )
	{
		bool hasUv = true;

		std::unordered_map<vertex_t, uint32_t> uniqueVertices {};
		std::unordered_map< uint32_t, uint32_t > indexFaceCount {};
		for( const auto& index : shape.mesh.indices ) {
			vertex_t vertex { };

			vertex.pos[ 0 ] = attrib.vertices[ 3 * index.vertex_index + 0 ];
			vertex.pos[ 1 ] = attrib.vertices[ 3 * index.vertex_index + 1 ];
			vertex.pos[ 2 ] = attrib.vertices[ 3 * index.vertex_index + 2 ];

			model.surfs[ model.surfCount ].centroid += vec3f( vertex.pos.xyz );

			model.bounds.Expand( vec3f( vertex.pos[ 0 ], vertex.pos[ 1 ], vertex.pos[ 2 ] ) );

			vertex.uv[ 0 ] = 0.0f;
			vertex.uv[ 1 ] = 0.0f;
			vertex.uv2[ 0 ] = 0.0f;
			vertex.uv2[ 1 ] = 0.0f;
			if( index.texcoord_index >= 0 ) {
				vertex.uv[ 0 ] = attrib.texcoords[ 2 * index.texcoord_index + 0 ];
				vertex.uv[ 1 ] = 1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ];
			}
			else {
				hasUv = false;
			}

			vertex.normal[ 0 ] = attrib.normals[ 3 * index.normal_index + 0 ];
			vertex.normal[ 1 ] = attrib.normals[ 3 * index.normal_index + 1 ];
			vertex.normal[ 2 ] = attrib.normals[ 3 * index.normal_index + 2 ];

			vertex.color[ 0 ] = attrib.colors[ 3 * index.vertex_index + 0 ];
			vertex.color[ 1 ] = attrib.colors[ 3 * index.vertex_index + 1 ];
			vertex.color[ 2 ] = attrib.colors[ 3 * index.vertex_index + 2 ];
			vertex.color[ 3 ] = 1.0f;

			if( uniqueVertices.count( vertex ) == 0 )
			{
				model.surfs[ model.surfCount ].vertices.push_back( vertex );
				uniqueVertices[ vertex ] = static_cast< uint32_t >( model.surfs[ model.surfCount ].vertices.size() - 1 );
			}

			const uint32_t index = uniqueVertices[ vertex ];
			model.surfs[ model.surfCount ].indices.push_back( index );
			indexFaceCount[ uniqueVertices[ vertex ] ]++;
		}

		const int indexCount = static_cast< int >( model.surfs[ model.surfCount ].indices.size() );
		assert( ( indexCount % 3 ) == 0 );

#ifdef USE_MIKKT
		Surface& surface = model.surfs[ model.surfCount ];

		GenerateMikkTangents( surface.vertices, surface.indices );
#else
		// Eric Lengyel "Computing Tangent Basis Vectors for an Arbitrary Mesh"
		for( int i = 0; i < indexCount; i += 3 )
		{
			int indices[ 3 ];
			float weights[ 3 ];
			indices[ 0 ] = model.surfs[ model.surfCount ].indices[ i + 0 ];
			indices[ 1 ] = model.surfs[ model.surfCount ].indices[ i + 1 ];
			indices[ 2 ] = model.surfs[ model.surfCount ].indices[ i + 2 ];

			assert( indexFaceCount[ indices[ 0 ] ] > 0 );
			assert( indexFaceCount[ indices[ 1 ] ] > 0 );
			assert( indexFaceCount[ indices[ 2 ] ] > 0 );

			weights[ 0 ] = ( 1.0f / indexFaceCount[ indices[ 0 ] ] );
			weights[ 1 ] = ( 1.0f / indexFaceCount[ indices[ 1 ] ] );
			weights[ 2 ] = ( 1.0f / indexFaceCount[ indices[ 2 ] ] );

			vertex_t& v0 = model.surfs[ model.surfCount ].vertices[ indices[ 0 ] ];
			vertex_t& v1 = model.surfs[ model.surfCount ].vertices[ indices[ 1 ] ];
			vertex_t& v2 = model.surfs[ model.surfCount ].vertices[ indices[ 2 ] ];

			const vec3f edge0 = vec3f( v1.pos.xyz - v0.pos.xyz );
			const vec3f edge1 = vec3f( v2.pos.xyz - v0.pos.xyz );

			const vec3f faceNormal = ( v0.normal + v1.normal + v2.normal ).Normalize();
			vec3f faceTangent;
			vec3f faceBitangent;

			vec2f uvEdgeDt0;
			vec2f uvEdgeDt1;

			if( hasUv )
			{
				uvEdgeDt0 = ( v1.uv - v0.uv );
				uvEdgeDt1 = ( v2.uv - v0.uv );
			}
			else
			{
				uvEdgeDt0 = vec2f( 1.0f, 0.0f );
				uvEdgeDt1 = vec2f( 0.0f, 1.0f );
			}

			const float denom = ( uvEdgeDt0[ 0 ] * uvEdgeDt1[ 1 ] - uvEdgeDt1[ 0 ] * uvEdgeDt0[ 1 ] ) + 0.00001f;
			if( denom != 0.0f )
			{
				const float r = 1.0f / denom;
				faceTangent = ( edge0 * uvEdgeDt1[ 1 ] - edge1 * uvEdgeDt0[ 1 ] ) * r;
				faceBitangent = ( edge1 * uvEdgeDt0[ 0 ] - edge0 * uvEdgeDt1[ 0 ] ) * r;

				v0.tangent += weights[ 0 ] * faceTangent;
				v0.bitangent += weights[ 0 ] * faceBitangent;

				v1.tangent += weights[ 1 ] * faceTangent;
				v1.bitangent += weights[ 1 ] * faceBitangent;

				v2.tangent += weights[ 2 ] * faceTangent;
				v2.bitangent += weights[ 2 ] * faceBitangent;
			}
		}

		const int vertexCount = static_cast< int >( model.surfs[ model.surfCount ].vertices.size() );
		for( int i = 0; i < vertexCount; ++i )
		{
			vertex_t& v = model.surfs[ model.surfCount ].vertices[ i ];
			FlushDenorms( v.tangent );
			FlushDenorms( v.bitangent );
			FlushDenorms( v.normal );

			// Gram-Schmidt orthogonalize
			v.normal = v.normal.Normalize();
			v.tangent = v.tangent.Normalize();
			v.tangent = ( v.tangent - v.normal * Dot( v.normal, v.tangent ) ).Normalize();
			v.bitangent = v.bitangent.Normalize();

			const uint32_t signBit = ( Dot( Cross( v.tangent, v.bitangent ), v.normal ) > 0.0f ) ? 0 : 1;
			union tangentBitPack_t
			{
				struct
				{
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
#endif

		model.surfs[ model.surfCount ].materialHdl = assets.GetLib<Material>()->GetDefault()->Handle();
		if( ( materials.size() > 0 ) && ( shape.mesh.material_ids.size() > 0 ) )
		{
			const int shapeMaterial = shape.mesh.material_ids[ 0 ];
			if( shapeMaterial == -1 ) {
				continue;
			}
			const hdl_t materialHdl = AssetLib<Material>::Handle( materials[ shapeMaterial ].name.c_str() );
			if( materialHdl.IsValid() ) {
				model.surfs[ model.surfCount ].materialHdl = materialHdl;
			}
		}
		++model.surfCount;
	}
	return true;
}


bool LoadRawModelGLTF( AssetManager& assets, const std::string& fileName, const std::string& modelPath, const std::string& texturePath, Model& model )
{
	// -------------------------------------------------------------------------
	// Parse + load buffers
	// -------------------------------------------------------------------------
	cgltf_options options = {};
	cgltf_data* data = nullptr;

	const std::string filePath = modelPath + fileName;
	if ( cgltf_parse_file( &options, filePath.c_str(), &data ) != cgltf_result_success ) {
		return false;
	}

	if ( cgltf_load_buffers( &options, data, filePath.c_str() ) != cgltf_result_success ) {
		cgltf_free( data );
		return false;
	}

	// -------------------------------------------------------------------------
	// Images
	// Each cgltf_image has either:
	//   .uri          -- relative path to an external file (nullptr if embedded)
	//   .buffer_view  -- non-null if image is embedded in the .glb buffer
	//                    read via: (uint8_t*)bv->buffer->data + bv->offset, length bv->size
	//   .mime_type    -- "image/png", "image/jpeg", etc.
	// -------------------------------------------------------------------------

	// TODO: load each image into an Image asset and register with the asset lib
	//   external:  LoadImage( (texturePath + img.uri).c_str(), isLinear, image )
	//   embedded:  decode from img.buffer_view using stb_image from memory
	//   register:  assets.GetLib<Image>()->Add( img.name, image )

	// -------------------------------------------------------------------------
	// Materials
	// cgltf_material flags: has_pbr_metallic_roughness, has_clearcoat,
	//                       has_sheen, has_anisotropy, has_transmission, has_ior
	// cgltf_pbr_metallic_roughness:
	//   .base_color_texture / .metallic_roughness_texture  (cgltf_texture_view)
	//   .base_color_factor[4], .metallic_factor, .roughness_factor
	// cgltf_clearcoat:
	//   .clearcoat_texture / .clearcoat_roughness_texture / .clearcoat_normal_texture
	//   .clearcoat_factor, .clearcoat_roughness_factor
	// cgltf_sheen:
	//   .sheen_color_texture / .sheen_roughness_texture
	//   .sheen_color_factor[3], .sheen_roughness_factor
	// cgltf_anisotropy:
	//   .anisotropy_texture, .anisotropy_strength, .anisotropy_rotation
	// cgltf_material:
	//   .normal_texture, .occlusion_texture, .emissive_texture, .emissive_factor[3]
	//   .alpha_mode  (cgltf_alpha_mode_opaque / _mask / _blend), .alpha_cutoff
	//   .double_sided
	// Texture slot: cgltf_texture_view.texture->image gives the cgltf_image*
	//               resolve to asset hdl via the image name registered above
	// -------------------------------------------------------------------------

	// TODO: translate each cgltf_material to Material
	//   assign shaders based on alpha_mode (opaque -> shadow+depth+opaque, blend -> trans)
	//   map base_color_texture   -> GGX_ALBEDO_MAP_SLOT
	//   map normal_texture       -> GGX_NORMAL_MAP_SLOT
	//   map metallic_roughness   -> GGX_ROUGHNESS_MAP_SLOT / GGX_METALLIC_MAP_SLOT
	//   map clearcoat_normal     -> GGX_CLEARCOAT_NML_MAP_SLOT
	//   pack scalar factors into materialParms_t
	//   register: assets.GetLib<Material>()->Add( mat.name, material )

	// -------------------------------------------------------------------------
	// Meshes / Surfaces
	// cgltf_mesh:      .name, .primitives[], .primitives_count
	// cgltf_primitive: .type (triangles), .indices (accessor*), .material*,
	//                  .attributes[], .attributes_count
	// cgltf_attribute: .type (cgltf_attribute_type_position/normal/tangent/
	//                         texcoord/color), .data (accessor*)
	// cgltf_accessor:  .count, .type (vec2/vec3/vec4), .component_type
	//   cgltf_accessor_unpack_floats( accessor, float* out, floatCount )
	//     -- reads any component format and converts to float; floatCount = count * num_components
	//   cgltf_accessor_unpack_indices( accessor, void* out, indexSize, indexCount )
	//     -- reads uint8/16/32 indices into your preferred size
	// NOTE: glTF tangents are vec4 -- .w = bitangent sign (+1.0 or -1.0)
	// NOTE: glTF is right-handed Y-up; verify axis convention against your world space
	// -------------------------------------------------------------------------

	// TODO: for each mesh, for each primitive -> one Surface
	//   scan attributes by type to find POSITION / NORMAL / TANGENT / TEXCOORD_0 / COLOR_0
	//   unpack floats via cgltf_accessor_unpack_floats into temp buffers, fill vertex_t
	//   unpack indices via cgltf_accessor_unpack_indices
	//   if TANGENT attribute absent: run GenerateMikkTangents
	//   if TANGENT present: extract vec3 + decode sign from .w, skip MikkT
	//   assign surf.materialHdl from primitive.material->name lookup

	// -------------------------------------------------------------------------
	// Node hierarchy
	// cgltf_node: .name, .parent, .children[], .mesh*, .camera*, .light*
	//   .has_translation / .translation[3]
	//   .has_rotation    / .rotation[4]  (quaternion xyzw)
	//   .has_scale       / .scale[3]
	//   .has_matrix      / .matrix[16]
	//   cgltf_node_transform_world( node, float[16] ) -- full world-space matrix
	// Walk data->scene->nodes[] (not data->nodes[] -- scenes filter the root set)
	// -------------------------------------------------------------------------

	// TODO: walk the scene node tree to apply per-instance transforms
	//   for now, can flatten all meshes without transforms as a first pass

	cgltf_free( data );
	return true;
}


bool LoadModel( Model& model, const hdl_t& hdl, const std::string& bakePath, const std::string& modelPath, const std::string& ext )
{
	Serializer* s = new Serializer( MB( 8 ), serializeMode_t::LOAD );
	std::string fileName = bakePath + modelPath + hdl.String() + ext;

	if( !s->ReadFile( fileName ) ) {
		return false;
	}

	uint8_t name[ 256 ];
	uint32_t nameLength = 0;
	memset( name, 0, 256 );
	s->Next( nameLength );
	assert( nameLength < 256 );
	s->NextArray( name, nameLength );

	name[ nameLength ] = '2'; // FIXME: test

	model.Serialize( s );
	return true;
}


bool WriteModel( Asset<Model>* model, const std::string& fileName )
{
	if( model == nullptr ) {
		return false;
	}
	std::string name = model->GetName();
	Serializer* s = new Serializer( MB( 8 ), serializeMode_t::STORE );

	uint8_t buffer[ 256 ];
	assert( name.length() < 256 );
	uint32_t nameLength = static_cast< uint32_t >( name.length() );
	memcpy( buffer, name.c_str(), nameLength );
	s->Next( nameLength );
	s->NextArray( buffer, nameLength );

	model->Get().Serialize( s );
	s->WriteFile( fileName );
	return true;
}
