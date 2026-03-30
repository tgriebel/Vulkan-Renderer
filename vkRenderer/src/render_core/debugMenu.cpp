
#include "../globals/common.h"
#include "gpuImage.h"
#include "debugMenu.h"
#include "../globals/assetDefs.h"
#include <gfxcore/scene/scene.h>
#include <sstream>

renderDebugData_t g_renderDebugData;

#include "../globals/render_util.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/imgui.h"
#include "../../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../../external/imgui/backends/imgui_impl_vulkan.h"

#include "debugMenu.h"

extern imguiControls_t g_imguiControls;

static const int defaultWidth = 100;

struct ImguiStyle
{
	static const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
};


#define EditFlagValue( NAME, FLAGS, CHECKFLAG )	{																					\
													bool NAME = HasFlags( FLAGS, CHECKFLAG );										\
													if ( ImGui::Checkbox( "##" #NAME, &NAME ) )										\
													{																				\
														if ( NAME ) {																\
															SetFlags( FLAGS, CHECKFLAG );											\
														} else {																	\
															ClearFlags( FLAGS, CHECKFLAG );											\
														}																			\
													}																				\
												}

void EditVector3Field( vec3f& v, const std::string& label, const float speedSlow = 0.1f, const float speedFast = 1.0f )
{
	float vec[ 3 ] = { v[ 0 ], v[ 1 ], v[ 2 ] };

	static ImGuiTableFlags vectorFieldFlags = ImGuiTableFlags_Hideable;

	if ( ImGui::BeginTable( ( "##" + label ).c_str(), 3, vectorFieldFlags ) )
	{
		ImGui::TableNextColumn();
		ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.5f, 0.0f, 0.0f, 1.0f ) );
		ImGui::InputFloat( "X", &vec[ 0 ], speedSlow, speedFast );
		ImGui::PopStyleColor();

		ImGui::TableNextColumn();
		ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.0f, 0.5f, 0.0f, 1.0f ) );
		ImGui::InputFloat( "Y", &vec[ 1 ], speedSlow, speedFast );
		ImGui::PopStyleColor();

		ImGui::TableNextColumn();
		ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.0f, 0.0f, 0.5f, 1.0f ) );
		ImGui::InputFloat( "Z", &vec[ 2 ], speedSlow, speedFast );
		ImGui::PopStyleColor();

		ImGui::EndTable();
	}
	v = vec;
}

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


static bool EditRgb( rgb32_t& rgb )
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
			const char* texName = TextureLib().FindName( texHdl );
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
									rgb32_t rgb = mat.##VALUE();							\
									if( EditRgb( rgb ) ) {									\
										mat.##VALUE( rgb );									\
										matAsset->QueueUpload();							\
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
										matAsset->QueueUpload();							\
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
			ss << "##Image" << t;
			std::string label = ss.str();
			ImGui::Text( label.c_str() + 2 );
			ImGui::SameLine();

			hdl_t texHdl = mat.GetTexture( t );
			DebugMenuLibComboEdit( label, texHdl, TextureLib() );
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
			hdl_t shaderHdl = mat.GetShader( drawPass_t( s ) );
			if ( shaderHdl.IsValid() == false )
			{
				ImGui::Text( "<none>" );
			}
			else
			{
				const char* shaderName = GpuProgramLib().FindName( shaderHdl );
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
					std::string modelName = "##" + std::string( MaterialLib().FindName( handle ) );
					DebugMenuLibComboEdit( modelName, handle, MaterialLib() );
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


void DebugMenuTextureTreeNode( Asset<Image>* texAsset )
{
	static ImGuiTableFlags tableFlags = ImguiStyle::TableFlags;

	const char* texName = texAsset->GetName().c_str();

	if ( ImGui::TreeNode( texName ) )
	{
		Image& texture = texAsset->Get();
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
			case IMAGE_TYPE_2D:
				ImGui::Text( "2D" );
				break;
			case IMAGE_TYPE_CUBE:
				ImGui::Text( "CUBE" );
				break;
			default:
				ImGui::Text( "Unknown" );
			}
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Upload Id" );
			ImGui::TableNextColumn();	ImGui::Text( "%u", texture.gpuImage->GetId() );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Size(bytes)" );
			if( texture.cpuImage != nullptr ) {
				ImGui::TableNextColumn();	ImGui::Text( "%u", texture.cpuImage->GetByteCount() );
			} else {
				ImGui::TableNextColumn();	ImGui::Text( "No CPU Mem" );
			}
			ImGui::TableNextRow();

			ImGui::EndTable();
		}

		ImGui::TreePop();
	}
}


