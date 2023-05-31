/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include <scene/camera.h>

#include "../globals/common.h"
#include "../../window.h"
#include "../../input.h"
#include "../globals/render_util.h"
#include "../render_state/deviceContext.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/shaderBinding.h"
#include "swapChain.h"
#include "../render_state/FrameState.h"
#include "../render_state/rhi.h"
#include "../globals/renderConstants.h"
#include "../globals/renderview.h"
#include "../io/io.h"
#include "../../GeoBuilder.h"
#include <core/assetLib.h>
#include <primitives/geom.h>
#include <resource_types/texture.h>
#include <resource_types/gpuProgram.h>
#include <scene/scene.h>

#include <raytracer/scene.h>
#include <raytracer/raytrace.h>
#include <SysCore/timer.h>
#include "drawpass.h"

#if defined( USE_IMGUI )
#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../external/imgui/backends/imgui_impl_vulkan.h"
#endif

#include <scene/entity.h>

#if defined( USE_IMGUI )
static ImGui_ImplVulkanH_Window imguiMainWindowData;
extern imguiControls_t g_imguiControls;
#endif

typedef AssetLib<pipelineObject_t>	AssetLibPipelines;

using renderPassMap_t = std::unordered_map<uint64_t, VkRenderPass>;
using pipelineMap_t = std::unordered_map<uint64_t, pipelineObject_t>;

extern Scene						scene;
extern Window						g_window;
extern SwapChain					g_swapChain;

struct renderConfig_t
{
	textureSamples_t mainColorSubSamples;
};

struct ComputeState
{
	int32_t				x;
	int32_t				y;
	int32_t				z;
	ShaderBindParms*	parms[ MAX_FRAMES_STATES ];
};


class Renderer
{
public:
	void Init()
	{
		InitApi();

		renderView.region = renderViewRegion_t::STANDARD_RASTER;
		renderView.name = "Main View";

		shadowView.region = renderViewRegion_t::SHADOW;
		shadowView.name = "Shadow View";
		
		view2D.region = renderViewRegion_t::POST;
		view2D.name = "Post View";
		
		InitRenderPasses( shadowView, shadowMap );
		InitRenderPasses( renderView, mainColor );
		InitRenderPasses( view2D, g_swapChain.framebuffers );

		for ( uint32_t i = 0; i < MAX_FRAMES_STATES; ++i ) {
			particleState.parms[ i ] = RegisterBindParm( &particleShaderBinds );
		}

		InitShaderResources();

		CreatePipelineObjects();

		InitImGui( view2D );
	}

	void RenderScene( Scene* scene );

	void Destroy()
	{
		vkDeviceWaitIdle( context.device );
		Cleanup();
	}

	bool IsReady() {
		return ( frameNumber > 0 );
	}

	void		InitGPU();
	void		ShutdownGPU();
	void		Resize();

	static void	GenerateGpuPrograms( AssetLibGpuProgram& lib );

	void		CreatePipelineObjects();

	void		UploadAssets();

	static textureSamples_t GetMaxUsableSampleCount()
	{
		VkSampleCountFlags counts = context.deviceProperties.limits.framebufferColorSampleCounts & context.deviceProperties.limits.framebufferDepthSampleCounts;
		if ( counts & VK_SAMPLE_COUNT_64_BIT ) { return TEXTURE_SMP_64; }
		if ( counts & VK_SAMPLE_COUNT_32_BIT ) { return TEXTURE_SMP_32; }
		if ( counts & VK_SAMPLE_COUNT_16_BIT ) { return TEXTURE_SMP_16; }
		if ( counts & VK_SAMPLE_COUNT_8_BIT ) { return TEXTURE_SMP_8; }
		if ( counts & VK_SAMPLE_COUNT_4_BIT ) { return TEXTURE_SMP_4; }
		if ( counts & VK_SAMPLE_COUNT_2_BIT ) { return TEXTURE_SMP_2; }

		return TEXTURE_SMP_1;
	}

private:

	static const uint32_t				ShadowMapWidth = 1024;
	static const uint32_t				ShadowMapHeight = 1024;

