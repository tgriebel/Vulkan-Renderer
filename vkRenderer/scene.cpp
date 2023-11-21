#include "common.h"
#include <gfxcore/core/util.h>
#include "io.h"
#include "window.h"
#include <gfxcore/scene/entity.h>
#include <gfxcore/scene/scene.h>
#include "src/globals/render_util.h"

#if defined( USE_IMGUI )
#include "external/imgui/imgui.h"
#endif

#include "src/render_core/debugMenu.h"

extern AssetManager g_assets;
extern Scene* g_scene;

#if defined( USE_IMGUI )
extern imguiControls_t			g_imguiControls;
#endif
extern Window					g_window;

void DrawSceneDebugMenu();
void DrawAssetDebugMenu();
void DrawManipDebugMenu();
void DrawEntityDebugMenu();
void DrawOutlinerDebugMenu();
void DeviceDebugMenu();

void CreateCodeAssets()
{
	// Textures
	{
		for( uint32_t t = 0; t < 4; ++t )
		{
			const RGBA rgba = Color( Color::Gold ).AsRGBA();

			std::stringstream ss;
			ss << "CODE_COLOR_" << t;
			std::string s = ss.str();

			hdl_t handle = g_assets.textureLib.Add( s.c_str(), Image() );
			Image& texture = g_assets.textureLib.Find( handle )->Get();

			texture.info.width = 256;
			texture.info.height = 240;
			texture.info.mipLevels = 1;
			texture.info.layers = 1;
			texture.info.type = IMAGE_TYPE_2D;
			texture.info.channels = 4;
			texture.info.fmt = IMAGE_FMT_RGBA_8;
			texture.info.tiling = IMAGE_TILING_MORTON;
			
			texture.cpuImage.Init( texture.info.width, texture.info.height );

			for ( uint32_t y = 0; y < texture.info.height; ++y ) {
				for ( uint32_t x = 0; x < texture.info.width; ++x ) {
					texture.cpuImage.SetPixel( x, y, rgba );
				}
			}			
		}

		const uint32_t debugColorCount = 18;

		const Color colorAlb = Color( 0.5f, 0.5f, 0.5f );
		const Color colorNml = Color( 0.0f, 0.0f, 0.5f );
		const Color colorRgh = Color( 1.0f, 0.0f, 0.0f );
		const Color colorMtl = Color( 0.0f, 0.0f, 0.0f );

		static const Color* colors[ debugColorCount ] =
		{
			&ColorRed,
			&ColorGreen,
			&ColorBlue,
			&ColorWhite,
			&ColorBlack,
			&ColorLGrey,
			&ColorDGrey,
			&ColorBrown,
			&ColorCyan,
			&ColorYellow,
			&ColorPurple,
			&ColorOrange,
			&ColorPink,
			&ColorGold,
			&colorAlb,
			&colorNml,
			&colorRgh,
			&colorMtl,
		};

		const char* names[ debugColorCount ] =
		{
			"_red",
			"_green",
			"_blue",
			"_white",
			"_black",
			"_lightGrey",
			"_darkGrey",
			"_brown",
			"_cyan",
			"_yellow",
			"_purple",
			"_orange",
			"_pink",
			"_gold",
			"_alb",
			"_nml",
			"_rgh",
			"_mtl",
		};

		imageInfo_t info {};
		info.width = 1;
		info.height = 1;
		info.mipLevels = MipCount( info.width, info.height );
		info.layers = 1;
		info.type = IMAGE_TYPE_2D;
		info.channels = 4;
		info.fmt = IMAGE_FMT_RGBA_8;
		info.tiling = IMAGE_TILING_MORTON;
		info.aspect = IMAGE_ASPECT_COLOR_FLAG;
		info.subsamples = IMAGE_SMP_1;

		for ( uint32_t t = 0; t < debugColorCount; ++t )
		{
			hdl_t handle = g_assets.textureLib.Add( names[ t ], Image() );
			Image& texture = g_assets.textureLib.Find( handle )->Get();
			texture.info = info;

			texture.cpuImage.Init( texture.info.width, texture.info.height, colors[ t ]->AsRGBA() );
		}

		// Checkerboard
		{
			hdl_t handle = g_assets.textureLib.Add( "_checkerboard", Image() );
			Image& texture = g_assets.textureLib.Find( handle )->Get();

			texture.info = info;
			texture.info.width = 32;
			texture.info.height = 32;
			texture.info.mipLevels = MipCount( texture.info.width, texture.info.height );

			texture.cpuImage.Init( texture.info.width, texture.info.height );

			for ( uint32_t y = 0; y < texture.info.height; ++y ) {
				for ( uint32_t x = 0; x < texture.info.width; ++x ) {
					if( ( x % 2 ) == ( y % 2 ) ) {
						texture.cpuImage.SetPixel( x, y, ColorBlack.AsRGBA() );
					} else {
						texture.cpuImage.SetPixel( x, y, ColorWhite.AsRGBA() );
					}
				}
			}		
		}
		g_assets.textureLib.SetDefault( "_checkerboard" );
	}

	// Materials
	{
		{
			Material material;
			material.usage = MATERIAL_USAGE_CODE;
			material.AddShader( DRAWPASS_2D, AssetLibGpuProgram::Handle( "PostProcess" ) );
			for ( uint32_t i = 0; i < Material::MaxMaterialTextures; ++i ) {
				material.AddTexture( i, i );
			}
			g_assets.materialLib.Add( "TONEMAP", material );
		}

		{
			Material material;
			material.usage = MATERIAL_USAGE_CODE;
			material.AddShader( DRAWPASS_2D, AssetLibGpuProgram::Handle( "Image2D" ) );
			g_assets.materialLib.Add( "IMAGE2D", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_WIREFRAME, AssetLibGpuProgram::Handle( "Debug" ) );
			g_assets.materialLib.Add( "DEBUG_WIRE", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_3D, AssetLibGpuProgram::Handle( "DebugSolid" ) );
			g_assets.materialLib.Add( "DEBUG_3D", material );
		}
		g_assets.materialLib.SetDefault( "DEBUG_WIRE" );
	}

	// Models
	{
		{
			Model model;
			CreateQuadSurface2D( "TONEMAP", model, vec2f( 1.0f, 1.0f ), vec2f( 2.0f ) );
			g_assets.modelLib.Add( "_postProcessQuad", model );
		}
		{
			Model model;
			CreateQuadSurface2D( "IMAGE2D", model, vec2f( 1.0f, 1.0f ), vec2f( 1.0f * ( 9.0 / 16.0f ), 1.0f ) );
			g_assets.modelLib.Add( "_quadTexDebug", model );
		}
		g_assets.modelLib.SetDefault( "_quadTexDebug" );
	}
}


