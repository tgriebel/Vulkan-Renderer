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

#include "../globals/common.h"
#include "../globals/renderConstants.h"
#include "../globals/renderview.h"

#if defined( USE_IMGUI )
#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#endif

class Window;
class SwapChain;
class Scene;

using renderPassMap_t = std::unordered_map<uint64_t, VkRenderPass>;
using pipelineMap_t = std::unordered_map<uint64_t, pipelineObject_t>;

#if defined( USE_IMGUI )
extern imguiControls_t g_imguiControls;
#endif

typedef void ( *debugMenuFuncPtr )( );

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
	bool				updateDescriptorSets;
	ShaderBindParms*	parms[ MAX_FRAMES_STATES ];
};

class Renderer
{
public:
	using lightBufferArray_t = Array<lightBufferObject_t, MaxLights>;
	using materialBufferArray_t = Array<materialBufferObject_t, MaxMaterials>;
	using bindParmArray_t = Array<ShaderBindParms, DescriptorPoolMaxSets>;
	using debugMenuArray_t = Array<debugMenuFuncPtr, 12>;
	using surfUploadArray_t = Array<surfaceUpload_t, MaxSurfaces * MaxViews>;

	inline bool IsReady() {
		return ( m_frameNumber > 0 );
	}

	void								Init();
	void								Shutdown();
	void								Commit( const Scene* scene );
	void								Render();

	void								InitGPU();
	void								ShutdownGPU();
	void								Resize();

	void								AttachDebugMenu( const debugMenuFuncPtr funcPtr );

private:
	static const uint32_t				ShadowMapWidth = 1024;
	static const uint32_t				ShadowMapHeight = 1024;

	static const bool					ValidateVerbose = false;
	static const bool					ValidateWarnings = false;
	static const bool					ValidateErrors = true;

	renderConstants_t					rc;
	RenderView							views[ MaxViews ];
	RenderView*							activeViews[ MaxViews ];
	RenderView*							renderViews[ Max3DViews ];
	RenderView*							shadowViews[ MaxShadowViews ];
	RenderView*							view2Ds[ Max2DViews ];
	uint32_t							viewCount;
	uint32_t							activeViewCount;

	const std::vector<const char*>		validationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*>		deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
	const bool enableValidationLayers = true;
#else
	const bool enableValidationLayers = true;
#endif

	// Timers
	Timer								frameTimer;
	float								renderTime = 0.0f;

	// Upload management
	std::set<hdl_t>						uploadTextures;
	std::set<hdl_t>						updateTextures;
	std::set<hdl_t>						uploadMaterials;

	uint32_t							imageFreeSlot = 0;
	uint32_t							vbBufElements = 0;
	uint32_t							ibBufElements = 0;
	
	// Render context
	GfxContext							gfxContext;
	ComputeContext						computeContext;
	UploadContext						uploadContext;
	ComputeState						particleState;

	// Frame Sync
	size_t								m_frameId = 0;
	uint32_t							m_bufferId = 0;
	uint32_t							m_frameNumber = 0;

	// Shader resources
	GpuBuffer							stagingBuffer;
	GpuBuffer							vb;	// move
	GpuBuffer							ib;
	ImageArray							gpuImages2D;
	ImageArray							gpuImagesCube;
	materialBufferArray_t				materialBuffer;
	lightBufferArray_t					lightsBuffer;
	surfUploadArray_t					surfUploads;

	Image								shadowMapImage[ MaxShadowViews ];
	Image								mainColorImage;
	Image								tempColorImage;
	Image								depthStencilImage;

	FrameState							frameState[ MAX_FRAMES_STATES ];
	FrameBuffer							shadowMap[ MaxShadowMaps ];
	FrameBuffer							mainColor;
	FrameBuffer							tempColor;

	VkSampler							vk_bilinearSampler;
	VkSampler							vk_depthShadowSampler;

	uint32_t							shadowCount = 0;

	// Memory
	AllocatorMemory						localMemory;
	AllocatorMemory						frameBufferMemory;
	AllocatorMemory						sharedMemory;

	// Shader binding
	bindParmArray_t						bindParmsList;
	ShaderBindSet						defaultBindSet;
	ShaderBindSet						particleShaderBinds;
	VkDescriptorPool					descriptorPool;

	// Misc
	VkDebugUtilsMessengerEXT			debugMessenger;
	debugMenuArray_t					debugMenus;
	renderConfig_t						config;

	// Init/Shutdown
	void								InitApi();
	void								InitShaderResources();
	void								InitConfig();
	void								InitImGui( RenderView& view );
	void								ShutdownImGui();
	void								ShutdownShaderResources();
	void								Cleanup();
	void								RecreateSwapChain();

	// Image Functions
	void								TransitionImageLayout( UploadContext& uploadContext, Image& image, VkImageLayout oldLayout, VkImageLayout newLayout );
	void								CopyBufferToImage( UploadContext& uploadContext, Image& texture, GpuBuffer& buffer, const uint64_t bufferOffset );
	void								GenerateMipmaps( UploadContext& uploadContext, Image& image );

	// API Resource Functions
	void								CreateImage( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory, Image& outImage );
	ShaderBindParms*					RegisterBindParm( const ShaderBindSet* set );
	void								AllocRegisteredBindParms();
	void								FreeRegisteredBindParms();
	void								CreateDescriptorPool();
	void								CreateInstance();
	void								CreateDevice();
	void								CreateSyncObjects();
	void								CreateCommandBuffers();
	void								CreateTextureSamplers();
	void								CreateCodeTextures();
	void								CreateBuffers();
	void								CreateFramebuffers();
	void								DestroyFramebuffers();
	void								CreateCommandPools();
	void								CreateTempCanvas( const imageInfo_t& info, const renderViewRegion_t region );

	// Draw Frame
	static bool							SkipPass( const drawSurf_t& surf, const drawPass_t pass );
	void								RenderViewSurfaces( RenderView& view, GfxContext& gfxContext );
	void								Dispatch( ComputeContext& computeContext, hdl_t progHdl, ShaderBindSet& shader, VkDescriptorSet descSet, const uint32_t x, const uint32_t y = 1, const uint32_t z = 1 );
	void								RenderViews();
	void								CommitModel( RenderView& view, const Entity& ent );
	void								MergeSurfaces( RenderView& view );
	void								DrawDebugMenu();
	void								FlushGPU();
	void								WaitForEndFrame();
	void								SubmitFrame();

	// Update/Upload
	void								BeginUploadCommands( UploadContext& uploadContext );
	void								EndUploadCommands( UploadContext& uploadContext );
	void								CopyGpuBuffer( GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion );

	void								UploadAssets();
	void								UpdateViews( const Scene* scene );
	void								UpdateTextures();
	void								UploadTextures();
	void								UpdateGpuMaterials();
	void								UploadModelsToGPU();
	void								UpdateBindSets( const uint32_t currentImage );
	void								UpdateBuffers( const uint32_t currentImage );
	void								UpdateFrameDescSet( const int currentImage );
	void								BuildPipelines();
	void								UpdateDescriptorSets();
	void								AppendDescriptorWrites( const ShaderBindParms& parms, std::vector<VkWriteDescriptorSet>& descSetWrites );

	// Debug
	std::vector<const char*>			GetRequiredExtensions() const;	
	bool								CheckValidationLayerSupport();
	void								PopulateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo );
	void								MarkerBeginRegion( GfxContext& cxt, const char* pMarkerName, const vec4f& color );
	void								MarkerEndRegion( GfxContext& cxt );
	void								MarkerInsert( GfxContext& cxt, std::string markerName, const vec4f& color );
};