void DebugMenuShaderTreeNode( Asset<GpuProgram>* shaderAsset )
{
	static ImGuiTableFlags tableFlags = ImguiStyle::TableFlags;

	GpuProgram& shader = shaderAsset->Get();
	const char* shaderName = shaderAsset->GetName().c_str();
	if ( ImGui::TreeNode( shaderName ) )
	{
		if ( ImGui::BeginTable( shaderName, 3, tableFlags ) )
		{
			ImGui::TableSetupColumn( "Name" );
			ImGui::TableSetupColumn( "Type" );
			ImGui::TableSetupColumn( "Bytes" );
			ImGui::TableHeadersRow();

			for ( uint32_t i = 0; i < shader.shaderCount; ++i )
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex( 0 );
				ImGui::Text( shader.shaders[ 0 ][ i ].name.c_str() );
				ImGui::TableSetColumnIndex( 1 );
				switch ( shader.shaders[ 0 ][ i ].type )
				{
				case VERTEX: ImGui::Text( "Vertex" ); break;
				case PIXEL: ImGui::Text( "Pixel" ); break;
				case COMPUTE: ImGui::Text( "Compute" ); break;
				}
				ImGui::TableSetColumnIndex( 2 );
				ImGui::Text( "%u", shader.shaders[ 0 ][ i ].blob.size() );
			}
			if( ImGui::Button( "Reload" ) ) {
				g_imguiControls.shaderHdl = shaderAsset->Handle();
			}
			ImGui::EndTable();
		}
		ImGui::TreePop();
	}
}


