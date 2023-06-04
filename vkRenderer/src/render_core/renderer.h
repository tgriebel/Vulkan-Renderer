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

#include <SysCore/timer.h>
#include <gfxcore/scene/camera.h>

#include "../globals/common.h"
#include "../globals/render_util.h"
#include "../globals/renderConstants.h"
#include "../globals/renderview.h"

#if defined( USE_IMGUI )
#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#endif

class Window;
class SwapChain;
class Scene;

using AssetLibPipelines = AssetLib<pipelineObject_t>;
using renderPassMap_t = std::unordered_map<uint64_t, VkRenderPass>;
using pipelineMap_t = std::unordered_map<uint64_t, pipelineObject_t>;

#if defined( USE_IMGUI )
extern imguiControls_t g_imguiControls;
#endif

extern Scene						scene;
extern Window						g_window;
extern SwapChain					g_swapChain;

struct renderConfig_t
{
	imageSamples_t mainColorSubSamples;
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
	static void	GenerateGpuPrograms( AssetLibGpuProgram& lib );

	inline bool IsReady() {
		return ( m_frameNumber > 0 );
	}

	void								Init();
	void								Destroy();
	void								RenderScene( Scene* scene );

	void								InitGPU();
	void								ShutdownGPU();
	void								Resize();
	void								CreatePipelineObjects();
	void								UploadAssets();

private:
	static const uint32_t				ShadowMapWidth = 1024;
	static const uint32_t				ShadowMapHeight = 1024;

	static const bool					ValidateVerbose = false;
	static const bool					ValidateWarnings = false;
	static const bool					ValidateErrors = true;

	renderConstants_t					rc;
	RenderView							views[ 3 ];
	RenderView							renderView;
	RenderView							shadowView;
	RenderView							view2D;

	const std::vector<const char*>		validationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*>		deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
	const bool enableValidationLayers = true;
#else
	const bool enableValidationLayers = true;
#endif

	Timer								frameTimer;
	float								renderTime = 0.0f;

	std::set<hdl_t>						uploadTextures;
	std::set<hdl_t>						updateTextures;
	std::set<hdl_t>						uploadMaterials;

	VkDebugUtilsMessengerEXT			debugMessenger;
	GfxContext							gfxContext;
	ComputeContext						computeContext;
	UploadContext						uploadContext;
	ComputeState						particleState;
	VkDescriptorPool					descriptorPool;
	renderConfig_t						config;
	size_t								m_frameId = 0;
	uint32_t							m_bufferId = 0;
	uint32_t							m_frameNumber = 0;
	GpuBuffer							stagingBuffer;
	GpuBuffer							vb;	// move
	GpuBuffer							ib;
	ImageArray							gpuImages2D;
	ImageArray							gpuImagesCube;
	materialBufferObject_t				materialBuffer[ MaxMaterials ];

	FrameState							frameState[ MAX_FRAMES_STATES ];
	FrameBuffer							shadowMap[ MAX_FRAMES_STATES ];
	FrameBuffer							mainColor[ MAX_FRAMES_STATES ];

	uint32_t							imageFreeSlot = 0;
	uint32_t							materialFreeSlot = 0;
	uint32_t							vbBufElements = 0;
	uint32_t							ibBufElements = 0;

	AllocatorMemory						localMemory;
	AllocatorMemory						frameBufferMemory;
	AllocatorMemory						sharedMemory;

	ShaderBindParms						bindParmsList[ DescriptorPoolMaxSets ];
	uint32_t							bindParmCount = 0;

	VkSampler							vk_bilinearSampler;
	VkSampler							vk_depthShadowSampler;

	renderPassMap_t						vk_renderPassCache;

	ShaderBindSet						defaultBindSet;
	ShaderBindSet						particleShaderBinds;

	bool								debugMarkersEnabled = false;
	PFN_vkDebugMarkerSetObjectTagEXT	vk_fnDebugMarkerSetObjectTag = VK_NULL_HANDLE;
	PFN_vkDebugMarkerSetObjectNameEXT	vk_fnDebugMarkerSetObjectName = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerBeginEXT		vk_fnCmdDebugMarkerBegin = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerEndEXT			vk_fnCmdDebugMarkerEnd = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerInsertEXT		vk_fnCmdDebugMarkerInsert = VK_NULL_HANDLE;