	static const bool					ValidateVerbose = false;
	static const bool					ValidateWarnings = false;
	static const bool					ValidateErrors = true;

	renderConstants_t					rc;
	RenderView							renderView;
	RenderView							shadowView;
	RenderView							view2D;

	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
	const bool enableValidationLayers = true;
#else
	const bool enableValidationLayers = true;
#endif

	Timer							frameTimer;
	float							renderTime = 0.0f;

	std::set<hdl_t>					uploadTextures;
	std::set<hdl_t>					updateTextures;
	std::set<hdl_t>					uploadMaterials;

	VkDebugUtilsMessengerEXT		debugMessenger;
	graphicsQueue_t					graphicsQueue;
	computeQueue_t					computeQueue;
	ComputeState					particleState;
	VkDescriptorPool				descriptorPool;
	renderConfig_t					config;
	size_t							frameId = 0;
	uint32_t						bufferId = 0;
	uint32_t						frameNumber = 0;
	GpuBuffer						stagingBuffer;
	GpuBuffer						vb;	// move
	GpuBuffer						ib;
	ImageArray						gpuImages2D;
	ImageArray						gpuImagesCube;
	materialBufferObject_t			materialBuffer[ MaxMaterials ];

	FrameState						frameState[ MAX_FRAMES_STATES ];
	FrameBuffer						shadowMap[ MAX_FRAMES_STATES ];
	FrameBuffer						mainColor[ MAX_FRAMES_STATES ];

	uint32_t						imageFreeSlot = 0;
	uint32_t						materialFreeSlot = 0;
	uint32_t						vbBufElements = 0;
	uint32_t						ibBufElements = 0;

	AllocatorMemory					localMemory;
	AllocatorMemory					frameBufferMemory;
	AllocatorMemory					sharedMemory;

	ShaderBindParms					bindParmsList[ DescriptorPoolMaxSets ];
	uint32_t						bindParmCount = 0;

	VkSampler						vk_bilinearSampler;
	VkSampler						vk_depthShadowSampler;

	renderPassMap_t					vk_renderPassCache;

	ShaderBindSet					defaultBindSet;
	ShaderBindSet					particleShaderBinds;

	bool								debugMarkersEnabled = false;
	PFN_vkDebugMarkerSetObjectTagEXT	vk_fnDebugMarkerSetObjectTag = VK_NULL_HANDLE;
	PFN_vkDebugMarkerSetObjectNameEXT	vk_fnDebugMarkerSetObjectName = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerBeginEXT		vk_fnCmdDebugMarkerBegin = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerEndEXT			vk_fnCmdDebugMarkerEnd = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerInsertEXT		vk_fnCmdDebugMarkerInsert = VK_NULL_HANDLE;

	float							shadowNearPlane = 0.1f;
	float							shadowFarPlane = 1000.0f;
	bool							restart = false;

	void RecreateSwapChain()
	{
		int width = 0, height = 0;
		g_window.GetWindowFrameBufferSize( width, height, true );

		vkDeviceWaitIdle( context.device );

		DestroyFrameResources();
		g_swapChain.Destroy();

		AllocateDeviceMemory( MaxFrameBufferMemory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameBufferMemory );
		frameBufferMemory.Reset();

		g_swapChain.Create( &g_window, width, height );
		CreateFramebuffers();

		RenderView* views[ 3 ] = { &shadowView, &renderView, &view2D }; // FIXME: TEMP!!

		for ( uint32_t viewIx = 0; viewIx < 3; ++viewIx )
		{
			for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
			{
				DrawPass* pass = views[ viewIx ]->passes[ passIx ];
				if ( pass == nullptr ) {
					continue;
				}
				pass->viewport.width = pass->fb[ 0 ]->GetWidth();
				pass->viewport.height = pass->fb[ 0 ]->GetHeight();
			}
		}
	}

	void AllocateDeviceMemory( const uint32_t allocSize, const VkMemoryPropertyFlagBits typeBits, AllocatorMemory& outAllocation )
	{
		uint32_t typeIndex = FindMemoryType( ~0x00, typeBits );

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
		outAllocation.memoryTypeIndex = typeIndex;
	}