void DebugMenuEntityEdit( Scene* scene )
{
	static ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

	struct outlinerEntry_t
	{
		std::string		label;
		int32_t			id;
		bool			isEntity;
	};

	const uint32_t entityCount = scene->EntityCount();
	const uint32_t lightCount = static_cast<uint32_t>( scene->lights.size() );

	std::vector<outlinerEntry_t> entries;
	entries.reserve( entityCount + lightCount );

	ImGui::BeginChild( "Scene", ImVec2( 0, 260 ) );
	
	for ( uint32_t i = 0; i < lightCount; ++i )
	{
		light_t& light = scene->lights[ i ];

		std::stringstream ss;
		ss << "Light" << i;

		outlinerEntry_t entry;
		entry.label = ss.str();
		entry.isEntity = false;
		entry.id = i;

		entries.push_back( entry );
	}

	for ( uint32_t i = 0; i < entityCount; ++i )
	{
		Entity* ent = scene->FindEntity( i );
		if ( ent == nullptr ) {
			continue;
		}
		
		std::stringstream ss;

		outlinerEntry_t entry;
		entry.label = scene->entities[ i ]->name;
		entry.isEntity = true;
		entry.id = i;

		entries.push_back( entry );
	}

	static ImVector<int32_t> selection;

	if ( ImGui::BeginTable( "Entities", 2, flags ) )
	{
		const uint32_t entryCount = static_cast<uint32_t>( entries.size() );
		for ( uint32_t i = 0; i < entryCount; ++i )
		{
			const int32_t itemId = i;

			const outlinerEntry_t& entry = entries[ i ];
			const bool isSelected = selection.contains( itemId );

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID( i );
			if( ImGui::Selectable( entry.label.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns ) )
			{
				if ( ImGui::GetIO().KeyCtrl )
				{
					if ( isSelected )
						selection.find_erase_unsorted( itemId );
					else
						selection.push_back( itemId );
				}
				else
				{
					selection.clear();
					selection.push_back( itemId );
				}
			}
			ImGui::TableNextColumn();
			ImGui::Text( entry.isEntity ? "Entity" : "Light" );

			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	ImGui::EndChild();

	ImGui::Separator();

	static ImGuiTableFlags tableFlags = ImguiStyle::TableFlags;

	if( selection.empty() ) {
		return;
	}

	const int32_t currentEntityId = selection[ 0 ];

	if ( currentEntityId < 0 ) {
		return;
	}

	const outlinerEntry_t& entry = entries[ currentEntityId ];

	ImGui::BeginChild( "##PropertyGrid", ImVec2( 0, 260 ), true );
	ImGui::Separator();
	ImGui::Text( "Property Grid" );
	ImGui::Separator();

	static ImGuiTableFlags propertyGridFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

	ImGui::PushItemWidth( 100 );
	ImGui::PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0.2f, 0.2f, 0.2f, 1.0f ) );

	if( entry.isEntity )
	{
		Entity* ent = scene->FindEntity( entry.id );

		if ( ImGui::BeginTable( "##EntityProperties", 2, propertyGridFlags ) )
		{
			vec3f origin = ent->GetOrigin();
			vec3f scale = ent->GetScale();
			const mat4x4f R = ent->GetRotation();

			vec3f rotation;
			MatrixToEulerZYX( R, rotation[ 0 ], rotation[ 1 ], rotation[ 2 ] );

			ImGui::TableNextColumn();
			ImGui::Text( "Origin" );
			ImGui::TableNextColumn();
			EditVector3Field( origin, "Origin", 0.1f, 1.0f );
			
			ImGui::TableNextColumn();
			ImGui::Text( "Scale" );
			ImGui::TableNextColumn();
			EditVector3Field( scale, "Scale", 0.1f, 1.0f );

			ImGui::TableNextColumn();
			ImGui::Text( "Rotation" );
			ImGui::TableNextColumn();
			EditVector3Field( rotation, "Rotation", 1.0f, 10.0f );

			ent->SetOrigin( origin );
			ent->SetScale( scale );
			ent->SetRotation( rotation );

			ImGui::EndTable();
		}		
	}
	else
	{
		light_t& light = scene->lights[ entry.id ];

		if ( ImGui::BeginTable( "##LightProperties", 2, propertyGridFlags ) )
		{
			vec3f origin = light.pos.xyz;
			vec3f dir = light.dir.xyz;

			ImGui::TableNextColumn();
			ImGui::Text( "Origin" );
			ImGui::TableNextColumn();
			EditVector3Field( origin, "Origin", 0.1f, 1.0f );

			ImGui::TableNextColumn();
			ImGui::Text( "Direction" );
			ImGui::TableNextColumn();
			EditVector3Field( dir, "Direction", 0.1f, 1.0f );

			light.pos = vec4f( origin, 0.0f );
			light.dir = vec4f( dir, 0.0f );

			ImGui::TableNextColumn();
			ImGui::Text( "Intensity" );
			ImGui::TableNextColumn();
			ImGui::InputFloat( "##lightIntesity", &light.intensity, 0.1f, 1.0f );

			ImGui::TableNextColumn();
			ImGui::Text( "Shadow" );
			ImGui::TableNextColumn();
			EditFlagValue( Shadow, light.flags, lightFlags_t::LIGHT_FLAGS_SHADOW );

			ImGui::TableNextColumn();
			ImGui::Text( "Point Light" );
			ImGui::TableNextColumn();
			EditFlagValue( PointLight, light.flags, lightFlags_t::LIGHT_FLAGS_POINT );

			rgb32_t rgb = light.color.AsRgb32();

			ImGui::TableNextColumn();
			ImGui::Text( "Color" );
			ImGui::TableNextColumn();
			EditRgb( rgb );
			light.color = rgb;

			ImGui::EndTable();
		}
	}
	ImGui::PopStyleColor();
	ImGui::PopItemWidth();
	
	ImGui::EndChild();
	ImGui::Separator();
}


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

