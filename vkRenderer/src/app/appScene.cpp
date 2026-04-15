#include "../globals/common.h"
#include <gfxcore/core/util.h>
#include "io.h"
#include "window.h"
#include "../scene/entity.h"
#include "../scene/sceneBase.h"
#include "../globals/render_util.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/imgui_internal.h"
#include "../../../external/imgui/imgui.h"
#endif

#include "../render_core/debugMenu.h"
#include "../render_core/renderer.h"
#include "../render_binding/allocator.h"
#include "../globals/assetDefs.h"
extern Scene* g_scene;
extern Renderer g_renderer;

#if defined( USE_IMGUI )
extern imguiControls_t g_imguiControls;
extern void AddImguiCallback( ImDrawList* dl, const imguiImageCallbackData_t& callbackData );
#endif
extern Window					g_window;

void DrawSceneDebugMenu();
void DrawAssetDebugMenu();
void DrawManipDebugMenu();
void DrawEntityDebugMenu();
void DrawOutlinerDebugMenu();
void DrawDrawGroupDebugMenu();
void DeviceDebugMenu();

void CreateCodeAssets()
{
	// ----------------- TEXTURES ----------------- //
	{
		for( uint32_t t = 0; t < 4; ++t )
		{
			const rgba8_t rgba = Color( Color::Gold ).AsRgba8();

			std::stringstream ss;
			ss << "CODE_COLOR_" << t;
			std::string s = ss.str();

			hdl_t handle = TextureLib().Add( s.c_str(), Image() );
			Image& texture = TextureLib().Find( handle )->Get();

			imageInfo_t info = DefaultImage2dInfo( 256, 240 );

			texture.Create( info );

			ImageBuffer<rgba8_t>* imageBuffer = reinterpret_cast<ImageBuffer<rgba8_t>*>( texture.cpuImage );

			for ( uint32_t y = 0; y < texture.info.height; ++y ) {
				for ( uint32_t x = 0; x < texture.info.width; ++x ) {
					imageBuffer->SetPixel( x, y, rgba );
				}
			}		
		}

		// Solid Colors
		{
			const uint32_t debugColorCount = 18;

			struct dbgColorImageInfo_t
			{
				char*		name;
				Color		color;
				imageFmt_t	format;
			};

			static const dbgColorImageInfo_t colorInfo[ debugColorCount ] =
			{
				{ "_red",		ColorRed,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_green",		ColorGreen,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_blue",		ColorBlue,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_white",		ColorWhite,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_black",		ColorBlack,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_lightGrey",	ColorLGrey,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_darkGrey",	ColorDGrey,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_brown",		ColorBrown,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_cyan",		ColorCyan,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_yellow",	ColorYellow,				imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_purple",	ColorPurple,				imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_orange",	ColorOrange,				imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_pink",		ColorPink,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_gold",		ColorGold,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_alb",		Color( 1.0f, 1.0f, 1.0f ),	imageFmt_t::IMAGE_FMT_RGBA_8_UNORM },
				{ "_nml",		Color( 0.5f, 0.5f, 1.0f ),	imageFmt_t::IMAGE_FMT_RGBA_8_UNORM },
				{ "_rgh",		Color( 1.0f, 0.0f, 0.0f ),	imageFmt_t::IMAGE_FMT_RGBA_8_UNORM },
				{ "_mtl",		Color( 0.6f, 0.0f, 0.0f ),	imageFmt_t::IMAGE_FMT_RGBA_8_UNORM },
			};

			imageInfo_t defaultInfo = DefaultImage2dInfo( 1, 1 );

			for ( uint32_t t = 0; t < debugColorCount; ++t )
			{
				hdl_t handle = TextureLib().Add( colorInfo[ t ].name, Image() );
				Image& texture = TextureLib().Find( handle )->Get();
			
				rgba8_t pixel = Swizzle( colorInfo[ t ].color.AsRgba8(), RGBA_A, RGBA_B, RGBA_G, RGBA_R );

				defaultInfo.fmt = colorInfo[ t ].format;

				texture.Create( defaultInfo, (uint8_t*)&pixel, sizeof( rgba8_t ) );
			}
		}

		// Default Image - Checkerboard
		{
			hdl_t handle = TextureLib().Add( "_default", Image() );
			Image& texture = TextureLib().Find( handle )->Get();

			const uint32_t cellSize = 16;

			imageInfo_t info = DefaultImage2dInfo( 128, 128 );

			texture.Create( info );

			ImageBuffer<rgba8_t>* imageBuffer = reinterpret_cast<ImageBuffer<rgba8_t>*>( texture.cpuImage );

			for ( uint32_t y = 0; y < info.height; ++y )
			{
				const uint32_t cellY = y / cellSize;
				for ( uint32_t x = 0; x < info.width; ++x )
				{
					const uint32_t cellX = x / cellSize;
					const float cellGradient = static_cast<float>( Max( 4, Max( abs(int( x % cellSize ) - 8), abs( int( y % cellSize ) - 8 ) ) ) / (0.5f * cellSize) );
					const Color color = ( ( cellX % 2 ) == ( cellY % 2 ) ) ? Lerp( ColorBlack, ColorLGrey, cellGradient ) : Lerp( ColorDGrey, ColorWhite, cellGradient );
					const rgba8_t pixel = Swizzle( color.AsRgba8(), RGBA_A, RGBA_B, RGBA_G, RGBA_R );
					imageBuffer->SetPixel( x, y, pixel );
				}
			}
		}
		TextureLib().SetDefault( "_default" );

		// Default Image Cube - Rainbow
		{
			hdl_t handle = TextureLib().Add( "_defaultCube", Image() );
			Image& texture = TextureLib().Find( handle )->Get();

			imageInfo_t info = DefaultImage2dInfo( 1, 1 );
			info.width = 8;
			info.height = 8;
			info.layers = 6;
			info.type = imageType_t::IMAGE_TYPE_CUBE;

			const Color* colors[ 6 ] = {
				&ColorYellow,
				&ColorGreen,
				&ColorBlue,
				&ColorCyan,
				&ColorRed,
				&ColorPink
			};

			texture.Create( info );

			ImageBuffer<rgba8_t>* imageBuffer = reinterpret_cast<ImageBuffer<rgba8_t>*>( texture.cpuImage );

			for ( uint32_t faceId = 0; faceId < 6; ++faceId ) {
				const Color* color = colors[ faceId ];
				for ( uint32_t y = 0; y < info.height; ++y ) {
					for ( uint32_t x = 0; x < info.width; ++x )
					{			
						const rgba8_t pixel = Swizzle( color->AsRgba8(), RGBA_A, RGBA_B, RGBA_G, RGBA_R );
						imageBuffer->SetPixel( x, y, faceId, pixel );
					}
				}
			}
		}
	}

	// ----------------- MATERIALS ----------------- //
	{
		{
			Material material;
			material.usage = MATERIAL_USAGE_CODE;
			material.AddShader( DRAWPASS_2D, AssetLibGpuProgram::Handle( "PostProcess" ) );
			for ( uint32_t i = 0; i < Material::MaxMaterialTextures; ++i ) {
				material.AddTexture( i, i );
			}
			MaterialLib().Add( "TONEMAP", material );
		}

		{
			Material material;
			material.usage = MATERIAL_USAGE_CODE;
			material.AddShader( DRAWPASS_DEBUG_2D, AssetLibGpuProgram::Handle( "Image2D" ) );
			MaterialLib().Add( "IMAGE2D", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_WIREFRAME, AssetLibGpuProgram::Handle( "Debug" ) );
			MaterialLib().Add( "DEBUG_WIRE", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_3D, AssetLibGpuProgram::Handle( "DebugSolid" ) );
			MaterialLib().Add( "DEBUG_3D", material );
		}
		MaterialLib().SetDefault( "DEBUG_WIRE" );
	}

	// ----------------- MODELS ----------------- //
	{
		{
			Model model;
			CreateQuadSurface2D( "TONEMAP", model, vec2f( 0.0f, 0.0f ), vec2f( 2.0f ) );
			ModelLib().Add( "_postProcessQuad", model );
		}
		{
			Model model;
			CreateQuadSurface2D( "IMAGE2D", model, vec2f( 0.0f, 0.0f ), vec2f( 1.0f, 1.0f ) );
			ModelLib().Add( "_quadTexDebug", model );
		}
		ModelLib().SetDefault( "_quadTexDebug" );
	}
}


