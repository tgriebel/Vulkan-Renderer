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

extern imguiControls_t			g_imguiControls;
extern Window					g_window;

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
			texture.sizeBytes = texture.info.channels * texture.info.width * texture.info.height;

			texture.bytes = new uint8_t[ texture.sizeBytes ];

			const uint32_t pixelCount = texture.info.width * texture.info.height;
			for ( uint32_t i = 0; i < pixelCount; ++i )
			{
				texture.bytes[ i * 4 + 0 ] = rgba.r;
				texture.bytes[ i * 4 + 1 ] = rgba.g;
				texture.bytes[ i * 4 + 2 ] = rgba.b;
				texture.bytes[ i * 4 + 3 ] = rgba.a;
			}
			
		}
	}

	// Materials
	{
		{
			Material material;
			material.usage = MATERIAL_USAGE_CODE;
			material.AddShader( DRAWPASS_POST_2D, AssetLibGpuProgram::Handle( "PostProcess" ) );
			for ( uint32_t i = 0; i < Material::MaxMaterialTextures; ++i ) {
				material.AddTexture( i, i );
			}
			g_assets.materialLib.Add( "TONEMAP", material );
		}

		{
			Material material;
			material.usage = MATERIAL_USAGE_CODE;
			material.AddShader( DRAWPASS_POST_2D, AssetLibGpuProgram::Handle( "Image2D" ) );
			g_assets.materialLib.Add( "IMAGE2D", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_WIREFRAME, AssetLibGpuProgram::Handle( "Debug" ) );
			g_assets.materialLib.Add( "DEBUG_WIRE", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_SOLID, AssetLibGpuProgram::Handle( "DebugSolid" ) );
			g_assets.materialLib.Add( "DEBUG_SOLID", material );
		}
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
		scene->lights[ 0 ].lightPos = vec4f( 0.0f, 0.0f, 6.0f, 0.0f );
		scene->lights[ 0 ].intensity = vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
		scene->lights[ 0 ].lightDir = vec4f( 0.0f, 0.0f, -1.0f, 0.0f );
		scene->lights[ 0 ].flags = LIGHT_FLAGS_SHADOW;

		scene->lights[ 1 ].lightPos = vec4f( 0.0f, 10.0f, 5.0f, 0.0f );
		scene->lights[ 1 ].intensity = vec4f( 0.5f, 0.5f, 0.5f, 1.0f );
		scene->lights[ 1 ].lightDir = vec4f( 0.0f, 0.0f, 1.0f, 0.0f );
		scene->lights[ 1 ].flags = LIGHT_FLAGS_NONE;

		scene->lights[ 2 ].lightPos = vec4f( 0.0f, -10.0f, 5.0f, 0.0f );
		scene->lights[ 2 ].intensity = vec4f( 0.5f, 0.5f, 0.5f, 1.0f );
		scene->lights[ 2 ].lightDir = vec4f( 0.0f, 0.0f, 1.0f, 0.0f );
		scene->lights[ 2 ].flags = LIGHT_FLAGS_NONE;
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

		const uint32_t pixelCount = texture.info.width * texture.info.height;
		for ( uint32_t i = 0; i < pixelCount; ++i )
		{
			texture.bytes[ i * 4 + 0 ] = rgba.r;
			texture.bytes[ i * 4 + 1 ] = rgba.g;
			texture.bytes[ i * 4 + 2 ] = rgba.b;
			texture.bytes[ i * 4 + 3 ] = rgba.a;
		}
		imageAsset->QueueUpload();
	}
	

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
	else {
		Entity* ent = scene->FindEntity( "_quadTexDebug" );
		if ( ent != nullptr ) {
			ent->SetFlag( ENT_FLAG_NO_DRAW );
		}
	}

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
				GpuProgram& shader = shaderAsset->Get();
				const char* shaderName = shaderAsset->GetName().c_str();
				if ( ImGui::TreeNode( shaderName ) )
				{
					for( uint32_t i = 0; i < shader.shaderCount; ++i )
					{
						ImGui::Text( shader.shaders[ i ].name.c_str() );
						switch( shader.shaders[ i ].type )
						{
							case VERTEX: ImGui::Text( "Vertex" ); break;
							case PIXEL: ImGui::Text( "Pixel" ); break;
							case COMPUTE: ImGui::Text( "Compute" ); break;
						}
						ImGui::Text( "Size: %u", shader.shaders[ i ].blob.size() );
					}
					ImGui::TreePop();
				}
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

			if ( ImGui::Button( "Add Bounds" ) )
			{
				AABB bounds = ent->GetBounds();
				const vec3f X = bounds.GetMax()[ 0 ] - bounds.GetMin()[ 0 ];
				const vec3f Y = bounds.GetMax()[ 1 ] - bounds.GetMin()[ 1 ];
				const vec3f Z = bounds.GetMax()[ 2 ] - bounds.GetMin()[ 2 ];

				//mat4x4f m = CreateMatrix4x4( )

				Entity* boundEnt = new Entity( *ent );
				boundEnt->name = ent->name + "_bounds";
				boundEnt->SetFlag( ENT_FLAG_WIREFRAME );
				boundEnt->materialHdl = g_assets.materialLib.RetrieveHdl( "DEBUG_WIRE" );

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