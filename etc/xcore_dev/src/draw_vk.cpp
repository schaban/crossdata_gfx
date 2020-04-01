#include "crosscore.hpp"
#include "oglsys.hpp"
#include "draw.hpp"

#if DRW_NO_VULKAN

namespace Draw {

static void noop_init(int, cxResourceManager*) {}
static void noop_reset() {}
static int dummy_get_screen_width() { return 0; }
static int dummy_get_screen_height() { return 0; }

static cxMtx dummy_get_shadow_bias_mtx() {
	return nxMtx::identity();
}

void noop_begin(const cxColor&) {}
void noop_end() {}
void noop_batch(cxModelWork*, const int, const Mode, const Context*) {}

Ifc get_impl_vk() {
	Ifc ifc;
	::memset(&ifc, 0, sizeof(ifc));
	s_ifc.info.pName = "_vk_nop_";
	s_ifc.info.needOGLContext = false;
	ifc.init = noop_init;
	ifc.reset = noop_reset;
	ifc.get_screen_width = dummy_get_screen_width;
	ifc.get_screen_height = dummy_get_screen_height;
	ifc.get_shadow_bias_mtx = dummy_get_shadow_bias_mtx;
	ifc.begin = noop_begin;
	ifc.end = noop_end;
	ifc.batch = noop_batch;
	return ifc;
}

} // Draw


#else // DRW_NO_VULKAN

DRW_IMPL_BEGIN

#if defined(OGLSYS_WINDOWS)
#	define VK_USE_PLATFORM_WIN32_KHR
#elif defined(OGLSYS_X11)
#	define VK_USE_PLATFORM_XLIB_KHR
#endif

#include <vulkan/vulkan.h>

#define FRAME_PRESENT_MAX 3

#define MAX_XFORMS 128

static bool s_useAllocCB = false;

struct GPUVtx {
	xt_float3   pos;
	xt_float3   nrm;
	xt_texcoord tex;
	xt_rgba     clr;
	xt_float4   wgt;
	xt_int4     idx;
};

struct GPXform {
	xt_mtx viewProj;
	xt_xmtx xforms[MAX_XFORMS];
};

struct MDL_GPU_WK {
	sxModelData* pMdl;
	VkBuffer* pVB;
	VkBuffer* pIB;
};

typedef cxPlexList<MDL_GPU_WK> MdlGPUWkList;

static MdlGPUWkList* s_pMdlGPUWkList = nullptr;

static void* VKAPI_CALL vk_alloc(void* pData, size_t size, size_t alignment, VkSystemAllocationScope scope) {
	const char* pTag = "vkMem";
	switch (scope) {
		case VK_SYSTEM_ALLOCATION_SCOPE_COMMAND:
			pTag = "vkCmd";
			break;
		case VK_SYSTEM_ALLOCATION_SCOPE_OBJECT:
			pTag = "vkObj";
			break;
		case VK_SYSTEM_ALLOCATION_SCOPE_CACHE:
			pTag = "vkCache";
			break;
		case VK_SYSTEM_ALLOCATION_SCOPE_DEVICE:
			pTag = "vkDevice";
			break;
		case VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE:
			pTag = "vkInstance";
			break;
	}
	void* pMem = nxCore::mem_alloc(size, pTag, (int)alignment);
	return pMem;
}

static void* VKAPI_CALL vk_realloc(void* pData, void* pOrig, size_t size, size_t alignment, VkSystemAllocationScope scope) {
	void* pMem = nxCore::mem_realloc(pOrig, size, (int)alignment);
	return pMem;
}

static void VKAPI_CALL vk_free(void* pData, void* pMem) {
	if (pMem) {
		nxCore::mem_free(pMem);
	}
}

static void VKAPI_CALL vk_internal_alloc_notify(void* pData, size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope) {
}

static void VKAPI_CALL vk_internal_free_notify(void* pData, size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope) {
}

static struct VK_GLB {
	cxResourceManager* mpRsrcMgr;

	VkAllocationCallbacks mAllocCB;
	VkAllocationCallbacks* mpAllocator;
	VkInstance mVkInst;
	VkSurfaceKHR mVkSurf;
	VkPhysicalDevice mVkGPU;
	VkDevice mVkDevice;
	VkPhysicalDeviceProperties mGPUProps;
	VkPhysicalDeviceMemoryProperties mGPUMemProps;
	VkQueueFamilyProperties* mpQueueFamProps;
	uint32_t mNumQueueFam;
	int mQueFamIdxGfx;
	VkQueue mGfxQue;
	VkFormat mSurfFormat;
	VkColorSpaceKHR mSurfColorSpace;
	VkPresentModeKHR mPresentMode;
	VkSemaphore mImgSema[FRAME_PRESENT_MAX];
	VkSemaphore mDrawSema[FRAME_PRESENT_MAX];
	VkFence mFences[FRAME_PRESENT_MAX];
	int mSyncIdx;
	VkCommandPool mCmdPool;
	VkExtent2D mSwapChainExtent;
	VkSwapchainKHR mSwapChain;
	uint32_t mSwapChainImgCnt;
	VkImage* mpSwapChainImgs;
	VkImageView* mpSwapChainImgViews;
	VkCommandBuffer* mpSwapChainCmdBufs;
	VkFramebuffer* mpSwapChainClearFrameBufs;
	VkFramebuffer* mpSwapChainDrawFrameBufs;
	VkBuffer* mpSwapChainXformBufs;
	VkDeviceMemory* mpSwapChainXformBufMems;
	VkDescriptorSet* mpSwapChainDescrSets;
	VkDescriptorPool mDescrPool;
	uint32_t mSwapChainIdx;
	VkFormat mDepthFormat;
	VkImage mDepthImg;
	VkMemoryAllocateInfo mDepthAllocInfo;
	VkDeviceMemory mDepthMem;
	VkImageView mDepthView;
	VkRenderPass mClearRP;
	VkRenderPass mDrawRP;
	VkShaderModule mVtxShader;
	VkShaderModule mPixShader;
	VkPipelineCache mPipelineCache;
	VkDescriptorSetLayout mDescrSetLayout;
	VkPipelineLayout mPipelineLayout;
	VkPipeline mPipeline;
	GPXform mXformWk;

	struct {
		PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
		PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupport;
		PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilities;
		PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormats;
		PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModes;
		PFN_vkGetSwapchainImagesKHR GetSwapchainImages;
	} mInstFuncs;

	struct {
		PFN_vkCreateSwapchainKHR CreateSwapchain;
		PFN_vkDestroySwapchainKHR DestroySwapchain;
		PFN_vkGetSwapchainImagesKHR GetSwapchainImages;
		PFN_vkAcquireNextImageKHR AcquireNextImage;
		PFN_vkQueuePresentKHR QueuePresent;
	} mDeviceFuncs;

	bool mInitFlg;

	void init_alloc_cb() {
		mAllocCB.pUserData = this;
		mAllocCB.pfnAllocation = vk_alloc;
		mAllocCB.pfnReallocation = vk_realloc;
		mAllocCB.pfnFree = vk_free;
		mAllocCB.pfnInternalAllocation = vk_internal_alloc_notify;
		mAllocCB.pfnInternalFree = vk_internal_free_notify;
	}

