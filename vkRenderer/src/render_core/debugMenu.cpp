
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

static bool EditFloat( float& f )
{
	bool edited = false;
	ImGui::PushItemWidth( defaultWidth );
	edited = edited || ImGui::InputFloat( "##editFloat", &f );
	ImGui::PopItemWidth();
	ImGui::PopID();

	return edited;
}


static bool EditRgb( rgbTuplef_t& rgb )
{
	bool edited = false;
	ImGui::PushID( "##editRgbTuple" );
	ImGui::PushItemWidth( defaultWidth );
	edited = edited || ImGui::InputFloat( "##R", &rgb.r, 0.1f, 1.0f );
	ImGui::SameLine();
	edited = edited || ImGui::InputFloat( "##G", &rgb.g, 0.1f, 1.0f );
	ImGui::SameLine();
	edited = edited || ImGui::InputFloat( "##B", &rgb.b, 0.1f, 1.0f );
	ImGui::PopItemWidth();
	ImGui::PopID();

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
#define EditRgbValue( VALUE )	{	ImGui::Text( #VALUE );			\
									ImGui::SameLine();				\
									rgbTuplef_t rgb = mat.##VALUE();\
									if( EditRgb( rgb ) ) {			\
										mat.##VALUE( rgb );			\
									}								\
								}

#define EditFloatValue( VALUE )	{	ImGui::Text( #VALUE );			\
									ImGui::SameLine();				\
									float value = mat.##VALUE();	\
									if( EditFloat( value ) ) {		\
										mat.##VALUE( value );		\
									}								\
								}

	Material& mat = matAsset->Get();
	ImGui::PushID( matAsset->GetName().c_str() );

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
	ImGui::PopID();
}