#define FeatureBool( FIELD )		ImGui::TableNextColumn();	ImGui::Text( #FIELD );									\
									ImGui::TableNextColumn();	ImGui::Text( "%u", deviceFeatures.##FIELD );


void DebugMenuDeviceProperties( VkPhysicalDeviceProperties deviceProperties, VkPhysicalDeviceFeatures deviceFeatures )
{
	static ImGuiTableFlags tableFlags = ImguiStyle::TableFlags;

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
	if ( ImGui::TreeNode( "Features" ) )
	{
		if ( ImGui::BeginTable( "Info", 2, tableFlags ) )
		{
			FeatureBool( robustBufferAccess )
			FeatureBool( fullDrawIndexUint32 )
			FeatureBool( imageCubeArray )
			FeatureBool( independentBlend )
			FeatureBool( geometryShader )
			FeatureBool( tessellationShader )
			FeatureBool( sampleRateShading )
			FeatureBool( dualSrcBlend )
			FeatureBool( logicOp )
			FeatureBool( multiDrawIndirect )
			FeatureBool( drawIndirectFirstInstance )
			FeatureBool( depthClamp )
			FeatureBool( depthBiasClamp )
			FeatureBool( fillModeNonSolid )
			FeatureBool( depthBounds )
			FeatureBool( wideLines )
			FeatureBool( largePoints )
			FeatureBool( alphaToOne )
			FeatureBool( multiViewport )
			FeatureBool( samplerAnisotropy )
			FeatureBool( textureCompressionETC2 )
			FeatureBool( textureCompressionASTC_LDR )
			FeatureBool( textureCompressionBC )
			FeatureBool( occlusionQueryPrecise )
			FeatureBool( pipelineStatisticsQuery )
			FeatureBool( vertexPipelineStoresAndAtomics )
			FeatureBool( fragmentStoresAndAtomics )
			FeatureBool( shaderTessellationAndGeometryPointSize )
			FeatureBool( shaderImageGatherExtended )
			FeatureBool( shaderStorageImageExtendedFormats )
			FeatureBool( shaderStorageImageMultisample )
			FeatureBool( shaderStorageImageReadWithoutFormat )
			FeatureBool( shaderStorageImageWriteWithoutFormat )
			FeatureBool( shaderUniformBufferArrayDynamicIndexing )
			FeatureBool( shaderSampledImageArrayDynamicIndexing )
			FeatureBool( shaderStorageBufferArrayDynamicIndexing )
			FeatureBool( shaderStorageImageArrayDynamicIndexing )
			FeatureBool( shaderClipDistance )
			FeatureBool( shaderCullDistance )
			FeatureBool( shaderFloat64 )
			FeatureBool( shaderInt64 )
			FeatureBool( shaderInt16 )
			FeatureBool( shaderResourceResidency )
			FeatureBool( shaderResourceMinLod )
			FeatureBool( sparseBinding )
			FeatureBool( sparseResidencyBuffer )
			FeatureBool( sparseResidencyImage2D )
			FeatureBool( sparseResidencyImage3D )
			FeatureBool( sparseResidency2Samples )
			FeatureBool( sparseResidency4Samples )
			FeatureBool( sparseResidency8Samples )
			FeatureBool( sparseResidency16Samples )
			FeatureBool( sparseResidencyAliased )
			FeatureBool( variableMultisampleRate )
			FeatureBool( inheritedQueries )
			ImGui::EndTable();
		}
		ImGui::TreePop();
	}
}

#undef LimitUint
#undef LimitDeviceSize
#undef LimitBool
#undef LimitFloat
#undef LimitInt
#undef LimitHex
#undef FeatureBool

#endif