	void init_inst_funcs() {
		mInstFuncs.GetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(mVkInst, "vkGetDeviceProcAddr");
		mInstFuncs.GetPhysicalDeviceSurfaceSupport = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)vkGetInstanceProcAddr(mVkInst, "vkGetPhysicalDeviceSurfaceSupportKHR");
		mInstFuncs.GetPhysicalDeviceSurfaceCapabilities = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(mVkInst, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
		mInstFuncs.GetPhysicalDeviceSurfaceFormats = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(mVkInst, "vkGetPhysicalDeviceSurfaceFormatsKHR");
		mInstFuncs.GetPhysicalDeviceSurfacePresentModes = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)vkGetInstanceProcAddr(mVkInst, "vkGetPhysicalDeviceSurfacePresentModesKHR");
		mInstFuncs.GetSwapchainImages = (PFN_vkGetSwapchainImagesKHR)vkGetInstanceProcAddr(mVkInst, "vkGetSwapchainImages");
	}

	void init_device_funcs() {
		PFN_vkGetDeviceProcAddr pfnAddr = mInstFuncs.GetDeviceProcAddr;
		if (!pfnAddr) return;
		mDeviceFuncs.CreateSwapchain = (PFN_vkCreateSwapchainKHR)pfnAddr(mVkDevice, "vkCreateSwapchainKHR");
		mDeviceFuncs.DestroySwapchain = (PFN_vkDestroySwapchainKHR)pfnAddr(mVkDevice, "vkDestroySwapchainKHR");
		mDeviceFuncs.GetSwapchainImages = (PFN_vkGetSwapchainImagesKHR)pfnAddr(mVkDevice, "vkGetSwapchainImagesKHR");
		mDeviceFuncs.AcquireNextImage = (PFN_vkAcquireNextImageKHR)pfnAddr(mVkDevice, "vkAcquireNextImageKHR");
		mDeviceFuncs.QueuePresent = (PFN_vkQueuePresentKHR)pfnAddr(mVkDevice, "vkQueuePresentKHR");
	}

	void init_rsrc_mgr(cxResourceManager* pRsrcMgr);
	bool init_vk();
	void reset_vk();

	void* load_spv(const char* pName, size_t* pSize);
	VkShaderModule mk_shader(const char* pName);
	void init_gpu_code();
	void reset_gpu_code();

	int get_mem_type_idx(const VkMemoryRequirements& memReqs, const uint32_t mask) {
		int idx = -1;
		for (uint32_t i = 0; i < mGPUMemProps.memoryTypeCount; ++i) {
			if ((memReqs.memoryTypeBits >> i) & 1) {
				if ((mGPUMemProps.memoryTypes[i].propertyFlags & mask) == mask) {
					idx = int(i);
					break;
				}
			}
		}
		return idx;
	}

	void mdl_prepare(sxModelData* pMdl);
	void mdl_release(sxModelData* pMdl);

	void begin(const cxColor& clearColor);
	void end();
	void draw_batch(cxModelWork* pWk, const int ibat, const Draw::Mode mode, const Draw::Context* pCtx);

} VKG = {};


static void prepare_model(sxModelData* pMdl) {
	VKG.mdl_prepare(pMdl);
}

static void release_model(sxModelData* pMdl) {
	VKG.mdl_release(pMdl);
}

static void prepare_texture(sxTextureData* pTex) {
	// ...
}

static void release_texture(sxTextureData* pTex) {
	// ...
}

void VK_GLB::init_rsrc_mgr(cxResourceManager* pRsrcMgr) {
	mpRsrcMgr = pRsrcMgr;
	if (!mpRsrcMgr) return;
	cxResourceManager::GfxIfc rsrcGfxIfc;
	rsrcGfxIfc.reset();
	rsrcGfxIfc.prepareTexture = prepare_texture;
	rsrcGfxIfc.releaseTexture = release_texture;
	rsrcGfxIfc.prepareModel = prepare_model;
	rsrcGfxIfc.releaseModel = release_model;
	mpRsrcMgr->set_gfx_ifc(rsrcGfxIfc);
}