	// Init/Shutdown
	void						InitApi();
	void						InitShaderResources();
	void						InitImGui( RenderView& view );
	void						ShutdownImGui();
	void						ShutdownShaderResources();
	void						Cleanup();

	// Image Functions
	void						TransitionImageLayout( VkCommandBuffer& commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, const textureInfo_t& info );
	void						CopyBufferToImage( VkCommandBuffer& commandBuffer, VkBuffer& buffer, const VkDeviceSize bufferOffset, Texture& texture );
	void						GenerateMipmaps( VkCommandBuffer& commandBuffer, VkImage image, VkFormat imageFormat, const textureInfo_t& info );

	void						CopyGpuBuffer( GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion );

	// API Creation Functions
	void						CreateGpuImage( const textureInfo_t& info, VkImageUsageFlags usage, GpuImage& image, AllocatorMemory& memory );
	ShaderBindParms*			RegisterBindParm( const ShaderBindSet* set );
	void						AllocRegisteredBindParms();
	void						FreeRegisteredBindParms();
	void						CreateDescriptorPool();
	void						CreateInstance();
	void						CreateLogicalDevice();
	void						CreateSyncObjects();
	void						CreateCommandBuffers();
	void						CreateTextureSamplers();
	void						CreateBuffers();
	void						CreateFramebuffers();
	void						CreateCommandPools();

	// Draw Frame
	static bool					SkipPass( const drawSurf_t& surf, const drawPass_t pass );
	static drawPass_t			ViewRegionPassBegin( const renderViewRegion_t region );
	static drawPass_t			ViewRegionPassEnd( const renderViewRegion_t region );
	void						InitRenderPasses( RenderView& view, FrameBuffer fb[ MAX_FRAMES_STATES ] );
	void						RenderViewSurfaces( RenderView& view, VkCommandBuffer commandBuffer );
	void						Dispatch( VkCommandBuffer commandBuffer, hdl_t progHdl, ShaderBindSet& shader, VkDescriptorSet descSet, const uint32_t x, const uint32_t y = 1, const uint32_t z = 1 );
	void						RenderViews();
	void						Commit( const Scene* scene );
	void						CommitModel( RenderView& view, const Entity& ent, const uint32_t objectOffset );
	void						MergeSurfaces( RenderView& view );
	DrawPass*					GetDrawPass( const drawPass_t pass );
	void						DrawDebugMenu();
	void						FlushGPU();
	void						WaitForEndFrame();
	void						SubmitFrame();

	// Misc
	void						DestroyFrameResources();
	void						CreateCodeTextures();
	VkCommandBuffer				BeginSingleTimeCommands();
	void						EndSingleTimeCommands( VkCommandBuffer commandBuffer );
	void						PickPhysicalDevice();

	// Update
	void						UpdateViews( const Scene* scene );
	void						UpdateTextures();
	void						UploadTextures();
	void						UpdateGpuMaterials();
	void						UploadModelsToGPU();
	void						UpdateBindSets( const uint32_t currentImage );
	void						UpdateBuffers( const uint32_t currentImage );
	void						UpdateFrameDescSet( const int currentImage );
	void						UpdateDescriptorSets();
	void						AppendDescriptorWrites( const ShaderBindParms& parms, std::vector<VkWriteDescriptorSet>& descSetWrites );

	// Debug
	static VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData )
	{

		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	std::vector<const char*>	GetRequiredExtensions() const;	
	bool						CheckValidationLayerSupport();
	void						PopulateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo );
	void						SetupMarkers();
	void						MarkerSetObjectName( uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name );
	void						MarkerSetObjectTag( uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag );
	void						MarkerBeginRegion( VkCommandBuffer cmdbuffer, const char* pMarkerName, const vec4f color );
	void						MarkerEndRegion( VkCommandBuffer cmdBuffer );
	void						MarkerInsert( VkCommandBuffer cmdbuffer, std::string markerName, const vec4f color );
	void						SetupDebugMessenger();
	static VkResult				CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger );
	static void					DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator );
};