void InitScene( Scene* scene )
{
	// FIXME: weird left-over stuff from refactors
	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	for ( uint32_t i = 0; i < entCount; ++i ) {
		scene->CreateEntityBounds( scene->entities[ i ]->modelHdl, *scene->entities[ i ] );
	}

	scene->Init();

	{
		Entity* ent = new Entity();
		scene->CreateEntityBounds( g_assets.modelLib.RetrieveHdl( "_postProcessQuad" ), *ent );
		ent->name = "_postProcessQuad";
		scene->entities.push_back( ent );
	}

	{
		Entity* ent = new Entity();
		scene->CreateEntityBounds( g_assets.modelLib.RetrieveHdl( "_quadTexDebug" ), *ent );
		ent->name = "_quadTexDebug";
		scene->entities.push_back( ent );
	}

	{
		scene->lights.resize( 3 );
		scene->lights[ 0 ].pos = vec4f( 0.0f, 0.0f, 6.0f, 0.0f );
		scene->lights[ 0 ].intensity = 1.0f;
		scene->lights[ 0 ].dir = vec4f( 0.0f, 0.0f, -1.0f, 0.0f );
		scene->lights[ 0 ].color = Color::White;
		scene->lights[ 0 ].flags = LIGHT_FLAGS_SHADOW;

		scene->lights[ 1 ].pos = vec4f( 0.0f, 10.0f, 5.0f, 0.0f );
		scene->lights[ 1 ].intensity = 1.0f;
		scene->lights[ 1 ].dir = vec4f( 0.0f, 0.0f, -1.0f, 0.0f );
		scene->lights[ 1 ].color = Color::Red;
		scene->lights[ 1 ].flags = LIGHT_FLAGS_SHADOW;

		scene->lights[ 2 ].pos = vec4f( 0.0f, -10.0f, 5.0f, 0.0f );
		scene->lights[ 2 ].intensity = 1.0f;
		scene->lights[ 2 ].dir = vec4f( 0.0f, 0.0f, -1.0f, 0.0f );
		scene->lights[ 2 ].color = Color::Blue;
		scene->lights[ 2 ].flags = LIGHT_FLAGS_SHADOW;
	}
}


