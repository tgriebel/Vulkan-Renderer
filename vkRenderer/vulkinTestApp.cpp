#include "stdafx.h"

#include <map>

#include "common.h"
#include "deviceContext.h"
#include "camera.h"
#include "scene.h"
#include "pipeline.h"
#include "swapChain.h"
#include "assetLib_GpuProgs.h"
#include "window.h"
#include "FrameState.h"
#include "renderConstants.h"

#define USE_IMGUI
#include "io.h"
#include "GeoBuilder.h"

#if defined( USE_IMGUI )
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_vulkan.h"
#endif

static const glm::vec3 forwardDefault = glm::vec3( 1.0f, 0.0f, 0.0f );
static const glm::vec3 sideDefault = glm::vec3( 0.0f, -1.0f, 0.0f );
static const glm::vec3 upDefault = glm::vec3( 0.0f, 0.0f, 1.0f );

#if defined( USE_IMGUI )
static ImGui_ImplVulkanH_Window imguiMainWindowData;
#endif

using frameRate_t = std::chrono::duration< double, std::ratio<1, 60> >;

struct imguiControls_t
{
	float		heightMapHeight;
	float		toneMapColor[ 4 ];
	int			dbgImageId;
	int			selectedModelId;
	glm::vec3	selectedModelOrigin;
};
static imguiControls_t imguiControls;

pipelineHdl_t						postProcessPipeline;
std::map<std::string, material_t>	materials;
MemoryAllocator						localMemory;
MemoryAllocator						sharedMemory;

AssetLibGpuProgram					gpuPrograms;

deviceContext_t						context;
renderConstants_t					r;

Scene								scene;

static bool							validateVerbose = false;
static bool							validateWarnings = false;
static bool							validateErrors = true;

glm::mat4 MatrixFromVector( const glm::vec3& v );
bool LoadTextureImage( const char* texturePath, textureSource_t& texture );
RenderProgram CreateShaders( const std::string& vsFile, const std::string& psFile );

class VkRenderer
{
public:
	void Run() {
		window.InitWindow();
		InitVulkan();
		InitImGui();
		MainLoop();
		Cleanup();
	}

private:

	static const uint32_t ShadowMapWidth = 1280;
	static const uint32_t ShadowMapHeight = 720;

	const std::vector<std::string> texturePaths = { "heightmap.png", "grass.jpg", "checker.png", "skybox.jpg", "viking_room.png",
													"checker.png", "desert.jpg", "palm_tree_diffuse.jpg", "checker.png", "checker.png", };

	std::vector<surfUpload_t>			modelUpload;
	std::map<std::string, texture_t>	textures;
	RenderView							renderView;
	RenderView							shadowView;

	const std::vector<const char*> validationLayers = {	"VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> deviceExtensions = {	VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

	Window							window;
	VkDebugUtilsMessengerEXT		debugMessenger;
	SwapChain						swapChain;
	std::vector< AllocRecord >		imageAllocations;
	graphicsQueue_t					graphicsQueue;
	DrawPassState					shadowPassState;
	DrawPassState					mainPassState;
	DrawPassState					postPassState;
	VkDescriptorSetLayout			globalLayout;
	VkDescriptorSetLayout			postProcessLayout;
	VkDescriptorPool				descriptorPool;
	FrameState						frameState[ MAX_FRAMES_STATES ];
	size_t							currentFrame = 0;
	GpuBuffer						stagingBuffer;
	GpuBuffer						vb;	// move
	GpuBuffer						ib;

	VkSampler						vk_bilinearSampler;
	VkSampler						vk_depthShadowSampler;
	VkSampler						vk_depthSampler;

	float							nearPlane = 1000.0f;
	float							farPlane = 0.1f;

	void CreateInstance()
	{
		if ( enableValidationLayers && !CheckValidationLayerSupport() )
		{
			throw std::runtime_error( "validation layers requested, but not available!" );
		}

		VkApplicationInfo appInfo{ };
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo createInfo{ };
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );
		std::vector<VkExtensionProperties> extensionProperties( extensionCount );
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensionProperties.data() );

		std::cout << "available extensions:\n";

		for ( const auto& extension : extensionProperties )
		{
			std::cout << '\t' << extension.extensionName << '\n';
		}

