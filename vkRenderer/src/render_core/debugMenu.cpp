
#include "../globals/common.h"
#include <scene/assetManager.h>
#include <scene/scene.h>
#include <sstream>

extern AssetManager gAssets;

#if defined( USE_IMGUI )
#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../external/imgui/backends/imgui_impl_vulkan.h"

extern imguiControls_t gImguiControls;

static const int defaultWidth = 100;

struct ImguiStyle
{
	static const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
};

static bool EditFloat( float& f )
{
	bool edited = false;
	ImGui::PushID( "##editFloat" );
	ImGui::PushItemWidth( defaultWidth );
	edited = edited || ImGui::InputFloat( "##editFloat", &f );
	ImGui::PopItemWidth();
	ImGui::PopID();

	return edited;
}


static bool EditRgb( rgbTuplef_t& rgb )
{
	ImGui::PushItemWidth( 3 * defaultWidth );

	float col[3] = { rgb.r, rgb.g, rgb.b };
	const bool edited = ImGui::ColorEdit3( "##RGB0", col );
	if( edited )
	{
		rgb.r = col[0];
		rgb.g = col[1];
		rgb.b = col[2];
	}

	ImGui::PopItemWidth();

	return edited;
}


void DebugMenuMaterial( const Material& mat )
{
	ImGui::Text( "Kd: (%1.2f, %1.2f, %1.2f)", mat.Kd().r, mat.Kd().g, mat.Kd().b );
	ImGui::Text( "Ks: (%1.2f, %1.2f, %1.2f)", mat.Ks().r, mat.Ks().g, mat.Ks().b );
	ImGui::Text( "Ke: (%1.2f, %1.2f, %1.2f)", mat.Ke().r, mat.Ke().g, mat.Ke().b );
	ImGui::Text( "Ka: (%1.2f, %1.2f, %1.2f)", mat.Ka().r, mat.Ka().g, mat.Ka().b );
	ImGui::Text( "Ni: %1.2f", mat.Ni() );
	ImGui::Text( "Tf: %1.2f", mat.Tf() );
	ImGui::Text( "Tr: %1.2f", mat.Tr() );
	ImGui::Text( "illum: %1.2f", mat.Illum() );
	ImGui::Separator();
	for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t )
	{
		hdl_t texHdl = mat.GetTexture( t );
		if ( texHdl.IsValid() == false )
		{
			ImGui::Text( "<none>" );
		}
		else
		{
			const char* texName = gAssets.textureLib.FindName( texHdl );
			ImGui::Text( texName );
		}
	}
}