	float								shadowNearPlane = 0.1f;
	float								shadowFarPlane = 1000.0f;
	bool								restart = false;

	// Init/Shutdown
	void								InitApi();
	void								InitShaderResources();
	void								InitImGui( RenderView& view );
	void								ShutdownImGui();
	void								ShutdownShaderResources();
	void								Cleanup();
	void								RecreateSwapChain();

	// Image Functions
	void								TransitionImageLayout( UploadContext& uploadContext, Image& image, VkImageLayout oldLayout, VkImageLayout newLayout );
	void								CopyBufferToImage( UploadContext& uploadContext, Image& texture, GpuBuffer& buffer, const uint64_t bufferOffset );
	void								GenerateMipmaps( UploadContext& uploadContext, Image& image );

	void								CopyGpuBuffer( GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion );

	// API Creation Functions
	void								CreateImage( const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory, Image& outImage );
	ShaderBindParms*					RegisterBindParm( const ShaderBindSet* set );
	void								AllocRegisteredBindParms();
	void								FreeRegisteredBindParms();
	void								CreateDescriptorPool();
	void								CreateInstance();
	void								CreateDevice();
	void								CreateSyncObjects();
	void								CreateCommandBuffers();
	void								CreateTextureSamplers();
	void								CreateBuffers();
	void								CreateFramebuffers();
	void								CreateCommandPools();

	// Draw Frame
	static bool							SkipPass( const drawSurf_t& surf, const drawPass_t pass );
	static drawPass_t					ViewRegionPassBegin( const renderViewRegion_t region );
	static drawPass_t					ViewRegionPassEnd( const renderViewRegion_t region );
	void								InitRenderPasses( RenderView& view, FrameBuffer fb[ MAX_FRAMES_STATES ] );
	void								RenderViewSurfaces( RenderView& view, GfxContext& gfxContext );
	void								Dispatch( ComputeContext& computeContext, hdl_t progHdl, ShaderBindSet& shader, VkDescriptorSet descSet, const uint32_t x, const uint32_t y = 1, const uint32_t z = 1 );
	void								RenderViews();
	void								Commit( const Scene* scene );
	void								CommitModel( RenderView& view, const Entity& ent, const uint32_t objectOffset );
	void								MergeSurfaces( RenderView& view );
	DrawPass*							GetDrawPass( const drawPass_t pass );
	void								DrawDebugMenu();
	void								FlushGPU();
	void								WaitForEndFrame();
	void								SubmitFrame();

	// Misc
	void								DestroyFramebuffers();
	void								CreateCodeTextures();
	void								BeginUploadCommands( UploadContext& uploadContext );
	void								EndUploadCommands( UploadContext& uploadContext );
	void								InitConfig();

	// Update
	void								UpdateViews( const Scene* scene );
	void								UpdateTextures();
	void								UploadTextures();
	void								UpdateGpuMaterials();
	void								UploadModelsToGPU();
	void								UpdateBindSets( const uint32_t currentImage );
	void								UpdateBuffers( const uint32_t currentImage );
	void								UpdateFrameDescSet( const int currentImage );
	void								UpdateDescriptorSets();
	void								AppendDescriptorWrites( const ShaderBindParms& parms, std::vector<VkWriteDescriptorSet>& descSetWrites );

	// Debug
	std::vector<const char*>			GetRequiredExtensions() const;	
	bool								CheckValidationLayerSupport();
	void								PopulateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo );
	void								SetupMarkers();
	void								MarkerSetObjectName( uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name );
	void								MarkerSetObjectTag( uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag );
	void								MarkerBeginRegion( GfxContext& cxt, const char* pMarkerName, const vec4f color );
	void								MarkerEndRegion( GfxContext& cxt );
	void								MarkerInsert( GfxContext& cxt, std::string markerName, const vec4f color );
	void								SetupDebugMessenger();
	static VkResult						CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger );
	static void							DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator );
};