		uint32_t glfwExtensionCount = 0;

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if ( enableValidationLayers )
		{
			createInfo.enabledLayerCount = static_cast< uint32_t >( validationLayers.size() );
			createInfo.ppEnabledLayerNames = validationLayers.data();

			PopulateDebugMessengerCreateInfo( debugCreateInfo );
			createInfo.pNext = ( VkDebugUtilsMessengerCreateInfoEXT* )&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		auto extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast< uint32_t >( extensions.size() );
		createInfo.ppEnabledExtensionNames = extensions.data();

		if ( vkCreateInstance( &createInfo, nullptr, &context.instance ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create instance!" );
		}
	}

	void UploadModels()
	{
		const VkDeviceSize vbSize = sizeof( VertexInput ) * MaxVertices;
		const VkDeviceSize ibSize = sizeof( uint32_t ) * MaxIndices;
		modelUpload.resize( scene.models.size() );
		for ( int i = 0; i < 1; ++i ) {
			CreateBuffer( vbSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vb.buffer, localMemory, vb.allocation );
			CreateBuffer( ibSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ib.buffer, localMemory, ib.allocation );

			static uint32_t vbBufElements = 0;
			static uint32_t ibBufElements = 0;
			for ( uint32_t m = 0; m < scene.models.size(); ++m )
			{
				VkDeviceSize vbCopySize = sizeof( scene.models[ m ].vertices[ 0 ] ) * scene.models[ m ].vertices.size();
				VkDeviceSize ibCopySize = sizeof( scene.models[ m ].indices[ 0 ] ) * scene.models[ m ].indices.size();
				
				// VB Copy
				modelUpload[ m ].vb = vb.GetVkObject();
				modelUpload[ m ].vertexOffset = vbBufElements;
				stagingBuffer.Reset();
				stagingBuffer.CopyData( scene.models[ m ].vertices.data(), static_cast<size_t>( vbCopySize ) );

				VkBufferCopy vbCopyRegion{ };
				vbCopyRegion.size = vbCopySize;
				vbCopyRegion.srcOffset = 0;
				vbCopyRegion.dstOffset = vb.allocation.subOffset;
				UploadToGPU( stagingBuffer.buffer, vb.GetVkObject(), vbCopyRegion );

				vb.allocation.subOffset += vbCopySize;
				vbBufElements += static_cast< uint32_t >( scene.models[ m ].vertices.size() );

				// IB Copy
				modelUpload[ m ].ib = ib.GetVkObject();
				modelUpload[ m ].firstIndex = ibBufElements;

				stagingBuffer.Reset();
				stagingBuffer.CopyData( scene.models[ m ].indices.data(), static_cast<size_t>( ibCopySize ) );

				VkBufferCopy ibCopyRegion{ };
				ibCopyRegion.size = ibCopySize;
				ibCopyRegion.srcOffset = 0;
				ibCopyRegion.dstOffset = ib.allocation.subOffset;
				UploadToGPU( stagingBuffer.buffer, ib.GetVkObject(), ibCopyRegion );

				ib.allocation.subOffset += ibCopySize;
				ibBufElements += static_cast<uint32_t>( scene.models[ m ].indices.size() );
			}
		}
	}

	void CommitModel( RenderView& view, entity_t& model, const uint32_t objectOffset )
	{
		drawSurf_t& surf		= view.surfaces[ view.committedModelCnt ];
		modelSource_t& source	= scene.models[ model.modelId ];
		surfUpload_t& upload	= modelUpload[ model.modelId ];

		surf.vertexCount		= upload.vertexCount;
		surf.vertexOffset		= upload.vertexOffset;
		surf.firstIndex			= upload.firstIndex;
		surf.indicesCnt			= static_cast<uint32_t>( source.indices.size() );
		surf.materialId			= model.materialId;
		surf.modelMatrix		= model.matrix;
		surf.objectId			= view.committedModelCnt + objectOffset;
		surf.flags				= model.flags;

		for ( int i = 0; i < DRAWPASS_COUNT; ++i ) {
			if ( source.material->shaders[ i ] != nullptr ) {
				surf.pipelineObject[ i ] = source.material->shaders[ i ]->pipeline;
			} else {
				surf.pipelineObject[ i ] = INVALID_HANDLE;
			}
		}

		++view.committedModelCnt;
	}

	void MakeBeachScene()
	{
		scene.models.resize( 6 );
		const int skyBoxId = 0;
		const int terrainId = 1;
		const int palmModelId = 2;
		const int waterId = 3;
		const int toneQuadId = 4;
		const int image2dId = 5;

		CreateSkyBoxSurf( scene.models[ skyBoxId ] );
		CreateTerrainSurface( scene.models[ terrainId ] );
		CreateStaticModel( "sphere.obj", "PALM", scene.models[ palmModelId ] );
		CreateWaterSurface( scene.models[ waterId ] );
		CreateQuadSurface2D( "TONEMAP", scene.models[ toneQuadId ], glm::vec2( 1.0f, 1.0f ), glm::vec2( 2.0f ) );
		CreateQuadSurface2D( "IMAGE2D", scene.models[ image2dId ], glm::vec2( 1.0f, 1.0f ), glm::vec2( 1.0f * ( 9.0 / 16.0f ), 1.0f ) );

		UploadModels();


		const int palmTreesNum = 300;

		int entId = 0;
		scene.entities.resize( 5 + palmTreesNum );
		CreateEntity( 0, scene.entities[ entId ] );
		++entId;
		CreateEntity( 1, scene.entities[ entId ] );
		++entId;
		CreateEntity( 3, scene.entities[ entId ] );
		++entId;
		CreateEntity( 4, scene.entities[ entId ] );
		++entId;
		CreateEntity( 5, scene.entities[ entId ] );
		++entId;

		for ( int i = 0; i < palmTreesNum; ++i )
		{
			CreateEntity( palmModelId, scene.entities[ entId ] );
			++entId;
		}

		// Terrain
		scene.entities[ 1 ].matrix = glm::identity<glm::mat4>();

		// Model
		const float scale = 0.0025f;
		{
			for ( int i = 5; i < ( 5 + palmTreesNum ); ++i )
			{
				glm::vec2 randPoint;
				RandPlanePoint( randPoint );
				randPoint = 10.0f * ( randPoint - 0.5f );
				scene.entities[ i ].matrix = scale * glm::identity<glm::mat4>();
				scene.entities[ i ].matrix[ 3 ][ 0 ] = randPoint.x;
				scene.entities[ i ].matrix[ 3 ][ 1 ] = randPoint.y;
				scene.entities[ i ].matrix[ 3 ][ 2 ] = 0.0f;
				scene.entities[ i ].matrix[ 3 ][ 3 ] = 1.0f;
			}
		}
	}

	void InitVulkan()
	{
		{
			// Device Set-up
			CreateInstance();
			SetupDebugMessenger();
			window.CreateSurface();
			PickPhysicalDevice();
			CreateLogicalDevice();	
			swapChain.Create( &window );
		}

		{
			// Passes
			CreateMainRenderPass( mainPassState.pass );
			CreateShadowPass( shadowPassState.pass );
			CreatePostProcessPass( postPassState.pass );
		}

		{
			// Pool Creation
			CreateDescriptorPool();
			CreateCommandPool();
		}

		{
			// Memory Allocations
			uint32_t type = FindMemoryType( ~0x00, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
			AllocateDeviceMemory( MaxSharedMemory, type, sharedMemory );
			type = FindMemoryType( ~0x00, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
			AllocateDeviceMemory( MaxLocalMemory, type, localMemory );
		}
		
		CreateResourceBuffers();
		CreateTextureSampler( vk_bilinearSampler );
		CreateDepthSampler( vk_depthShadowSampler );

		InitScene( renderView );

		CreateAssetLibs();

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );

		CreateFramebuffers();

		CreateSyncObjects();

		CreateDescSetLayouts();
		CreatePipelineObjects();

		CreateUniformBuffers();

		UpdateDescriptorSets();

		MakeBeachScene();
	}

	void InitImGui()
	{
#if defined( USE_IMGUI )
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForVulkan( window.window, true );

		ImGui_ImplVulkan_InitInfo vkInfo = {};
		vkInfo.Instance = context.instance;
		vkInfo.PhysicalDevice = context.physicalDevice;
		vkInfo.Device = context.device;
		vkInfo.QueueFamily = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
		vkInfo.Queue = context.graphicsQueue;
		vkInfo.PipelineCache = nullptr;
		vkInfo.DescriptorPool = descriptorPool;
		vkInfo.Allocator = nullptr;
		vkInfo.MinImageCount = 2;
		vkInfo.ImageCount = 2;
		vkInfo.CheckVkResultFn = nullptr;
		ImGui_ImplVulkan_Init( &vkInfo, postPassState.pass );
		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Upload Fonts
		{
			vkResetCommandPool( context.device, graphicsQueue.commandPool, 0 );
			VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
			ImGui_ImplVulkan_CreateFontsTexture( commandBuffer );
			EndSingleTimeCommands( commandBuffer );
			ImGui_ImplVulkan_DestroyFontUploadObjects();
		}
#endif
		imguiControls.heightMapHeight = 1.0f;
		imguiControls.toneMapColor[ 0 ] = 1.0f;
		imguiControls.toneMapColor[ 1 ] = 1.0f;
		imguiControls.toneMapColor[ 2 ] = 1.0f;
		imguiControls.toneMapColor[ 3 ] = 1.0f;
		imguiControls.selectedModelId = -1;
		imguiControls.selectedModelOrigin = glm::vec3( 0.0f );
	}

	void ShutdownImGui()
	{
#if defined( USE_IMGUI )
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
#endif
	}

	void CreateDescSetLayouts()
	{
		for ( RenderProgram& program : gpuPrograms.programs )
		{
			CreateDescriptorSetLayout( program );
		}

		CreateSceneRenderDescriptorSetLayout( globalLayout );
		CreateSceneRenderDescriptorSetLayout( postProcessLayout );
		CreateDescriptorSets( globalLayout, mainPassState.descriptorSets );
		CreateDescriptorSets( globalLayout, shadowPassState.descriptorSets );
		CreateDescriptorSets( postProcessLayout, postPassState.descriptorSets );
	}

	gfxStateBits_t GetStateBitsForDrawPass( const drawPass_t pass )
	{
		uint64_t stateBits = 0;
		if ( pass == DRAWPASS_SKYBOX  )
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_DEPTH_WRITE;
			stateBits |= GFX_STATE_CULL_MODE_BACK;
		}
		else if ( pass == DRAWPASS_SHADOW )
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_DEPTH_WRITE;
		//	stateBits |= GFX_STATE_COLOR_MASK;
			stateBits |= GFX_STATE_DEPTH_OP_0; // FIXME: 3 bits, just set one for now
		//	stateBits |= GFX_STATE_CULL_MODE_FRONT;
		}
		else if ( pass == DRAWPASS_DEPTH )
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_DEPTH_WRITE;
			stateBits |= GFX_STATE_COLOR_MASK;
			stateBits |= GFX_STATE_CULL_MODE_BACK;
		}
		else if ( pass == DRAWPASS_TERRAIN )
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_DEPTH_WRITE;
			stateBits |= GFX_STATE_CULL_MODE_BACK;
		}
		else if ( pass == DRAWPASS_OPAQUE )
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_DEPTH_WRITE;
			stateBits |= GFX_STATE_CULL_MODE_BACK;
		}
		else if ( pass == DRAWPASS_TRANS )
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_CULL_MODE_BACK;
			stateBits |= GFX_STATE_BLEND_ENABLE;
		}
		else if ( pass == DRAWPASS_WIREFRAME )
		{
			stateBits |= GFX_STATE_WIREFRAME_ENABLE;
		}
		else if ( pass == DRAWPASS_POST_2D )
		{
			stateBits |= GFX_STATE_BLEND_ENABLE;
		}
		else
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_DEPTH_WRITE;
			stateBits |= GFX_STATE_CULL_MODE_BACK;
		}

		return static_cast<gfxStateBits_t>( stateBits );
	}

	viewport_t GetDrawPassViewport( const drawPass_t pass )
	{
		return renderView.viewport;
	}

	uint32_t GetTextureId( const std::string& name )
	{
		auto it = textures.find( name );

		if ( it != textures.end() )
		{
			return static_cast<uint32_t>( std::distance( textures.begin(), it ) );
		}
		else
		{
			return 0;
		}
	}

	uint32_t GetMaterialId( const std::string& name )
	{
		auto it = materials.find( name );

		if ( it != materials.end() )
		{
			return static_cast<uint32_t>( std::distance( materials.begin(), it ) );
		}
		else
		{
			return 0;
		}
	}

	void CreatePipelineObjects()
	{
		for ( auto itt = materials.begin(); itt != materials.end(); ++itt )
		{
			material_t& m = itt->second;

			for ( int i = 0; i < DRAWPASS_COUNT; ++i ) {
				if ( m.shaders[ i ] == nullptr ) {
					continue;
				}

				pipelineState_t state;
				state.viewport = GetDrawPassViewport( (drawPass_t)i );
				state.stateBits = GetStateBitsForDrawPass( (drawPass_t)i );
				state.shaders = m.shaders[ i ];

				VkRenderPass pass;
				VkDescriptorSetLayout layout;
				if ( i == DRAWPASS_SHADOW ) {
					pass = shadowPassState.pass;
					layout = globalLayout;
				} else if ( i == DRAWPASS_POST_2D ) {
					pass = postPassState.pass;
					layout = postProcessLayout;
				} else {
					pass = mainPassState.pass;
					layout = globalLayout;
				}

				CreateGraphicsPipeline( layout, pass, state, m.shaders[ i ]->pipeline );
			}
		}
	}

	void CreateAssetLibs()
	{
		gpuPrograms.Create();
		CreateTextureImage( texturePaths );
		CreateMaterials(); // TODO: enforce ordering somehow. This comes after programs and textures
	}

	void CreateMaterials()
	{
		materials[ "TERRAIN" ].shaders[ DRAWPASS_SHADOW ] = &gpuPrograms.programs[ RENDER_PROGRAM_TERRAIN_SHADOW ];
		materials[ "TERRAIN" ].shaders[ DRAWPASS_DEPTH ] = &gpuPrograms.programs[ RENDER_PROGRAM_TERRAIN_DEPTH ];
		materials[ "TERRAIN" ].shaders[ DRAWPASS_TERRAIN ] = &gpuPrograms.programs[ RENDER_PROGRAM_TERRAIN ];
		materials[ "TERRAIN" ].shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.programs[ RENDER_PROGRAM_TERRAIN_DEPTH ];
		materials[ "TERRAIN" ].texture0 = GetTextureId( "heightmap.png" );
		materials[ "TERRAIN" ].texture1 = GetTextureId( "grass.jpg" );
		materials[ "TERRAIN" ].texture2 = GetTextureId( "desert.jpg" );

		materials[ "SKY" ].shaders[ DRAWPASS_SKYBOX ] = &gpuPrograms.programs[ RENDER_PROGRAM_SKY ];
		materials[ "SKY" ].texture0 = GetTextureId( "skybox.jpg" );

		materials[ "VIKING" ].shaders[ DRAWPASS_SHADOW ] = &gpuPrograms.programs[ RENDER_PROGRAM_SHADOW ];
		materials[ "VIKING" ].shaders[ DRAWPASS_DEPTH ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
		materials[ "VIKING" ].shaders[ DRAWPASS_OPAQUE ] = &gpuPrograms.programs[ RENDER_PROGRAM_LIT_OPAQUE ];
		materials[ "VIKING" ].shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
		materials[ "VIKING" ].texture0 = GetTextureId( "viking_room.png" );

		materials[ "WATER" ].shaders[ DRAWPASS_TRANS ] = &gpuPrograms.programs[ RENDER_PROGRAM_LIT_TRANS ];
		materials[ "WATER" ].shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
		materials[ "WATER" ].texture0 = GetTextureId( "checker.png" );

		materials[ "TONEMAP" ].shaders[ DRAWPASS_POST_2D ] = &gpuPrograms.programs[ RENDER_PROGRAM_POST_PROCESS ];
		materials[ "TONEMAP" ].texture0 = 0;
		materials[ "TONEMAP" ].texture1 = 1;

		materials[ "IMAGE2D" ].shaders[ DRAWPASS_POST_2D ] = &gpuPrograms.programs[ RENDER_PROGRAM_IMAGE_2D ];
		materials[ "IMAGE2D" ].texture0 = 2;

		materials[ "PALM" ].shaders[ DRAWPASS_SHADOW ] = &gpuPrograms.programs[ RENDER_PROGRAM_SHADOW ];
		materials[ "PALM" ].shaders[ DRAWPASS_DEPTH ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
		materials[ "PALM" ].shaders[ DRAWPASS_OPAQUE ] = &gpuPrograms.programs[ RENDER_PROGRAM_LIT_OPAQUE ];
		materials[ "PALM" ].shaders[ DRAWPASS_WIREFRAME ] = &gpuPrograms.programs[ RENDER_PROGRAM_DEPTH_PREPASS ];
		materials[ "PALM" ].texture0 = GetTextureId( "palm_tree_diffuse.jpg" );
	}

	void CopyGeoBuilderResult( const GeoBuilder& gb, std::vector<VertexInput>& vb, std::vector<uint32_t>& ib )
	{
		vb.reserve( gb.vb.size() );
		for ( const GeoBuilder::vertex_t& v : gb.vb )
		{
			VertexInput vert;
			vert.pos = v.pos;
			vert.color = v.color;
			vert.tangent = v.tangent;
			vert.bitangent = v.bitangent;
			vert.texCoord = v.texCoord;

			vb.push_back( vert );
		}

		ib.reserve( gb.ib.size() );
		for ( uint32_t index : gb.ib )
		{
			ib.push_back( index );
		}
	}

	void CreateQuadSurface2D( const std::string& materialName, modelSource_t& outModel, glm::vec2& origin, glm::vec2& size )
	{
		GeoBuilder::planeInfo_t info;
		info.gridSize = size;
		info.widthInQuads = 1;
		info.heightInQuads = 1;
		info.uv[ 0 ] = glm::vec2( 0.0f, 0.0f );
		info.uv[ 1 ] = glm::vec2( -1.0f, 1.0f );
		info.origin = glm::vec3( origin.x, origin.y, 0.0f );
		info.normalDirection = NORMAL_Z_NEG;

		GeoBuilder gb;
		gb.AddPlaneSurf( info );

		CopyGeoBuilderResult( gb, outModel.vertices, outModel.indices );

		outModel.name = "_quad";
		outModel.material = &materials[ materialName ];
		outModel.materialId = GetMaterialId( materialName );
	}

	void CreateTerrainSurface( modelSource_t& outModel )
	{
		GeoBuilder::planeInfo_t info;
		info.gridSize			= glm::vec2( 0.1f );
		info.widthInQuads		= 100;
		info.heightInQuads		= 100;
		info.uv[ 0 ]			= glm::vec2( 0.0f, 0.0f );
		info.uv[ 1 ]			= glm::vec2( 1.0f, 1.0f );
		info.origin				= glm::vec3( 0.5f * info.gridSize.x * info.widthInQuads, 0.5f * info.gridSize.y * info.heightInQuads, 0.0f );
		info.color				= glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
		info.normalDirection	= NORMAL_Z_NEG;

		GeoBuilder gb;
		gb.AddPlaneSurf( info );
		
		CopyGeoBuilderResult( gb, outModel.vertices, outModel.indices );

		outModel.name		= "_terrain";
		outModel.material	= &materials[ "TERRAIN" ];
		outModel.materialId	= GetMaterialId( "TERRAIN" );
	}

	void CreateWaterSurface( modelSource_t& outModel )
	{
		const float gridSize = 10.f;
		const uint32_t width = 1;
		const uint32_t height = 1;
		const glm::vec2 uvs[] = { glm::vec2( 0.0f, 0.0f ), glm::vec2( 1.0f, 1.0f ) };

		GeoBuilder::planeInfo_t info;
		info.gridSize			= glm::vec2( 10.0f );
		info.widthInQuads		= 1;
		info.heightInQuads		= 1;
		info.uv[ 0 ]			= glm::vec2( 0.0f, 0.0f );
		info.uv[ 1 ]			= glm::vec2( 1.0f, 1.0f );
		info.origin				= glm::vec3( 0.5f * info.gridSize.x * info.widthInQuads, 0.5f * info.gridSize.y * info.heightInQuads, -0.15f );
		info.color				= glm::vec4( 0.0f, 0.0f, 1.0f, 0.2f );
		info.normalDirection	= NORMAL_Z_NEG;

		GeoBuilder gb;
		gb.AddPlaneSurf( info );

		CopyGeoBuilderResult( gb, outModel.vertices, outModel.indices );

		outModel.name		= "_water";
		outModel.material	= &materials[ "WATER" ];
		outModel.materialId	= GetMaterialId( "WATER" );
	}

	void CreateSkyBoxSurf( modelSource_t& outModel )
	{
		const float gridSize = 1.0f;
		const uint32_t width = 1;
		const uint32_t height = 1;

		const float third = 1.0f / 3.0f;
		const float quarter = 1.0f / 4.0f;

		GeoBuilder::planeInfo_t info[ 6 ];
		info[ 0 ].gridSize			= glm::vec2( gridSize );
		info[ 0 ].widthInQuads		= width;
		info[ 0 ].heightInQuads		= height;
		info[ 0 ].uv[ 0 ]			= glm::vec2( quarter, 2.0f * third );
		info[ 0 ].uv[ 1 ]			= glm::vec2( quarter, third );
		info[ 0 ].origin			= glm::vec3( 0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
		info[ 0 ].color				= glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
		info[ 0 ].normalDirection	= NORMAL_Z_NEG;

		info[ 1 ].gridSize			= glm::vec2( gridSize );
		info[ 1 ].widthInQuads		= width;
		info[ 1 ].heightInQuads		= height;
		info[ 1 ].uv[ 0 ]			= glm::vec2( quarter, 0.0f );
		info[ 1 ].uv[ 1 ]			= glm::vec2( quarter, third );
		info[ 1 ].origin			= glm::vec3( 0.5f * gridSize * width, 0.5f * gridSize * height, -gridSize );
		info[ 1 ].color				= glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
		info[ 1 ].normalDirection	= NORMAL_Z_POS;

		info[ 2 ].gridSize			= glm::vec2( gridSize );
		info[ 2 ].widthInQuads		= width;
		info[ 2 ].heightInQuads		= height;
		info[ 2 ].uv[ 0 ]			= glm::vec2( 0.0f, third );
		info[ 2 ].uv[ 1 ]			= glm::vec2( quarter, third );
		info[ 2 ].origin			= glm::vec3( -0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
		info[ 2 ].color				= glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
		info[ 2 ].normalDirection	= NORMAL_X_POS;

		info[ 3 ].gridSize			= glm::vec2( gridSize );
		info[ 3 ].widthInQuads		= width;
		info[ 3 ].heightInQuads		= height;
		info[ 3 ].uv[ 0 ]			= glm::vec2( 2.0f * quarter, third );
		info[ 3 ].uv[ 1 ]			= glm::vec2( quarter, third );
		info[ 3 ].origin			= glm::vec3( 0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
		info[ 3 ].color				= glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
		info[ 3 ].normalDirection	= NORMAL_X_NEG;

		info[ 4 ].gridSize			= glm::vec2( gridSize );
		info[ 4 ].widthInQuads		= width;
		info[ 4 ].heightInQuads		= height;
		info[ 4 ].uv[ 0 ]			= glm::vec2( 3.0f * quarter, third );
		info[ 4 ].uv[ 1 ]			= glm::vec2( quarter, third );
		info[ 4 ].origin			= glm::vec3( 0.5f * gridSize * width, -0.5f * gridSize * height, 0.0f );
		info[ 4 ].color				= glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
		info[ 4 ].normalDirection	= NORMAL_Y_NEG;

		info[ 5 ].gridSize			= glm::vec2( gridSize );
		info[ 5 ].widthInQuads		= width;
		info[ 5 ].heightInQuads		= height;
		info[ 5 ].uv[ 0 ]			= glm::vec2( quarter, third );
		info[ 5 ].uv[ 1 ]			= glm::vec2( quarter, third );
		info[ 5 ].origin			= glm::vec3( 0.5f * gridSize * width, 0.5f * gridSize * height, 0.0f );
		info[ 5 ].color				= glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
		info[ 5 ].normalDirection	= NORMAL_Y_POS;

		GeoBuilder gb;
		for ( int i = 0; i < 6; ++i ) {
			gb.AddPlaneSurf( info[ i ] );
		}

		CopyGeoBuilderResult( gb, outModel.vertices, outModel.indices );

		outModel.name		= "_skybox";
		outModel.material	= &materials[ "SKY" ];
		outModel.materialId	= GetMaterialId( "SKY" );
	}

	void CreateStaticModel( const std::string& modelName, const std::string& materialName, modelSource_t& outModel )
	{
		LoadModel( modelName, outModel );

		outModel.name			= modelName;
		outModel.material		= &materials[ materialName ]; // FIXME: needs to be a look up since an index auto-inserts
		outModel.materialId		= GetMaterialId( materialName );
	}

	void CreateEntity( const uint32_t modelId, entity_t& instance )
	{
		const modelSource_t& model = scene.models[ modelId ];

		instance.modelId = modelId;
		instance.materialId = model.materialId;
		instance.matrix = glm::identity<glm::mat4>();
	}

	void PickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices( context.instance, &deviceCount, nullptr );

		if ( deviceCount == 0 ) {
			throw std::runtime_error( "Failed to find GPUs with Vulkan support!" );
		}

		std::vector<VkPhysicalDevice> devices( deviceCount );
		vkEnumeratePhysicalDevices( context.instance, &deviceCount, devices.data() );

		for ( const auto& device : devices )
		{
			if ( IsDeviceSuitable( device, window.vk_surface, deviceExtensions ) )
			{		
				VkPhysicalDeviceProperties deviceProperties;
				vkGetPhysicalDeviceProperties( device, &deviceProperties );
				context.physicalDevice = device;
				context.limits = deviceProperties.limits;
				break;
			}
		}

		if ( context.physicalDevice == VK_NULL_HANDLE ) {
			throw std::runtime_error( "Failed to find a suitable GPU!" );
		}
	}

	void CreateLogicalDevice()
	{
		QueueFamilyIndices indices = FindQueueFamilies( context.physicalDevice, window.vk_surface );
		context.queueFamilyIndices[ QUEUE_GRAPHICS ] = indices.graphicsFamily.value();
		context.queueFamilyIndices[ QUEUE_PRESENT ] = indices.presentFamily.value();
		context.queueFamilyIndices[ QUEUE_COMPUTE ] = indices.computeFamily.value();

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value(), indices.computeFamily.value() };

		float queuePriority = 1.0f;
		for ( uint32_t queueFamily : uniqueQueueFamilies )
		{
			VkDeviceQueueCreateInfo queueCreateInfo{ };
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back( queueCreateInfo );
		}

		VkDeviceCreateInfo createInfo{ };
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast< uint32_t >( queueCreateInfos.size() );
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		VkPhysicalDeviceFeatures deviceFeatures{ };
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.fillModeNonSolid = VK_TRUE;
		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = static_cast< uint32_t >( deviceExtensions.size() );
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		if ( enableValidationLayers )
		{
			createInfo.enabledLayerCount = static_cast< uint32_t >( validationLayers.size() );
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VkPhysicalDeviceDescriptorIndexingFeatures descIndexing;
		memset( &descIndexing, 0, sizeof( VkPhysicalDeviceDescriptorIndexingFeatures ) );
		descIndexing.runtimeDescriptorArray = true;
		descIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		descIndexing.pNext = NULL;
		createInfo.pNext = &descIndexing;

		if ( vkCreateDevice( context.physicalDevice, &createInfo, nullptr, &context.device ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create logical context.device!" );
		}

		vkGetDeviceQueue( context.device, indices.graphicsFamily.value(), 0, &context.graphicsQueue );
		vkGetDeviceQueue( context.device, indices.presentFamily.value(), 0, &context.presentQueue );
		vkGetDeviceQueue( context.device, indices.computeFamily.value(), 0, &context.computeQueue );
	}

	void CleanupFrameResources()
	{
		vkDestroyRenderPass( context.device, mainPassState.pass, nullptr );
		vkDestroyRenderPass( context.device, shadowPassState.pass, nullptr );
		vkDestroyRenderPass( context.device, postPassState.pass, nullptr );

		for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ )
		{
			vkDestroyBuffer( context.device, frameState[ i ].surfParms.buffer, nullptr );
			vkDestroyImageView( context.device, frameState[ i ].viewColorImage.view, nullptr );
			vkDestroyImageView( context.device, frameState[ i ].shadowMapImage.view, nullptr );
			vkDestroyImageView( context.device, frameState[ i ].viewColorImage.view, nullptr );
			vkDestroyImage( context.device, frameState[ i ].viewColorImage.image, nullptr );
			vkDestroyImage( context.device, frameState[ i ].shadowMapImage.image, nullptr );
			vkDestroyImage( context.device, frameState[ i ].depthImage.image, nullptr );
		}

		vkFreeMemory( context.device, localMemory.GetDeviceMemory(), nullptr );
		vkFreeMemory( context.device, sharedMemory.GetDeviceMemory(), nullptr );
		localMemory.Unbind();
		sharedMemory.Unbind();

		vkDestroyDescriptorPool( context.device, descriptorPool, nullptr );

		vkFreeCommandBuffers( context.device, graphicsQueue.commandPool, static_cast<uint32_t>( MAX_FRAMES_STATES ), graphicsQueue.commandBuffers );
	}

	void RecreateSwapChain()
	{
		assert( 0 );

		int width = 0, height = 0;
		window.GetWindowFrameBufferSize( width, height, true );

		vkDeviceWaitIdle( context.device );

		CleanupFrameResources();
		swapChain.Destroy();
		swapChain.Create( &window );
	}

	void CreateDescriptorSets( VkDescriptorSetLayout& layout, VkDescriptorSet descSets[ MAX_FRAMES_STATES ] )
	{
		std::vector<VkDescriptorSetLayout> layouts( swapChain.GetBufferCount(), layout );
		VkDescriptorSetAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = MAX_FRAMES_STATES;
		allocInfo.pSetLayouts = layouts.data();

		if ( vkAllocateDescriptorSets( context.device, &allocInfo, descSets ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to allocate descriptor sets!" );
		}
	}

	void UpdateFrameDescSet( const int currentImage )
	{
		const int i = currentImage;

		//////////////////////////////////////////////////////
		//													//
		// Scene Descriptor Sets							//
		//													//
		//////////////////////////////////////////////////////

		std::vector<VkDescriptorBufferInfo> globalConstantsInfo;
		globalConstantsInfo.reserve( 1 );
		{
			VkDescriptorBufferInfo info{ };
			info.buffer = frameState[ i ].globalConstants.GetVkObject();
			info.offset = 0;
			info.range = sizeof( globalUboConstants_t );
			globalConstantsInfo.push_back( info );
		}

		std::vector<VkDescriptorBufferInfo> bufferInfo;
		bufferInfo.reserve( MaxSurfaces );
		for ( int j = 0; j < MaxSurfaces; ++j )
		{
			VkDescriptorBufferInfo info{ };
			info.buffer = frameState[ i ].surfParms.GetVkObject();
			info.offset = j * sizeof( uniformBufferObject_t );
			info.range = sizeof( uniformBufferObject_t );
			bufferInfo.push_back( info );
		}

		std::vector<VkDescriptorImageInfo> imageInfo;
		imageInfo.reserve( MaxImageDescriptors );
		for ( auto it = textures.begin(); it != textures.end(); ++it )
		{
			texture_t& texture = it->second;
			VkImageView& imageView = texture.vk_imageView;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			imageInfo.push_back( info );
		}
		// Defaults
		for ( size_t j = textures.size(); j < MaxImageDescriptors; ++j )
		{
			texture_t& texture = textures[ "checker.png" ];
			VkImageView& imageView = texture.vk_imageView;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			imageInfo.push_back( info );
		}

		std::vector<VkDescriptorBufferInfo> materialBufferInfo;
		materialBufferInfo.reserve( MaxMaterialDescriptors );
		for ( int j = 0; j < MaxMaterialDescriptors; ++j )
		{
			VkDescriptorBufferInfo info{ };
			info.buffer = frameState[ i ].materialBuffers.GetVkObject();
			info.offset = j * sizeof( materialBufferObject_t );
			info.range = sizeof( materialBufferObject_t );
			materialBufferInfo.push_back( info );
		}

		std::vector<VkDescriptorBufferInfo> lightBufferInfo;
		lightBufferInfo.reserve( MaxLights );
		for ( int j = 0; j < MaxLights; ++j )
		{
			VkDescriptorBufferInfo info{ };
			info.buffer = frameState[ i ].lightParms.GetVkObject();
			info.offset = j * sizeof( light_t );
			info.range = sizeof( light_t );
			lightBufferInfo.push_back( info );
		}

		std::vector<VkDescriptorImageInfo> codeImageInfo;
		codeImageInfo.reserve( MaxCodeImages );
		// Shadow Map
		{
			VkImageView& imageView = frameState[ currentImage ].shadowMapImage.view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_depthShadowSampler;
			codeImageInfo.push_back( info );
		}

		const uint32_t descriptorSetCnt = 6;
		std::array<VkWriteDescriptorSet, descriptorSetCnt> descriptorWrites{ };

		uint32_t descriptorId = 0;
		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 0;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = 1;
		descriptorWrites[ descriptorId ].pBufferInfo = &globalConstantsInfo[ 0 ];
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 1;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxSurfaces;
		descriptorWrites[ descriptorId ].pBufferInfo = &bufferInfo[ 0 ];
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 2;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
		descriptorWrites[ descriptorId ].pImageInfo = &imageInfo[ 0 ];
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 3;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxMaterialDescriptors;
		descriptorWrites[ descriptorId ].pBufferInfo = &materialBufferInfo[ 0 ]; // TODO: replace
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 4;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxLights;
		descriptorWrites[ descriptorId ].pBufferInfo = &lightBufferInfo[ 0 ];
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 5;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxCodeImages;
		descriptorWrites[ descriptorId ].pImageInfo = &codeImageInfo[ 0 ];
		++descriptorId;

		assert( descriptorId == descriptorSetCnt );

		vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( descriptorWrites.size() ), descriptorWrites.data(), 0, nullptr );

		//////////////////////////////////////////////////////
		//													//
		// Shadow Descriptor Sets							//
		//													//
		//////////////////////////////////////////////////////

		std::vector<VkDescriptorImageInfo> shadowImageInfo;
		shadowImageInfo.reserve( MaxImageDescriptors );
		for ( auto it = textures.begin(); it != textures.end(); ++it )
		{
			texture_t& texture = it->second;
			VkImageView& imageView = texture.vk_imageView;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			shadowImageInfo.push_back( info );
		}
		// Defaults
		for( size_t j = textures.size(); j < MaxImageDescriptors; ++j )
		{
			texture_t& texture = textures[ "checker.png" ];
			VkImageView& imageView = texture.vk_imageView;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			shadowImageInfo.push_back( info );
		}

		std::vector<VkDescriptorImageInfo> shadowCodeImageInfo;
		shadowCodeImageInfo.reserve( MaxCodeImages );
		for ( size_t j = 0; j < MaxCodeImages; ++j )
		{
			texture_t& texture = textures[ "checker.png" ];
			VkImageView& imageView = texture.vk_imageView;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			shadowCodeImageInfo.push_back( info );
		}

		std::array<VkWriteDescriptorSet, descriptorSetCnt> shadowDescriptorWrites{ };

		descriptorId = 0;
		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 0;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = 1;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &globalConstantsInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 1;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxSurfaces;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &bufferInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 2;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
		shadowDescriptorWrites[ descriptorId ].pImageInfo = &shadowImageInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 3;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxMaterialDescriptors;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &materialBufferInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 4;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxLights;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &lightBufferInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 5;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxCodeImages;
		shadowDescriptorWrites[ descriptorId ].pImageInfo = &shadowCodeImageInfo[ 0 ];
		++descriptorId;

		vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( shadowDescriptorWrites.size() ), shadowDescriptorWrites.data(), 0, nullptr );

		//////////////////////////////////////////////////////
		//													//
		// Post Descriptor Sets								//
		//													//
		//////////////////////////////////////////////////////
		std::vector<VkDescriptorImageInfo> postImageInfo;
		postImageInfo.reserve( 2 );
		// View Color Map
		{
			VkImageView& imageView = frameState[ currentImage ].viewColorImage.view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			postImageInfo.push_back( info );
		}
		// View Depth Map
		{
			VkImageView& imageView = frameState[ currentImage ].depthImage.view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_depthShadowSampler;
			postImageInfo.push_back( info );
		}

		std::array<VkWriteDescriptorSet, 6> postDescriptorWrites{ };

		descriptorId = 0;
		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 0;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = 1;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &globalConstantsInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 1;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxSurfaces;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &bufferInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 2;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
		postDescriptorWrites[ descriptorId ].pImageInfo = &imageInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 3;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxMaterialDescriptors;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &materialBufferInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 4;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxLights;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &lightBufferInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 5;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxCodeImages;
		postDescriptorWrites[ descriptorId ].pImageInfo = &postImageInfo[ 0 ];
		++descriptorId;

		vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( postDescriptorWrites.size() ), postDescriptorWrites.data(), 0, nullptr );
	}

	void UpdateDescriptorSets()
	{
		const uint32_t frameBufferCount = static_cast<uint32_t>( swapChain.GetBufferCount() );
		for ( size_t i = 0; i < frameBufferCount; i++ )
		{
			UpdateFrameDescSet( static_cast<uint32_t>( i ) );
		}
	}

	uint32_t FindMemoryType( uint32_t typeFilter, VkMemoryPropertyFlags properties )
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );

		for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ )
		{
			if ( typeFilter & ( 1 << i ) && ( memProperties.memoryTypes[i].propertyFlags & properties ) == properties )
			{
				return i;
			}
		}

		throw std::runtime_error( "Failed to find suitable memory type!" );
	}

	VkFormat FindDepthFormat()
	{
		return FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	VkFormat FindSupportedFormat( const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features )
	{
		for ( VkFormat format : candidates )
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties( context.physicalDevice, format, &props );

			if ( tiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & features ) == features )
			{
				return format;
			}
			else if ( tiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & features ) == features )
			{
				return format;
			}
		}

		throw std::runtime_error( "Failed to find supported format!" );
	}

	bool HasStencilComponent( VkFormat format )
	{
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	void AllocateDeviceMemory( const uint32_t allocSize, const uint32_t typeIndex, MemoryAllocator& outAllocation )
	{
		VkMemoryAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = allocSize;
		allocInfo.memoryTypeIndex = typeIndex;

		VkDeviceMemory memory;
		if ( vkAllocateMemory( context.device, &allocInfo, nullptr, &memory ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to allocate buffer memory!" );
		}

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );
		VkMemoryType type = memProperties.memoryTypes[ typeIndex ];
		
		void* memPtr = nullptr;
		if ( ( type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) == 0 ) {
			if ( vkMapMemory( context.device, memory, 0, VK_WHOLE_SIZE, 0, &memPtr ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to map memory to context.device memory!" );
			}
		}

		outAllocation.Bind( memory, memPtr, allocSize, typeIndex );
	}

	void CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, MemoryAllocator& bufferMemory, AllocRecord& subAlloc )
	{
		VkBufferCreateInfo bufferInfo{ };
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if ( vkCreateBuffer( context.device, &bufferInfo, nullptr, &buffer ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create buffer!" );
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements( context.device, buffer, &memRequirements );

		VkMemoryAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType( memRequirements.memoryTypeBits, properties );

		if ( bufferMemory.CreateAllocation( memRequirements.alignment, memRequirements.size, subAlloc ) )
		{
			vkBindBufferMemory( context.device, buffer, bufferMemory.GetDeviceMemory(), subAlloc.offset );
		}
		else
		{
			throw std::runtime_error( "buffer could not allocate!" );
		}
	}

	void UploadToGPU( VkBuffer srcBuffer, VkBuffer dstBuffer, VkBufferCopy copyRegion )
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
		vkCmdCopyBuffer( commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion );
		EndSingleTimeCommands( commandBuffer );
	}

	void CreateTextureSampler( VkSampler& outSampler )
	{
		VkSamplerCreateInfo samplerInfo{ };
		samplerInfo.sType					= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter				= VK_FILTER_LINEAR;
		samplerInfo.minFilter				= VK_FILTER_LINEAR;
		samplerInfo.addressModeU			= VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV			= VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW			= VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable		= VK_TRUE;
		samplerInfo.maxAnisotropy			= 16.0f;
		samplerInfo.borderColor				= VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates	= VK_FALSE;
		samplerInfo.compareEnable			= VK_FALSE;
		samplerInfo.compareOp				= VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode				= VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod					= 0.0f;
		samplerInfo.maxLod					= 16.0f;
		samplerInfo.mipLodBias				= 0.0f;

		if ( vkCreateSampler( context.device, &samplerInfo, nullptr, &outSampler ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create texture sampler!" );
		}
	}

	void CreateDepthSampler( VkSampler& outSampler )
	{
		VkSamplerCreateInfo samplerInfo{ };
		samplerInfo.sType					= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter				= VK_FILTER_LINEAR;
		samplerInfo.minFilter				= VK_FILTER_LINEAR;
		samplerInfo.addressModeU			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeV			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeW			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.anisotropyEnable		= VK_FALSE;
		samplerInfo.maxAnisotropy			= 0.0f;
		samplerInfo.borderColor				= VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		samplerInfo.unnormalizedCoordinates	= VK_FALSE;
		samplerInfo.compareEnable			= VK_FALSE;
		samplerInfo.compareOp				= VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode				= VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod					= 0.0f;
		samplerInfo.maxLod					= 16.0f;
		samplerInfo.mipLodBias				= 0.0f;

		if ( vkCreateSampler( context.device, &samplerInfo, nullptr, &outSampler ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create depth sampler!" );
		}
	}

	void CreateVertexBuffer( modelSource_t& surf, VkBuffer& vb, AllocRecord& subAlloc )
	{
		VkDeviceSize bufferSize = sizeof( surf.vertices[0] ) * surf.vertices.size();

		stagingBuffer.Reset();
		stagingBuffer.CopyData( surf.vertices.data(), static_cast<size_t>( bufferSize ) );

		CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vb, localMemory, subAlloc );

		VkBufferCopy copyRegion{ };
		copyRegion.size = bufferSize;
		UploadToGPU( stagingBuffer.buffer, vb, copyRegion );
	}

	void CreateIndexBuffer( modelSource_t& surf, VkBuffer& ib, AllocRecord& subAlloc )
	{
		VkDeviceSize bufferSize = sizeof( surf.indices[0] ) * surf.indices.size();

		stagingBuffer.Reset();
		stagingBuffer.CopyData( surf.indices.data(), static_cast< size_t >( bufferSize ) );

		CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ib, localMemory, subAlloc );

		VkBufferCopy copyRegion{ };
		copyRegion.size = bufferSize;
		UploadToGPU( stagingBuffer.buffer, ib, copyRegion );
	}

	void CreateUniformBuffers()
	{
		for ( size_t i = 0; i < swapChain.GetBufferCount(); ++i )
		{
			// Globals Buffer
			{
				const VkDeviceSize stride = std::max( context.limits.minUniformBufferOffsetAlignment, sizeof( globalUboConstants_t ) );
				const VkDeviceSize bufferSize = stride;
				CreateBuffer( bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, frameState[ i ].globalConstants.buffer, sharedMemory, frameState[ i ].globalConstants.allocation );
			}

			// Model Buffer
			{
				const VkDeviceSize stride = std::max( context.limits.minUniformBufferOffsetAlignment, sizeof( uniformBufferObject_t ) );
				const VkDeviceSize bufferSize = MaxSurfaces * stride;
				CreateBuffer( bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, frameState[ i ].surfParms.buffer, sharedMemory, frameState[ i ].surfParms.allocation );
			}

			// Material Buffer
			{
				const VkDeviceSize stride = std::max( context.limits.minUniformBufferOffsetAlignment, sizeof( materialBufferObject_t ) );
				const VkDeviceSize materialBufferSize = MaxMaterialDescriptors * stride;
				CreateBuffer( materialBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, frameState[ i ].materialBuffers.buffer, sharedMemory, frameState[ i ].materialBuffers.allocation );
			}

			// Light Buffer
			{
				const VkDeviceSize stride = std::max( context.limits.minUniformBufferOffsetAlignment, sizeof( light_t ) );
				const VkDeviceSize lightBufferSize = MaxLights * stride;
				CreateBuffer( lightBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, frameState[ i ].lightParms.buffer, sharedMemory, frameState[ i ].lightParms.allocation );
			}
		}
	}

	void CreateMainRenderPass( VkRenderPass& pass )
	{
		VkAttachmentDescription colorAttachment{ };
		colorAttachment.format = swapChain.GetBackBufferFormat();
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference colorAttachmentRef{ };
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment{ };
		depthAttachment.format = FindDepthFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{ };
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{ };
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[ 0 ].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[ 0 ].dstSubpass = 0;
		dependencies[ 0 ].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[ 0 ].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[ 0 ].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[ 0 ].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[ 0 ].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[ 1 ].srcSubpass = 0;
		dependencies[ 1 ].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[ 1 ].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[ 1 ].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[ 1 ].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[ 1 ].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[ 1 ].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
		VkRenderPassCreateInfo renderPassInfo{ };
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = static_cast<uint32_t>( dependencies.size() );
		renderPassInfo.pDependencies = dependencies.data();

		if ( vkCreateRenderPass( context.device, &renderPassInfo, nullptr, &pass ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create render pass!" );
		}
	}

	void CreateShadowPass( VkRenderPass& pass )
	{
		VkAttachmentDescription colorAttachment{ };
		colorAttachment.format = swapChain.GetBackBufferFormat();
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{ };
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment{ };
		depthAttachment.format = FindDepthFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{ };
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{ };
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[ 0 ].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[ 0 ].dstSubpass = 0;
		dependencies[ 0 ].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[ 0 ].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependencies[ 0 ].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[ 0 ].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[ 0 ].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[ 1 ].srcSubpass = 0;
		dependencies[ 1 ].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[ 1 ].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[ 1 ].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[ 1 ].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[ 1 ].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[ 1 ].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
		VkRenderPassCreateInfo renderPassInfo{ };
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = static_cast<uint32_t>( dependencies.size() );
		renderPassInfo.pDependencies = dependencies.data();

		if ( vkCreateRenderPass( context.device, &renderPassInfo, nullptr, &pass ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create render pass!" );
		}
	}

	void CreatePostProcessPass( VkRenderPass& pass )
	{
		VkAttachmentDescription colorAttachment{ };
		colorAttachment.format = swapChain.GetBackBufferFormat();
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{ };
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{ };
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = nullptr;

		VkSubpassDependency dependency{ };
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		std::array<VkAttachmentDescription, 1> attachments = { colorAttachment };
		VkRenderPassCreateInfo renderPassInfo{ };
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if ( vkCreateRenderPass( context.device, &renderPassInfo, nullptr, &pass ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create render pass!" );
		}
	}

	void CreateFramebuffers()
	{
		/////////////////////////////////
		//       Shadow Map Render     //
		/////////////////////////////////
		const VkFormat depthFormat = FindDepthFormat();
		for ( size_t i = 0; i < MAX_FRAMES_STATES; ++i )
		{
			imageAllocations.push_back( AllocRecord() );
			CreateImage( ShadowMapWidth, ShadowMapHeight, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameState[ i ].shadowMapImage.image, localMemory, imageAllocations.back() );
			frameState[ i ].shadowMapImage.view = CreateImageView( frameState[ i ].shadowMapImage.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1 );
		}

		for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ )
		{
			std::array<VkImageView, 2> attachments = {
				r.whiteImage.view,
				frameState[ i ].shadowMapImage.view,
			};

			VkFramebufferCreateInfo framebufferInfo{ };
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = shadowPassState.pass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = ShadowMapWidth;
			framebufferInfo.height = ShadowMapHeight;
			framebufferInfo.layers = 1;

			if ( vkCreateFramebuffer( context.device, &framebufferInfo, nullptr, &shadowPassState.fb[ i ] ) != VK_SUCCESS )
			{
				throw std::runtime_error( "Failed to create framebuffer!" );
			}
		}

		/////////////////////////////////
		//     Main Scene 3D Render    //
		/////////////////////////////////
		for ( size_t i = 0; i < swapChain.GetBufferCount(); i++ )
		{
			imageAllocations.push_back( AllocRecord() );
			CreateImage( DISPLAY_WIDTH, DISPLAY_HEIGHT, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameState[ i ].viewColorImage.image, localMemory, imageAllocations.back() );
			frameState[ i ].viewColorImage.view = CreateImageView( frameState[ i ].viewColorImage.image, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1 );

			VkFormat depthFormat = FindDepthFormat();
			imageAllocations.push_back( AllocRecord() );
			CreateImage( DISPLAY_WIDTH, DISPLAY_HEIGHT, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameState[ i ].depthImage.image, localMemory, imageAllocations.back() );
			frameState[ i ].depthImage.view = CreateImageView( frameState[ i ].depthImage.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1 );

			std::array<VkImageView, 2> attachments = {
				frameState[ i ].viewColorImage.view,
				frameState[ i ].depthImage.view
			};

			VkFramebufferCreateInfo framebufferInfo{ };
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = mainPassState.pass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = DISPLAY_WIDTH;
			framebufferInfo.height = DISPLAY_HEIGHT;
			framebufferInfo.layers = 1;

			if ( vkCreateFramebuffer( context.device, &framebufferInfo, nullptr, &mainPassState.fb[ i ] ) != VK_SUCCESS )
			{
				throw std::runtime_error( "Failed to create scene framebuffer!" );
			}
		}
		/////////////////////////////////
		//       Swap Chain Images     //
		/////////////////////////////////
		swapChain.vk_framebuffers.resize( swapChain.GetBufferCount() );
		for ( size_t i = 0; i < swapChain.GetBufferCount(); i++ )
		{
			std::array<VkImageView, 1> attachments = {
				swapChain.vk_swapChainImageViews[ i ]
			};

			VkFramebufferCreateInfo framebufferInfo{ };
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = postPassState.pass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapChain.vk_swapChainExtent.width;
			framebufferInfo.height = swapChain.vk_swapChainExtent.height;
			framebufferInfo.layers = 1;

			if ( vkCreateFramebuffer( context.device, &framebufferInfo, nullptr, &swapChain.vk_framebuffers[ i ] ) != VK_SUCCESS )
			{
				throw std::runtime_error( "Failed to create swap chain framebuffer!" );
			}
			postPassState.fb[ i ] = swapChain.vk_framebuffers[ i ];
		}
	}

	void CreateCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
		poolInfo.flags = 0; // Optional

		if ( vkCreateCommandPool( context.device, &poolInfo, nullptr, &graphicsQueue.commandPool ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create command pool!" );
		}
	}

	void CreateDescriptorPool()
	{
		std::array<VkDescriptorPoolSize, 3> poolSizes{ };
		poolSizes[ 0 ].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[ 0 ].descriptorCount = DescriptorPoolMaxUniformBuffers;
		poolSizes[ 1 ].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[ 1 ].descriptorCount = DescriptorPoolMaxComboImages;
		poolSizes[ 2 ].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		poolSizes[ 2 ].descriptorCount = DescriptorPoolMaxImages;

		VkDescriptorPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = DescriptorPoolMaxSets;

		if ( vkCreateDescriptorPool( context.device, &poolInfo, nullptr, &descriptorPool ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create descriptor pool!" );
		}
	}

	VkCommandBuffer BeginSingleTimeCommands()
	{
		VkCommandBufferAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = graphicsQueue.commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers( context.device, &allocInfo, &commandBuffer );

		VkCommandBufferBeginInfo beginInfo{ };
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer( commandBuffer, &beginInfo );

		return commandBuffer;
	}

	void EndSingleTimeCommands( VkCommandBuffer commandBuffer )
	{
		vkEndCommandBuffer( commandBuffer );

		VkSubmitInfo submitInfo{ };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit( context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE );
		vkQueueWaitIdle( context.graphicsQueue );

		vkFreeCommandBuffers( context.device, graphicsQueue.commandPool, 1, &commandBuffer );
	}

	void TransitionImageLayout( VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels )
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		VkImageMemoryBarrier barrier{ };
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if ( oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if ( oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else {
			throw std::invalid_argument( "unsupported layout transition!" );
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		EndSingleTimeCommands( commandBuffer );
	}

	void CopyBufferToImage( VkCommandBuffer& commandBuffer, VkBuffer& buffer, const VkDeviceSize bufferOffset, VkImage& image, const uint32_t width, const uint32_t height )
	{
		VkBufferImageCopy region{ };
		memset( &region, 0, sizeof( region ) );
		region.bufferOffset = bufferOffset;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			width,
			height,
			1
		};

		vkCmdCopyBufferToImage(
			commandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);
	}

	void CreateCommandBuffers( RenderView& view )
	{
		VkCommandBufferAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = graphicsQueue.commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast< uint32_t >( MAX_FRAMES_STATES );

		if ( vkAllocateCommandBuffers( context.device, &allocInfo, graphicsQueue.commandBuffers ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to allocate command buffers!" );
		}

		for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ )
		{
			VkCommandBufferBeginInfo beginInfo{ };
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0; // Optional
			beginInfo.pInheritanceInfo = nullptr; // Optional

			if ( vkBeginCommandBuffer( graphicsQueue.commandBuffers[ i ], &beginInfo ) != VK_SUCCESS )
			{
				throw std::runtime_error( "Failed to begin recording command buffer!" );
			}

			VkBuffer vertexBuffers[] = { vb.GetVkObject() };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers( graphicsQueue.commandBuffers[ i ], 0, 1, vertexBuffers, offsets );
			vkCmdBindIndexBuffer( graphicsQueue.commandBuffers[ i ], ib.GetVkObject(), 0, VK_INDEX_TYPE_UINT32 );

			// Shadow Passes
			{
				VkRenderPassBeginInfo shadowPassInfo{ };
				shadowPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				shadowPassInfo.renderPass = shadowPassState.pass;
				shadowPassInfo.framebuffer = shadowPassState.fb[ i ];
				shadowPassInfo.renderArea.offset = { 0, 0 };
				shadowPassInfo.renderArea.extent = { ShadowMapWidth, ShadowMapHeight };

				std::array<VkClearValue, 2> shadowClearValues{ };
				shadowClearValues[ 0 ].color = { 1.0f, 1.0f, 1.0f, 1.0f };
				shadowClearValues[ 1 ].depthStencil = { 1.0f, 0 };

				shadowPassInfo.clearValueCount = static_cast<uint32_t>( shadowClearValues.size() );
				shadowPassInfo.pClearValues = shadowClearValues.data();

				// TODO: how to handle views better?
				vkCmdBeginRenderPass( graphicsQueue.commandBuffers[ i ], &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE );

				for ( size_t surfIx = 0; surfIx < shadowView.committedModelCnt; surfIx++ )
				{
					drawSurf_t& surface = shadowView.surfaces[ surfIx ];

					pipelineObject_t* pipelineObject;
					if ( surface.pipelineObject[ DRAWPASS_SHADOW ] == INVALID_HANDLE ) {
						continue;
					}
					GetPipelineObject( surface.pipelineObject[ DRAWPASS_SHADOW ], &pipelineObject );

					// vkCmdSetDepthBias
					vkCmdBindPipeline( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
					vkCmdBindDescriptorSets( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &shadowPassState.descriptorSets[ i ], 0, nullptr );

					pushConstants_t pushConstants = { surface.objectId, surface.materialId };
					vkCmdPushConstants( graphicsQueue.commandBuffers[ i ], pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

					vkCmdDrawIndexed( graphicsQueue.commandBuffers[ i ], surface.indicesCnt, 1, surface.firstIndex, surface.vertexOffset, 0 );
				}
				vkCmdEndRenderPass( graphicsQueue.commandBuffers[ i ] );
			}

			// Main Passes
			{
				VkRenderPassBeginInfo renderPassInfo{ };
				renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassInfo.renderPass = mainPassState.pass;
				renderPassInfo.framebuffer = mainPassState.fb[ i ];
				renderPassInfo.renderArea.offset = { 0, 0 };
				renderPassInfo.renderArea.extent = swapChain.vk_swapChainExtent;

				std::array<VkClearValue, 2> clearValues{ };
				clearValues[ 0 ].color = { 0.0f, 0.1f, 0.5f, 1.0f };
				clearValues[ 1 ].depthStencil = { 0.0f, 0 };

				renderPassInfo.clearValueCount = static_cast<uint32_t>( clearValues.size() );
				renderPassInfo.pClearValues = clearValues.data();

				vkCmdBeginRenderPass( graphicsQueue.commandBuffers[ i ], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

				for ( uint32_t pass = DRAWPASS_DEPTH; pass < DRAWPASS_POST_2D; ++pass )
				{
					for ( size_t surfIx = 0; surfIx < view.committedModelCnt; surfIx++ )
					{
						drawSurf_t& surface = view.surfaces[ surfIx ];

						pipelineObject_t* pipelineObject;
						if ( surface.pipelineObject[ pass ] == INVALID_HANDLE ) {
							continue;
						}
						if ( ( pass == DRAWPASS_WIREFRAME ) && ( ( surface.flags & WIREFRAME ) == 0 ) ) {
							continue;
						}
						GetPipelineObject( surface.pipelineObject[ pass ], &pipelineObject );

						vkCmdBindPipeline( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
						vkCmdBindDescriptorSets( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &mainPassState.descriptorSets[ i ], 0, nullptr );

						pushConstants_t pushConstants = { surface.objectId, surface.materialId };
						vkCmdPushConstants( graphicsQueue.commandBuffers[ i ], pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

						vkCmdDrawIndexed( graphicsQueue.commandBuffers[ i ], surface.indicesCnt, 1, surface.firstIndex, surface.vertexOffset, 0 );
					}
				}
				vkCmdEndRenderPass( graphicsQueue.commandBuffers[ i ] );
			}

			// Post Process Passes
			{
				VkRenderPassBeginInfo postProcessPassInfo{ };
				postProcessPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				postProcessPassInfo.renderPass = postPassState.pass;
				postProcessPassInfo.framebuffer = postPassState.fb[ i ];
				postProcessPassInfo.renderArea.offset = { 0, 0 };
				postProcessPassInfo.renderArea.extent = swapChain.vk_swapChainExtent;

				std::array<VkClearValue, 2> postProcessClearValues{ };
				postProcessClearValues[ 0 ].color = { 0.0f, 0.1f, 0.5f, 1.0f };
				postProcessClearValues[ 1 ].depthStencil = { 0.0f, 0 };

				postProcessPassInfo.clearValueCount = static_cast<uint32_t>( postProcessClearValues.size() );
				postProcessPassInfo.pClearValues = postProcessClearValues.data();

				vkCmdBeginRenderPass( graphicsQueue.commandBuffers[ i ], &postProcessPassInfo, VK_SUBPASS_CONTENTS_INLINE );
				for ( size_t surfIx = 0; surfIx < view.committedModelCnt; surfIx++ )
				{
					drawSurf_t& surface = shadowView.surfaces[ surfIx ];
					if ( surface.pipelineObject[ DRAWPASS_POST_2D ] == INVALID_HANDLE ) {
						continue;
					}
					pipelineObject_t* pipelineObject;
					GetPipelineObject( surface.pipelineObject[ DRAWPASS_POST_2D ], &pipelineObject );
					vkCmdBindPipeline( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
					vkCmdBindDescriptorSets( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &postPassState.descriptorSets[ i ], 0, nullptr );

					pushConstants_t pushConstants = { surface.objectId, surface.materialId };
					vkCmdPushConstants( graphicsQueue.commandBuffers[ i ], pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

					vkCmdDrawIndexed( graphicsQueue.commandBuffers[ i ], surface.indicesCnt, 1, surface.firstIndex, surface.vertexOffset, 0 );
				}

#if defined( USE_IMGUI )
				ImGui_ImplVulkan_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();

				ImGui::Begin( "Control Panel" );
				ImGui::InputFloat( "Heightmap Height", &imguiControls.heightMapHeight, 0.1f, 1.0f );
				ImGui::InputFloat( "Tone Map R", &imguiControls.toneMapColor[ 0 ], 0.1f, 1.0f );
				ImGui::InputFloat( "Tone Map G", &imguiControls.toneMapColor[ 1 ], 0.1f, 1.0f );
				ImGui::InputFloat( "Tone Map B", &imguiControls.toneMapColor[ 2 ], 0.1f, 1.0f );
				ImGui::InputFloat( "Tone Map A", &imguiControls.toneMapColor[ 3 ], 0.1f, 1.0f );
				ImGui::InputInt( "Image Id", &imguiControls.dbgImageId );
				ImGui::Text( "Mouse: (%f, %f )", (float) window.input.mouse.x, (float)window.input.mouse.y );
				const glm::vec2 screenPoint = glm::vec2( (float)window.input.mouse.x, (float)window.input.mouse.y );
				const glm::vec2 ndc = 2.0f * screenPoint * glm::vec2( 1.0f / DISPLAY_WIDTH, 1.0f / DISPLAY_HEIGHT ) - 1.0f;
				char entityName[ 256 ];
				if ( imguiControls.selectedModelId >= 0 ) {
					modelSource_t& model = scene.models[ scene.entities[ imguiControls.selectedModelId ].modelId ];
					sprintf_s( entityName, "%i: %s", imguiControls.selectedModelId, model.name.c_str() );
				} else {
					memset( &entityName[ 0 ], 0, 256 );
				}
				static glm::vec3 tempOrigin;
				ImGui::Text( "NDC: (%f, %f )", (float) ndc.x, (float) ndc.y );
				ImGui::Text( "Camera Origin: (%f, %f, %f)", scene.camera.GetOrigin().x, scene.camera.GetOrigin().y, scene.camera.GetOrigin().z );
				if ( ImGui::BeginCombo( "Models", entityName ) )
				{
					for ( int entityIx = 0; entityIx < scene.entities.size(); ++entityIx ) {
						modelSource_t& model = scene.models[ scene.entities[ entityIx ].modelId ];
						sprintf_s( entityName, "%i: %s", entityIx, model.name.c_str() );
						if ( ImGui::Selectable( entityName, ( imguiControls.selectedModelId == entityIx ) ) ) {
							imguiControls.selectedModelId = entityIx;
							scene.entities[ entityIx ].flags = WIREFRAME;
							tempOrigin.x = scene.entities[ entityIx ].matrix[ 3 ][ 0 ];
							tempOrigin.y = scene.entities[ entityIx ].matrix[ 3 ][ 1 ];
							tempOrigin.z = scene.entities[ entityIx ].matrix[ 3 ][ 2 ];
						} else {
							scene.entities[ entityIx ].flags = (renderFlags) ( scene.entities[ entityIx ].flags & ~WIREFRAME );
						}
					}
					ImGui::EndCombo();
				}
				ImGui::InputFloat( "Selected Model X: ", &imguiControls.selectedModelOrigin.x, 0.1f, 1.0f );
				ImGui::InputFloat( "Selected Model Y: ", &imguiControls.selectedModelOrigin.y, 0.1f, 1.0f );
				ImGui::InputFloat( "Selected Model Z: ", &imguiControls.selectedModelOrigin.z, 0.1f, 1.0f );

				if ( imguiControls.selectedModelId >= 0 ) {
					entity_t& entity = scene.entities[ imguiControls.selectedModelId ];
					entity.matrix[ 3 ][ 0 ] = tempOrigin.x + imguiControls.selectedModelOrigin.x;
					entity.matrix[ 3 ][ 1 ] = tempOrigin.y + imguiControls.selectedModelOrigin.y;
					entity.matrix[ 3 ][ 2 ] = tempOrigin.z + imguiControls.selectedModelOrigin.z;
				}
				//ImGui::Text( "Model %i: %s", 0, models[ 0 ].name.c_str() );
				ImGui::End();

				// Render dear imgui into screen
				ImGui::Render();
				ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), graphicsQueue.commandBuffers[ i ] );
#endif
				vkCmdEndRenderPass( graphicsQueue.commandBuffers[ i ] );
			}


			if ( vkEndCommandBuffer( graphicsQueue.commandBuffers[i] ) != VK_SUCCESS )
			{
				throw std::runtime_error( "Failed to record command buffer!" );
			}
		}
	}

	void CreateImage( uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, MemoryAllocator& memory, AllocRecord& subAlloc )
	{
		VkImageCreateInfo imageInfo{ };
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast< uint32_t >( width );
		imageInfo.extent.height = static_cast< uint32_t >( height );
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0; // Optional

		if ( vkCreateImage( context.device, &imageInfo, nullptr, &image ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create image!" );
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements( context.device, image, &memRequirements );

		VkMemoryAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType( memRequirements.memoryTypeBits, properties );

		if ( memory.CreateAllocation( memRequirements.alignment, memRequirements.size, subAlloc ) )
		{
			vkBindImageMemory( context.device, image, memory.GetDeviceMemory(), subAlloc.offset );
		}
		else
		{
			throw std::runtime_error( "Buffer could not be allocated!" );
		}
	}

	void GenerateMipmaps( VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels )
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties( context.physicalDevice, imageFormat, &formatProperties );

		if ( !( formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT ) )
		{
			throw std::runtime_error( "texture image format does not support linear blitting!" );
		}

		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		VkImageMemoryBarrier barrier{ };
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for ( uint32_t i = 1; i < mipLevels; i++ )
		{
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier( commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier );

			VkImageBlit blit{ };
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage( commandBuffer,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR );

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier( commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier );

			if ( mipWidth > 1 ) mipWidth /= 2;
			if ( mipHeight > 1 ) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier( commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier );

		EndSingleTimeCommands( commandBuffer );
	}

	void CreateResourceBuffers()
	{
		stagingBuffer.currentOffset = 0;
		stagingBuffer.size = 128 * MB;
		CreateBuffer( stagingBuffer.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.buffer, sharedMemory, stagingBuffer.allocation );
	}

	void DestroyResourceBuffers()
	{
	//	vkDestroyBuffer( context.device, stagingBuffer.buffer, nullptr );
	//	vkFreeMemory( context.device, stagingBuffer.memory, nullptr );
	}

	void CreateTextureImage( const std::vector<std::string>& texturePaths )
	{
		std::vector<textureSource_t> textureSources;
		textureSources.reserve( texturePaths.size() );
		for ( const std::string& texturePath : texturePaths )
		{
			textureSource_t texture;
			if ( LoadTextureImage( ( TexturePath + texturePath ).c_str(), texture ) ) {
				texture.name = texturePath.c_str();
				textureSources.push_back( texture );
			}
		}

		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
		for ( const textureSource_t& textureSource : textureSources )
		{
			texture_t texture;
			texture.width = textureSource.width;
			texture.height = textureSource.height;
			texture.channels = textureSource.channels;
			texture.mipLevels = static_cast< uint32_t >( std::floor( std::log2( std::max( texture.width, texture.height ) ) ) ) + 1;

			CreateImage( texture.width, texture.height, texture.mipLevels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.vk_image, localMemory, texture.memory );
		
			TransitionImageLayout( texture.vk_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.mipLevels );

			const VkDeviceSize currentOffset = stagingBuffer.currentOffset;
			stagingBuffer.CopyData( textureSource.bytes, textureSource.sizeBytes );

			CopyBufferToImage( commandBuffer, stagingBuffer.buffer, currentOffset, texture.vk_image, static_cast< uint32_t >( texture.width ), static_cast< uint32_t >( texture.height ) );
		
			textures[ textureSource.name ] = texture;
		}
		EndSingleTimeCommands( commandBuffer );

		for ( auto it = textures.begin(); it != textures.end(); ++it )
		{
			const texture_t& texture = it->second;
			GenerateMipmaps( texture.vk_image, VK_FORMAT_R8G8B8A8_SRGB, texture.width, texture.height, texture.mipLevels );
		}

		for ( auto it = textures.begin(); it != textures.end(); ++it )
		{
			texture_t& texture = it->second;
			texture.vk_imageView = CreateImageView( texture.vk_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, texture.mipLevels );
		}

		// Default Images
		imageAllocations.push_back( AllocRecord() );
		CreateImage( ShadowMapWidth, ShadowMapHeight, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, r.whiteImage.image, localMemory, imageAllocations.back() );
		r.whiteImage.view = CreateImageView( r.whiteImage.image, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1 );

		imageAllocations.push_back( AllocRecord() );
		CreateImage( ShadowMapWidth, ShadowMapHeight, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, r.blackImage.image, localMemory, imageAllocations.back() );
		r.blackImage.view = CreateImageView( r.blackImage.image, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1 );
	}

	void CreateSyncObjects()
	{
		VkSemaphoreCreateInfo semaphoreInfo{ };
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{ };
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
		{
			if ( vkCreateSemaphore( context.device, &semaphoreInfo, nullptr, &graphicsQueue.imageAvailableSemaphores[ i ] ) != VK_SUCCESS ||
				vkCreateSemaphore( context.device, &semaphoreInfo, nullptr, &graphicsQueue.renderFinishedSemaphores[ i ] ) != VK_SUCCESS ||
				vkCreateFence( context.device, &fenceInfo, nullptr, &graphicsQueue.inFlightFences[ i ] ) != VK_SUCCESS )
			{
				throw std::runtime_error( "Failed to create synchronization objects for a frame!" );
			}
		}
	}

	void InitScene( RenderView& view )
	{
		view.viewport.x = 0.0f;
		view.viewport.y = 0.0f;
		view.viewport.width = (float)DISPLAY_WIDTH;
		view.viewport.height = (float)DISPLAY_HEIGHT;
		view.viewport.near = 1.0f;
		view.viewport.far = 0.0f;

		scene.camera = Camera( glm::vec4( 0.0f, 1.66f, 1.0f, 0.0f ) );
		scene.camera.fov = glm::radians( 90.0f );
		scene.camera.far = farPlane;
		scene.camera.near = nearPlane;
		scene.camera.aspect = ( DISPLAY_WIDTH / (float)DISPLAY_HEIGHT );
	}

	void ProcessInput()
	{
		const input_t & input = window.input;
		if ( input.keys['D'] ) {
			scene.camera.MoveForward( 0.1f );
		}
		if ( input.keys['A'] ) {
			scene.camera.MoveForward( -0.1f );
		}
		if ( input.keys['W'] ) {
			scene.camera.MoveVertical( -0.1f );
		}
		if ( input.keys['S'] ) {
			scene.camera.MoveVertical( 0.1f );
		}
		if ( input.keys['+'] ) {
			scene.camera.fov++;
		}
		if ( input.keys['-'] ) {
			scene.camera.fov--;
		}

		const mouse_t & mouse = input.mouse;
		if( mouse.centered )
		{
			const float yawDelta = mouse.speed * static_cast<float>( 0.5f * DISPLAY_WIDTH - mouse.x );
			const float pitchDelta = -mouse.speed * static_cast<float>( 0.5f * DISPLAY_HEIGHT - mouse.y );
			scene.camera.SetYaw( yawDelta );
			scene.camera.SetPitch( pitchDelta );
		}
	}

	void MainLoop()
	{
		while ( !glfwWindowShouldClose( window.window ) )
		{
			glfwPollEvents();
			ProcessInput();
			UpdateScene();
			CommitScene();

			DrawFrame();
		}

		vkDeviceWaitIdle( context.device );
	}

	void UpdateScene()
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

		const glm::vec3 lightPos0 = glm::vec4( glm::cos( time ), 0.0f, -6.0f, 0.0f );
		const glm::vec3 lightDir0 = glm::vec4( 0.0f, 0.0f, 1.0f, 0.0f );

		renderView.viewMatrix = scene.camera.GetViewMatrix();
		renderView.projMatrix  = scene.camera.GetPerspectiveMatrix();
		renderView.lights[ 0 ].lightPos = glm::vec4( lightPos0[ 0 ], lightPos0[ 1 ], lightPos0[ 2 ], 0.0f );
		renderView.lights[ 0 ].intensity = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
		renderView.lights[ 0 ].lightDir = glm::vec4( lightDir0[ 0 ], lightDir0[ 1 ], lightDir0[ 2 ], 0.0f );
		renderView.lights[ 1 ].lightPos = glm::vec4( 0.0f, glm::cos( time ), -1.0f, 0.0 );
		renderView.lights[ 1 ].intensity = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
		renderView.lights[ 1 ].lightDir = glm::vec4( 0.0f, 0.0f, 1.0f, 0.0f );
		renderView.lights[ 2 ].lightPos = glm::vec4( glm::sin( time ), 0.0f, -1.0f, 0.0f );
		renderView.lights[ 2 ].intensity = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
		renderView.lights[ 2 ].lightDir = glm::vec4( 0.0f, 0.0f, 1.0f, 0.0f );

		const glm::vec3 shadowLightPos = lightPos0;
		const glm::vec3 shadowLightDir = lightDir0;

		// Temp shadow map set-up
		shadowView.viewport = renderView.viewport;
		shadowView.viewMatrix = MatrixFromVector( shadowLightDir );
		shadowView.viewMatrix = glm::transpose( shadowView.viewMatrix );
		shadowView.viewMatrix[ 3 ][ 0 ] = shadowLightPos[ 0 ];
		shadowView.viewMatrix[ 3 ][ 1 ] = shadowLightPos[ 1 ];
		shadowView.viewMatrix[ 3 ][ 2 ] = shadowLightPos[ 2 ];

		Camera shadowCam;
		shadowCam = Camera( glm::vec4( 0.0f, 0.0f, 0.0f, 0.0f ) );
		shadowCam.fov = glm::radians( 90.0f );
		shadowCam.far = nearPlane;
		shadowCam.near = farPlane;
		shadowCam.aspect = ( ShadowMapWidth / (float)ShadowMapHeight );
		shadowView.projMatrix = shadowCam.GetPerspectiveMatrix();

		// Skybox
		scene.entities[ 0 ].matrix = glm::identity<glm::mat4>();
		scene.entities[ 0 ].matrix[ 3 ][ 0 ] = scene.camera.GetOrigin()[ 0 ];
		scene.entities[ 0 ].matrix[ 3 ][ 1 ] = scene.camera.GetOrigin()[ 1 ];
		scene.entities[ 0 ].matrix[ 3 ][ 2 ] = scene.camera.GetOrigin()[ 2 ] - 0.5f;
	}

	void CommitScene()
	{
		renderView.committedModelCnt = 0;
		for ( uint32_t i = 0; i < scene.entities.size(); ++i )
		{
			CommitModel( renderView, scene.entities[ i ], 0 );
		}

		shadowView.committedModelCnt = 0;
		for ( uint32_t i = 0; i < scene.entities.size(); ++i )
		{
			if ( scene.entities[ i ].flags == NO_SHADOWS ) {
				continue;
			}
			CommitModel( shadowView, scene.entities[ i ], MaxModels );
		}
	}

	void WaitForEndFrame()
	{
		vkWaitForFences( context.device, 1, &graphicsQueue.inFlightFences[ currentFrame ], VK_TRUE, UINT64_MAX );
	}

	void DrawFrame()
	{
		WaitForEndFrame();

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR( context.device, swapChain.vk_swapChain, UINT64_MAX, graphicsQueue.imageAvailableSemaphores[ currentFrame ], VK_NULL_HANDLE, &imageIndex );

		if ( result == VK_ERROR_OUT_OF_DATE_KHR )
		{
			RecreateSwapChain();
			return;
		}
		else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR )
		{
			throw std::runtime_error( "Failed to acquire swap chain image!" );
		}

		// Check if a previous frame is using this image (i.e. there is its fence to wait on)
		if ( graphicsQueue.imagesInFlight[ imageIndex ] != VK_NULL_HANDLE ) {
			vkWaitForFences( context.device, 1, &graphicsQueue.imagesInFlight[ imageIndex ], VK_TRUE, UINT64_MAX );
		}
		// Mark the image as now being in use by this frame
		graphicsQueue.imagesInFlight[ imageIndex ] = graphicsQueue.inFlightFences[ currentFrame ];

		UpdateBufferContents( imageIndex );
		UpdateFrameDescSet( imageIndex );
		CreateCommandBuffers( renderView );

		VkSubmitInfo submitInfo{ };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { graphicsQueue.imageAvailableSemaphores[ currentFrame ] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &graphicsQueue.commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = { graphicsQueue.renderFinishedSemaphores[ currentFrame ] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences( context.device, 1, &graphicsQueue.inFlightFences[ currentFrame ] );

		if ( vkQueueSubmit( context.graphicsQueue, 1, &submitInfo, graphicsQueue.inFlightFences[ currentFrame ] ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to submit draw command buffer!" );
		}

		VkPresentInfoKHR presentInfo{ };
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain.GetApiObject() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr; // Optional

		result = vkQueuePresentKHR( context.presentQueue, &presentInfo );

		if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.IsResizeRequested() )
		{
			RecreateSwapChain();
			window.AcceptImageResize();
			return;
		}
		else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR )
		{
			throw std::runtime_error( "Failed to acquire swap chain image!" );
		}

		currentFrame = ( currentFrame + 1 ) % MAX_FRAMES_IN_FLIGHT;
	}

	void Cleanup()
	{
		CleanupFrameResources();
		swapChain.Destroy();

		ShutdownImGui();

		DestroyResourceBuffers();

		for ( auto it = textures.begin(); it != textures.end(); ++it )
		{
			const texture_t& texture = it->second;
			vkDestroyImageView( context.device, texture.vk_imageView, nullptr );
			vkDestroyImage( context.device, texture.vk_image, nullptr );
		}

		vkDestroySampler( context.device, vk_bilinearSampler, nullptr );
		vkDestroySampler( context.device, vk_depthShadowSampler, nullptr );

		for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
		{
			vkDestroySemaphore( context.device, graphicsQueue.renderFinishedSemaphores[ i ], nullptr );
			vkDestroySemaphore( context.device, graphicsQueue.imageAvailableSemaphores[ i ], nullptr );
			vkDestroyFence( context.device, graphicsQueue.inFlightFences[ i ], nullptr );
		}

		vkDestroyCommandPool( context.device, graphicsQueue.commandPool, nullptr );
		vkDestroyDevice( context.device, nullptr );

		if ( enableValidationLayers )
		{
			DestroyDebugUtilsMessengerEXT( context.instance, debugMessenger, nullptr );
		}

		vkDestroySurfaceKHR( context.instance, window.vk_surface, nullptr );
		vkDestroyInstance( context.instance, nullptr );

		window.~Window();
	}

	void UpdatePostProcessBuffers( uint32_t currentImage )
	{
		std::vector< uniformBufferObject_t > uboBuffer;
		uboBuffer.reserve( MaxSurfaces );
		for ( uint32_t i = 0; i < MaxSurfaces; ++i )
		{
			uniformBufferObject_t ubo;
			ubo.model = glm::identity<glm::mat4>();
			ubo.view = glm::identity<glm::mat4>();
			ubo.proj = glm::identity<glm::mat4>();
			uboBuffer.push_back( ubo );
		}

		void* uboMemoryMap = frameState[ currentImage ].surfParms.allocation.memory->GetMemoryMapPtr( frameState[ currentImage ].surfParms.allocation );
		if ( uboMemoryMap != nullptr ) {
			memcpy( uboMemoryMap, uboBuffer.data(), sizeof( uniformBufferObject_t ) * uboBuffer.size() );
		}
	}

	void UpdateBufferContents( uint32_t currentImage )
	{
		std::vector< globalUboConstants_t > globalsBuffer;
		globalsBuffer.reserve( 1 );
		{
			globalUboConstants_t globals;
			static auto startTime = std::chrono::high_resolution_clock::now();
			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

			float intPart = 0;
			const float fracPart = modf( time, &intPart );

			globals.time = glm::vec4( time, intPart, fracPart, 1.0f );
			globals.heightmap = glm::vec4( imguiControls.heightMapHeight, 0.0f, 0.0f, 0.0f );
			globals.tonemap = glm::vec4( imguiControls.toneMapColor[ 0 ], imguiControls.toneMapColor[ 1 ], imguiControls.toneMapColor[ 2 ], imguiControls.toneMapColor[ 3 ] );
			globalsBuffer.push_back( globals );
		}

		std::vector< uniformBufferObject_t > uboBuffer;
		uboBuffer.reserve( MaxSurfaces );
		assert( renderView.committedModelCnt < MaxModels );
		for ( uint32_t i = 0; i < renderView.committedModelCnt; ++i )
		{
			uniformBufferObject_t ubo;
			ubo.model = renderView.surfaces[ i ].modelMatrix;
			ubo.view = renderView.viewMatrix;
			ubo.proj = renderView.projMatrix;
			uboBuffer.push_back( ubo );
		}
		uboBuffer.resize( MaxModels );
		assert( shadowView.committedModelCnt < MaxModels );
		for ( uint32_t i = 0; i < shadowView.committedModelCnt; ++i )
		{
			uniformBufferObject_t ubo;
			ubo.model = shadowView.surfaces[ i ].modelMatrix;
			ubo.view = shadowView.viewMatrix;
			ubo.proj = shadowView.projMatrix;
			uboBuffer.push_back( ubo );
		}


		// Hack fix
		//materials[ "IMAGE_2D" ].texture0 = imguiControls.dbgImageId;

		std::vector< materialBufferObject_t > materialBuffer;
		materialBuffer.reserve( materials.size() );
		int materialAllocIx = 0;
		for ( auto it = materials.begin(); it != materials.end(); ++it )
		{
			materialBufferObject_t ubo;
			ubo.texture0 = it->second.texture0;
			ubo.texture1 = it->second.texture1;
			ubo.texture2 = it->second.texture2;
			ubo.texture3 = it->second.texture3;
			ubo.texture4 = it->second.texture4;
			ubo.texture5 = it->second.texture5;
			ubo.texture6 = it->second.texture6;
			ubo.texture7 = it->second.texture7;
			materialBuffer.push_back( ubo );
		}

		std::vector< light_t > lightBuffer;
		lightBuffer.reserve( MaxLights );
		for ( int i = 0; i < MaxLights; ++i )
		{
			lightBuffer.push_back( renderView.lights[ i ] );
		}

		frameState[ currentImage ].globalConstants.Reset();
		frameState[ currentImage ].globalConstants.CopyData( globalsBuffer.data(), sizeof( globalUboConstants_t ) * globalsBuffer.size() );

		frameState[ currentImage ].surfParms.Reset();
		frameState[ currentImage ].surfParms.CopyData( uboBuffer.data(), sizeof( uniformBufferObject_t ) * uboBuffer.size() );

		frameState[ currentImage ].materialBuffers.Reset();
		frameState[ currentImage ].materialBuffers.CopyData( materialBuffer.data(), sizeof( materialBufferObject_t ) * materialBuffer.size() );

		frameState[ currentImage ].lightParms.Reset();
		frameState[ currentImage ].lightParms.CopyData( lightBuffer.data(), sizeof( light_t ) * lightBuffer.size() );
	}

	std::vector<const char*> GetRequiredExtensions() const
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

		std::vector<const char*> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );

		if ( enableValidationLayers ) {
			extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
		}

		return extensions;
	}

	bool CheckValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

		std::vector<VkLayerProperties> availableLayers( layerCount );
		vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

		for ( const char* layerName : validationLayers ) {
			bool layerFound = false;
			for ( const auto& layerProperties : availableLayers ) {
				if ( strcmp( layerName, layerProperties.layerName ) == 0 ) {
					layerFound = true;
					break;
				}
			}
			if ( !layerFound ) {
				return false;
			}
		}
		return true;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData )
	{

		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	void PopulateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo )
	{
		createInfo = { };
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

		createInfo.messageSeverity = 0;
		if ( validateVerbose ) {
			createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
		}

		if ( validateWarnings ) {
			createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		}

		if ( validateErrors ) {
			createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		}

		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
	}

	void SetupDebugMessenger()
	{
		if ( !enableValidationLayers ) {
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		PopulateDebugMessengerCreateInfo( createInfo );

		if ( CreateDebugUtilsMessengerEXT( context.instance, &createInfo, nullptr, &debugMessenger ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to set up debug messenger!" );
		}
	}

	VkResult CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger )
	{
		auto func = ( PFN_vkCreateDebugUtilsMessengerEXT )vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
		if ( func != nullptr ) {
			return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
		} else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	static void DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator )
	{
		auto func = ( PFN_vkDestroyDebugUtilsMessengerEXT )vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
		if ( func != nullptr ) {
			func( instance, debugMessenger, pAllocator );
		}
	}
};

int main()
{
	VkRenderer renderer;

	try
	{
		renderer.Run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}