void InitScene( Scene* scene )
{
	// FIXME: weird left-over stuff from refactors
	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	for ( uint32_t i = 0; i < entCount; ++i ) {
		scene->CreateEntityBounds( scene->entities[ i ]->modelHdl, *scene->entities[ i ] );

		scene->entities[ i ]->envMap = TextureLib().RetrieveHdl( scene->envMap.c_str() );
		scene->entities[ i ]->diffuseIblMap = TextureLib().RetrieveHdl( scene->diffuseIblMap.c_str() );
	}

	scene->Init();

	scene->mainCamera->Translate( vec4f( 0.0f, 1.66f, 1.0f, 0.0f ) );

	{
		Entity* ent = new Entity();
		scene->CreateEntityBounds( ModelLib().RetrieveHdl( "_postProcessQuad" ), *ent );
		ent->name = "_postProcessQuad";
		scene->entities.push_back( ent );
	}

	{
		Entity* ent = new Entity();
		scene->CreateEntityBounds( ModelLib().RetrieveHdl( "_quadTexDebug" ), *ent );
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
		scene->lights[ 1 ].flags = LIGHT_FLAGS_SHADOW | LIGHT_FLAGS_POINT;

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

	static float smoothDeltaTime = 0.0f;
	smoothDeltaTime = 0.9f * smoothDeltaTime + 0.1f * dt;

	// Key controls
	{
		if ( g_window.input.IsKeyPressed( KEY_D ) ) {
			scene->mainCamera->Truck( cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( KEY_A ) ) {
			scene->mainCamera->Truck( -cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( KEY_W ) ) {
			scene->mainCamera->Dolly( cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( KEY_S ) ) {
			scene->mainCamera->Dolly( -cameraSpeed * dt );
		}
		if ( g_window.input.IsKeyPressed( KEY_1 ) )
		{
			scene->mainCamera->SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
			scene->mainCamera->Pan( 0.0f * PI );
		}
		if ( g_window.input.IsKeyPressed( KEY_2 ) )
		{
			scene->mainCamera->SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
			scene->mainCamera->Pan( 0.5f * PI );
		}
		if ( g_window.input.IsKeyPressed( KEY_3 ) )
		{
			scene->mainCamera->SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
			scene->mainCamera->Pan( 1.0f * PI );
		}
		if ( g_window.input.IsKeyPressed( KEY_4 ) )
		{
			scene->mainCamera->SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
			scene->mainCamera->Pan( 1.5f * PI );
		}
		if ( g_window.input.IsKeyPressed( KEY_5 ) )
		{
			scene->mainCamera->SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
			scene->mainCamera->Tilt( -0.5f * PI );
		}
		if ( g_window.input.IsKeyPressed( KEY_6 ) )
		{
			scene->mainCamera->SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
			scene->mainCamera->Tilt( 0.5f * PI );
		}
		if ( g_window.input.IsKeyPressed( KEY_ADD ) ) {
			scene->mainCamera->SetFov( scene->mainCamera->GetFov() + Radians( 0.1f ), g_window.QueryWindowFrameBufferAspect() );
		}
		if ( g_window.input.IsKeyPressed( KEY_SUB ) ) {
			scene->mainCamera->SetFov( scene->mainCamera->GetFov() - Radians( 0.1f ), g_window.QueryWindowFrameBufferAspect() );
		}
	}
	scene->mainCamera->SetFov( scene->mainCamera->GetFov(), g_window.QueryWindowFrameBufferAspect() );

	const mouse_t& mouse = g_window.input.GetMouse();
	if ( mouse.centered )
	{
		const float maxSpeed = mouse.speed * smoothDeltaTime;
		const float yawDelta = -maxSpeed * mouse.dx;
		const float pitchDelta = maxSpeed * mouse.dy;
		scene->mainCamera->Pan( yawDelta );
		scene->mainCamera->Tilt( pitchDelta );
	}
	else if ( mouse.leftDown )
	{
		Ray ray = scene->mainCamera->GetViewRay( vec2f( 0.5f * mouse.x + 0.5f, 0.5f * mouse.y + 0.5f ) );
		scene->selectedEntity = scene->GetTracedEntity( ray );
	}

	scene->camera2D.SetAngles( vec3f( 0.0f, 0.0f, 0.0f ) );
	scene->camera2D.SetClip( -1.0f, 1.0f );

	const uint32_t entCount = static_cast<uint32_t>( scene->entities.size() );
	for ( uint32_t i = 0; i < entCount; ++i )
	{
		scene->entities[ i ]->envMap = TextureLib().RetrieveHdl( scene->specIblMap.c_str() );
		scene->entities[ i ]->diffuseIblMap = TextureLib().RetrieveHdl( scene->diffuseIblMap.c_str() );
	}

	// Skybox
	scene->FindEntity( "_skybox" )->SetFlag( ENT_FLAG_CAMERA_LOCKED );	

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
		DrawDrawGroupDebugMenu();
		DeviceDebugMenu();

		ImGui::EndTabBar();
	}

	std::vector<const Image*> images;

	const uint32_t imageCount = TextureLib().Count();
	for ( uint32_t i = 0; i < imageCount; ++i )
	{
		const Image* img = &TextureLib().Find( i )->Get();
		if ( img == nullptr ) {
			continue;
		}
		images.push_back( img );
	}

	const uint32_t outputImageCount = g_renderer.OutputImageCount();
	for ( uint32_t i = 0; i < outputImageCount; ++i )
	{
		const Image* img = g_renderer.FindOutputImage( i );
		if ( img == nullptr ) {
			continue;
		}
		images.push_back( img );
	}

	const char* debugImageName = ( g_imguiControls.dbgImageId >= 0 ) ? images[ g_imguiControls.dbgImageId ]->gpuImage->GetDebugName() : "";

	if ( ImGui::BeginCombo( "Images", debugImageName ) )
	{
		const uint32_t imageCount = static_cast<uint32_t>( images.size() );
		for ( uint32_t i = 0; i < imageCount; ++i )
		{
			const bool selected = ( i == g_imguiControls.dbgImageId );

			if( _stricmp( images[ i ]->gpuImage->GetDebugName(), "" ) == 0 ) {
				continue;
			}
			if ( ImGui::Selectable( images[ i ]->gpuImage->GetDebugName(), selected ) ) {
				g_imguiControls.dbgImageId = i;
			}
			if ( selected ) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if( g_imguiControls.dbgImageId >= 0 )
	{
		const Image* image = images[ g_imguiControls.dbgImageId ];

		const float aspect = image->info.width / (float)image->info.height;
		static float scale = 1.0f;

		ImGui::Begin( "Image Viewer" );

		ImGuiIO& io = ImGui::GetIO();

		ImGui::ColorButton( "button", ImVec4( 1.0f, 1.0f, 1.0f, 0.0f ), 0, ImVec2( scale * 200.0f, scale * ( 200.0f / aspect ) ) );

		if ( ImGui::IsItemHovered() )
		{
			float wheelDelta = io.MouseWheel;

			if ( wheelDelta != 0.0f )
			{
				scale += 0.1f * wheelDelta;
			}
		}

		ImVec2 pos = ImGui::GetItemRectMin();  // top-left of last item
		ImVec2 size = ImGui::GetItemRectSize();
		ImVec2 max = ImGui::GetItemRectMax();  // bottom-right

		// Get the current window's clip rect
		ImRect clipRectV4 = ImGui::GetCurrentContext()->CurrentWindow->ClipRect;
		ImVec2 clipMin = ImVec2( clipRectV4.Min.x, clipRectV4.Min.y );
		ImVec2 clipMax = ImVec2( clipRectV4.Max.x, clipRectV4.Max.y );

		// Intersect
		ImVec2 visibleMin = ImVec2( Max( pos.x, clipMin.x ), Max( pos.y, clipMin.y ) );
		ImVec2 visibleMax = ImVec2( Min( max.x, clipMax.x ), Min( max.y, clipMax.y ) );

		ImVec2 visibleSize = size;

		// Check if visible at all
		if ( visibleMin.x < visibleMax.x && visibleMin.y < visibleMax.y )
		{
			visibleSize = ImVec2( visibleMax.x - visibleMin.x, visibleMax.y - visibleMin.y );
		}

		imguiImageCallbackData_t data;
		data.progAsset = GpuProgramLib().Find( "Image2D" );
		data.permSet = static_cast<uint32_t>( shaderPermId_t::NONE );
		data.image = image;
		data.x = pos.x;
		data.y = pos.y;
		data.width = visibleSize.x;
		data.height = visibleSize.y;

		AddImguiCallback( ImGui::GetWindowDrawList(), data );

		ImGui::End();
	}

	char entityName[ 256 ];
	if ( g_imguiControls.selectedEntityId >= 0 ) {
		sprintf_s( entityName, "%i: %s", g_imguiControls.selectedEntityId, ModelLib().FindName( g_scene->entities[ g_imguiControls.selectedEntityId ]->modelHdl ) );
	}
	else {
		memset( &entityName[ 0 ], 0, 256 );
	}

	ImGui::Text( "Mouse: (%f, %f)", (float)g_window.input.GetMouse().x, (float)g_window.input.GetMouse().y );
	ImGui::Text( "Mouse Dt: (%f, %f)", (float)g_window.input.GetMouse().dx, (float)g_window.input.GetMouse().dy );
	const vec4f cameraOrigin = g_scene->mainCamera->GetOrigin();
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
		//g_imguiControls.rebuildRaytraceScene = ImGui::Button( "Rebuild Raytrace Scene" );
		ImGui::SameLine();
		//g_imguiControls.raytraceScene = ImGui::Button( "Raytrace Scene" );
		//ImGui::SameLine();
		//g_imguiControls.rasterizeScene = ImGui::Button( "Rasterize Scene" );
		//ImGui::SameLine();
		g_imguiControls.captureScreenshot = ImGui::Button( "Capture ScreenShot" );
		ImGui::SameLine();
		if ( ImGui::Button( "Dump VMA Stats" ) )
		{
			VmaTotalStatistics stats;
			vmaCalculateStatistics( AllocatorMemory::GetVmaAllocator(), &stats );

			VkPhysicalDeviceMemoryProperties memProps;
			vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProps );

			std::cout << "=== VMA Memory Statistics ===" << std::endl;
			for ( uint32_t i = 0; i < memProps.memoryHeapCount; ++i )
			{
				const VmaStatistics& heap = stats.memoryHeap[ i ].statistics;
				if ( heap.blockCount == 0 ) {
					continue;
				}

				const VkMemoryHeapFlags heapFlags = memProps.memoryHeaps[ i ].flags;
				std::string heapType;
				if ( heapFlags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) { heapType += "DEVICE_LOCAL "; }
				if ( heapFlags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT ) { heapType += "MULTI_INSTANCE "; }
				if ( heapType.empty() ) { heapType = "HOST"; }

				std::string memTypeFlags;
				for ( uint32_t t = 0; t < memProps.memoryTypeCount; ++t )
				{
					if ( memProps.memoryTypes[ t ].heapIndex != i ) {
						continue;
					}
					const VkMemoryPropertyFlags flags = memProps.memoryTypes[ t ].propertyFlags;
					memTypeFlags += "  [Type " + std::to_string( t ) + "]:";
					if ( flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT )  { memTypeFlags += " DEVICE_LOCAL"; }
					if ( flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT )  { memTypeFlags += " HOST_VISIBLE"; }
					if ( flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) { memTypeFlags += " HOST_COHERENT"; }
					if ( flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT )   { memTypeFlags += " HOST_CACHED"; }
					if ( flags == 0 )                                   { memTypeFlags += " (none)"; }
					memTypeFlags += "\n";
				}

				std::cout << "  Heap " << i << " (" << heapType << ") - "
						  << ( memProps.memoryHeaps[ i ].size / ( 1024 * 1024 ) ) << " MB capacity:" << std::endl;
				std::cout << "    " << ( heap.allocationBytes / ( 1024 * 1024 ) ) << " MB used / "
						  << ( heap.blockBytes / ( 1024 * 1024 ) ) << " MB allocated ("
						  << heap.allocationCount << " allocations, "
						  << heap.blockCount << " blocks)" << std::endl;
				std::cout << memTypeFlags;
			}

			const VmaStatistics& total = stats.total.statistics;
			std::cout << "  Total: "
					  << ( total.allocationBytes / ( 1024 * 1024 ) ) << " MB used / "
					  << ( total.blockBytes / ( 1024 * 1024 ) ) << " MB allocated ("
					  << total.allocationCount << " allocations, "
					  << total.blockCount << " blocks)" << std::endl;
			std::cout << "=============================" << std::endl;
		}

		ImGui::Checkbox( "Is Textured", &g_imguiControls.isTextured );

		ImGui::InputFloat( "Heightmap Height", &g_imguiControls.heightMapHeight, 0.1f, 1.0f );
		ImGui::SliderFloat( "Roughness Scale", &g_imguiControls.roughnessScale, 0.0f, 1.0f );
		ImGui::SliderFloat( "Roughness Bias", &g_imguiControls.roughnessBias, -1.0f, 1.0f );
		ImGui::SliderFloat( "Metalness Scale", &g_imguiControls.metalnessScale, 0.0f, 1.0f );
		ImGui::SliderFloat( "Metalness Bias", &g_imguiControls.metalnessBias, -1.0f, 1.0f );
		ImGui::SliderFloat( "Shadow Strength", &g_imguiControls.shadowStrength, 0.0f, 1.0f );
		ImGui::InputFloat( "Tone Map R", &g_imguiControls.toneMapColor[ 0 ], 0.1f, 1.0f );
		ImGui::InputFloat( "Tone Map G", &g_imguiControls.toneMapColor[ 1 ], 0.1f, 1.0f );
		ImGui::InputFloat( "Tone Map B", &g_imguiControls.toneMapColor[ 2 ], 0.1f, 1.0f );
		ImGui::InputFloat( "Tone Map A", &g_imguiControls.toneMapColor[ 3 ], 0.1f, 1.0f );
		ImGui::Checkbox( "Bloom Enabled", &g_imguiControls.bloomEnable );
		ImGui::InputFloat( "Bloom Blend Weight", &g_imguiControls.bloomBlendWeight, 0.001f, 1.0f );
		ImGui::Checkbox( "Auto-Exposure Enabled", &g_imguiControls.autoExposureEnable );
		ImGui::InputFloat( "Exposure Middle-Gray", &g_imguiControls.exposureMidGray, 0.1f, 0.9f );
		ImGui::InputFloat( "Exposure Adaptation", &g_imguiControls.exposureAdaptation, 0.01f, 5.0f );
		ImGui::InputFloat( "Exposure WhitePoint", &g_imguiControls.exposureWhitePoint, 0.8f, 1000.0f );
		ImGui::InputFloat( "Exposure Dark Cutoff", &g_imguiControls.exposureDarkLimit, 0.005f, 0.2f );
		ImGui::Checkbox( "DoF Enabled", &g_imguiControls.dofEnable );
		ImGui::SliderFloat( "DoF Focal Depth", &g_imguiControls.dofFocalDepth, 0.0f, 1.0f );
		ImGui::SliderFloat( "DoF Focal Range", &g_imguiControls.dofFocalRange, 0.0f, 1.0f );
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
		const uint32_t matCount = MaterialLib().Count();
		if ( ImGui::TreeNode( "Materials", "Materials (%i)", matCount ) )
		{
			for ( uint32_t m = 0; m < matCount; ++m )
			{
				Asset<Material>* matAsset = MaterialLib().Find( m );
				Material& mat = matAsset->Get();
				const char* matName = MaterialLib().FindName( m );

				if ( ImGui::TreeNode( matAsset->GetName().c_str() ) )
				{
					DebugMenuMaterialEdit( matAsset );
					ImGui::Separator();
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		const uint32_t modelCount = ModelLib().Count();
		if ( ImGui::TreeNode( "Models", "Models (%i)", modelCount ) )
		{
			for ( uint32_t m = 0; m < modelCount; ++m )
			{
				Asset<Model>* modelAsset = ModelLib().Find( m );
				DebugMenuModelTreeNode( modelAsset );
			}
			ImGui::TreePop();
		}
		const uint32_t texCount = TextureLib().Count();
		if ( ImGui::TreeNode( "Textures", "Textures (%i)", texCount ) )
		{
			for ( uint32_t t = 0; t < texCount; ++t )
			{
				Asset<Image>* texAsset = TextureLib().Find( t );
				DebugMenuTextureTreeNode( texAsset );
			}
			ImGui::TreePop();
		}

		const uint32_t shaderCount = GpuProgramLib().Count();
		if ( ImGui::TreeNode( "Shaders", "Shaders (%i)", shaderCount ) )
		{
			for ( uint32_t s = 0; s < shaderCount; ++s )
			{
				Asset<GpuProgram>* shaderAsset = GpuProgramLib().Find( s );
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
			const uint32_t modelCount = ModelLib().Count();
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
				const vec3f boundCenter = ( ent->GetMatrix() * vec4f( bounds.GetCenter(), 1.0f ) ).xyz;

				Entity* boundEnt = new Entity( *ent );
				boundEnt->name = ent->name + "_bounds";
				boundEnt->SetFlag( ENT_FLAG_WIREFRAME );
				boundEnt->materialHdl = MaterialLib().RetrieveHdl( "DEBUG_WIRE" );
				boundEnt->SetOrigin( boundCenter );
				boundEnt->SetScale( Multiply( vec3f( scale ), vec3f( boundScale[ 0 ], boundScale[ 1 ], boundScale[ 2 ] ) ) );
				boundEnt->SetRotation( rotation );

				g_scene->entities.push_back( boundEnt );
				g_scene->CreateEntityBounds( ModelLib().RetrieveHdl( "cube" ), *boundEnt );
			}

			ImGui::SameLine();
			if ( ImGui::Button( "Export Model" ) )
			{
				Asset<Model>* asset = ModelLib().Find( ent->modelHdl );
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
		const char* previewValue = ModelLib().FindName( currentIdx );
		if ( ImGui::BeginCombo( "Model", previewValue ) )
		{
			const uint32_t modelCount = ModelLib().Count();
			for ( uint32_t m = 0; m < modelCount; ++m )
			{
				Asset<Model>* modelAsset = ModelLib().Find( m );

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
			g_scene->CreateEntityBounds( ModelLib().RetrieveHdl( ModelLib().FindName( currentIdx ) ), *ent );
		}

		ImGui::EndTabItem();
	}
#endif
}



void DrawDrawGroupDebugMenu()
{
#if defined( USE_IMGUI )
	if ( ImGui::BeginTabItem( "Draw Groups" ) )
	{
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

		DebugMenuEntityEdit( g_scene );

		ImGui::EndTabItem();
	}
#endif
}