bool VK_GLB::init_vk() {
	VkResult vres;
	int extsInUse = 0;
	uint32_t instExtsCnt = 0;
	vres = vkEnumerateInstanceExtensionProperties(nullptr, &instExtsCnt, nullptr);
	if (VK_SUCCESS != vres) {
		return false;
	}
	int surfExtIdx = -1;
	int sysSurfExtIdx = -1;
	char* pSysSurfExtName =
#if defined(OGLSYS_WINDOWS)
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(OGLSYS_X11)
		VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#else
		nullptr
#endif
	;
	if (instExtsCnt > 0) {
		VkExtensionProperties* pInstExts = (VkExtensionProperties*)nxCore::mem_alloc(sizeof(VkExtensionProperties)*instExtsCnt, "VkTmpInstExts");
		if (pInstExts) {
			vres = vkEnumerateInstanceExtensionProperties(nullptr, &instExtsCnt, pInstExts);
			if (VK_SUCCESS != vres) {
				nxCore::mem_free(pInstExts);
				return false;
			}
			for (uint32_t i = 0; i < instExtsCnt; ++i) {
				const char* pExtName = pInstExts[i].extensionName;
				if (nxCore::str_eq(pExtName, VK_KHR_SURFACE_EXTENSION_NAME)) {
					++extsInUse;
					surfExtIdx = int(i);
				}
				if (pSysSurfExtName) {
					if (nxCore::str_eq(pExtName, pSysSurfExtName)) {
						++extsInUse;
						sysSurfExtIdx = int(i);
					}
				}
			}
			nxCore::mem_free(pInstExts);
		}
	}
	if (surfExtIdx < 0) {
		return false;
	}
	if (sysSurfExtIdx < 0) {
		return false;
	}
	char** ppInstExtNames = nullptr;
	if (extsInUse > 0) {
		ppInstExtNames = (char**)nxCore::mem_alloc(sizeof(char*)*extsInUse, "VkTmpInstExtNames");
		if (!ppInstExtNames) {
			return false;
		}
	}
	int extIdx = 0;
	if (surfExtIdx >= 0) {
		ppInstExtNames[extIdx++] = VK_KHR_SURFACE_EXTENSION_NAME;
	}
	if (sysSurfExtIdx >= 0) {
		ppInstExtNames[extIdx++] = pSysSurfExtName;
	}
	VkApplicationInfo appInfo;
	::memset(&appInfo, 0, sizeof(appInfo));
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = "crosscore_gfx";
	appInfo.applicationVersion = 1;
	appInfo.pEngineName = "draw_vk";
	appInfo.engineVersion = 1;
	appInfo.apiVersion = VK_API_VERSION_1_0;
	VkInstanceCreateInfo instCrInfo;
	::memset(&instCrInfo, 0, sizeof(instCrInfo));
	instCrInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instCrInfo.pNext = nullptr;
	instCrInfo.flags = 0;
	instCrInfo.pApplicationInfo = &appInfo;
	instCrInfo.enabledLayerCount = 0;
	instCrInfo.ppEnabledLayerNames = nullptr;
	instCrInfo.enabledExtensionCount = extIdx;
	instCrInfo.ppEnabledExtensionNames = ppInstExtNames;
	vres = vkCreateInstance(&instCrInfo, mpAllocator, &VKG.mVkInst);
	nxCore::mem_free(ppInstExtNames);
	ppInstExtNames = nullptr;
	if (VK_SUCCESS != vres) {
		return false;
	}
	init_inst_funcs();
	uint32_t ngpu = 0;
	vres = vkEnumeratePhysicalDevices(VKG.mVkInst, &ngpu, nullptr);
	if (VK_SUCCESS != vres) {
		return false;
	}
	if (ngpu > 0) {
		VkPhysicalDevice* pDev = (VkPhysicalDevice*)nxCore::mem_alloc(sizeof(VkPhysicalDevice)*ngpu, "VkTmpGPUList");
		if (pDev) {
			vkEnumeratePhysicalDevices(VKG.mVkInst, &ngpu, pDev);
			mVkGPU = pDev[0];
			nxCore::mem_free(pDev);
		} else {
			return false;
		}
	} else {
		return false;
	}
	vkGetPhysicalDeviceProperties(mVkGPU, &mGPUProps);
	vkGetPhysicalDeviceMemoryProperties(mVkGPU, &mGPUMemProps);
	mNumQueueFam = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(mVkGPU, &mNumQueueFam, nullptr);
	if (mNumQueueFam < 1) {
		return false;
	}
	int swapChainExtIdx = -1;
	uint32_t devExtsCnt = 0;
	extsInUse = 0;
	vres = vkEnumerateDeviceExtensionProperties(mVkGPU, nullptr, &devExtsCnt, nullptr);
	if (VK_SUCCESS == vres) {
		VkExtensionProperties* pDevExts = (VkExtensionProperties*)nxCore::mem_alloc(sizeof(VkExtensionProperties)*devExtsCnt, "VkTmpDevExts");
		if (pDevExts) {
			vres = vkEnumerateDeviceExtensionProperties(mVkGPU, nullptr, &devExtsCnt, pDevExts);
			for (uint32_t i = 0; i < devExtsCnt; ++i) {
				const char* pExtName = pDevExts[i].extensionName;
				if (nxCore::str_eq(pExtName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
					++extsInUse;
					swapChainExtIdx = int(i);
				}
			}
			nxCore::mem_free(pDevExts);
		}
	}
	if (swapChainExtIdx < 0) {
		return false;
	}
	mpQueueFamProps = (VkQueueFamilyProperties*)nxCore::mem_alloc(sizeof(VkQueueFamilyProperties)*mNumQueueFam, "VkQueueProps");
	if (mpQueueFamProps) {
		vkGetPhysicalDeviceQueueFamilyProperties(mVkGPU, &mNumQueueFam, mpQueueFamProps);
	} else {
		return false;
	}
#if defined(OGLSYS_WINDOWS)
	VkWin32SurfaceCreateInfoKHR surfCrInfo;
	::memset(&surfCrInfo, 0, sizeof(surfCrInfo));
	surfCrInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfCrInfo.hinstance = (HINSTANCE)OGLSys::get_instance();
	surfCrInfo.hwnd = (HWND)OGLSys::get_window();
	vres = vkCreateWin32SurfaceKHR(VKG.mVkInst, &surfCrInfo, mpAllocator, &mVkSurf);
#elif defined(OGLSYS_X11)
	VkXlibSurfaceCreateInfoKHR surfCrInfo;
	::memset(&surfCrInfo, 0, sizeof(surfCrInfo));
	surfCrInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	surfCrInfo.window = (Window)OGLSys::get_window();
	surfCrInfo.dpy = (Display*)OGLSys::get_display();
	vres = vkCreateXlibSurfaceKHR(VKG.mVkInst, &surfCrInfo, mpAllocator, &mVkSurf);
#endif
	if (VK_SUCCESS != vres) {
		nxCore::mem_free(mpQueueFamProps);
		mpQueueFamProps = nullptr;
		return false;
	}
	int queIdxGfx = -1;
	int queIdxPresent = -1;
	int queIdxPresentSep = -1;
	for (uint32_t i = 0; i < mNumQueueFam; ++i) {
		VkBool32 presentFlg = VK_FALSE;
		mInstFuncs.GetPhysicalDeviceSurfaceSupport(mVkGPU, i, mVkSurf, &presentFlg);
		if (presentFlg) {
			queIdxPresentSep = int(i);
		}
		const VkQueueFlags gfxQueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;
		if (!!(mpQueueFamProps[i].queueFlags & gfxQueFlags)) {
			if (queIdxGfx < 0) {
				queIdxGfx = int(i);
			}
			if (presentFlg) {
				queIdxGfx = int(i);
				queIdxPresent = int(i);
				break;
			}
		}
	}
	if (queIdxPresent < 0) {
		queIdxPresent = queIdxPresentSep;
	}
	if (queIdxGfx != queIdxPresent) {
		return false;
	}
	mQueFamIdxGfx = queIdxGfx;
	if (queIdxGfx < 0 || queIdxPresent < 0) {
		return false;
	}
	char** ppDevExtNames = nullptr;
	if (extsInUse > 0) {
		ppDevExtNames = (char**)nxCore::mem_alloc(sizeof(char*)*extsInUse, "VkTmpDevExtNames");
		if (!ppDevExtNames) {
			return false;
		}
	}
	extIdx = 0;
	if (swapChainExtIdx >= 0) {
		ppDevExtNames[extIdx++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	}
	VkDeviceQueueCreateInfo queInfo;
	float quePrio = 0.0f;
	::memset(&queInfo, 0, sizeof(VkDeviceQueueCreateInfo));
	queInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queInfo.queueCount = 1;
	queInfo.queueFamilyIndex = mQueFamIdxGfx;
	queInfo.pQueuePriorities = &quePrio;
	VkDeviceCreateInfo devCrInfo;
	::memset(&devCrInfo, 0, sizeof(VkDeviceCreateInfo));
	devCrInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	devCrInfo.queueCreateInfoCount = 1;
	devCrInfo.pQueueCreateInfos = &queInfo;
	devCrInfo.enabledExtensionCount = extIdx;
	devCrInfo.ppEnabledExtensionNames = ppDevExtNames;
	vres = vkCreateDevice(mVkGPU, &devCrInfo, mpAllocator, &mVkDevice);
	nxCore::mem_free(ppDevExtNames);
	ppDevExtNames = nullptr;
	if (VK_SUCCESS != vres) {
		nxCore::mem_free(mpQueueFamProps);
		mpQueueFamProps = nullptr;
		vkDestroySurfaceKHR(mVkInst, mVkSurf, mpAllocator);
		vkDestroyInstance(mVkInst, mpAllocator);
		return false;
	}
	init_device_funcs();
	vkGetDeviceQueue(mVkDevice, mQueFamIdxGfx, 0, &mGfxQue);
	mSurfFormat = VK_FORMAT_B8G8R8A8_UNORM;
	mSurfColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	uint32_t surfFmtCnt = 0;
	vres = mInstFuncs.GetPhysicalDeviceSurfaceFormats(mVkGPU, mVkSurf, &surfFmtCnt, nullptr);
	VkSurfaceFormatKHR* pSurfFmts = (VkSurfaceFormatKHR*)nxCore::mem_alloc(sizeof(VkSurfaceFormatKHR)*surfFmtCnt, "VkTmpSurfFmts");
	if (pSurfFmts) {
		vres = mInstFuncs.GetPhysicalDeviceSurfaceFormats(mVkGPU, mVkSurf, &surfFmtCnt, pSurfFmts);
		if (pSurfFmts[0].format != VK_FORMAT_UNDEFINED) {
			mSurfFormat = pSurfFmts[0].format;
		}
		mSurfColorSpace = pSurfFmts[0].colorSpace;
		nxCore::mem_free(pSurfFmts);
		pSurfFmts = nullptr;
	}
	VkSemaphoreCreateInfo semaCrInfo;
	::memset(&semaCrInfo, 0, sizeof(VkSemaphoreCreateInfo));
	semaCrInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (int i = 0; i < FRAME_PRESENT_MAX; ++i) {
		vkCreateSemaphore(mVkDevice, &semaCrInfo, mpAllocator, &mImgSema[i]);
		vkCreateSemaphore(mVkDevice, &semaCrInfo, mpAllocator, &mDrawSema[i]);
	}
	VkFenceCreateInfo fenceCrInfo;
	::memset(&fenceCrInfo, 0, sizeof(VkFenceCreateInfo));
	fenceCrInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCrInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (int i = 0; i < FRAME_PRESENT_MAX; ++i) {
		vres = vkCreateFence(mVkDevice, &fenceCrInfo, mpAllocator, &mFences[i]);
	}
	mSyncIdx = 0;
	VkCommandPoolCreateInfo cmdPoolCrInfo;
	::memset(&cmdPoolCrInfo, 0, sizeof(VkCommandPoolCreateInfo));
	cmdPoolCrInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCrInfo.queueFamilyIndex = mQueFamIdxGfx;
	cmdPoolCrInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	vres = vkCreateCommandPool(mVkDevice, &cmdPoolCrInfo, mpAllocator, &mCmdPool);
	VkSurfaceCapabilitiesKHR surfCaps;
	mInstFuncs.GetPhysicalDeviceSurfaceCapabilities(mVkGPU, mVkSurf, &surfCaps);
	uint32_t presentModesCnt = 0;
	mInstFuncs.GetPhysicalDeviceSurfacePresentModes(mVkGPU, mVkSurf, &presentModesCnt, nullptr);
	VkPresentModeKHR* pPresentModes = (VkPresentModeKHR*)nxCore::mem_alloc(sizeof(VkPresentModeKHR)*presentModesCnt, "VkTmpPresentModes");
	mInstFuncs.GetPhysicalDeviceSurfacePresentModes(mVkGPU, mVkSurf, &presentModesCnt, pPresentModes);
	mPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	VkPresentModeKHR swapChainPM = mPresentMode;
	uint32_t swapChainImgNum = 3;
	if (swapChainImgNum < surfCaps.minImageCount) {
		swapChainImgNum = surfCaps.minImageCount;
	}
	if (surfCaps.maxImageCount > 0) {
		if (swapChainImgNum > surfCaps.maxImageCount) {
			swapChainImgNum = surfCaps.maxImageCount;
		}
	}
	VkSurfaceTransformFlagBitsKHR xformBits = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	if (!(surfCaps.supportedTransforms & xformBits)) {
		xformBits = surfCaps.currentTransform;
	}
	VkCompositeAlphaFlagBitsKHR alphaBits = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	VkExtent2D swapChainExtent = surfCaps.currentExtent;
	mSwapChainExtent = swapChainExtent;
	VkSwapchainCreateInfoKHR swapChainCrInfo;
	::memset(&swapChainCrInfo, 0, sizeof(VkSwapchainCreateInfoKHR));
	swapChainCrInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCrInfo.surface = mVkSurf;
	swapChainCrInfo.imageFormat = mSurfFormat;
	swapChainCrInfo.imageColorSpace = mSurfColorSpace;
	swapChainCrInfo.imageExtent = swapChainExtent;
	swapChainCrInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCrInfo.imageArrayLayers = 1;
	swapChainCrInfo.minImageCount = swapChainImgNum;
	swapChainCrInfo.preTransform = xformBits;
	swapChainCrInfo.compositeAlpha = alphaBits;
	swapChainCrInfo.presentMode = swapChainPM;
	swapChainCrInfo.clipped = VK_TRUE;
	vres = mDeviceFuncs.CreateSwapchain(mVkDevice, &swapChainCrInfo, mpAllocator, &mSwapChain);
	mSwapChainImgCnt = 0;
	if (VK_SUCCESS == vres) {
		vres = mDeviceFuncs.GetSwapchainImages(mVkDevice, mSwapChain, &mSwapChainImgCnt, nullptr);
		if (mSwapChainImgCnt > 0) {
			mpSwapChainImgs = (VkImage*)nxCore::mem_alloc(sizeof(VkImage)*mSwapChainImgCnt, "VkSwapChainImgs");
			vres = mDeviceFuncs.GetSwapchainImages(mVkDevice, mSwapChain, &mSwapChainImgCnt, mpSwapChainImgs);
			mpSwapChainImgViews = (VkImageView*)nxCore::mem_alloc(sizeof(VkImageView)*mSwapChainImgCnt, "VkSwapChainImgViews");
			VkImageViewCreateInfo imgViewCrInfo;
			::memset(&imgViewCrInfo, 0, sizeof(VkImageViewCreateInfo));
			imgViewCrInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imgViewCrInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imgViewCrInfo.format = mSurfFormat;
			imgViewCrInfo.components.r = VK_COMPONENT_SWIZZLE_R;
			imgViewCrInfo.components.g = VK_COMPONENT_SWIZZLE_G;
			imgViewCrInfo.components.b = VK_COMPONENT_SWIZZLE_B;
			imgViewCrInfo.components.a = VK_COMPONENT_SWIZZLE_A;
			imgViewCrInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imgViewCrInfo.subresourceRange.levelCount = 1;
			imgViewCrInfo.subresourceRange.layerCount = 1;
			for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
				imgViewCrInfo.image = mpSwapChainImgs[i];
				vres = vkCreateImageView(mVkDevice, &imgViewCrInfo, mpAllocator, &mpSwapChainImgViews[i]);
			}
			VkCommandBufferAllocateInfo cmdBufAllocInfo;
			::memset(&cmdBufAllocInfo, 0, sizeof(VkCommandBufferAllocateInfo));
			cmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			cmdBufAllocInfo.commandPool = mCmdPool;
			cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			cmdBufAllocInfo.commandBufferCount = 1;
			mpSwapChainCmdBufs = (VkCommandBuffer*)nxCore::mem_alloc(sizeof(VkCommandBuffer)*mSwapChainImgCnt, "VkSwapChainCmdBufs");
			for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
				vres = vkAllocateCommandBuffers(mVkDevice, &cmdBufAllocInfo, &mpSwapChainCmdBufs[i]);
			}
			VkDescriptorPoolSize descrPoolSize = {};
			descrPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descrPoolSize.descriptorCount = mSwapChainImgCnt;
			VkDescriptorPoolCreateInfo descrPoolCrInfo;
			::memset(&descrPoolCrInfo, 0, sizeof(VkDescriptorPoolCreateInfo));
			descrPoolCrInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descrPoolCrInfo.maxSets = mSwapChainImgCnt;
			descrPoolCrInfo.poolSizeCount = 1;
			descrPoolCrInfo.pPoolSizes = &descrPoolSize;
			vres = vkCreateDescriptorPool(mVkDevice, &descrPoolCrInfo, mpAllocator, &mDescrPool);
			if (VK_SUCCESS == vres) {
				VkDescriptorSetLayoutBinding bindings[1];
				for (size_t i = 0; i < XD_ARY_LEN(bindings); ++i) {
					::memset(&bindings[i], 0, sizeof(VkDescriptorSetLayoutBinding));
					bindings[i].binding = (uint32_t)i;
				}
				bindings[0].descriptorCount = 1;
				bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
				VkDescriptorSetLayoutCreateInfo bindingsCrInfo;
				::memset(&bindingsCrInfo, 0, sizeof(VkDescriptorSetLayoutCreateInfo));
				bindingsCrInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				bindingsCrInfo.bindingCount = XD_ARY_LEN(bindings);
				bindingsCrInfo.pBindings = bindings;
				vres = vkCreateDescriptorSetLayout(mVkDevice, &bindingsCrInfo, mpAllocator, &mDescrSetLayout);
				mpSwapChainXformBufs = (VkBuffer*)nxCore::mem_alloc(sizeof(VkBuffer)*mSwapChainImgCnt, "VkSwapChainXformBufs");
				mpSwapChainXformBufMems = (VkDeviceMemory*)nxCore::mem_alloc(sizeof(VkDeviceMemory)*mSwapChainImgCnt, "VkSwapChainXformBufMems");
				VkBufferCreateInfo gpBufCrInfo;
				::memset(&gpBufCrInfo, 0, sizeof(VkBufferCreateInfo));
				gpBufCrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				gpBufCrInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
				gpBufCrInfo.size = sizeof(GPXform);
				gpBufCrInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
					vres = vkCreateBuffer(mVkDevice, &gpBufCrInfo, mpAllocator, &mpSwapChainXformBufs[i]);
					if (VK_SUCCESS == vres) {
						VkMemoryRequirements gpMemReqs;
						vkGetBufferMemoryRequirements(mVkDevice, mpSwapChainXformBufs[i], &gpMemReqs);
						int gpMemIdx = get_mem_type_idx(gpMemReqs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
						if (gpMemIdx >= 0) {
							VkMemoryAllocateInfo gpAllocInfo;
							::memset(&gpAllocInfo, 0, sizeof(VkMemoryAllocateInfo));
							gpAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
							gpAllocInfo.memoryTypeIndex = (uint32_t)gpMemIdx;
							gpAllocInfo.allocationSize = gpMemReqs.size;
							vres = vkAllocateMemory(mVkDevice, &gpAllocInfo, mpAllocator, &mpSwapChainXformBufMems[i]);
							if (VK_SUCCESS == vres) {
								vres = vkBindBufferMemory(mVkDevice, mpSwapChainXformBufs[i], mpSwapChainXformBufMems[i], 0);
							}
						}
					}
				}

				VkDescriptorSetAllocateInfo descrSetAllocInfo;
				::memset(&descrSetAllocInfo, 0, sizeof(VkDescriptorSetAllocateInfo));
				descrSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				descrSetAllocInfo.descriptorPool = mDescrPool;
				descrSetAllocInfo.descriptorSetCount = 1;
				descrSetAllocInfo.pSetLayouts = &mDescrSetLayout;
				mpSwapChainDescrSets = (VkDescriptorSet*)nxCore::mem_alloc(sizeof(VkDescriptorSet)*mSwapChainImgCnt, "VkSwapChainDescrSets");
				for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
					vres = vkAllocateDescriptorSets(mVkDevice, &descrSetAllocInfo, &mpSwapChainDescrSets[i]);
				}
				VkDescriptorBufferInfo writeBufInfo;
				writeBufInfo.offset = 0;
				writeBufInfo.range = sizeof(GPXform);
				VkWriteDescriptorSet write;
				::memset(&write, 0, sizeof(VkWriteDescriptorSet));
				write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				write.descriptorCount = 1;
				write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				write.pBufferInfo = &writeBufInfo;
				for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
					writeBufInfo.buffer = mpSwapChainXformBufs[i];
					write.dstSet = mpSwapChainDescrSets[i];
					vkUpdateDescriptorSets(mVkDevice, 1, &write, 0, nullptr);
				}
			}
		}
	}
	nxCore::mem_free(pPresentModes);
	pPresentModes = nullptr;
	mSwapChainIdx = 0;
	mDepthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
	VkImageCreateInfo depthImgCrInfo;
	::memset(&depthImgCrInfo, 0, sizeof(VkImageCreateInfo));
	depthImgCrInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthImgCrInfo.imageType = VK_IMAGE_TYPE_2D;
	depthImgCrInfo.format = mDepthFormat;
	depthImgCrInfo.extent.width = mSwapChainExtent.width;
	depthImgCrInfo.extent.height = mSwapChainExtent.height;
	depthImgCrInfo.extent.depth = 1;
	depthImgCrInfo.mipLevels = 1;
	depthImgCrInfo.arrayLayers = 1;
	depthImgCrInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	depthImgCrInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthImgCrInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	vres = vkCreateImage(mVkDevice, &depthImgCrInfo, mpAllocator, &mDepthImg);
	if (VK_SUCCESS == vres) {
		VkMemoryRequirements depthMemReqs;
		vkGetImageMemoryRequirements(mVkDevice, mDepthImg, &depthMemReqs);
		::memset(&mDepthAllocInfo, 0, sizeof(VkMemoryAllocateInfo));
		mDepthAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mDepthAllocInfo.allocationSize = depthMemReqs.size;
		int depthMemIdx = get_mem_type_idx(depthMemReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (depthMemIdx >= 0) {
			mDepthAllocInfo.memoryTypeIndex = (uint32_t)depthMemIdx;
		}
		vres = vkAllocateMemory(mVkDevice, &mDepthAllocInfo, mpAllocator, &mDepthMem);
		if (VK_SUCCESS == vres) {
			vres = vkBindImageMemory(mVkDevice, mDepthImg, mDepthMem, 0);
			if (VK_SUCCESS == vres) {
				VkImageViewCreateInfo depthViewCrInfo;
				::memset(&depthViewCrInfo, 0, sizeof(VkImageViewCreateInfo));
				depthViewCrInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				depthViewCrInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				depthViewCrInfo.image = mDepthImg;
				depthViewCrInfo.format = mDepthFormat;
				depthViewCrInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				depthViewCrInfo.subresourceRange.levelCount = 1;
				depthViewCrInfo.subresourceRange.layerCount = 1;
				vres = vkCreateImageView(mVkDevice, &depthViewCrInfo, mpAllocator, &mDepthView);
				if (VK_SUCCESS == vres) {
					VkAttachmentDescription attachDescr[2];
					::memset(attachDescr, 0, sizeof(VkAttachmentDescription)*2);
					attachDescr[0].format = mSurfFormat;
					attachDescr[0].samples = VK_SAMPLE_COUNT_1_BIT;
					attachDescr[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
					attachDescr[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					attachDescr[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachDescr[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					attachDescr[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					attachDescr[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
					attachDescr[1].format = mDepthFormat;
					attachDescr[1].samples = VK_SAMPLE_COUNT_1_BIT;
					attachDescr[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
					attachDescr[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					attachDescr[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachDescr[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					attachDescr[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					attachDescr[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					VkAttachmentReference colorAttachRef = {};
					colorAttachRef.attachment = 0;
					colorAttachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					VkAttachmentReference depthAttachRef = {};
					depthAttachRef.attachment = 1;
					depthAttachRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					VkSubpassDescription subpassDescr;
					::memset(&subpassDescr, 0, sizeof(VkSubpassDescription));
					subpassDescr.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
					subpassDescr.colorAttachmentCount = 1;
					subpassDescr.pColorAttachments = &colorAttachRef;
					subpassDescr.pDepthStencilAttachment = &depthAttachRef;
					VkRenderPassCreateInfo rdrPassCrInfo;
					::memset(&rdrPassCrInfo, 0, sizeof(VkRenderPassCreateInfo));
					rdrPassCrInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
					rdrPassCrInfo.attachmentCount = 2;
					rdrPassCrInfo.pAttachments = attachDescr;
					rdrPassCrInfo.subpassCount = 1;
					rdrPassCrInfo.pSubpasses = &subpassDescr;
					vres = vkCreateRenderPass(mVkDevice, &rdrPassCrInfo, mpAllocator, &mClearRP);
					if (VK_SUCCESS == vres) {
						VkFramebufferCreateInfo frameBufCrInfo;
						::memset(&frameBufCrInfo, 0, sizeof(VkFramebufferCreateInfo));
						frameBufCrInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
						frameBufCrInfo.renderPass = mClearRP;
						frameBufCrInfo.attachmentCount = 2;
						frameBufCrInfo.width = mSwapChainExtent.width;
						frameBufCrInfo.height = mSwapChainExtent.height;
						frameBufCrInfo.layers = 1;
						mpSwapChainClearFrameBufs = (VkFramebuffer*)nxCore::mem_alloc(sizeof(VkFramebuffer)*mSwapChainImgCnt, "VkSwapChainClearFrameBufs");
						for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
							VkImageView fbufAttachments[2] = { mpSwapChainImgViews[i], mDepthView };
							frameBufCrInfo.pAttachments = fbufAttachments;
							vres = vkCreateFramebuffer(mVkDevice, &frameBufCrInfo, mpAllocator, &mpSwapChainClearFrameBufs[i]);
						}
					}
					attachDescr[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
					attachDescr[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
					vres = vkCreateRenderPass(mVkDevice, &rdrPassCrInfo, mpAllocator, &mDrawRP);
					if (VK_SUCCESS == vres) {
						VkFramebufferCreateInfo frameBufCrInfo;
						::memset(&frameBufCrInfo, 0, sizeof(VkFramebufferCreateInfo));
						frameBufCrInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
						frameBufCrInfo.renderPass = mDrawRP;
						frameBufCrInfo.attachmentCount = 2;
						frameBufCrInfo.width = mSwapChainExtent.width;
						frameBufCrInfo.height = mSwapChainExtent.height;
						frameBufCrInfo.layers = 1;
						mpSwapChainDrawFrameBufs = (VkFramebuffer*)nxCore::mem_alloc(sizeof(VkFramebuffer)*mSwapChainImgCnt, "VkSwapChainDrawFrameBufs");
						for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
							VkImageView fbufAttachments[2] = { mpSwapChainImgViews[i], mDepthView };
							frameBufCrInfo.pAttachments = fbufAttachments;
							vres = vkCreateFramebuffer(mVkDevice, &frameBufCrInfo, mpAllocator, &mpSwapChainDrawFrameBufs[i]);
						}
					}
				}
			}
		}
	}
	for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
		VkCommandBufferBeginInfo cmdBufBeginInfo;
		::memset(&cmdBufBeginInfo, 0, sizeof(VkCommandBufferBeginInfo));
		cmdBufBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		vres = vkBeginCommandBuffer(mpSwapChainCmdBufs[i], &cmdBufBeginInfo);
	}
	return true;
}

void VK_GLB::reset_vk() {
	vkDeviceWaitIdle(mVkDevice);
	reset_gpu_code();
	vkDestroyRenderPass(mVkDevice, mDrawRP, mpAllocator);
	vkDestroyRenderPass(mVkDevice, mClearRP, mpAllocator);
	vkDestroyImageView(mVkDevice, mDepthView, mpAllocator);
	vkDestroyImage(mVkDevice, mDepthImg, mpAllocator);
	vkFreeMemory(mVkDevice, mDepthMem, mpAllocator);
	if (mpSwapChainClearFrameBufs) {
		for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
			vkDestroyFramebuffer(mVkDevice, mpSwapChainClearFrameBufs[i], mpAllocator);
		}
		nxCore::mem_free(mpSwapChainClearFrameBufs);
		mpSwapChainClearFrameBufs = nullptr;
	}
	if (mpSwapChainDrawFrameBufs) {
		for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
			vkDestroyFramebuffer(mVkDevice, mpSwapChainDrawFrameBufs[i], mpAllocator);
		}
		nxCore::mem_free(mpSwapChainDrawFrameBufs);
		mpSwapChainDrawFrameBufs = nullptr;
	}
	if (mpSwapChainCmdBufs) {
		for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
			vkFreeCommandBuffers(mVkDevice, mCmdPool, 1, &mpSwapChainCmdBufs[i]);
		}
		nxCore::mem_free(mpSwapChainCmdBufs);
		mpSwapChainCmdBufs = nullptr;
	}
	if (mpSwapChainImgViews) {
		for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
			vkDestroyImageView(mVkDevice, mpSwapChainImgViews[i], mpAllocator);
		}
		nxCore::mem_free(mpSwapChainImgViews);
		mpSwapChainImgViews = nullptr;
	}
	nxCore::mem_free(mpSwapChainImgs);
	mpSwapChainImgs = nullptr;
	if (mpSwapChainXformBufMems) {
		for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
			vkFreeMemory(mVkDevice, mpSwapChainXformBufMems[i], mpAllocator);
		}
		nxCore::mem_free(mpSwapChainXformBufMems);
		mpSwapChainXformBufMems = nullptr;
	}
	if (mpSwapChainXformBufs) {
		for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
			vkDestroyBuffer(mVkDevice, mpSwapChainXformBufs[i], mpAllocator);
		}
		nxCore::mem_free(mpSwapChainXformBufs);
		mpSwapChainXformBufs = nullptr;
	}
	if (mpSwapChainDescrSets) {
		for (uint32_t i = 0; i < mSwapChainImgCnt; ++i) {
			vkFreeDescriptorSets(mVkDevice, mDescrPool, 1, &mpSwapChainDescrSets[i]);
		}
		nxCore::mem_free(mpSwapChainDescrSets);
		mpSwapChainDescrSets = nullptr;
	}
	vkDestroyDescriptorSetLayout(mVkDevice, mDescrSetLayout, mpAllocator);
	vkDestroyDescriptorPool(mVkDevice, mDescrPool, mpAllocator);
	mDeviceFuncs.DestroySwapchain(mVkDevice, mSwapChain, mpAllocator);
	vkDestroyCommandPool(mVkDevice, mCmdPool, mpAllocator);
	for (int i = 0; i < FRAME_PRESENT_MAX; ++i) {
		vkDestroyFence(mVkDevice, mFences[i], mpAllocator);
	}
	for (int i = 0; i < FRAME_PRESENT_MAX; ++i) {
		vkDestroySemaphore(mVkDevice, mDrawSema[i], mpAllocator);
		vkDestroySemaphore(mVkDevice, mImgSema[i], mpAllocator);
	}
	vkDeviceWaitIdle(mVkDevice);
	vkDestroyDevice(mVkDevice, mpAllocator);
	vkDestroySurfaceKHR(mVkInst, mVkSurf, mpAllocator);
	vkDestroyInstance(mVkInst, mpAllocator);
	nxCore::mem_free(mpQueueFamProps);
	mpQueueFamProps = nullptr;
}

void* VK_GLB::load_spv(const char* pName, size_t* pSize) {
	void* pBin = nullptr;
	size_t binSize = 0;
	const char* pDataPath = mpRsrcMgr ? mpRsrcMgr->get_data_path() : nullptr;
	if (pName) {
		char path[1024];
		XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/vk/%s", pDataPath ? pDataPath : ".", pName);
		pBin = nxCore::bin_load(path, &binSize, false, true);
	}
	if (pSize) {
		*pSize = binSize;
	}
	return pBin;
}

VkShaderModule VK_GLB::mk_shader(const char* pName) {
	VkShaderModule sm = VK_NULL_HANDLE;
	size_t binSize = 0;
	void* pBin = load_spv(pName, &binSize);
	if (pBin) {
		VkShaderModuleCreateInfo smCrInfo;
		::memset(&smCrInfo, 0, sizeof(smCrInfo));
		smCrInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		smCrInfo.pCode = (const uint32_t*)pBin;
		smCrInfo.codeSize = binSize;
		VkResult vres = vkCreateShaderModule(mVkDevice, &smCrInfo, mpAllocator, &sm);
		if (VK_SUCCESS != vres) {
			nxCore::dbg_msg("Error creating shader module for '%s'.\n", pName);
		}
		nxCore::bin_unload(pBin);
	}
	return sm;
}

void VK_GLB::init_gpu_code() {
	VkResult vres;
	mVtxShader = mk_shader("vtx.vert.spv");
	mPixShader = mk_shader("pix.frag.spv");
	VkPipelineCacheCreateInfo cacheCrInfo;
	::memset(&cacheCrInfo, 0, sizeof(VkPipelineCacheCreateInfo));
	cacheCrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	vres = vkCreatePipelineCache(mVkDevice, &cacheCrInfo, mpAllocator, &mPipelineCache);
	VkPipelineLayoutCreateInfo layoutCrInfo;
	::memset(&layoutCrInfo, 0, sizeof(VkPipelineLayoutCreateInfo));
	layoutCrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCrInfo.setLayoutCount = 1;
	layoutCrInfo.pSetLayouts = &mDescrSetLayout;
	vres = vkCreatePipelineLayout(mVkDevice, &layoutCrInfo, mpAllocator, &mPipelineLayout);
	VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynStateCrInfo;
	::memset(&dynStateCrInfo, 0, sizeof(VkPipelineDynamicStateCreateInfo));
	dynStateCrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynStateCrInfo.dynamicStateCount = XD_ARY_LEN(dynStates);
	dynStateCrInfo.pDynamicStates = dynStates;
	VkPipelineInputAssemblyStateCreateInfo iaCrInfo;
	::memset(&iaCrInfo, 0, sizeof(VkPipelineInputAssemblyStateCreateInfo));
	iaCrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	iaCrInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	static const VkVertexInputAttributeDescription vtxAttrs[] = {
		{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GPUVtx, pos) },
		{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GPUVtx, nrm) },
		{ 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(GPUVtx, tex) },
		{ 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GPUVtx, clr) },
		{ 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GPUVtx, wgt) },
		{ 5, 0, VK_FORMAT_R32G32B32A32_SINT, offsetof(GPUVtx, idx) }
	};
	static const VkVertexInputBindingDescription vtxDescr[] = {
		{0, sizeof(GPUVtx), VK_VERTEX_INPUT_RATE_VERTEX }
	};
	VkPipelineVertexInputStateCreateInfo viCrInfo;
	::memset(&viCrInfo, 0, sizeof(VkPipelineVertexInputStateCreateInfo));
	viCrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	viCrInfo.vertexAttributeDescriptionCount = XD_ARY_LEN(vtxAttrs);
	viCrInfo.pVertexAttributeDescriptions = vtxAttrs;
	viCrInfo.vertexBindingDescriptionCount = XD_ARY_LEN(vtxDescr);
	viCrInfo.pVertexBindingDescriptions = vtxDescr;
	VkPipelineRasterizationStateCreateInfo rsCrInfo;
	::memset(&rsCrInfo, 0, sizeof(VkPipelineRasterizationStateCreateInfo));
	rsCrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rsCrInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rsCrInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rsCrInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rsCrInfo.lineWidth = 1.0f;
	VkPipelineViewportStateCreateInfo vpCrInfo;
	::memset(&vpCrInfo, 0, sizeof(VkPipelineViewportStateCreateInfo));
	vpCrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vpCrInfo.viewportCount = 1;
	vpCrInfo.scissorCount = 1;
	VkPipelineDepthStencilStateCreateInfo dsCrInfo;
	::memset(&dsCrInfo, 0, sizeof(VkPipelineDepthStencilStateCreateInfo));
	dsCrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	dsCrInfo.depthTestEnable = VK_TRUE;
	dsCrInfo.depthWriteEnable = VK_TRUE;
	dsCrInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	dsCrInfo.front.failOp = VK_STENCIL_OP_KEEP;
	dsCrInfo.front.passOp = VK_STENCIL_OP_KEEP;
	dsCrInfo.front.compareOp = VK_COMPARE_OP_ALWAYS;
	dsCrInfo.back.failOp = VK_STENCIL_OP_KEEP;
	dsCrInfo.back.passOp = VK_STENCIL_OP_KEEP;
	dsCrInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
	VkPipelineMultisampleStateCreateInfo msCrInfo;
	::memset(&msCrInfo, 0, sizeof(VkPipelineMultisampleStateCreateInfo));
	msCrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	msCrInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState blendOpaq;
	::memset(&blendOpaq, 0, sizeof(VkPipelineColorBlendAttachmentState));
	blendOpaq.colorWriteMask = 0xF;
	VkPipelineColorBlendStateCreateInfo bsCrInfo;
	::memset(&bsCrInfo, 0, sizeof(VkPipelineColorBlendStateCreateInfo));
	bsCrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	bsCrInfo.attachmentCount = 1;
	bsCrInfo.pAttachments = &blendOpaq;
	VkPipelineShaderStageCreateInfo shaderStgCrInfo[2];
	::memset(shaderStgCrInfo, 0, sizeof(VkPipelineShaderStageCreateInfo)*2);
	shaderStgCrInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStgCrInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStgCrInfo[0].module = mVtxShader;
	shaderStgCrInfo[0].pName = "main";
	shaderStgCrInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStgCrInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStgCrInfo[1].module = mPixShader;
	shaderStgCrInfo[1].pName = "main";
	VkGraphicsPipelineCreateInfo pipeCrInfo;
	::memset(&pipeCrInfo, 0, sizeof(VkGraphicsPipelineCreateInfo));
	pipeCrInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeCrInfo.layout = mPipelineLayout;
	pipeCrInfo.renderPass = mDrawRP;
	pipeCrInfo.stageCount = 2;
	pipeCrInfo.pStages = shaderStgCrInfo;
	pipeCrInfo.pDynamicState = &dynStateCrInfo;
	pipeCrInfo.pInputAssemblyState = &iaCrInfo;
	pipeCrInfo.pVertexInputState = &viCrInfo;
	pipeCrInfo.pRasterizationState = &rsCrInfo;
	pipeCrInfo.pViewportState = &vpCrInfo;
	pipeCrInfo.pDepthStencilState = &dsCrInfo;
	pipeCrInfo.pMultisampleState = &msCrInfo;
	pipeCrInfo.pColorBlendState = &bsCrInfo;
	vres = vkCreateGraphicsPipelines(mVkDevice, mPipelineCache, 1, &pipeCrInfo, mpAllocator, &mPipeline);
}

void VK_GLB::reset_gpu_code() {
	vkDestroyPipeline(mVkDevice, mPipeline, mpAllocator);
	vkDestroyPipelineCache(mVkDevice, mPipelineCache, mpAllocator);
	vkDestroyPipelineLayout(mVkDevice, mPipelineLayout, mpAllocator);
	vkDestroyShaderModule(mVkDevice, mVtxShader, mpAllocator);
	vkDestroyShaderModule(mVkDevice, mPixShader, mpAllocator);
}

void VK_GLB::mdl_prepare(sxModelData* pMdl) {
	if (!pMdl) return;
	if (!mInitFlg) return;
	int npnt = pMdl->mPntNum;
	if (npnt <= 0) return;
	int nidx = pMdl->mIdx16Num;
	if (nidx <= 0) return;

	VkResult vres;
	VkBuffer* pBufs = pMdl->get_gpu_wk<VkBuffer>();
	VkBuffer* pVB = &pBufs[0];
	VkBuffer* pIB = &pBufs[1];
	VkDeviceMemory* pMemVB = (VkDeviceMemory*)(pIB + 1);
	VkDeviceMemory* pMemIB = pMemVB + 1;
	if (*pVB != VK_NULL_HANDLE) {
		return;
	}

	VkBufferCreateInfo vbCrInfo;
	::memset(&vbCrInfo, 0, sizeof(VkBufferCreateInfo));
	vbCrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vbCrInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	vbCrInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vbCrInfo.size = sizeof(GPUVtx) * npnt;
	vres = vkCreateBuffer(mVkDevice, &vbCrInfo, mpAllocator, pVB);
	if (VK_SUCCESS != vres) {
		return;
	}
	VkMemoryRequirements vbMemReqs;
	vkGetBufferMemoryRequirements(mVkDevice, *pVB, &vbMemReqs);
	int vbMemIdx = get_mem_type_idx(vbMemReqs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (vbMemIdx >= 0) {
		VkMemoryAllocateInfo vbAllocInfo;
		::memset(&vbAllocInfo, 0, sizeof(VkMemoryAllocateInfo));
		vbAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		vbAllocInfo.memoryTypeIndex = (uint32_t)vbMemIdx;
		vbAllocInfo.allocationSize = vbMemReqs.size;
		vres = vkAllocateMemory(mVkDevice, &vbAllocInfo, mpAllocator, pMemVB);
		if (VK_SUCCESS == vres) {
			void* pMappedVB = nullptr;
			vres = vkMapMemory(mVkDevice, *pMemVB, 0, vbMemReqs.size, 0, &pMappedVB);
			if (VK_SUCCESS == vres) {
				GPUVtx* pVtx = (GPUVtx*)pMappedVB;
				for (int i = 0; i < npnt; ++i) {
					pVtx[i].pos = pMdl->get_pnt_pos(i);
					pVtx[i].nrm = pMdl->get_pnt_nrm(i);
					pVtx[i].tex = pMdl->get_pnt_tex(i);
					pVtx[i].clr = pMdl->get_pnt_clr(i);
					pVtx[i].wgt.fill(0.0f);
					pVtx[i].idx.fill(0);
					sxModelData::PntSkin skn = pMdl->get_pnt_skin(i);
					if (skn.num > 0) {
						for (int j = 0; j < skn.num; ++j) {
							pVtx[i].wgt.set_at(j, skn.wgt[j]);
							pVtx[i].idx.set_at(j, skn.idx[j]);
						}
					} else {
						pVtx[i].wgt[0] = 1.0f;
					}
					pVtx[i].idx.scl(3);
				}
				vkUnmapMemory(mVkDevice, *pMemVB);
				vres = vkBindBufferMemory(mVkDevice, *pVB, *pMemVB, 0);
			}
		}
	}

	VkBufferCreateInfo ibCrInfo;
	::memset(&ibCrInfo, 0, sizeof(VkBufferCreateInfo));
	ibCrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ibCrInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	ibCrInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ibCrInfo.size = sizeof(uint16_t) * nidx;
	vres = vkCreateBuffer(mVkDevice, &ibCrInfo, mpAllocator, pIB);
	if (VK_SUCCESS != vres) {
		return;
	}
	VkMemoryRequirements ibMemReqs;
	vkGetBufferMemoryRequirements(mVkDevice, *pIB, &ibMemReqs);
	int ibMemIdx = get_mem_type_idx(ibMemReqs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (ibMemIdx >= 0) {
		VkMemoryAllocateInfo ibAllocInfo;
		::memset(&ibAllocInfo, 0, sizeof(VkMemoryAllocateInfo));
		ibAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ibAllocInfo.memoryTypeIndex = (uint32_t)ibMemIdx;
		ibAllocInfo.allocationSize = ibMemReqs.size;
		vres = vkAllocateMemory(mVkDevice, &ibAllocInfo, mpAllocator, pMemIB);
		if (VK_SUCCESS == vres) {
			void* pMappedIB = nullptr;
			vres = vkMapMemory(mVkDevice, *pMemIB, 0, ibMemReqs.size, 0, &pMappedIB);
			if (VK_SUCCESS == vres) {
				const uint16_t* pIdxData = pMdl->get_idx16_top();
				if (pIdxData) {
					::memcpy(pMappedIB, pIdxData, (size_t)ibMemReqs.size);
				}
				vkUnmapMemory(mVkDevice, *pMemIB);
				vres = vkBindBufferMemory(mVkDevice, *pIB, *pMemIB, 0);
			}
		}
	}
}

void VK_GLB::mdl_release(sxModelData* pMdl) {
	if (!pMdl) return;
	if (!mInitFlg) return;
	VkBuffer* pBufs = pMdl->get_gpu_wk<VkBuffer>();
	VkBuffer* pVB = &pBufs[0];
	VkBuffer* pIB = &pBufs[1];
	VkDeviceMemory* pMemVB = (VkDeviceMemory*)(pIB + 1);
	VkDeviceMemory* pMemIB = pMemVB + 1;
	if (*pVB != VK_NULL_HANDLE) {
		vkDestroyBuffer(mVkDevice, *pVB, mpAllocator);
		*pVB = VK_NULL_HANDLE;
		if (*pMemVB != VK_NULL_HANDLE) {
			vkFreeMemory(mVkDevice, *pMemVB, mpAllocator);
			*pMemVB = VK_NULL_HANDLE;
		}
	}
	if (*pIB != VK_NULL_HANDLE) {
		vkDestroyBuffer(mVkDevice, *pIB, mpAllocator);
		*pIB = VK_NULL_HANDLE;
		if (*pMemIB != VK_NULL_HANDLE) {
			vkFreeMemory(mVkDevice, *pMemIB, mpAllocator);
			*pMemIB = VK_NULL_HANDLE;
		}
	}
	pMdl->clear_tex_wk();
}

void VK_GLB::begin(const cxColor& clearColor) {
	if (!mpSwapChainCmdBufs) return;
	VkResult vres;
	VkCommandBufferBeginInfo cmdBufBeginInfo;
	::memset(&cmdBufBeginInfo, 0, sizeof(VkCommandBufferBeginInfo));
	cmdBufBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	vres = vkBeginCommandBuffer(mpSwapChainCmdBufs[mSwapChainIdx], &cmdBufBeginInfo);
	VkClearValue clearVals[2];
	for (int i = 0; i < 4; ++i) {
		clearVals[0].color.float32[i] = clearColor.ch[i];
	}
	clearVals[1].depthStencil.depth = 1.0f;
	clearVals[1].depthStencil.stencil = 0;
	VkRenderPassBeginInfo rdrPassBeginInfo;
	::memset(&rdrPassBeginInfo, 0, sizeof(VkRenderPassBeginInfo));
	rdrPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rdrPassBeginInfo.renderPass = mClearRP;
	rdrPassBeginInfo.framebuffer = mpSwapChainClearFrameBufs[mSwapChainIdx];
	rdrPassBeginInfo.renderArea.extent = mSwapChainExtent;
	rdrPassBeginInfo.clearValueCount = 2;
	rdrPassBeginInfo.pClearValues = clearVals;
	vkCmdBeginRenderPass(mpSwapChainCmdBufs[mSwapChainIdx], &rdrPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport;
	::memset(&viewport, 0, sizeof(VkViewport));
	viewport.width = float(mSwapChainExtent.width);
	viewport.height = float(mSwapChainExtent.height);
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(mpSwapChainCmdBufs[mSwapChainIdx], 0, 1, &viewport);
	VkRect2D scis;
	::memset(&scis, 0, sizeof(VkRect2D));
	scis.extent = mSwapChainExtent;
	vkCmdSetScissor(mpSwapChainCmdBufs[mSwapChainIdx], 0, 1, &scis);
	vkCmdEndRenderPass(mpSwapChainCmdBufs[mSwapChainIdx]);
}

void VK_GLB::end() {
	if (!mpSwapChainCmdBufs) return;
	VkResult vres;
	vkEndCommandBuffer(mpSwapChainCmdBufs[mSwapChainIdx]);

	vkWaitForFences(mVkDevice, 1, &mFences[mSyncIdx], VK_TRUE, UINT64_MAX);
	vkResetFences(mVkDevice, 1, &mFences[mSyncIdx]);

	uint32_t nextImgIdx = mSwapChainIdx;
	while (true) {
		vres = mDeviceFuncs.AcquireNextImage(mVkDevice, mSwapChain, UINT64_MAX, mImgSema[mSyncIdx], VK_NULL_HANDLE, &nextImgIdx);
		if (VK_SUCCESS == vres || VK_SUBOPTIMAL_KHR == vres) {
			break;
		} else if (VK_ERROR_OUT_OF_DATE_KHR == vres) {
			VKG.reset_vk();
			VKG.mInitFlg = VKG.init_vk();
			if (VKG.mInitFlg) {
				VKG.init_gpu_code();
			}
		}
	}
	mSwapChainIdx = nextImgIdx;

	VkPipelineStageFlags pipeFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo;
	::memset(&submitInfo, 0, sizeof(VkSubmitInfo));
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pWaitDstStageMask = &pipeFlags;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &mImgSema[mSyncIdx];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &mDrawSema[mSyncIdx];
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &mpSwapChainCmdBufs[mSwapChainIdx];
	vres = vkQueueSubmit(mGfxQue, 1, &submitInfo, mFences[mSyncIdx]);

	VkPresentInfoKHR presentInfo;
	::memset(&presentInfo, 0, sizeof(VkPresentInfoKHR));
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &mSwapChain;
	presentInfo.pImageIndices = &mSwapChainIdx;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &mDrawSema[mSyncIdx];
	vres = mDeviceFuncs.QueuePresent(mGfxQue, &presentInfo);

	++mSyncIdx;
	mSyncIdx %= FRAME_PRESENT_MAX;
}

void VK_GLB::draw_batch(cxModelWork* pWk, const int ibat, const Draw::Mode mode, const Draw::Context* pCtx) {
	sxModelData* pMdl = pWk->mpData;
	if (!pMdl) return;
	const sxModelData::Batch* pBat = pMdl->get_batch_ptr(ibat);
	if (!pBat) return;
	if (!pBat->is_idx16()) return;
	VkBuffer* pBufs = pMdl->get_gpu_wk<VkBuffer>();
	VkBuffer* pVB = &pBufs[0];
	VkBuffer* pIB = &pBufs[1];
	VkDeviceMemory* pMemVB = (VkDeviceMemory*)(pIB + 1);
	VkDeviceMemory* pMemIB = pMemVB + 1;
	if (VK_NULL_HANDLE == *pVB || VK_NULL_HANDLE == *pIB) return;
	VkCommandBuffer cmd = mpSwapChainCmdBufs[mSwapChainIdx];
	static float yflipMtx[] = {
		1.0f,  0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f,  0.0f, 1.0f, 0.0f,
		0.0f,  0.0f, 0.0f, 1.0f,
	};
	mXformWk.viewProj = pCtx->view.viewProjMtx * nxMtx::from_mem(yflipMtx);
	int xformsNum = 1;
	if (pWk->mpSkinXforms) {
		xformsNum = pMdl->mSknNum;
		if (xformsNum > MAX_XFORMS) xformsNum = MAX_XFORMS;
		::memcpy(mXformWk.xforms, pWk->mpSkinXforms, xformsNum * sizeof(xt_xmtx));
	} else if (pWk->mpWorldXform) {
		mXformWk.xforms[0] = *pWk->mpWorldXform;
	} else {
		mXformWk.xforms[0].identity();
	}
	VkDeviceSize xformSize = sizeof(xt_mtx) + sizeof(xt_xmtx)*xformsNum;
	vkCmdUpdateBuffer(cmd, mpSwapChainXformBufs[mSwapChainIdx], 0, xformSize, &mXformWk);

	VkRenderPassBeginInfo rdrPassBeginInfo;
	::memset(&rdrPassBeginInfo, 0, sizeof(VkRenderPassBeginInfo));
	rdrPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rdrPassBeginInfo.renderPass = mDrawRP;
	rdrPassBeginInfo.framebuffer = mpSwapChainDrawFrameBufs[mSwapChainIdx];
	rdrPassBeginInfo.renderArea.extent = mSwapChainExtent;
	rdrPassBeginInfo.clearValueCount = 0;
	rdrPassBeginInfo.pClearValues = nullptr;
	vkCmdBeginRenderPass(mpSwapChainCmdBufs[mSwapChainIdx], &rdrPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport;
	::memset(&viewport, 0, sizeof(VkViewport));
	viewport.width = float(mSwapChainExtent.width);
	viewport.height = float(mSwapChainExtent.height);
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(mpSwapChainCmdBufs[mSwapChainIdx], 0, 1, &viewport);
	VkRect2D scis;
	::memset(&scis, 0, sizeof(VkRect2D));
	scis.extent = mSwapChainExtent;
	vkCmdSetScissor(mpSwapChainCmdBufs[mSwapChainIdx], 0, 1, &scis);
	vkCmdBindPipeline(mpSwapChainCmdBufs[mSwapChainIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
	vkCmdBindDescriptorSets(mpSwapChainCmdBufs[mSwapChainIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mpSwapChainDescrSets[mSwapChainIdx], 0, nullptr);
	VkDeviceSize vbOffs = pBat->mMinIdx * sizeof(GPUVtx);
	vkCmdBindVertexBuffers(cmd, 0, 1, pVB, &vbOffs);
	vkCmdBindIndexBuffer(cmd, *pIB, pBat->mIdxOrg * sizeof(uint16_t), VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(cmd, pBat->mTriNum * 3, 1, 0, 0, 0);
	vkCmdEndRenderPass(mpSwapChainCmdBufs[mSwapChainIdx]);
}

static void init(int shadowSize, cxResourceManager* pRsrcMgr) {
	::memset(&VKG, 0, sizeof(VKG));
	VKG.init_alloc_cb();
	VKG.mpAllocator = s_useAllocCB ? &VKG.mAllocCB : nullptr;
	VKG.init_rsrc_mgr(pRsrcMgr);
	if (!pRsrcMgr) return;
	VKG.mInitFlg = VKG.init_vk();
	if (VKG.mInitFlg) {
		s_pMdlGPUWkList = MdlGPUWkList::create("MdlGPUWkList");
		VKG.init_gpu_code();
	}
}

static void reset() {
	if (!VKG.mInitFlg) return;
	VKG.reset_vk();
	MdlGPUWkList::destroy(s_pMdlGPUWkList);
	s_pMdlGPUWkList = nullptr;
	VKG.mInitFlg = false;
}

static int get_screen_width() {
	return int(VKG.mSwapChainExtent.width);
}

static int get_screen_height() {
	return int(VKG.mSwapChainExtent.height);
}

static cxMtx get_shadow_bias_mtx() {
	static const float bias[4 * 4] = {
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	};
	return nxMtx::from_mem(bias);
}

static void batch(cxModelWork* pWk, const int ibat, const Draw::Mode mode, const Draw::Context* pCtx) {
	if (!VKG.mInitFlg) return;
	if (!pCtx) return;
	if (!pWk) return;
	sxModelData* pMdl = pWk->mpData;
	if (!pMdl) return;
	prepare_model(pMdl);
	VKG.draw_batch(pWk, ibat, mode, pCtx);
}

static void begin(const cxColor& clearColor) {
	VKG.begin(clearColor);
}

static void end() {
	VKG.end();
}

Draw::Ifc s_ifc;

struct DrwInit {
	DrwInit() {
		::memset(&s_ifc, 0, sizeof(s_ifc));
		s_ifc.info.pName = "vk";
		s_ifc.info.needOGLContext = false;
		s_ifc.init = init;
		s_ifc.reset = reset;
		s_ifc.get_screen_width = get_screen_width;
		s_ifc.get_screen_height = get_screen_height;
		s_ifc.get_shadow_bias_mtx = get_shadow_bias_mtx;
		s_ifc.begin = begin;
		s_ifc.end = end;
		s_ifc.batch = batch;
		Draw::register_ifc_impl(&s_ifc);
	}
} s_drwInit;

DRW_IMPL_END


#endif // DRW_NO_VULKAN

