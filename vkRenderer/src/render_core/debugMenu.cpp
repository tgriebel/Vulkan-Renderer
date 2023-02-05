
#include "../globals/common.h"
#include <scene/assetManager.h>
#include <sstream>

extern AssetManager gAssets;

#if defined( USE_IMGUI )
#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../external/imgui/backends/imgui_impl_vulkan.h"
#endif

#if defined( USE_IMGUI )
extern imguiControls_t gImguiControls;
#endif

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
	bool edited = false;
	ImGui::PushItemWidth( defaultWidth );
	edited = edited || ImGui::InputFloat( "##R", &rgb.r, 0.1f, 1.0f );
	ImGui::SameLine();
	edited = edited || ImGui::InputFloat( "##G", &rgb.g, 0.1f, 1.0f );
	ImGui::SameLine();
	edited = edited || ImGui::InputFloat( "##B", &rgb.b, 0.1f, 1.0f );
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
					const char* modelName = gAssets.materialLib.FindName( model.surfs[ s ].materialHdl );
					ImGui::Text( modelName );
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
			ImGui::Text( "Width" );
			ImGui::TableNextColumn();
			ImGui::Text( "%u", texture.info.width );
			ImGui::TableNextRow();

			ImGui::Text( "Height" );
			ImGui::TableNextColumn();
			ImGui::Text( "%u", texture.info.height );
			ImGui::TableNextRow();

			ImGui::Text( "Layers" );
			ImGui::TableNextColumn();
			ImGui::Text( "%u", texture.info.layers );
			ImGui::TableNextRow();

			ImGui::Text( "Channels" );
			ImGui::TableNextColumn();
			ImGui::Text( "%u", texture.info.channels );
			ImGui::TableNextRow();

			ImGui::Text( "Mips" );
			ImGui::TableNextColumn();
			ImGui::Text( "%u", texture.info.mipLevels );
			ImGui::TableNextRow();

			ImGui::Text( "Layers" );
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

			ImGui::Text( "Upload Id" );
			ImGui::TableNextColumn();
			ImGui::Text( "%u", texture.uploadId );
			ImGui::TableNextRow();

			ImGui::Text( "Size(bytes)" );
			ImGui::TableNextColumn();
			ImGui::Text( "%u", texture.sizeBytes );
			ImGui::TableNextRow();

			ImGui::EndTable();
		}

		ImGui::TreePop();
	}
}