template<class AssetType>
void DebugMenuLibComboEdit( const std::string label, hdl_t& currentHdl, const AssetLib<AssetType>& lib )
{
	const Asset<AssetType>* selectedAsset = lib.Find( currentHdl );
	const char* previewName = ( currentHdl.IsValid() ) ? selectedAsset->GetName().c_str() : "<none>";
	if ( ImGui::BeginCombo( label.c_str(), previewName ) )
	{
		if ( ImGui::Selectable( "<none>", !currentHdl.IsValid() ) ) {
			currentHdl = INVALID_HDL;
		}

		const uint32_t libCount = lib.Count();
		for ( uint32_t i = 0; i < libCount; ++i )
		{
			const Asset<AssetType>* asset = lib.Find(i);
			if( asset == nullptr ) {
				continue;
			}

			const hdl_t menuHdl = asset->Handle();

			const bool selected = ( currentHdl == menuHdl );
			if ( ImGui::Selectable( asset->GetName().c_str(), selected ) ) {
				currentHdl = menuHdl;
			}

			if ( selected ) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
}


void DebugMenuMaterialEdit( Asset<Material>* matAsset )
{
#define EditRgbValue( VALUE )	{															\
									ImGui::PushID( ( matAsset->GetName() + "." + #VALUE ).c_str() ); \
									ImGui::Text( #VALUE );									\
									ImGui::SameLine();										\
									rgbTuplef_t rgb = mat.##VALUE();						\
									if( EditRgb( rgb ) ) {									\
										mat.##VALUE( rgb );									\
									}														\
									ImGui::PopID();											\
								}

#define EditFloatValue( VALUE )	{															\
									ImGui::PushID( ( matAsset->GetName() + "." + #VALUE ).c_str() ); \
									ImGui::Text( #VALUE );									\
									ImGui::SameLine();										\
									float value = mat.##VALUE();							\
									if( EditFloat( value ) ) {								\
										mat.##VALUE( value );								\
									}														\
									ImGui::PopID();											\
								}

	Material& mat = matAsset->Get();

	EditRgbValue(Kd);
	EditRgbValue(Ks);
	EditRgbValue(Ke);
	EditRgbValue(Ka);
	EditRgbValue(Tf);
	EditFloatValue(Ni);
	EditFloatValue( Tr );
	EditFloatValue( Illum );
	
	if ( ImGui::TreeNode( "Textures" ) )
	{
		for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t )
		{
			std::stringstream ss;
			ss << "##Texture" << t;
			std::string label = ss.str();
			ImGui::Text( label.c_str() + 2 );
			ImGui::SameLine();

			hdl_t texHdl = mat.GetTexture( t );
			DebugMenuLibComboEdit( label, texHdl, gAssets.textureLib );
			mat.AddTexture( t, texHdl );
		}
		ImGui::TreePop();
	}

	if ( ImGui::TreeNode( "Shaders" ) )
	{
		for ( uint32_t s = 0; s < Material::MaxMaterialShaders; ++s )
		{
			ImGui::Text( "%i:", s );
			ImGui::SameLine();
			hdl_t shaderHdl = mat.GetShader( s );
			if ( shaderHdl.IsValid() == false )
			{
				ImGui::Text( "<none>" );
			}
			else
			{
				const char* shaderName = gAssets.gpuPrograms.FindName( shaderHdl );
				ImGui::Text( shaderName );
			}
		}
		ImGui::TreePop();
	}

#undef EditRgbValue
#undef EditFloatValue
}


void DebugMenuModelTreeNode( Asset<Model>* modelAsset )
{
	static ImGuiTableFlags tableFlags = ImguiStyle::TableFlags;

	const char* modelName = modelAsset->GetName().c_str();
	if ( ImGui::TreeNode( modelName ) )
	{
		Model& model = modelAsset->Get();
		const vec3f& min = model.bounds.GetMin();
		const vec3f& max = model.bounds.GetMax();
		if ( ImGui::BeginTable( "Bounds", 4, tableFlags ) )
		{
			ImGui::TableSetupColumn( "" );
			ImGui::TableSetupColumn( "X" );
			ImGui::TableSetupColumn( "Y" );
			ImGui::TableSetupColumn( "Z" );
			ImGui::TableHeadersRow();

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex( 0 );
			ImGui::Text( "Min" );
			ImGui::TableSetColumnIndex( 1 );
			ImGui::Text( "%4.3f", min[ 0 ] );
			ImGui::TableSetColumnIndex( 2 );
			ImGui::Text( "%4.3f", min[ 1 ] );
			ImGui::TableSetColumnIndex( 3 );
			ImGui::Text( "%4.3f", min[ 2 ] );

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex( 0 );
			ImGui::Text( "Max" );
			ImGui::TableSetColumnIndex( 1 );
			ImGui::Text( "%4.3f", max[ 0 ] );
			ImGui::TableSetColumnIndex( 2 );
			ImGui::Text( "%4.3f", max[ 1 ] );
			ImGui::TableSetColumnIndex( 3 );
			ImGui::Text( "%4.3f", max[ 2 ] );

			ImGui::EndTable();
		}

		if ( ImGui::TreeNode( "##Surfaces", "Surfaces (%u)", model.surfCount ) )
		{
			if ( ImGui::BeginTable( "Surface", 5, tableFlags ) )
			{
				ImGui::TableSetupColumn( "Number" );
				ImGui::TableSetupColumn( "Material" );
				ImGui::TableSetupColumn( "Vertices" );
				ImGui::TableSetupColumn( "Indices" );
				ImGui::TableSetupColumn( "Centroid" );
				ImGui::TableHeadersRow();

				for ( uint32_t s = 0; s < model.surfCount; ++s )
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 0 );
					ImGui::Text( "%u", s );
					ImGui::TableSetColumnIndex( 1 );
					hdl_t& handle = model.surfs[ s ].materialHdl;
					std::string modelName = "##" + std::string( gAssets.materialLib.FindName( handle ) );
					DebugMenuLibComboEdit( modelName, handle, gAssets.materialLib );
					ImGui::TableSetColumnIndex( 2 );
					ImGui::Text( "%i", (int)model.surfs[ s ].vertices.size() );
					ImGui::TableSetColumnIndex( 3 );
					ImGui::Text( "%i", (int)model.surfs[ s ].indices.size() );
					ImGui::TableSetColumnIndex( 4 );
					ImGui::Text( "(%.2f %.2f %.2f)", model.surfs[ s ].centroid[ 0 ], model.surfs[ s ].centroid[ 1 ], model.surfs[ s ].centroid[ 2 ] );
				}
				ImGui::EndTable();
			}
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}
}


void DebugMenuTextureTreeNode( Asset<Texture>* texAsset )
{
	static ImGuiTableFlags tableFlags = ImguiStyle::TableFlags;

	const char* texName = texAsset->GetName().c_str();

	if ( ImGui::TreeNode( texName ) )
	{
		Texture& texture = texAsset->Get();
		if ( ImGui::BeginTable( "Info", 2, tableFlags ) )
		{
			ImGui::TableNextColumn();	ImGui::Text( "Width" );
			ImGui::TableNextColumn();	ImGui::Text( "%u", texture.info.width );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Height" );
			ImGui::TableNextColumn();	ImGui::Text( "%u", texture.info.height );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Layers" );
			ImGui::TableNextColumn();	ImGui::Text( "%u", texture.info.layers );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Channels" );
			ImGui::TableNextColumn();	ImGui::Text( "%u", texture.info.channels );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Mips" );
			ImGui::TableNextColumn();
			ImGui::Text( "%u", texture.info.mipLevels );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Layers" );
			ImGui::TableNextColumn();
			switch ( texture.info.type )
			{
			case TEXTURE_TYPE_2D:
				ImGui::Text( "2D" );
				break;
			case TEXTURE_TYPE_CUBE:
				ImGui::Text( "CUBE" );
				break;
			default:
				ImGui::Text( "Unknown" );
			}
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Upload Id" );
			ImGui::TableNextColumn();	ImGui::Text( "%u", texture.uploadId );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Size(bytes)" );
			ImGui::TableNextColumn();	ImGui::Text( "%u", texture.sizeBytes );
			ImGui::TableNextRow();

			ImGui::EndTable();
		}

		ImGui::TreePop();
	}
}


void DebugMenuLightEdit( Scene* scene )
{
	const uint32_t lightCount = static_cast<uint32_t>( scene->lights.size() );
	for ( uint32_t i = 0; i < lightCount; ++i )
	{
		std::stringstream ss;
		ss << "##Light" << i;
		std::string label = ss.str();

		if ( ImGui::TreeNode( label.c_str() + 2 ) )
		{
			ImGui::PushID( "##editLight_x" );
			EditFloat( scene->lights[ i ].lightPos[0] );
			ImGui::PopID();
			ImGui::PushID( "##editLight_y" );
			EditFloat( scene->lights[ i ].lightPos[1] );
			ImGui::PopID();
			ImGui::PushID( "##editLight_z" );
			EditFloat( scene->lights[ i ].lightPos[2] );
			ImGui::PopID();

			rgbTuplef_t rgb = scene->lights[ i ].color.AsRGBf();
			EditRgb( rgb );

			ImGui::TreePop();
		}
	}
}


void DebugMenuDeviceProperties( VkPhysicalDeviceProperties deviceProperties )
{
	static ImGuiTableFlags tableFlags = ImguiStyle::TableFlags;

#define LimitUint( FIELD )			ImGui::TableNextColumn();	ImGui::Text( #FIELD );									\
									ImGui::TableNextColumn();	ImGui::Text( "%u", deviceProperties.limits.##FIELD );

#define LimitDeviceSize( FIELD )	ImGui::TableNextColumn();	ImGui::Text( #FIELD );									\
									ImGui::TableNextColumn();	ImGui::Text( "%u", deviceProperties.limits.##FIELD );

#define LimitSizeT( FIELD )			ImGui::TableNextColumn();	ImGui::Text( #FIELD );									\
									ImGui::TableNextColumn();	ImGui::Text( "%ull", deviceProperties.limits.##FIELD );

#define LimitBool( FIELD )			ImGui::TableNextColumn();	ImGui::Text( #FIELD );									\
									ImGui::TableNextColumn();	ImGui::Text( "%u", deviceProperties.limits.##FIELD );

#define LimitFloat( FIELD )			ImGui::TableNextColumn();	ImGui::Text( #FIELD );									\
									ImGui::TableNextColumn();	ImGui::Text( "%f", deviceProperties.limits.##FIELD );

#define LimitInt( FIELD )			ImGui::TableNextColumn();	ImGui::Text( #FIELD );									\
									ImGui::TableNextColumn();	ImGui::Text( "%i", deviceProperties.limits.##FIELD );

#define LimitHex( FIELD )			ImGui::TableNextColumn();	ImGui::Text( #FIELD );									\
									ImGui::TableNextColumn();	ImGui::Text( "%X", deviceProperties.limits.##FIELD );

	if ( ImGui::BeginTable( "Info", 2, tableFlags ) )
	{
		ImGui::TableNextColumn();	ImGui::Text( "Device" );
		ImGui::TableNextColumn();	ImGui::Text( deviceProperties.deviceName );

		ImGui::TableNextColumn();	ImGui::Text( "API Version" );
		ImGui::TableNextColumn();	ImGui::Text( "%u", deviceProperties.apiVersion );

		ImGui::TableNextColumn();	ImGui::Text( "Driver Version" );
		ImGui::TableNextColumn();	ImGui::Text( "%u", deviceProperties.driverVersion );

		ImGui::TableNextColumn();
		ImGui::Text( "Type" );
		ImGui::TableNextColumn();
		switch( deviceProperties.deviceType )
		{
			case VK_PHYSICAL_DEVICE_TYPE_OTHER:				ImGui::Text( "Unknown" );	break;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:	ImGui::Text( "Integrated" );break;
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:		ImGui::Text( "Discrete" );	break;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:		ImGui::Text( "Virtual" );	break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:				ImGui::Text( "CPU" );		break;
		}

		ImGui::EndTable();
	}

	if ( ImGui::TreeNode( "Limits" ) )
	{
		if ( ImGui::BeginTable( "Info", 2, tableFlags ) )
		{
			LimitUint( maxImageDimension1D )
			LimitUint( maxImageDimension2D )
			LimitUint( maxImageDimension3D )
			LimitUint( maxImageDimensionCube )
			LimitUint( maxImageArrayLayers )
			LimitUint( maxTexelBufferElements )
			LimitUint( maxUniformBufferRange )
			LimitUint( maxStorageBufferRange )
			LimitUint( maxPushConstantsSize )
			LimitUint( maxMemoryAllocationCount )
			LimitUint( maxSamplerAllocationCount )
			LimitUint( maxBoundDescriptorSets )
			LimitUint( maxPerStageDescriptorSamplers )
			LimitUint( maxPerStageDescriptorUniformBuffers )
			LimitUint( maxPerStageDescriptorStorageBuffers )
			LimitUint( maxPerStageDescriptorSampledImages )
			LimitUint( maxPerStageDescriptorStorageImages )
			LimitUint( maxPerStageDescriptorInputAttachments )
			LimitUint( maxPerStageResources )
			LimitUint( maxDescriptorSetSamplers )
			LimitUint( maxDescriptorSetUniformBuffers )
			LimitUint( maxDescriptorSetUniformBuffersDynamic )
			LimitUint( maxDescriptorSetStorageBuffers )
			LimitUint( maxDescriptorSetStorageBuffersDynamic )
			LimitUint( maxDescriptorSetSampledImages )
			LimitUint( maxDescriptorSetStorageImages )
			LimitUint( maxDescriptorSetInputAttachments )
			LimitUint( maxVertexInputAttributes )
			LimitUint( maxVertexInputBindings )
			LimitUint( maxVertexInputAttributeOffset )
			LimitUint( maxVertexInputBindingStride )
			LimitUint( maxVertexOutputComponents )
			LimitUint( maxTessellationGenerationLevel )
			LimitUint( maxTessellationPatchSize )
			LimitUint( maxTessellationControlPerVertexInputComponents )
			LimitUint( maxTessellationControlPerVertexOutputComponents )
			LimitUint( maxTessellationControlPerPatchOutputComponents )
			LimitUint( maxTessellationControlTotalOutputComponents )
			LimitUint( maxTessellationEvaluationInputComponents )
			LimitUint( maxTessellationEvaluationOutputComponents )
			LimitUint( maxGeometryShaderInvocations )
			LimitUint( maxGeometryInputComponents )
			LimitUint( maxGeometryOutputComponents )
			LimitUint( maxGeometryOutputVertices )
			LimitUint( maxGeometryTotalOutputComponents )
			LimitUint( maxFragmentInputComponents )
			LimitUint( maxFragmentOutputAttachments )
			LimitUint( maxFragmentDualSrcAttachments )
			LimitUint( maxFragmentCombinedOutputResources )
			LimitUint( maxComputeSharedMemorySize )
			LimitUint( maxComputeWorkGroupCount[0] )
			LimitUint( maxComputeWorkGroupCount[1] )
			LimitUint( maxComputeWorkGroupCount[2] )
			LimitUint( maxComputeWorkGroupInvocations )
			LimitUint( maxComputeWorkGroupSize[0] )
			LimitUint( maxComputeWorkGroupSize[1] )
			LimitUint( maxComputeWorkGroupSize[2] )
			LimitUint( subPixelPrecisionBits )
			LimitUint( subTexelPrecisionBits )
			LimitUint( mipmapPrecisionBits )
			LimitUint( maxDrawIndexedIndexValue )
			LimitUint( maxDrawIndirectCount )
			LimitUint( maxViewports )
			LimitUint( maxViewportDimensions[0] )
			LimitUint( maxViewportDimensions[1] )
			LimitUint( viewportSubPixelBits )
			LimitUint( maxTexelOffset )
			LimitUint( maxTexelGatherOffset )
			LimitUint( subPixelInterpolationOffsetBits )
			LimitUint( maxFramebufferWidth )
			LimitUint( maxFramebufferHeight )
			LimitUint( maxFramebufferLayers )
			LimitUint( maxColorAttachments )
			LimitUint( maxSampleMaskWords )
			LimitUint( maxClipDistances )
			LimitUint( maxCullDistances )
			LimitUint( maxCombinedClipAndCullDistances )
			LimitUint( discreteQueuePriorities )
			LimitDeviceSize( bufferImageGranularity )
			LimitDeviceSize( sparseAddressSpaceSize )
			LimitDeviceSize( minTexelBufferOffsetAlignment )
			LimitDeviceSize( minUniformBufferOffsetAlignment )
			LimitDeviceSize( minStorageBufferOffsetAlignment )
			LimitDeviceSize( optimalBufferCopyOffsetAlignment )
			LimitDeviceSize( optimalBufferCopyRowPitchAlignment )
			LimitDeviceSize( nonCoherentAtomSize )
			LimitBool( timestampComputeAndGraphics )
			LimitBool( strictLines )
			LimitBool( standardSampleLocations )
			LimitFloat( maxSamplerLodBias )
			LimitFloat( maxSamplerAnisotropy )
			LimitFloat( viewportBoundsRange[0] )
			LimitFloat( viewportBoundsRange[1] )
			LimitFloat( minInterpolationOffset )
			LimitFloat( maxInterpolationOffset )
			LimitFloat( timestampPeriod )
			LimitFloat( pointSizeRange[0] )
			LimitFloat( pointSizeRange[1] )
			LimitFloat( lineWidthRange[0] )
			LimitFloat( lineWidthRange[1] )
			LimitFloat( pointSizeGranularity )
			LimitFloat( lineWidthGranularity )
			LimitInt( minTexelOffset )
			LimitInt( minTexelGatherOffset )
			LimitSizeT( minMemoryMapAlignment )
			LimitHex( framebufferColorSampleCounts )
			LimitHex( framebufferDepthSampleCounts )
			LimitHex( framebufferStencilSampleCounts )
			LimitHex( framebufferNoAttachmentsSampleCounts )
			LimitHex( sampledImageColorSampleCounts )
			LimitHex( sampledImageIntegerSampleCounts )
			LimitHex( sampledImageDepthSampleCounts )
			LimitHex( sampledImageStencilSampleCounts )
			LimitHex( storageImageSampleCounts )

			ImGui::EndTable();
		}

		ImGui::TreePop();
	}

#undef LimitUint
#undef LimitDeviceSize
#undef LimitBool
#undef LimitFloat
#undef LimitInt
#undef LimitHex
}
#endif