void ShutdownScene( Scene* scene )
{
	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	for( uint32_t i = 0; i < entCount; ++i )
	{
		delete scene->entities[i];
	}
}


void UpdateScene( Scene* scene )
{

#if defined( USE_IMGUI )
	ImGui::NewFrame();
#endif

	const float dt = scene->DeltaTime();
	const float cameraSpeed = 5.0f;

	if( g_window.IsFocused() == false )
	{
		// FIXME: race conditions
		// Need to do a ping-pong update
		if ( g_window.input.IsKeyPressed( 'D' ) ) {
			scene->camera.MoveRight( cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( 'A' ) ) {
			scene->camera.MoveRight( -cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( 'W' ) ) {
			scene->camera.MoveForward( cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( 'S' ) ) {
			scene->camera.MoveForward( -cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( '8' ) ) {
			scene->camera.AdjustPitch( -cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( '2' ) ) {
			scene->camera.AdjustPitch( cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( '4' ) ) {
			scene->camera.AdjustYaw( cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( '6' ) ) {
			scene->camera.AdjustYaw( -cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( '+' ) ) {
			scene->camera.SetFov( scene->camera.GetFov() + Radians( 0.1f ) );
		}
		if ( g_window.input.IsKeyPressed( '-' ) ) {
			scene->camera.SetFov( scene->camera.GetFov() - Radians( 0.1f ) );
		}
	}
	scene->camera.SetAspectRatio( g_window.GetWindowFrameBufferAspect() );

	const mouse_t& mouse = g_window.input.GetMouse();
	if ( mouse.centered )
	{
		const float maxSpeed = mouse.speed;
		const float yawDelta = maxSpeed * mouse.dx;
		const float pitchDelta = -maxSpeed * mouse.dy;
		scene->camera.AdjustYaw( yawDelta );
		scene->camera.AdjustPitch( pitchDelta );
	}
	else if ( mouse.leftDown )
	{
		Ray ray = scene->camera.GetViewRay( vec2f( 0.5f * mouse.x + 0.5f, 0.5f * mouse.y + 0.5f ) );
		scene->selectedEntity = scene->GetTracedEntity( ray );
	}

	// Skybox
	vec3f skyBoxOrigin;
	skyBoxOrigin[ 0 ] = scene->camera.GetOrigin()[ 0 ];
	skyBoxOrigin[ 1 ] = scene->camera.GetOrigin()[ 1 ];
	skyBoxOrigin[ 2 ] = scene->camera.GetOrigin()[ 2 ] - 0.5f;
	( scene->FindEntity( "_skybox" ) )->SetOrigin( skyBoxOrigin );

	
	if(0)
	{
		Color randomColor( Random(), Random(), Random(), 1.0f );
		const RGBA rgba = Color( randomColor ).AsRGBA();
		Asset<Image>* imageAsset = g_assets.textureLib.Find( "CODE_COLOR_0" );
		Image& texture = imageAsset->Get();

		texture.cpuImage.Init( texture.info.width, texture.info.height );

		for ( uint32_t y = 0; y < texture.info.height; ++y ) {
			for ( uint32_t x = 0; x < texture.info.width; ++x ) {
				texture.cpuImage.SetPixel( x, y, rgba );
			}
		}

		imageAsset->QueueUpload();
	}
	

#if defined( USE_IMGUI )
	if ( g_imguiControls.dbgImageId >= 0 )
	{
		Entity* ent = scene->FindEntity( "_quadTexDebug" );
		if( ent != nullptr )
		{
			ent->ClearFlag( ENT_FLAG_NO_DRAW );
			Asset<Material>* matAsset = g_assets.materialLib.Find( "IMAGE2D" );
			if( matAsset != nullptr )
			{
				Material& mat = matAsset->Get();
				mat.AddTexture( 0, g_imguiControls.dbgImageId );
			}
		}
	}
	else
#endif
	{
		Entity* ent = scene->FindEntity( "_quadTexDebug" );
		if ( ent != nullptr ) {
			ent->SetFlag( ENT_FLAG_NO_DRAW );
		}
	}

#if defined( USE_IMGUI )
	if ( ImGui::BeginMainMenuBar() )
	{
		if ( ImGui::BeginMenu( "File" ) )
		{
			if ( ImGui::MenuItem( "Open Scene", "CTRL+O" ) ) {
				g_imguiControls.openSceneFileDialog = true;
			}
			if ( ImGui::MenuItem( "Reload", "CTRL+R" ) ) {
				g_imguiControls.reloadScene = true;
			}
			if ( ImGui::MenuItem( "Import Obj", "CTRL+I" ) ) {
				g_imguiControls.openModelImportFileDialog = true;
			}
			ImGui::EndMenu();
		}
		if ( ImGui::BeginMenu( "Edit" ) )
		{
			if ( ImGui::MenuItem( "Undo", "CTRL+Z" ) ) {}
			if ( ImGui::MenuItem( "Redo", "CTRL+Y", false, false ) ) {}  // Disabled item
			ImGui::Separator();
			if ( ImGui::MenuItem( "Cut", "CTRL+X" ) ) {}
			if ( ImGui::MenuItem( "Copy", "CTRL+C" ) ) {}
			if ( ImGui::MenuItem( "Paste", "CTRL+V" ) ) {}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	ImGui::Begin( "Control Panel" );

	if ( ImGui::BeginTabBar( "Tabs" ) )
	{
		DrawSceneDebugMenu();
		DrawAssetDebugMenu();
		DrawManipDebugMenu();
		DrawEntityDebugMenu();
		DrawOutlinerDebugMenu();
		DeviceDebugMenu();

		ImGui::EndTabBar();
	}

	ImGui::InputInt( "Image Id", &g_imguiControls.dbgImageId );
	g_imguiControls.dbgImageId = Clamp( g_imguiControls.dbgImageId, -1, int( g_assets.textureLib.Count() - 1 ) );

	char entityName[ 256 ];
	if ( g_imguiControls.selectedEntityId >= 0 ) {
		sprintf_s( entityName, "%i: %s", g_imguiControls.selectedEntityId, g_assets.modelLib.FindName( g_scene->entities[ g_imguiControls.selectedEntityId ]->modelHdl ) );
	}
	else {
		memset( &entityName[ 0 ], 0, 256 );
	}

	ImGui::Text( "Mouse: (%f, %f)", (float)g_window.input.GetMouse().x, (float)g_window.input.GetMouse().y );
	ImGui::Text( "Mouse Dt: (%f, %f)", (float)g_window.input.GetMouse().dx, (float)g_window.input.GetMouse().dy );
	const vec4f cameraOrigin = g_scene->camera.GetOrigin();
	ImGui::Text( "Camera: (%f, %f, %f)", cameraOrigin[ 0 ], cameraOrigin[ 1 ], cameraOrigin[ 2 ] );

	const vec2f ndc = g_window.GetNdc( g_window.input.GetMouse().x, g_window.input.GetMouse().y );

	ImGui::Text( "NDC: (%f, %f )", (float)ndc[ 0 ], (float)ndc[ 1 ] );
	ImGui::Text( "Frame Number: %d", g_renderDebugData.frameNumber );
	ImGui::SameLine();
	ImGui::Text( "FPS: %f", 1000.0f / g_renderDebugData.frameTimeMs );

	ImGui::End();
#endif

	scene->Update();
}


void DrawSceneDebugMenu()
{
#if defined( USE_IMGUI )
	if ( ImGui::BeginTabItem( "Debug" ) )
	{
		g_imguiControls.rebuildShaders = ImGui::Button( "Reload Shaders" );
		g_imguiControls.rebuildRaytraceScene = ImGui::Button( "Rebuild Raytrace Scene" );
		ImGui::SameLine();
		g_imguiControls.raytraceScene = ImGui::Button( "Raytrace Scene" );
		ImGui::SameLine();
		g_imguiControls.rasterizeScene = ImGui::Button( "Rasterize Scene" );

		ImGui::InputFloat( "Heightmap Height", &g_imguiControls.heightMapHeight, 0.1f, 1.0f );
		ImGui::SliderFloat( "Roughness", &g_imguiControls.roughness, 0.1f, 1.0f );
		ImGui::SliderFloat( "Shadow Strength", &g_imguiControls.shadowStrength, 0.0f, 1.0f );
		ImGui::InputFloat( "Tone Map R", &g_imguiControls.toneMapColor[ 0 ], 0.1f, 1.0f );
		ImGui::InputFloat( "Tone Map G", &g_imguiControls.toneMapColor[ 1 ], 0.1f, 1.0f );
		ImGui::InputFloat( "Tone Map B", &g_imguiControls.toneMapColor[ 2 ], 0.1f, 1.0f );
		ImGui::InputFloat( "Tone Map A", &g_imguiControls.toneMapColor[ 3 ], 0.1f, 1.0f );
		ImGui::EndTabItem();
	}
#endif
}


void DrawAssetDebugMenu()
{
#if defined( USE_IMGUI )
	static ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

	if ( ImGui::BeginTabItem( "Assets" ) )
	{
		const uint32_t matCount = g_assets.materialLib.Count();
		if ( ImGui::TreeNode( "Materials", "Materials (%i)", matCount ) )
		{
			for ( uint32_t m = 0; m < matCount; ++m )
			{
				Asset<Material>* matAsset = g_assets.materialLib.Find( m );
				Material& mat = matAsset->Get();
				const char* matName = g_assets.materialLib.FindName( m );

				if ( ImGui::TreeNode( matAsset->GetName().c_str() ) )
				{
					DebugMenuMaterialEdit( matAsset );
					ImGui::Separator();
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		const uint32_t modelCount = g_assets.modelLib.Count();
		if ( ImGui::TreeNode( "Models", "Models (%i)", modelCount ) )
		{
			for ( uint32_t m = 0; m < modelCount; ++m )
			{
				Asset<Model>* modelAsset = g_assets.modelLib.Find( m );
				DebugMenuModelTreeNode( modelAsset );
			}
			ImGui::TreePop();
		}
		const uint32_t texCount = g_assets.textureLib.Count();
		if ( ImGui::TreeNode( "Textures", "Textures (%i)", texCount ) )
		{
			for ( uint32_t t = 0; t < texCount; ++t )
			{
				Asset<Image>* texAsset = g_assets.textureLib.Find( t );
				DebugMenuTextureTreeNode( texAsset );
			}
			ImGui::TreePop();
		}

		const uint32_t shaderCount = g_assets.gpuPrograms.Count();
		if ( ImGui::TreeNode( "Shaders", "Shaders (%i)", shaderCount ) )
		{
			for ( uint32_t s = 0; s < shaderCount; ++s )
			{
				Asset<GpuProgram>* shaderAsset = g_assets.gpuPrograms.Find( s );
				DebugMenuShaderTreeNode( shaderAsset );
			}
			ImGui::TreePop();
		}
		ImGui::EndTabItem();
	}
#endif
}


void DrawManipDebugMenu()
{
#if defined( USE_IMGUI )
	if ( ImGui::BeginTabItem( "Manip" ) )
	{
		static uint32_t currentIdx = 0;
		Entity* ent = g_scene->FindEntity( currentIdx );
		const char* previewValue = ent->name.c_str();
		if ( ImGui::BeginCombo( "Entity", previewValue ) )
		{
			const uint32_t modelCount = g_assets.modelLib.Count();
			for ( uint32_t e = 0; e < g_scene->EntityCount(); ++e )
			{
				Entity* comboEnt = g_scene->FindEntity( e );

				const bool selected = ( currentIdx == e );
				if ( ImGui::Selectable( comboEnt->name.c_str(), selected ) ) {
					currentIdx = e;
				}

				if ( selected ) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		if ( ent != nullptr )
		{
			const vec3f o = ent->GetOrigin();
			const vec3f s = ent->GetScale();
			const mat4x4f r = ent->GetRotation();
			float origin[ 3 ] = { o[ 0 ], o[ 1 ], o[ 2 ] };
			ImGui::PushItemWidth( 100 );
			ImGui::Text( "Origin" );
			ImGui::SameLine();
			ImGui::InputFloat( "##OriginX", &origin[ 0 ], 0.1f, 1.0f );
			ImGui::SameLine();
			ImGui::InputFloat( "##OriginY", &origin[ 1 ], 0.1f, 1.0f );
			ImGui::SameLine();
			ImGui::InputFloat( "##OriginZ", &origin[ 2 ], 0.1f, 1.0f );

			float scale[ 3 ] = { s[ 0 ], s[ 1 ], s[ 2 ] };
			ImGui::Text( "Scale" );
			ImGui::SameLine();
			ImGui::InputFloat( "##ScaleX", &scale[ 0 ], 0.1f, 1.0f );
			ImGui::SameLine();
			ImGui::InputFloat( "##ScaleY", &scale[ 1 ], 0.1f, 1.0f );
			ImGui::SameLine();
			ImGui::InputFloat( "##ScaleZ", &scale[ 2 ], 0.1f, 1.0f );

			float rotation[ 3 ] = { 0.0f, 0.0f, 0.0f };
			MatrixToEulerZYX( r, rotation[ 0 ], rotation[ 1 ], rotation[ 2 ] );

			ImGui::Text( "Rotation" );
			ImGui::SameLine();
			ImGui::InputFloat( "##RotationX", &rotation[ 0 ], 1.0f, 10.0f );
			ImGui::SameLine();
			ImGui::InputFloat( "##RotationY", &rotation[ 1 ], 1.0f, 10.0f );
			ImGui::SameLine();
			ImGui::InputFloat( "##RotationZ", &rotation[ 2 ], 1.0f, 10.0f );
			ImGui::PopItemWidth();

			ent->SetOrigin( origin );
			ent->SetScale( scale );
			ent->SetRotation( rotation );

			if ( ImGui::Button( "Add OBB" ) )
			{
				AABB bounds = ent->GetLocalBounds();
				const vec3f boundScale = 0.5f * ( bounds.GetMax() - bounds.GetMin() );
				const vec3f boundCenter = vec3f( ent->GetMatrix() * vec4f( bounds.GetCenter(), 1.0f ) );

				Entity* boundEnt = new Entity( *ent );
				boundEnt->name = ent->name + "_bounds";
				boundEnt->SetFlag( ENT_FLAG_WIREFRAME );
				boundEnt->materialHdl = g_assets.materialLib.RetrieveHdl( "DEBUG_WIRE" );
				boundEnt->SetOrigin( boundCenter );
				boundEnt->SetScale( Multiply( vec3f( scale ), vec3f( boundScale[ 0 ], boundScale[ 1 ], boundScale[ 2 ] ) ) );
				boundEnt->SetRotation( rotation );

				g_scene->entities.push_back( boundEnt );
				g_scene->CreateEntityBounds( g_assets.modelLib.RetrieveHdl( "cube" ), *boundEnt );
			}

			ImGui::SameLine();
			if ( ImGui::Button( "Export Model" ) )
			{
				Asset<Model>* asset = g_assets.modelLib.Find( ent->modelHdl );
				WriteModel( asset, BakePath + asset->GetName() + BakedModelExtension );
			}
			ImGui::SameLine();
			bool hidden = ent->HasFlag( ENT_FLAG_NO_DRAW );
			if ( ImGui::Checkbox( "Hide", &hidden ) )
			{
				if ( hidden ) {
					ent->SetFlag( ENT_FLAG_NO_DRAW );
				}
				else {
					ent->ClearFlag( ENT_FLAG_NO_DRAW );
				}
			}
			ImGui::SameLine();
			bool wireframe = ent->HasFlag( ENT_FLAG_WIREFRAME );
			if ( ImGui::Checkbox( "Wireframe", &wireframe ) )
			{
				if ( wireframe ) {
					ent->SetFlag( ENT_FLAG_WIREFRAME );
				}
				else {
					ent->ClearFlag( ENT_FLAG_WIREFRAME );
				}
			}
		}
		ImGui::EndTabItem();
	}
#endif
}


void DrawEntityDebugMenu()
{
#if defined( USE_IMGUI )
	if ( ImGui::BeginTabItem( "Create Entity" ) )
	{
		static char name[ 128 ] = {};
		static uint32_t currentIdx = 0;
		const char* previewValue = g_assets.modelLib.FindName( currentIdx );
		if ( ImGui::BeginCombo( "Model", previewValue ) )
		{
			const uint32_t modelCount = g_assets.modelLib.Count();
			for ( uint32_t m = 0; m < modelCount; ++m )
			{
				Asset<Model>* modelAsset = g_assets.modelLib.Find( m );

				const bool selected = ( currentIdx == m );
				if ( ImGui::Selectable( modelAsset->GetName().c_str(), selected ) ) {
					currentIdx = m;
				}

				if ( selected ) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::InputText( "Name", name, 128 );

		if ( ImGui::Button( "Create" ) )
		{
			Entity* ent = new Entity();
			ent->name = name;
			ent->SetFlag( ENT_FLAG_DEBUG );
			g_scene->entities.push_back( ent );
			g_scene->CreateEntityBounds( g_assets.modelLib.RetrieveHdl( g_assets.modelLib.FindName( currentIdx ) ), *ent );
		}

		ImGui::EndTabItem();
	}
#endif
}


void DrawOutlinerDebugMenu()
{
#if defined( USE_IMGUI )
	if ( ImGui::BeginTabItem( "Outliner" ) )
	{
		static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

		DebugMenuLightEdit( g_scene );

		if ( ImGui::BeginTable( "3ways", 3, flags ) )
		{
			// The first column will use the default _WidthStretch when ScrollX is Off and _WidthFixed when ScrollX is On
			ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthFixed, 50.0f );
			ImGui::TableSetupColumn( "Size", ImGuiTableColumnFlags_WidthFixed, 200.0f );
			ImGui::TableSetupColumn( "Type", ImGuiTableColumnFlags_WidthFixed, 200.0f );
			ImGui::TableHeadersRow();

			//const uint32_t entCount = scene.entities.size();
			//for ( uint32_t i = 0; i < entCount; ++i )
			//{
			//	Entity* ent = scene.entities[ i ];

			//	std::stringstream numberStream;
			//	numberStream << i;
			//	ImGui::Text( numberStream.str().c_str() );
			//	ImGui::NextColumn();

			//	ImGui::Text( ent->dbgName.c_str() );
			//	ImGui::NextColumn();
			//}

			struct EntityTreeNode
			{
				const char* Name;
				const char* Type;
				int			Size;
				int			ChildIdx;
				int			ChildCount;
				static void DisplayNode( const EntityTreeNode* node, const EntityTreeNode* all_nodes )
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					const bool is_folder = ( node->ChildCount > 0 );
					if ( is_folder )
					{
						bool open = ImGui::TreeNodeEx( node->Name, ImGuiTreeNodeFlags_SpanFullWidth );
						ImGui::TableNextColumn();
						ImGui::TextDisabled( "--" );
						ImGui::TableNextColumn();
						ImGui::TextUnformatted( node->Type );
						if ( open )
						{
							for ( int child_n = 0; child_n < node->ChildCount; child_n++ )
								DisplayNode( &all_nodes[ node->ChildIdx + child_n ], all_nodes );
							ImGui::TreePop();
						}
					}
					else
					{
						ImGui::TreeNodeEx( node->Name, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth );
						ImGui::TableNextColumn();
						ImGui::Text( "%d", node->Size );
						ImGui::TableNextColumn();
						ImGui::TextUnformatted( node->Type );
					}
				}
			};
			static const EntityTreeNode nodes[] =
			{
				{ "Root",                         "Folder",       -1,       1, 3    }, // 0
				{ "Music",                        "Folder",       -1,       4, 2    }, // 1
				{ "Textures",                     "Folder",       -1,       6, 3    }, // 2
				{ "desktop.ini",                  "System file",  1024,    -1,-1    }, // 3
				{ "File1_a.wav",                  "Audio file",   123000,  -1,-1    }, // 4
				{ "File1_b.wav",                  "Audio file",   456000,  -1,-1    }, // 5
				{ "Image001.png",                 "Image file",   203128,  -1,-1    }, // 6
				{ "Copy of Image001.png",         "Image file",   203256,  -1,-1    }, // 7
				{ "Copy of Image001 (Final2).png","Image file",   203512,  -1,-1    }, // 8
			};

			EntityTreeNode::DisplayNode( &nodes[ 0 ], nodes );

			ImGui::EndTable();
		}
		ImGui::EndTabItem();
	}
#endif
}