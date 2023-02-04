
#include "../globals/common.h"
#include <scene/assetManager.h>

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

static void EditFloat( float& f )
{
	ImGui::PushItemWidth( defaultWidth );
	ImGui::InputFloat( "##editFloat", &f );
	ImGui::PopID();
}


static void EditRgb( rgbTuplef_t& rgb )
{
	ImGui::PushID( "##editRgbTuple" );
	ImGui::PushItemWidth( defaultWidth );
	ImGui::InputFloat( "##R", &rgb.r, 0.1f, 1.0f );
	ImGui::SameLine();
	ImGui::InputFloat( "##G", &rgb.g, 0.1f, 1.0f );
	ImGui::SameLine();
	ImGui::InputFloat( "##B", &rgb.b, 0.1f, 1.0f );
	ImGui::PopItemWidth();
	ImGui::PopID();
}


void DebugMenuMaterial( const Material& mat )
{
	ImGui::Text( "Kd: (%1.2f, %1.2f, %1.2f)", mat.Kd.r, mat.Kd.g, mat.Kd.b );
	ImGui::Text( "Ks: (%1.2f, %1.2f, %1.2f)", mat.Ks.r, mat.Ks.g, mat.Ks.b );
	ImGui::Text( "Ke: (%1.2f, %1.2f, %1.2f)", mat.Ke.r, mat.Ke.g, mat.Ke.b );
	ImGui::Text( "Ka: (%1.2f, %1.2f, %1.2f)", mat.Ka.r, mat.Ka.g, mat.Ka.b );
	ImGui::Text( "Ni: %1.2f", mat.Ni );
	ImGui::Text( "Tf: %1.2f", mat.Tf );
	ImGui::Text( "Tr: %1.2f", mat.Tr );
	ImGui::Text( "d: %1.2f", mat.d );
	ImGui::Text( "illum: %1.2f", mat.illum );
	ImGui::Separator();
	for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t )
	{
		hdl_t texHdl = mat.GetTexture( t );
		if ( texHdl.IsValid() == false )
		{
			ImGui::Text( "<None>" );
		}
		else
		{
			const char* texName = gAssets.textureLib.FindName( texHdl );
			ImGui::Text( texName );
		}
	}
}


void DebugMenuMaterialEdit( Material& mat )
{
	ImGui::Text( "Kd:" ); ImGui::SameLine(); EditRgb( mat.Kd );
	ImGui::Text( "Ks:" ); ImGui::SameLine(); EditRgb( mat.Ks );
	ImGui::Text( "Ke:" ); ImGui::SameLine(); EditRgb( mat.Ke );
	ImGui::Text( "Ka:" ); ImGui::SameLine(); EditRgb( mat.Ka );
	ImGui::Text( "Tf:" ); ImGui::SameLine(); EditRgb( mat.Tf );
	ImGui::Text( "Ni:" ); ImGui::SameLine(); EditFloat( mat.Ni );
	ImGui::Text( "Tr:" ); ImGui::SameLine(); EditFloat( mat.Tr );
	ImGui::Text( "illum:" ); ImGui::SameLine(); EditFloat( mat.illum );

	ImGui::Separator();
	for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t )
	{
		hdl_t texHdl = mat.GetTexture( t );
		if ( texHdl.IsValid() == false )
		{
			ImGui::Text( "<None>" );
		}
		else
		{
			const char* texName = gAssets.textureLib.FindName( texHdl );
			ImGui::Text( texName );
		}
	}
}