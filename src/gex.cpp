/*
 * Author: Sergey Chaban <sergey.chaban@gmail.com>
 */

#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <tchar.h>

#include <d3d11.h>

#include "crossdata.hpp"
#include "gex.hpp"

#include "gpu/context.h"
#include "gpu/code/vs_obj.h"
#include "gpu/code/ps_mtl.h"
#include "gpu/code/vs_sdw.h"
#include "gpu/code/ps_sdw.h"
#include "gpu/code/vs_env.h"
#include "gpu/code/ps_env.h"
#include "gpu/code/hs_pntri.h"
#include "gpu/code/ds_pntri.h"

#include "gpu/font.inc"

#define D_GEX_CPU_STD 0
#define D_GEX_CPU_SSE 1
#define D_GEX_CPU_AVX 2
#define D_GEX_CPU D_GEX_CPU_AVX

#if D_GEX_CPU > D_GEX_CPU_STD
#	include <intrin.h>
#	define D_GEX_SIMD_SET(_x, _y, _z, _w) _mm_set_ps(_w, _z, _y, _x)
#	define D_GEX_SIMD_MIX_MASK(_ix0, _iy0, _iz1, _iw1) ( (_ix0)|((_iy0)<<2)|((_iz1)<<4)|((_iw1)<<6) )
#	define D_GEX_SIMD_ELEM_MASK(_idx) D_GEX_SIMD_MIX_MASK(_idx, _idx, _idx, _idx)
#	define D_GEX_SIMD_ELEM(_v, _idx) _mm_shuffle_ps(_v, _v, D_GEX_SIMD_ELEM_MASK(_idx))
#	define D_GEX_SIMD_SHUF(_v, _ix, _iy, _iz, _iw) _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128((_v)), D_GEX_SIMD_MIX_MASK(_ix, _iy, _iz, _iw)))
#endif

#define D_GEX_SORT_BITS 24
#define D_GEX_SORT_MASK ((1<<(D_GEX_SORT_BITS)) - 1)

#define D_GEX_LYR_CAST_START 0
#define D_GEX_LYR_CAST 1
#define D_GEX_LYR_CAST_END 2
#define D_GEX_LYR_OPAQ 3
#define D_GEX_LYR_SEMI 4

#define D_GEX_MTL_TAG XD_FOURCC('G', 'M', 'T', 'L')

static void gexStoreTMtx(xt_mtx* pMtxDst, const cxMtx& mtxSrc) {
	if (!pMtxDst) return;
	cxMtx tm = mtxSrc.get_transposed();
	::memcpy(pMtxDst, &tm, sizeof(xt_mtx));
}

static cxMtx gexLoadTMtx(const xt_mtx& tm) {
	cxMtx m;
	m.from_mem(tm);
	m.transpose();
	return m;
}

static void gexStoreWMtx(xt_wmtx* pWmtxDst, const cxMtx& mtxSrc) {
	if (!pWmtxDst) return;
#if D_GEX_CPU > D_GEX_CPU_STD
	float* pSrc = (float*)&mtxSrc;
	__m128 r0 = _mm_loadu_ps(&pSrc[0x0]);
	__m128 r1 = _mm_loadu_ps(&pSrc[0x4]);
	__m128 r2 = _mm_loadu_ps(&pSrc[0x8]);
	__m128 r3 = _mm_loadu_ps(&pSrc[0xC]);
	__m128 t0 = _mm_unpacklo_ps(r0, r1);
	__m128 t1 = _mm_unpackhi_ps(r0, r1);
	__m128 t2 = _mm_unpacklo_ps(r2, r3);
	__m128 t3 = _mm_unpackhi_ps(r2, r3);
	float* pDst = (float*)pWmtxDst;
	_mm_storeu_ps(&pDst[0], _mm_movelh_ps(t0, t2));
	_mm_storeu_ps(&pDst[4], _mm_movehl_ps(t2, t0));
	_mm_storeu_ps(&pDst[8], _mm_movelh_ps(t1, t3));
#else
	cxMtx tm = mtxSrc.get_transposed();
	::memcpy(pWmtxDst, &tm, sizeof(xt_wmtx));
#endif
}

static void gexStoreVec(xt_float3* pVec, const cxVec& vec) {
	if (!pVec) return;
	::memcpy(pVec, &vec, sizeof(xt_float3));
}

static void gexStoreVecAsF4(xt_float4* pVec, const cxVec& vec) {
	if (!pVec) return;
	pVec->set(vec.x, vec.y, vec.z, 0.0f);
}

static void gexStoreRGB(xt_float3* pRGB, const cxColor& clr) {
	if (!pRGB) return;
	pRGB->set(clr.r, clr.g, clr.b);
}

static void gexStoreRGBA(xt_float4* pRGBA, const cxColor& clr) {
	if (!pRGBA) return;
	pRGBA->set(clr.r, clr.g, clr.b, clr.a);
}

static void gexEncodeHalf4(xt_half4* pDst, const xt_float4& src) {
	if (!pDst) return;
#if D_GEX_CPU >= D_GEX_CPU_AVX
	__m128 c128 = _mm_loadu_ps(src);
	__m128i c64 = _mm_cvtps_ph(c128, 0);
	uint32_t* pDst32 = (uint32_t*)pDst;
	pDst32[0] = _mm_cvtsi128_si32(c64);
	pDst32[1] = _mm_cvtsi128_si32(_mm_shuffle_epi32(c64, 0x55));
#else
	pDst->set(src);
#endif
}


struct GEX_VTX {
	xt_float3 mPos;
	xt_int    mPID;
#if D_GEX_VTXENC == D_GEX_VTXENC_COMPACT
	xt_half4  mNTO;
	xt_half4  mClr;
	xt_half4  mTex;
#else
	xt_float3 mNrm;
	xt_float3 mTng;
	xt_float4 mClr;
	xt_float4 mTex;
#endif


#if D_GEX_VTXENC == D_GEX_VTXENC_COMPACT
	void set_nrm_tng(const cxVec& nrm, const cxVec& tng) {
		xt_float2 octN = nrm.encode_octa();
		xt_float2 octT = tng.encode_octa();
		xt_float4 nto;
		nto.set(octN.x, octN.y, octT.x, octT.y);
		gexEncodeHalf4(&mNTO, nto);
	}

	void set_clr(const cxColor& clr) {
		gexEncodeHalf4(&mClr, *(const xt_float4*)&clr);
	}

	void set_tex(const xt_texcoord& uv, const xt_texcoord& uv2) {
		xt_float4 tex;
		tex.set(uv.u, uv.v, uv2.u, uv2.v);
		gexEncodeHalf4(&mTex, tex);
	}
#else
	void set_nrm_tng(const cxVec& nrm, const cxVec& tng) {
		mNrm = nrm;
		mTng = tng;
	}

	void set_clr(const cxColor& clr) {
		mClr.set(clr.r, clr.g, clr.b, clr.a);
	}

	void set_tex(const xt_texcoord& uv, const xt_texcoord& uv2) {
		mTex.set(uv.u, uv.v, uv2.u, uv2.v);
	}
#endif
};

struct GEX_DISP_ENTRY {
	void* mpData;
	void (*mpFunc)(GEX_DISP_ENTRY&);
	GEX_LIT* mpLit;
	uint32_t mSortKey;

	int get_layer() const {
		int mask = (1 << (32 - D_GEX_SORT_BITS)) - 1;
		return (mSortKey >> D_GEX_SORT_BITS) & mask;
	}
};

template<typename T> struct StructuredBuffer {
	ID3D11Buffer* mpBuf;
	ID3D11ShaderResourceView* mpSRV;

	bool is_valid() const { return (mpBuf && mpSRV); }

	void alloc(int nelems) {
		D3D11_BUFFER_DESC dsc;
		::ZeroMemory(&dsc, sizeof(dsc));
		dsc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		dsc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		dsc.ByteWidth = (UINT)(sizeof(T)*nelems);
		dsc.StructureByteStride = (UINT)sizeof(T);
		GWK.mpDev->CreateBuffer(&dsc, nullptr, &mpBuf);
		if (mpBuf) {
			D3D11_SHADER_RESOURCE_VIEW_DESC dscSRV;
			::ZeroMemory(&dscSRV, sizeof(dscSRV));
			dscSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			dscSRV.Format = DXGI_FORMAT_UNKNOWN;
			dscSRV.BufferEx.NumElements = nelems;
			GWK.mpDev->CreateShaderResourceView(mpBuf, &dscSRV, &mpSRV);
		}
	}

	void release() {
		if (mpSRV) {
			mpSRV->Release();
			mpSRV = nullptr;
		}
		if (mpBuf) {
			mpBuf->Release();
			mpBuf = nullptr;
		}
	}

	void copy_data(const T* pData) {
		if (!pData) return;
		if (!is_valid()) return;
		ID3D11DeviceContext* pCtx = GWK.mpCtx;
		if (pCtx) {
			pCtx->UpdateSubresource(mpBuf, 0, nullptr, pData, 0, 0);
		}
	}
};

template<typename T> T* gexTypeAlloc(uint32_t tag, int n = 1, bool zeroFlg = true) {
	size_t memsize = sizeof(T) * n;
	T* pT = nullptr;
	if (n > 0) {
		pT = reinterpret_cast<T*>(nxCore::mem_alloc(memsize, tag));
		if (pT) {
			if (zeroFlg) {
				::memset(pT, 0, memsize);
			}
		}
	}
	return pT;
}

template<typename T> T* gexTypeCpy(const T* pSrc, uint32_t tag, int n = 1) {
	T* pDst = gexTypeAlloc<T>(tag, n, false);
	if (pDst && pSrc) {
		::memcpy(pDst, pSrc, sizeof(T) * n);
	}
	return pDst;
}

template<typename T> void gexLstLink(T* pItem, T** ppHead, T** ppTail) {
	pItem->mpPrev = nullptr;
	pItem->mpNext = nullptr;
	if (*ppHead) {
		pItem->mpPrev = *ppTail;
	} else {
		*ppHead = pItem;
	}
	if (*ppTail) {
		(*ppTail)->mpNext = pItem;
	}
	*ppTail = pItem;
}

template<typename T> void gexLstUnlink(T* pItem, T** ppHead, T** ppTail) {
	T* pNext = pItem->mpNext;
	if (pItem->mpPrev) {
		pItem->mpPrev->mpNext = pNext;
		if (pNext) {
			pNext->mpPrev = pItem->mpPrev;
		} else {
			*ppTail = pItem->mpPrev;
		}
	} else {
		*ppHead = pNext;
		if (pNext) {
			pNext->mpPrev = nullptr;
		} else {
			*ppTail = nullptr;
		}
	}
}

template<typename T> T* gexLstFind(T* pHead, const char* pName, const char* pPath) {
	T* pRes = nullptr;
	if (pName) {
		T* pWk = pHead;
		uint32_t h = nxCore::str_hash32(pName);
		if (pPath) {
			uint32_t h2 = nxCore::str_hash32(pPath);
			while (pWk) {
				if (h == pWk->mNameHash && h2 == pWk->mPathHash) {
					if (nxCore::str_eq(pName, pWk->mpName) && nxCore::str_eq(pPath, pWk->mpPath)) {
						pRes = pWk;
						break;
					}
				}
				pWk = pWk->mpNext;
			}
		} else {
			while (pWk) {
				if (h == pWk->mNameHash) {
					if (nxCore::str_eq(pName, pWk->mpName)) {
						pRes = pWk;
						break;
					}
				}
				pWk = pWk->mpNext;
			}
		}
	}
	return pRes;
}

template<typename T> T* gexLstFind(T* pHead, const char* pName) {
	T* pRes = nullptr;
	if (pName) {
		T* pWk = pHead;
		uint32_t h = nxCore::str_hash32(pName);
		while (pWk) {
			if (h == pWk->mNameHash) {
				if (nxCore::str_eq(pName, pWk->mpName)) {
					pRes = pWk;
					break;
				}
			}
			pWk = pWk->mpNext;
		}
	}
	return pRes;
}

static struct GEX_GWK {
	HINSTANCE mhInstance;
	ATOM mClassAtom;
	int mWidth;
	int mHeight;
	float mAspect;
	HWND mhWnd;
	void (*mpMsgFunc)(const GEX_WNDMSG&);
	int mDispFreq;
	int64_t mFrameCount;
	HMODULE mhD3D11;
	PFN_D3D11_CREATE_DEVICE mpD3D11CreateDevice;
	PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN mpD3D11CreateDeviceAndSwapChain;
	ID3D11Device* mpDev;
	ID3D11DeviceContext* mpCtx;
	IDXGISwapChain* mpSwapChain;
	ID3D11RenderTargetView* mpTargetView;
	ID3D11DepthStencilView* mpDepthView;
	ID3D11DepthStencilState* mpDepthState;
	ID3D11Texture2D* mpDepthStencil;
	ID3D11RasterizerState* mpOneSidedRS;
	ID3D11RasterizerState* mpDblSidedRS;
	ID3D11InputLayout* mpObjVtxLayout;
	ID3D11VertexShader* mpObjVS;
	ID3D11PixelShader* mpMtlPS;
	ID3D11VertexShader* mpSdwVS;
	ID3D11PixelShader* mpSdwPS;
	ID3D11HullShader* mpPNTriHS;
	ID3D11DomainShader* mpPNTriDS;
	ID3D11SamplerState* mpSmpStateLinWrap;
	ID3D11SamplerState* mpSmpStateLinClamp;
	ID3D11SamplerState* mpSmpStatePnt;
	ID3D11BlendState* mpBlendOpaq;
	ID3D11BlendState* mpBlendSemi;
	ID3D11BlendState* mpBlendSemiCov;
	ID3D11BlendState* mpBlendSemiBlendCov;
	ID3D11InputLayout* mpEnvVtxLayout;
	ID3D11VertexShader* mpEnvVS;
	ID3D11PixelShader* mpEnvPS;
	ID3D11Buffer* mpEnvVB;
	ID3D11Buffer* mpEnvIB;
	StructuredBuffer<XFORM_CTX> mEnvXformCtx;
	StructuredBuffer<GLB_CTX> mGlbBuf;
	GLB_CTX mGlbWk;
	GEX_OBJ* mpObjLstHead;
	GEX_OBJ* mpObjLstTail;
	GEX_POL* mpPolLstHead;
	GEX_POL* mpPolLstTail;
	GEX_TEX* mpTexLstHead;
	GEX_TEX* mpTexLstTail;
	GEX_CAM* mpCamLstHead;
	GEX_CAM* mpCamLstTail;
	GEX_LIT* mpLitLstHead;
	GEX_LIT* mpLitLstTail;
	int mObjCount;
	int mPolCount;
	int mTexCount;
	int mCamCount;
	int mLitCount;
	GEX_DISP_ENTRY* mpDispList;
	int mDispListSize;
	int mDispListPtr;
	GEX_CAM* mpScnCam;
	GEX_CAM* mpDefCam;
	GEX_LIT* mpDefLit;
	GEX_TEX* mpDefTexWhite;
	GEX_TEX* mpDefTexBlack;
	cxVec mDefLightPos;
	cxVec mDefLightDir;
	int mObjTriCnt;
	int mObjTriDrawCnt;
	int mObjCullCnt;
	int mBatCullCnt;
	int mPolCullCnt;
	int mShadowCastCnt;
	int mShadowRecvCnt;
	float mShadowFadeStart;
	float mShadowFadeEnd;
	float mShadowViewSize;
	cxVec mShadowDir;
	float mShadowDensity;
	cxColor mShadowColor;
	cxMtx mShadowViewProj;
	cxMtx mShadowMtx;
	cxAABB mShadowCastersBBox;
	cxAABB mShadowReceiversBBox;
	float mShadowSpecValSel;
	int mShadowLightIdx;
	int mShadowMapSize;
	GEX_SHADOW_PROJ mShadowProj;
	ID3D11Texture2D* mpTexSM;
	ID3D11RenderTargetView* mpSMTargetView;
	ID3D11ShaderResourceView* mpSMRrsrcView;
	ID3D11Texture2D* mpSMDepthTex;
	ID3D11DepthStencilView* mpSMDepthView;
	StructuredBuffer<SDW_CTX> mSdwBuf;
	GEX_TEX* mpBgPanoTex;
	GEX_BG_MODE mBgMode;
	bool mPreciseCulling;
} GWK;

static bool s_initFlg = false;


static LRESULT CALLBACK gexWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	LRESULT res = 0;
	if (GWK.mpMsgFunc) {
		GEX_WNDMSG msgInfo;
		::memset(&msgInfo, 0, sizeof(msgInfo));
		msgInfo.mHandle = hWnd;
		msgInfo.mCode = msg;
		msgInfo.mParams[0] = (uint32_t)wParam;
		msgInfo.mParams[1] = (uint32_t)lParam;
		(*GWK.mpMsgFunc)(msgInfo);
	}
	switch (msg) {
		case WM_DESTROY:
			::PostQuitMessage(0);
			break;
		default:
			res = ::DefWindowProc(hWnd, msg, wParam, lParam);
			break;
	}
	return res;
}

static const TCHAR* s_className = _T("gexWindow");

void gexInit(const GEX_CONFIG& cfg) {
	if (s_initFlg) {
		return;
	}
	::ZeroMemory(&GWK, sizeof(GWK));

	cxQuat q;
	q.set_rot_degrees(cxVec(-35.0f, -45.0f, 0.0f));
	GWK.mDefLightDir = q.apply(nxVec::get_axis(exAxis::MINUS_Z));
	GWK.mDefLightPos.set(-2.0f, 2.0f, 2.0f);

	GWK.mhInstance = (HINSTANCE)cfg.mSysHandle;
	GWK.mpMsgFunc = cfg.mpMsgFunc;
	WNDCLASSEX wc;
	::ZeroMemory(&wc, sizeof(WNDCLASSEX));
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_VREDRAW | CS_HREDRAW;
	wc.hInstance = GWK.mhInstance;
	wc.hCursor = ::LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)::GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = s_className;
	wc.lpfnWndProc = gexWndProc;
	wc.cbWndExtra = 0x10;
	GWK.mClassAtom = ::RegisterClassEx(&wc);

	GWK.mWidth = cfg.mWidth;
	GWK.mHeight = cfg.mHeight;
	GWK.mAspect = (float)cfg.mWidth / cfg.mHeight;
	RECT rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = GWK.mWidth;
	rect.bottom = GWK.mHeight;
	int style = WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_GROUP;
	::AdjustWindowRect(&rect, style, FALSE);
	int wndW = rect.right - rect.left;
	int wndH = rect.bottom - rect.top;
	TCHAR title[128];
	::ZeroMemory(title, sizeof(title));
	::_stprintf_s(title, XD_ARY_LEN(title), _T("%s: build %s"), _T("GexDX11"), _T(__DATE__));
	GWK.mhWnd = ::CreateWindowEx(0, s_className, title, style, 0, 0, wndW, wndH, NULL, NULL, GWK.mhInstance, NULL);
	if (GWK.mhWnd) {
		::ShowWindow(GWK.mhWnd, SW_SHOW);
		::UpdateWindow(GWK.mhWnd);
	}

	int freq = 60;
	DEVMODE devmode;
	::ZeroMemory(&devmode, sizeof(DEVMODE));
	devmode.dmSize = sizeof(DEVMODE);
	if (::EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devmode)) {
		freq = devmode.dmDisplayFrequency;
	}
	GWK.mDispFreq = freq;

	HRESULT hres;
	GWK.mhD3D11 = ::LoadLibraryW(L"d3d11.dll");
	if (GWK.mhD3D11) {
		*(FARPROC*)&GWK.mpD3D11CreateDevice = ::GetProcAddress(GWK.mhD3D11, "D3D11CreateDevice");
		*(FARPROC*)&GWK.mpD3D11CreateDeviceAndSwapChain = ::GetProcAddress(GWK.mhD3D11, "D3D11CreateDeviceAndSwapChain");
		hres = GWK.mpD3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &GWK.mpDev, NULL, &GWK.mpCtx);
		if (FAILED(hres)) {
			GWK.mpDev = nullptr;
			GWK.mpCtx = nullptr;
		}
	}

	DXGI_SWAP_CHAIN_DESC scd;
	if (GWK.mpDev) {
		::ZeroMemory(&scd, sizeof(scd));
		scd.BufferCount = 1;
		scd.BufferDesc.Width = GWK.mWidth;
		scd.BufferDesc.Height = GWK.mHeight;
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd.BufferDesc.RefreshRate.Numerator = (UINT)GWK.mDispFreq;
		scd.BufferDesc.RefreshRate.Denominator = 1;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.OutputWindow = GWK.mhWnd;
		scd.SampleDesc.Count = 1;
		scd.SampleDesc.Quality = 0;
		scd.Windowed = TRUE;
		if (cfg.mMSAA > 0) {
			UINT msaaNSmp = cfg.mMSAA > 1 ? 8 : 4;
			UINT msaaQLvl = 0;
			if (SUCCEEDED(GWK.mpDev->CheckMultisampleQualityLevels(scd.BufferDesc.Format, msaaNSmp, &msaaQLvl))) {
				if (msaaQLvl > 0) {
					scd.SampleDesc.Count = msaaNSmp;
					scd.SampleDesc.Quality = msaaQLvl - 1;
				}
			}
		}
		IDXGIDevice* pDXGIDev = nullptr;
		hres = GWK.mpDev->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDev);
		if (SUCCEEDED(hres)) {
			IDXGIAdapter* pAdapter = nullptr;
			hres = pDXGIDev->GetAdapter(&pAdapter);
			if (SUCCEEDED(hres)) {
				IDXGIFactory* pFct = nullptr;
				hres = pAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&pFct);
				if (SUCCEEDED(hres)) {
					hres = pFct->CreateSwapChain(GWK.mpDev, &scd, &GWK.mpSwapChain);
					if (FAILED(hres)) {
						GWK.mpSwapChain = nullptr;
					}
					pFct->Release();
				}
				pAdapter->Release();
			}
			pDXGIDev->Release();
		}
	}

	if (GWK.mpDev) {
		GWK.mGlbBuf.alloc(1);
		gexDiffLightFactor(cxColor(2.0f));
		gexDiffSHFactor(cxColor(2.0f));
		gexReflSHFactor(cxColor(1.0f));
		gexLinearWhite(1.0f);
		gexLinearGain(1.0f);
		gexLinearBias(0.0f);
		gexGamma(2.2f);
		gexUpdateGlobals();
	}

	if (GWK.mpDev && GWK.mpCtx) {
		D3D11_VIEWPORT vp;
		::ZeroMemory(&vp, sizeof(vp));
		vp.Width = (FLOAT)GWK.mWidth;
		vp.Height = (FLOAT)GWK.mHeight;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		GWK.mpCtx->RSSetViewports(1, &vp);

		D3D11_RASTERIZER_DESC dscRS;
		::ZeroMemory(&dscRS, sizeof(dscRS));
		dscRS.AntialiasedLineEnable = FALSE;
		dscRS.CullMode = D3D11_CULL_BACK;
		dscRS.DepthClipEnable = TRUE;
		dscRS.FillMode = D3D11_FILL_SOLID;
		dscRS.FrontCounterClockwise = FALSE;
		hres = GWK.mpDev->CreateRasterizerState(&dscRS, &GWK.mpOneSidedRS);
		if (SUCCEEDED(hres)) {
			GWK.mpCtx->RSSetState(GWK.mpOneSidedRS);
		} else {
			GWK.mpOneSidedRS = nullptr;
		}
		dscRS.CullMode = D3D11_CULL_NONE;
		hres = GWK.mpDev->CreateRasterizerState(&dscRS, &GWK.mpDblSidedRS);
		if (FAILED(hres)) {
			GWK.mpDblSidedRS = nullptr;
		}
	}

	if (GWK.mpSwapChain) {
		ID3D11Texture2D* pBack = nullptr;
		hres = GWK.mpSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBack);
		if (SUCCEEDED(hres)) {
			hres = GWK.mpDev->CreateRenderTargetView(pBack, nullptr, &GWK.mpTargetView);
			pBack->Release();
			pBack = nullptr;
			if (SUCCEEDED(hres)) {
				gexClearTarget();
				GWK.mpSwapChain->Present(0, 0);
			}

			D3D11_TEXTURE2D_DESC dscDepth;
			::ZeroMemory(&dscDepth, sizeof(dscDepth));
			dscDepth.Width = GWK.mWidth;
			dscDepth.Height = GWK.mHeight;
			dscDepth.MipLevels = 1;
			dscDepth.ArraySize = 1;
			dscDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dscDepth.SampleDesc.Count = scd.SampleDesc.Count;
			dscDepth.SampleDesc.Quality = scd.SampleDesc.Quality;
			dscDepth.Usage = D3D11_USAGE_DEFAULT;
			dscDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			hres = GWK.mpDev->CreateTexture2D(&dscDepth, nullptr, &GWK.mpDepthStencil);
			if (SUCCEEDED(hres)) {
				D3D11_DEPTH_STENCIL_VIEW_DESC dscDV;
				::ZeroMemory(&dscDV, sizeof(dscDV));
				dscDV.Format = dscDepth.Format;
				dscDV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
				if (scd.SampleDesc.Count > 1) {
					dscDV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
				}
				hres = GWK.mpDev->CreateDepthStencilView(GWK.mpDepthStencil, &dscDV, &GWK.mpDepthView);
				if (SUCCEEDED(hres)) {
					gexClearDepth();
				} else {
					GWK.mpDepthView = nullptr;
				}
			}

			D3D11_DEPTH_STENCIL_DESC dscDepthState;
			::ZeroMemory(&dscDepthState, sizeof(dscDepthState));
			dscDepthState.DepthEnable = TRUE;
			dscDepthState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			dscDepthState.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
			dscDepthState.StencilEnable = FALSE;
			hres = GWK.mpDev->CreateDepthStencilState(&dscDepthState, &GWK.mpDepthState);
			if (SUCCEEDED(hres)) {
				GWK.mpCtx->OMSetDepthStencilState(GWK.mpDepthState, 0);
			}
		}
	}

	if (GWK.mpTargetView && GWK.mpDepthView) {
		GWK.mpCtx->OMSetRenderTargets(1, &GWK.mpTargetView, GWK.mpDepthView);
	}

	static D3D11_INPUT_ELEMENT_DESC vtxDsc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GEX_VTX, mPos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "PSIZE", 0, DXGI_FORMAT_R32_UINT, 0, offsetof(GEX_VTX, mPID), D3D11_INPUT_PER_VERTEX_DATA, 0 },
#if D_GEX_VTXENC == D_GEX_VTXENC_COMPACT
		{ "NORMAL", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, offsetof(GEX_VTX, mNTO), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, offsetof(GEX_VTX, mClr), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, offsetof(GEX_VTX, mTex), D3D11_INPUT_PER_VERTEX_DATA, 0 }
#else
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GEX_VTX, mNrm), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GEX_VTX, mTng), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(GEX_VTX, mClr), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(GEX_VTX, mTex), D3D11_INPUT_PER_VERTEX_DATA, 0 }
#endif
	};
	ID3D11InputLayout* pIL = nullptr;
	hres = GWK.mpDev->CreateInputLayout(vtxDsc, XD_ARY_LEN(vtxDsc), vs_obj, sizeof(vs_obj), &pIL);
	if (SUCCEEDED(hres)) {
		GWK.mpObjVtxLayout = pIL;
	}

	ID3D11VertexShader* pVS = nullptr;
	hres = GWK.mpDev->CreateVertexShader(vs_obj, sizeof(vs_obj), nullptr, &pVS);
	if (SUCCEEDED(hres)) {
		GWK.mpObjVS = pVS;
	}

	ID3D11PixelShader* pPS = nullptr;
	hres = GWK.mpDev->CreatePixelShader(ps_mtl, sizeof(ps_mtl), nullptr, &pPS);
	if (SUCCEEDED(hres)) {
		GWK.mpMtlPS = pPS;
	}

	pVS = nullptr;
	hres = GWK.mpDev->CreateVertexShader(vs_sdw, sizeof(vs_sdw), nullptr, &pVS);
	if (SUCCEEDED(hres)) {
		GWK.mpSdwVS = pVS;
	}

	pPS = nullptr;
	hres = GWK.mpDev->CreatePixelShader(ps_sdw, sizeof(ps_sdw), nullptr, &pPS);
	if (SUCCEEDED(hres)) {
		GWK.mpSdwPS = pPS;
	}

	ID3D11HullShader* pHS = nullptr;
	hres = GWK.mpDev->CreateHullShader(hs_pntri, sizeof(hs_pntri), nullptr, &pHS);
	if (SUCCEEDED(hres)) {
		GWK.mpPNTriHS = pHS;
	}

	ID3D11DomainShader* pDS = nullptr;
	hres = GWK.mpDev->CreateDomainShader(ds_pntri, sizeof(ds_pntri), nullptr, &pDS);
	if (SUCCEEDED(hres)) {
		GWK.mpPNTriDS = pDS;
	}

	static D3D11_INPUT_ELEMENT_DESC envVtxDsc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	hres = GWK.mpDev->CreateInputLayout(envVtxDsc, XD_ARY_LEN(envVtxDsc), vs_env, sizeof(vs_env), &GWK.mpEnvVtxLayout);
	hres = GWK.mpDev->CreateVertexShader(vs_env, sizeof(vs_env), nullptr, &GWK.mpEnvVS);
	hres = GWK.mpDev->CreatePixelShader(ps_env, sizeof(ps_env), nullptr, &GWK.mpEnvPS);
	GWK.mEnvXformCtx.alloc(1);
	static float envVtx[] = { -1,-1,1, -1,1,1, 1,1,1, 1,-1,1, -1,-1,-1, -1,1,-1, 1,1,-1, 1,-1,-1 };
	static uint16_t envIdx[] = { 0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,6,2,3,6,3,7,1,6,5,1,2,6,0,4,7,0,7,3 };
	struct {
		D3D11_BUFFER_DESC dsc;
		D3D11_SUBRESOURCE_DATA dat;
	} envBuf;
	::ZeroMemory(&envBuf, sizeof(envBuf));
	envBuf.dsc.ByteWidth = sizeof(envVtx);
	envBuf.dsc.Usage = D3D11_USAGE_DEFAULT;
	envBuf.dsc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	envBuf.dat.pSysMem = envVtx;
	hres = GWK.mpDev->CreateBuffer(&envBuf.dsc, &envBuf.dat, &GWK.mpEnvVB);
	if (SUCCEEDED(hres)) {
		::ZeroMemory(&envBuf, sizeof(envBuf));
		envBuf.dsc.ByteWidth = sizeof(envIdx);
		envBuf.dsc.Usage = D3D11_USAGE_DEFAULT;
		envBuf.dsc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		envBuf.dat.pSysMem = envIdx;
		hres = GWK.mpDev->CreateBuffer(&envBuf.dsc, &envBuf.dat, &GWK.mpEnvIB);
	}
	GWK.mBgMode = GEX_BG_MODE::NONE;

	D3D11_SAMPLER_DESC smpDsc;
	::ZeroMemory(&smpDsc, sizeof(D3D11_SAMPLER_DESC));
	smpDsc.Filter = D3D11_FILTER_ANISOTROPIC;
	smpDsc.MaxAnisotropy = 16;
	smpDsc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	smpDsc.MaxLOD = D3D11_FLOAT32_MAX;
	smpDsc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	smpDsc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	smpDsc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	hres = GWK.mpDev->CreateSamplerState(&smpDsc, &GWK.mpSmpStateLinWrap);
	smpDsc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	smpDsc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	smpDsc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	hres = GWK.mpDev->CreateSamplerState(&smpDsc, &GWK.mpSmpStateLinClamp);
	smpDsc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	hres = GWK.mpDev->CreateSamplerState(&smpDsc, &GWK.mpSmpStatePnt);

	D3D11_BLEND_DESC blendDsc;
	::ZeroMemory(&blendDsc, sizeof(D3D11_BLEND_DESC));
	blendDsc.AlphaToCoverageEnable = FALSE;
	blendDsc.IndependentBlendEnable = TRUE;
	int nrt = XD_ARY_LEN(blendDsc.RenderTarget);
	for (int i = 0; i < nrt; ++i) {
		blendDsc.RenderTarget[i].BlendEnable = FALSE;
		blendDsc.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
		blendDsc.RenderTarget[i].DestBlend = D3D11_BLEND_ZERO;
		blendDsc.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
		blendDsc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDsc.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendDsc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDsc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	}
	GWK.mpDev->CreateBlendState(&blendDsc, &GWK.mpBlendOpaq);
	blendDsc.AlphaToCoverageEnable = TRUE;
	GWK.mpDev->CreateBlendState(&blendDsc, &GWK.mpBlendSemiCov);
	blendDsc.RenderTarget[0].BlendEnable = TRUE;
	blendDsc.AlphaToCoverageEnable = FALSE;
	blendDsc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDsc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDsc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDsc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blendDsc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDsc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	GWK.mpDev->CreateBlendState(&blendDsc, &GWK.mpBlendSemi);
	blendDsc.AlphaToCoverageEnable = TRUE;
	GWK.mpDev->CreateBlendState(&blendDsc, &GWK.mpBlendSemiBlendCov);

	GWK.mDispListSize = 32768*2;
	GWK.mpDispList = gexTypeAlloc<GEX_DISP_ENTRY>(XD_FOURCC('D', 'I', 'S', 'P'), GWK.mDispListSize);
	GWK.mDispListPtr = 0;

	GWK.mpDefCam = gexCamCreate(D_GEX_SYS_CAM_NAME);
	GWK.mpDefLit = gexLitCreate(D_GEX_SYS_LIT_NAME);

	GWK.mpDefTexWhite = gexTexCreateConst(cxColor(1.0f), "<white>");
	GWK.mpDefTexBlack = gexTexCreateConst(cxColor(0.0f), "<black>");

	GWK.mShadowViewSize = 2.0f;
	GWK.mShadowDir = GWK.mDefLightDir;
	GWK.mShadowColor = cxColor(0.0f, 0.0f, 0.0f, 1.0f);
	GWK.mShadowDensity = 0.5f;
	GWK.mShadowProj = GEX_SHADOW_PROJ::UNIFORM;
	gexShadowFade(0.0f, 0.0f);
	if (cfg.mShadowMapSize > 0) {
		int smapSize = nxCalc::min(cfg.mShadowMapSize, 4096*2);
		GWK.mShadowMapSize = smapSize;

		DXGI_FORMAT smapFmt = DXGI_FORMAT_R32_FLOAT;
		//DXGI_FORMAT smapFmt = DXGI_FORMAT_R32G32_FLOAT; //////////////////// DBG
		D3D11_TEXTURE2D_DESC dscSM;
		::ZeroMemory(&dscSM, sizeof(dscSM));
		dscSM.ArraySize = 1;
		dscSM.Format = smapFmt;
		dscSM.Width = smapSize;
		dscSM.Height = smapSize;
		dscSM.MipLevels = 1;
		dscSM.SampleDesc.Count = 1;
		dscSM.SampleDesc.Quality = 0;
		dscSM.Usage = D3D11_USAGE_DEFAULT;
		dscSM.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		ID3D11Texture2D* pTexSM = nullptr;
		hres = GWK.mpDev->CreateTexture2D(&dscSM, nullptr, &pTexSM);
		if (SUCCEEDED(hres)) {
			GWK.mpTexSM = pTexSM;
			hres = GWK.mpDev->CreateRenderTargetView(GWK.mpTexSM, nullptr, &GWK.mpSMTargetView);
			if (SUCCEEDED(hres)) {
				dscSM.Format = DXGI_FORMAT_R32_TYPELESS;
				dscSM.BindFlags = D3D11_BIND_DEPTH_STENCIL;
				hres = GWK.mpDev->CreateTexture2D(&dscSM, nullptr, &pTexSM);
				if (SUCCEEDED(hres)) {
					GWK.mpSMDepthTex = pTexSM;
					D3D11_DEPTH_STENCIL_VIEW_DESC dscSMDV;
					::ZeroMemory(&dscSMDV, sizeof(dscSMDV));
					dscSMDV.Format = DXGI_FORMAT_D32_FLOAT;
					dscSMDV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
					hres = GWK.mpDev->CreateDepthStencilView(GWK.mpSMDepthTex, &dscSMDV, &GWK.mpSMDepthView);
				}
			}
			D3D11_SHADER_RESOURCE_VIEW_DESC dscSMSRV;
			::ZeroMemory(&dscSMSRV, sizeof(dscSMSRV));
			dscSMSRV.Format = smapFmt;
			dscSMSRV.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
			dscSMSRV.Texture2D.MipLevels = 1;
			ID3D11ShaderResourceView* pSMSRV = nullptr;
			hres = GWK.mpDev->CreateShaderResourceView(GWK.mpTexSM, &dscSMSRV, &pSMSRV);
			if (SUCCEEDED(hres)) {
				GWK.mpSMRrsrcView = pSMSRV;
			}
		}
		GWK.mSdwBuf.alloc(1);
	}

	GWK.mPreciseCulling = true;

	s_initFlg = true;
}

void gexReset() {
	if (!s_initFlg) {
		return;
	}

	gexTexDestroy(GWK.mpDefTexBlack);
	gexTexDestroy(GWK.mpDefTexWhite);
	gexLitDestroy(GWK.mpDefLit);
	gexCamDestroy(GWK.mpDefCam);

	while (GWK.mpObjLstTail) {
		nxCore::dbg_msg("[GEX] Warning: destroying object \"%s\".\n", gexObjName(GWK.mpObjLstTail));
		gexObjDestroy(GWK.mpObjLstTail);
	}
	while (GWK.mpPolLstTail) {
		nxCore::dbg_msg("[GEX] Warning: destroying polygons \"%s\".\n", gexPolName(GWK.mpPolLstTail));
		gexPolDestroy(GWK.mpPolLstTail);
	}
	while (GWK.mpTexLstTail) {
		nxCore::dbg_msg("[GEX] Warning: destroying texture \"%s\".\n", gexTexName(GWK.mpTexLstTail));
		gexTexDestroy(GWK.mpTexLstTail);
	}
	while (GWK.mpCamLstTail) {
		nxCore::dbg_msg("[GEX] Warning: destroying camera \"%s\".\n", gexCamName(GWK.mpCamLstTail));
		gexCamDestroy(GWK.mpCamLstTail);
	}
	while (GWK.mpLitLstTail) {
		nxCore::dbg_msg("[GEX] Warning: destroying lighting \"%s\".\n", gexLitName(GWK.mpLitLstTail));
		gexLitDestroy(GWK.mpLitLstTail);
	}

	nxCore::mem_free(GWK.mpDispList);

	GWK.mSdwBuf.release();
	if (GWK.mpSMRrsrcView) {
		GWK.mpSMRrsrcView->Release();
	}
	if (GWK.mpSMDepthView) {
		GWK.mpSMDepthView->Release();
	}
	if (GWK.mpSMDepthTex) {
		GWK.mpSMDepthTex->Release();
	}
	if (GWK.mpSMTargetView) {
		GWK.mpSMTargetView->Release();
	}
	if (GWK.mpTexSM) {
		GWK.mpTexSM->Release();
	}
	if (GWK.mpBlendOpaq) {
		GWK.mpBlendOpaq->Release();
	}
	if (GWK.mpBlendSemi) {
		GWK.mpBlendSemi->Release();
	}
	if (GWK.mpBlendSemiCov) {
		GWK.mpBlendSemiCov->Release();
	}
	if (GWK.mpBlendSemiBlendCov) {
		GWK.mpBlendSemiBlendCov->Release();
	}
	if (GWK.mpSmpStatePnt) {
		GWK.mpSmpStatePnt->Release();
	}
	if (GWK.mpSmpStateLinClamp) {
		GWK.mpSmpStateLinClamp->Release();
	}
	if (GWK.mpSmpStateLinWrap) {
		GWK.mpSmpStateLinWrap->Release();
	}
	if (GWK.mpPNTriDS) {
		GWK.mpPNTriDS->Release();
	}
	if (GWK.mpPNTriHS) {
		GWK.mpPNTriHS->Release();
	}
	if (GWK.mpSdwPS) {
		GWK.mpSdwPS->Release();
	}
	if (GWK.mpSdwVS) {
		GWK.mpSdwVS->Release();
	}
	if (GWK.mpMtlPS) {
		GWK.mpMtlPS->Release();
	}
	if (GWK.mpObjVS) {
		GWK.mpObjVS->Release();
	}
	if (GWK.mpObjVtxLayout) {
		GWK.mpObjVtxLayout->Release();
	}
	if (GWK.mpEnvPS) {
		GWK.mpEnvPS->Release();
	}
	if (GWK.mpEnvVS) {
		GWK.mpEnvVS->Release();
	}
	if (GWK.mpEnvVB) {
		GWK.mpEnvVB->Release();
	}
	if (GWK.mpEnvIB) {
		GWK.mpEnvIB->Release();
	}
	if (GWK.mpEnvVtxLayout) {
		GWK.mpEnvVtxLayout->Release();
	}
	GWK.mEnvXformCtx.release();
	if (GWK.mpDepthState) {
		GWK.mpDepthState->Release();
	}
	if (GWK.mpDepthView) {
		GWK.mpDepthView->Release();
	}
	if (GWK.mpDepthStencil) {
		GWK.mpDepthStencil->Release();
	}
	if (GWK.mpTargetView) {
		GWK.mpTargetView->Release();
	}
	if (GWK.mpDblSidedRS) {
		GWK.mpDblSidedRS->Release();
	}
	if (GWK.mpOneSidedRS) {
		GWK.mpOneSidedRS->Release();
	}
	GWK.mGlbBuf.release();
	if (GWK.mpSwapChain) {
		GWK.mpSwapChain->Release();
	}
	if (GWK.mpCtx) {
		GWK.mpCtx->Release();
	}
	if (GWK.mpDev) {
		GWK.mpDev->Release();
	}
	if (GWK.mhD3D11) {
		::FreeLibrary(GWK.mhD3D11);
	}
	::UnregisterClass(s_className, GWK.mhInstance);
	s_initFlg = false;
	::ZeroMemory(&GWK, sizeof(GWK));
}

void gexLoop(void (*pLoop)()) {
	MSG msg;
	bool done = false;
	while (!done) {
		if (::PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE)) {
			if (::GetMessage(&msg, NULL, 0, 0)) {
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			} else {
				done = true;
				break;
			}
		} else {
			pLoop();
		}
	}
}

int gexGetDispWidth() {
	return GWK.mWidth;
}

int gexGetDispHeight() {
	return GWK.mHeight;
}

int gexGetDispFreq() {
	return GWK.mDispFreq;
}

void gexClearTarget(const cxColor& clr) {
	if (s_initFlg && GWK.mpCtx && GWK.mpTargetView) {
		float c[4] = { clr.r, clr.g, clr.b, clr.a };
		GWK.mpCtx->ClearRenderTargetView(GWK.mpTargetView, c);
	}
}

void gexClearDepth() {
	if (s_initFlg && GWK.mpCtx && GWK.mpDepthView) {
		GWK.mpCtx->ClearDepthStencilView(GWK.mpDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
}

void gexDiffLightFactor(const cxColor& clr) {
	gexStoreRGB(&GWK.mGlbWk.diffLightFactor, clr);
}

void gexDiffSHFactor(const cxColor& clr) {
	gexStoreRGB(&GWK.mGlbWk.diffSHFactor, clr);
}

void gexReflSHFactor(const cxColor& clr) {
	gexStoreRGB(&GWK.mGlbWk.reflSHFactor, clr);
}

void gexGamma(float y) {
	GWK.mGlbWk.invGamma.fill(nxCalc::rcp0(y));
	GWK.mGlbWk.invGamma.w = 1.0f;
}

void gexGammaRGBA(float yr, float yg, float yb, float ya) {
	GWK.mGlbWk.invGamma.set(nxCalc::rcp0(yr), nxCalc::rcp0(yg), nxCalc::rcp0(yb), nxCalc::rcp0(ya));
}

void gexExposure(float e) {
	GWK.mGlbWk.exposure.fill(e);
}

void gexExposureRGB(float er, float eg, float eb) {
	GWK.mGlbWk.exposure.set(er, eg, eb);
}

void gexLinearWhite(float w) {
	GWK.mGlbWk.linWhite.fill(w);
}

void gexLinearWhiteRGB(float wr, float wg, float wb) {
	GWK.mGlbWk.linWhite.set(wr, wg, wb);
}

void gexLinearGain(float gain) {
	GWK.mGlbWk.linGain.fill(gain);
}

void gexLinearGainRGB(float r, float g, float b) {
	GWK.mGlbWk.linGain.set(r, g, b);
}

void gexLinearBias(float bias) {
	GWK.mGlbWk.linBias.fill(bias);
}

void gexLinearBiasRGB(float r, float g, float b) {
	GWK.mGlbWk.linBias.set(r, g, b);
}

void gexUpdateGlobals() {
	GWK.mGlbBuf.copy_data(&GWK.mGlbWk);
}

void gexBgMode(GEX_BG_MODE mode) {
	GWK.mBgMode = mode;
}

void gexBgPanoramaTex(GEX_TEX* pTex) {
	GWK.mpBgPanoTex = pTex;
}

float gexCalcFOVY(float focal, float aperture) {
	return nxCalc::calc_fovy(focal, aperture, GWK.mAspect);
}

void gexMtxMul(cxMtx* pDst, const cxMtx* pSrcA, const cxMtx* pSrcB) {
#if D_GEX_CPU == D_GEX_CPU_SSE
	float* pA = (float*)pSrcA;
	float* pB = (float*)pSrcB;
	__m128 rA0 = _mm_loadu_ps(&pA[0x0]);
	__m128 rA1 = _mm_loadu_ps(&pA[0x4]);
	__m128 rA2 = _mm_loadu_ps(&pA[0x8]);
	__m128 rA3 = _mm_loadu_ps(&pA[0xC]);
	__m128 rB0 = _mm_loadu_ps(&pB[0x0]);
	__m128 rB1 = _mm_loadu_ps(&pB[0x4]);
	__m128 rB2 = _mm_loadu_ps(&pB[0x8]);
	__m128 rB3 = _mm_loadu_ps(&pB[0xC]);
	float* pD = (float*)pDst;
	_mm_storeu_ps(&pD[0x0],
	    _mm_add_ps(_mm_add_ps(_mm_add_ps(
	        _mm_mul_ps(D_GEX_SIMD_ELEM(rA0, 0), rB0), _mm_mul_ps(D_GEX_SIMD_ELEM(rA0, 1), rB1)),
	        _mm_mul_ps(D_GEX_SIMD_ELEM(rA0, 2), rB2)), _mm_mul_ps(D_GEX_SIMD_ELEM(rA0, 3), rB3)));
	_mm_storeu_ps(&pD[0x4],
	    _mm_add_ps(_mm_add_ps(_mm_add_ps(
	        _mm_mul_ps(D_GEX_SIMD_ELEM(rA1, 0), rB0), _mm_mul_ps(D_GEX_SIMD_ELEM(rA1, 1), rB1)),
	        _mm_mul_ps(D_GEX_SIMD_ELEM(rA1, 2), rB2)), _mm_mul_ps(D_GEX_SIMD_ELEM(rA1, 3), rB3)));
	_mm_storeu_ps(&pD[0x8],
	    _mm_add_ps(_mm_add_ps(_mm_add_ps(
	        _mm_mul_ps(D_GEX_SIMD_ELEM(rA2, 0), rB0), _mm_mul_ps(D_GEX_SIMD_ELEM(rA2, 1), rB1)),
	        _mm_mul_ps(D_GEX_SIMD_ELEM(rA2, 2), rB2)), _mm_mul_ps(D_GEX_SIMD_ELEM(rA2, 3), rB3)));
	_mm_storeu_ps(&pD[0xC],
	    _mm_add_ps(_mm_add_ps(_mm_add_ps(
	        _mm_mul_ps(D_GEX_SIMD_ELEM(rA3, 0), rB0), _mm_mul_ps(D_GEX_SIMD_ELEM(rA3, 1), rB1)),
	        _mm_mul_ps(D_GEX_SIMD_ELEM(rA3, 2), rB2)), _mm_mul_ps(D_GEX_SIMD_ELEM(rA3, 3), rB3)));
#elif D_GEX_CPU == D_GEX_CPU_AVX
	__m256* pRA = (__m256*)pSrcA;
	__m128* pRB = (__m128*)pSrcB;
	__m256* pD = (__m256*)pDst;
	__m256 rA0A1 = pRA[0];
	__m256 rA2A3 = pRA[1];
	__m256 rB0 = _mm256_broadcast_ps(&pRB[0]);
	__m256 rB1 = _mm256_broadcast_ps(&pRB[1]);
	__m256 rB2 = _mm256_broadcast_ps(&pRB[2]);
	__m256 rB3 = _mm256_broadcast_ps(&pRB[3]);
	pD[0] = _mm256_add_ps(
		_mm256_add_ps(
			_mm256_mul_ps(_mm256_shuffle_ps(rA0A1, rA0A1, D_GEX_SIMD_ELEM_MASK(0)), rB0),
			_mm256_mul_ps(_mm256_shuffle_ps(rA0A1, rA0A1, D_GEX_SIMD_ELEM_MASK(1)), rB1)
		),
		_mm256_add_ps(
			_mm256_mul_ps(_mm256_shuffle_ps(rA0A1, rA0A1, D_GEX_SIMD_ELEM_MASK(2)), rB2),
			_mm256_mul_ps(_mm256_shuffle_ps(rA0A1, rA0A1, D_GEX_SIMD_ELEM_MASK(3)), rB3)
		)
	);
	pD[1] = _mm256_add_ps(
		_mm256_add_ps(
			_mm256_mul_ps(_mm256_shuffle_ps(rA2A3, rA2A3, D_GEX_SIMD_ELEM_MASK(0)), rB0),
			_mm256_mul_ps(_mm256_shuffle_ps(rA2A3, rA2A3, D_GEX_SIMD_ELEM_MASK(1)), rB1)
		),
		_mm256_add_ps(
			_mm256_mul_ps(_mm256_shuffle_ps(rA2A3, rA2A3, D_GEX_SIMD_ELEM_MASK(2)), rB2),
			_mm256_mul_ps(_mm256_shuffle_ps(rA2A3, rA2A3, D_GEX_SIMD_ELEM_MASK(3)), rB3)
		)
	);
#else
	pDst->mul(*pSrcA, *pSrcB);
#endif
}

void gexMtxAryMulWM(xt_wmtx* pDst, const cxMtx* pSrcA, const cxMtx* pSrcB, int n) {
	cxMtx tm;
	for (int i = 0; i < n; ++i) {
		gexMtxMul(&tm, &pSrcA[i], &pSrcB[i]);
		gexStoreWMtx(&pDst[i], tm);
	}
}

cxVec gexWMtxCalcPnt(const xt_wmtx& m, const cxVec& v) {
#if D_GEX_CPU == D_GEX_CPU_AVX
	__m256 c01 = *(__m256*)&m;
	__m128 c2 = ((__m128*)&m)[2];
	__m128 xv = D_GEX_SIMD_SHUF(_mm_loadh_pi(_mm_load_ss((float*)&v), (__m64 const*)(((float*)&v) + 1)), 0, 2, 3, 1);
	xv = _mm_castsi128_ps(_mm_insert_epi16(_mm_castps_si128(xv), 0x3F80, 7));
	__m256 yv = _mm256_set_m128(xv, xv);
	yv = _mm256_dp_ps(yv, c01, 0xF1);
	xv = _mm_dp_ps(xv, c2, 0xF1);
	cxVec res;
	res.x = _mm_cvtss_f32(_mm256_extractf128_ps(yv, 0));
	res.y = _mm_cvtss_f32(_mm256_extractf128_ps(yv, 1));
	res.z = _mm_cvtss_f32(xv);
	return res;
#else
	return nxMtx::wmtx_calc_pnt(m, v);
#endif
}

void gexDispListEntry(GEX_DISP_ENTRY& ent) {
	if (!GWK.mpDispList) return;
	int ptr = GWK.mDispListPtr;
	if (ptr < GWK.mDispListSize) {
		GWK.mpDispList[ptr] = ent;
		++ptr;
		GWK.mDispListPtr = ptr;
	}
}


void gexCalcShadowMtx();
void gexShadowCastStart(GEX_DISP_ENTRY& ent);
void gexShadowCastEnd(GEX_DISP_ENTRY& ent);


void gexBeginScene(GEX_CAM* pCam) {
	GWK.mpScnCam = pCam ? pCam : GWK.mpDefCam;
	GWK.mDispListPtr = 0;
	GWK.mObjTriCnt = 0;
	GWK.mObjTriDrawCnt = 0;
	GWK.mObjCullCnt = 0;
	GWK.mBatCullCnt = 0;
	GWK.mPolCullCnt = 0;
	GWK.mShadowCastCnt = 0;
	GWK.mShadowRecvCnt = 0;
}

static int dlcmp(const void* pA, const void* pB) {
	GEX_DISP_ENTRY* pDisp1 = (GEX_DISP_ENTRY*)pA;
	GEX_DISP_ENTRY* pDisp2 = (GEX_DISP_ENTRY*)pB;
	uint32_t k1 = pDisp1->mSortKey;
	uint32_t k2 = pDisp2->mSortKey;
	if (k1 > k2) return 1;
	if (k1 < k2) return -1;
	return 0;
}

static void gexBgDraw();

void gexEndScene() {
	// NOTE: if checking for no receivers here, then the same check must be done in gexBatDLFuncCast;
	if (GWK.mShadowCastCnt > 0 /*&& GWK.mShadowRecvCnt > 0*/) {
		GEX_DISP_ENTRY ent;
		::memset(&ent, 0, sizeof(ent));
		ent.mpFunc = gexShadowCastStart;
		ent.mSortKey = D_GEX_LYR_CAST_START << D_GEX_SORT_BITS;
		gexDispListEntry(ent);
		ent.mpFunc = gexShadowCastEnd;
		ent.mSortKey = D_GEX_LYR_CAST_END << D_GEX_SORT_BITS;
		gexDispListEntry(ent);
	}
	int n = GWK.mDispListPtr;
	if (GWK.mpDispList && n) {
		if (n > 1) {
			::qsort(GWK.mpDispList, n, sizeof(GEX_DISP_ENTRY), dlcmp);
		}
	}
	gexBgDraw();
	for (int i = 0; i < n; ++i) {
		GEX_DISP_ENTRY* pEnt = &GWK.mpDispList[i];
		if (pEnt->mpFunc) {
			pEnt->mpFunc(*pEnt);
		}
	}
	GWK.mpScnCam = nullptr;

	//::printf("culled: objs=%d, bats=%d; #tris drawn %d of %d (%.2f%%)\n", GWK.mObjCullCnt, GWK.mBatCullCnt, GWK.mObjTriDrawCnt, GWK.mObjTriCnt, (float)GWK.mObjTriDrawCnt/(GWK.mObjTriCnt/100.0f));
}

void gexSwap(uint32_t syncInterval) {
	if (GWK.mpSwapChain) {
		GWK.mpSwapChain->Present(syncInterval, 0);
	}
	++GWK.mFrameCount;
}


void gexShadowDir(const cxVec& dir) {
	GWK.mShadowDir = dir.get_normalized();
}

void gexShadowDensity(float dens) {
	GWK.mShadowDensity = nxCalc::saturate(dens);
}

void gexShadowViewSize(float vsize) {
	GWK.mShadowViewSize = vsize;
}

void gexShadowColor(const cxColor& clr) {
	GWK.mShadowColor = clr;
}

void gexShadowProjection(GEX_SHADOW_PROJ prj) {
	GWK.mShadowProj = prj;
}

void gexShadowFade(float start, float end) {
	GWK.mShadowFadeStart = start;
	GWK.mShadowFadeEnd = end;
}

void gexShadowSpecCtrl(int lightIdx, float valSelector) {
	GWK.mShadowLightIdx = lightIdx;
	GWK.mShadowSpecValSel = nxCalc::saturate(valSelector);
}


struct GEX_CAM {
	char* mpName;
	GEX_CAM* mpPrev;
	GEX_CAM* mpNext;
	StructuredBuffer<CAM_CTX> mCtxBuf;
	CAM_CTX mCtxWk;
	cxFrustum mFrustum;
	cxVec mPos;
	cxVec mTgt;
	cxVec mUp;
	cxVec mDir;
	float mFOVY;
	float mNear;
	float mFar;
	uint32_t mNameHash;
	char mNameBuf[32];

	void update(const cxVec& pos, const cxVec& tgt, const cxVec& up, float fovy, float znear, float zfar) {
		mPos = pos;
		mTgt = tgt;
		mUp = up;
		mFOVY = fovy;
		mNear = znear;
		mFar = zfar;
		mDir = (tgt - pos).get_normalized();

		cxMtx viewMtx;
		viewMtx.mk_view(pos, tgt, up);
		cxMtx projMtx;
		projMtx.mk_proj(fovy, GWK.mAspect, znear, zfar);
		cxMtx viewProjMtx;
		viewProjMtx = viewMtx * projMtx;
		gexStoreTMtx(&mCtxWk.view, viewMtx);
		gexStoreTMtx(&mCtxWk.proj, projMtx);
		gexStoreTMtx(&mCtxWk.viewProj, viewProjMtx);

		cxMtx im;
		im = viewMtx.get_inverted();
		mFrustum.init(im, mFOVY, GWK.mAspect, mNear, mFar);
		gexStoreTMtx(&mCtxWk.invView, im);
		gexStoreTMtx(&mCtxWk.invProj, projMtx.get_inverted());
		gexStoreTMtx(&mCtxWk.invViewProj, viewProjMtx.get_inverted());

		mCtxWk.viewPos = pos;

		mCtxBuf.copy_data(&mCtxWk);
	}
};

GEX_CAM* gexCamCreate(const char* pName) {
	GEX_CAM* pCam = gexTypeAlloc<GEX_CAM>(D_GEX_CAM_TAG);
	if (pCam) {
		pCam->mCtxBuf.alloc(1);
		pCam->update(cxVec(2.0f, 2.0f, 2.0f), cxVec(0.0f, 1.5f, 0.0f), nxVec::get_axis(exAxis::PLUS_Y), XD_DEG2RAD(40.0f), 0.001f, 1000.0f);

		if (pName) {
			size_t nameSize = ::strlen(pName) + 1;
			if (nameSize > sizeof(pCam->mNameBuf)) {
				pCam->mpName = nxCore::str_dup(pName);
			} else {
				::memcpy(pCam->mNameBuf, pName, nameSize);
				pCam->mpName = pCam->mNameBuf;
			}
		} else {
			::sprintf_s(pCam->mNameBuf, sizeof(pCam->mNameBuf), "<$cam@%p>", pCam);
			pCam->mpName = pCam->mNameBuf;
		}
		pCam->mNameHash = nxCore::str_hash32(pCam->mpName);

		gexLstLink(pCam, &GWK.mpCamLstHead, &GWK.mpCamLstTail);
		++GWK.mCamCount;
	}
	return pCam;
}

void gexCamDestroy(GEX_CAM* pCam) {
	if (pCam) {
		if (pCam->mpName != pCam->mNameBuf) {
			nxCore::mem_free(pCam->mpName);
		}
		pCam->mCtxBuf.release();
		gexLstUnlink(pCam, &GWK.mpCamLstHead, &GWK.mpCamLstTail);
		--GWK.mCamCount;
		nxCore::mem_free(pCam);
	}
}

void gexCamUpdate(GEX_CAM* pCam, const cxVec& pos, const cxVec& tgt, const cxVec& up, float fovy, float znear, float zfar) {
	if (!pCam) return;
	pCam->update(pos, tgt, up, fovy, znear, zfar);
}

float gexCalcViewDist(const GEX_CAM* pCam, const cxVec& pos) {
	return pCam ? pCam->mDir.dot(pos - pCam->mPos) : 0.0f;
}

float gexCalcViewDepth(const GEX_CAM* pCam, const cxVec& pos, float bias = 0.0f) {
	if (!pCam) return 0.0f;
	float dist = gexCalcViewDist(pCam, pos + pCam->mDir*bias);
	float z = nxCalc::div0(dist - pCam->mNear, pCam->mFar - pCam->mNear);
	return nxCalc::saturate(z);
}

uint32_t gexCalcSortKey(const GEX_CAM* pCam, const cxVec& pos, float bias, bool backToFront) {
	float z = gexCalcViewDepth(pCam, pos, bias);
	if (backToFront) z = 1.0f - z;
	int32_t iz = (int32_t)(z * D_GEX_SORT_MASK);
	return (uint32_t)(iz & D_GEX_SORT_MASK);
}

const char* gexCamName(const GEX_CAM* pCam) {
	const char* pName = nullptr;
	if (pCam) {
		pName = pCam->mpName;
	}
	return pName;
}

GEX_CAM* gexCamFind(const char* pName) {
	return gexLstFind(GWK.mpCamLstHead, pName);
}

bool gexSphereVisibility(const GEX_CAM* pCam, const cxSphere& sph) {
	if (!pCam) return false;
	if (pCam->mFrustum.cull(sph)) return false;
	return pCam->mFrustum.overlaps(sph);
}

bool gexBoxVisibility(const GEX_CAM* pCam, const cxAABB& box) {
	if (!pCam) return false;
	if (pCam->mFrustum.cull(box)) return false;
	return pCam->mFrustum.overlaps(box);
}


struct GEX_LIT {
	char* mpName;
	GEX_LIT* mpPrev;
	GEX_LIT* mpNext;
	StructuredBuffer<LIT_CTX> mCtxBuf;
	LIT_CTX mCtxWk;
	StructuredBuffer<SHL_CTX> mSHLBuf;
	SHL_CTX mSHLWk;
	StructuredBuffer<FOG_CTX> mFogBuf;
	FOG_CTX mFogWk;
	uint32_t mNameHash;
	char mNameBuf[32];
};

GEX_LIT* gexLitCreate(const char* pName) {
	GEX_LIT* pLit = gexTypeAlloc<GEX_LIT>(D_GEX_LIT_TAG);
	if (pLit) {
		pLit->mCtxBuf.alloc(1);
		cxColor defHemiSky = cxColor(0.35f, 0.45f, 0.65f);
		defHemiSky.scl_rgb(0.75f);
		cxColor defHemiGnd = cxColor(0.6f, 0.4f, 0.3f);
		defHemiGnd.scl_rgb(0.2f);
		gexHemi(pLit, defHemiSky, defHemiGnd);
		for (int i = 0; i < D_GEX_MAX_DYN_LIGHTS; ++i) {
			gexLightMode(pLit, i, GEX_LIGHT_MODE::NONE);
			gexLightPos(pLit, i, GWK.mDefLightPos);
			gexLightDir(pLit, i, GWK.mDefLightDir);
			gexLightColor(pLit, i, cxColor(0.5f));
		}
		gexLightMode(pLit, 0, GEX_LIGHT_MODE::DIST);
		pLit->mSHLBuf.alloc(1);
		pLit->mFogBuf.alloc(1);
		gexLitUpdate(pLit);

		if (pName) {
			size_t nameSize = ::strlen(pName) + 1;
			if (nameSize > sizeof(pLit->mNameBuf)) {
				pLit->mpName = nxCore::str_dup(pName);
			} else {
				::memcpy(pLit->mNameBuf, pName, nameSize);
				pLit->mpName = pLit->mNameBuf;
			}
		} else {
			::sprintf_s(pLit->mNameBuf, sizeof(pLit->mNameBuf), "<$lit@%p>", pLit);
			pLit->mpName = pLit->mNameBuf;
		}
		pLit->mNameHash = nxCore::str_hash32(pLit->mpName);

		gexLstLink(pLit, &GWK.mpLitLstHead, &GWK.mpLitLstTail);
		++GWK.mLitCount;
	}
	return pLit;
}

void gexLitDestroy(GEX_LIT* pLit) {
	if (pLit) {
		if (pLit->mpName != pLit->mNameBuf) {
			nxCore::mem_free(pLit->mpName);
		}
		pLit->mCtxBuf.release();
		pLit->mSHLBuf.release();
		pLit->mFogBuf.release();
		gexLstUnlink(pLit, &GWK.mpLitLstHead, &GWK.mpLitLstTail);
		--GWK.mLitCount;
		nxCore::mem_free(pLit);
	}
}

void gexLitUpdate(GEX_LIT* pLit) {
	if (!pLit) return;
	int maxIdx = -1;
	for (int i = 0; i < D_GEX_MAX_DYN_LIGHTS; ++i) {
		if (pLit->mCtxWk.mode[i] > 0) {
			maxIdx = i;
		}
	}
	pLit->mCtxWk.maxDynIdx = maxIdx;
	pLit->mCtxBuf.copy_data(&pLit->mCtxWk);
	if (pLit->mCtxWk.shMode != D_GEX_SHL_NONE) {
		pLit->mSHLBuf.copy_data(&pLit->mSHLWk);
	}
	pLit->mFogBuf.copy_data(&pLit->mFogWk);
}

void gexAmbient(GEX_LIT* pLit, const cxColor& clr) {
	gexHemi(pLit, clr, clr);
}

void gexHemi(GEX_LIT* pLit, const cxColor& skyClr, const cxColor& groundClr, const cxVec& upVec) {
	if (!pLit) return;
	pLit->mCtxWk.hemiUp = upVec.get_normalized();
	gexStoreRGB(&pLit->mCtxWk.hemiSky, skyClr);
	gexStoreRGB(&pLit->mCtxWk.hemiGround, groundClr);
}

bool gexCkLightIdx(int idx) {
	return (uint32_t)idx < (uint32_t)D_GEX_MAX_DYN_LIGHTS;
}

void gexLightMode(GEX_LIT* pLit, int idx, GEX_LIGHT_MODE mode) {
	if (!pLit || !gexCkLightIdx(idx)) return;
	pLit->mCtxWk.mode[idx] = (xt_int)mode;
	if (mode == GEX_LIGHT_MODE::DIST) {
		pLit->mCtxWk.atn[idx].set(-1.0f, 0.0f, 0.0f, 0.0f);
	}
}

void gexLightPos(GEX_LIT* pLit, int idx, const cxVec& pos) {
	if (!pLit || !gexCkLightIdx(idx)) return;
	pLit->mCtxWk.pos[idx] = pos;
}

void gexLightDir(GEX_LIT* pLit, int idx, const cxVec& dir) {
	if (!pLit || !gexCkLightIdx(idx)) return;
	pLit->mCtxWk.dir[idx] = dir.get_normalized();
}

void gexLightDirDegrees(GEX_LIT* pLit, int idx, float dx, float dy) {
	if (!pLit || !gexCkLightIdx(idx)) return;
	cxVec dir = nxQuat::from_degrees(dx, dy, 0.0f).apply(nxVec::get_axis(exAxis::MINUS_Z));
	gexLightDir(pLit, idx, dir);
}

void gexLightColor(GEX_LIT* pLit, int idx, const cxColor& clr, float diff, float spec) {
	if (!pLit || !gexCkLightIdx(idx)) return;
	gexStoreRGB(&pLit->mCtxWk.clr[idx], clr);
	pLit->mCtxWk.scl[idx].set(diff, spec);
}

void gexLightRange(GEX_LIT* pLit, int idx, float attnStart, float rangeEnd) {
	if (!pLit || !gexCkLightIdx(idx)) return;
	pLit->mCtxWk.atn[idx].x = attnStart;
	pLit->mCtxWk.atn[idx].y = attnStart < 0.0f ? 0.0f : nxCalc::rcp0(rangeEnd - attnStart);
	pLit->mCtxWk.rad[idx] = rangeEnd;
}

void gexLightCone(GEX_LIT* pLit, int idx, float angle, float range) {
	if (!pLit || !gexCkLightIdx(idx)) return;
	float cone = ::cosf(angle * 0.5f);
	pLit->mCtxWk.atn[idx].z = cone;
	pLit->mCtxWk.atn[idx].w = nxCalc::rcp0(::cosf((angle - range) * 0.5f) - cone);
}

void gexFog(GEX_LIT* pLit, const cxColor& clr, float start, float end, float curveP1, float curveP2) {
	if (!pLit) return;
	gexStoreRGBA(&pLit->mFogWk.color, clr);
	if (start < end) {
		pLit->mFogWk.curve.set(nxCalc::saturate(curveP1), nxCalc::saturate(curveP2));
		pLit->mFogWk.start = start;
		pLit->mFogWk.invRange = nxCalc::rcp0(end - start);
	} else {
		pLit->mFogWk.color.w = 0.0f;
	}
}

void gexSHLW(GEX_LIT* pLit, GEX_SHL_MODE mode, int order, float* pR, float* pG, float* pB, float* pWgtDiff, float* pWgtRefl) {
	if (!pLit) return;
	if (order <= 0) {
		mode = GEX_SHL_MODE::NONE;
	}
	pLit->mCtxWk.shMode = (int)mode;
	::memset(&pLit->mSHLWk, 0, sizeof(SHL_CTX));
	if (mode != GEX_SHL_MODE::NONE) {
		float wdiff[D_GEX_MAX_SH_ORDER];
		order = nxCalc::min(order, D_GEX_MAX_SH_ORDER);
		if (!pWgtDiff) {
			::memset(wdiff, 0, sizeof(wdiff));
			wdiff[0] = 1.0f;
			wdiff[1] = 1.0f / 1.5f;
			wdiff[2] = 1.0f / 4.0f;
			pWgtDiff = wdiff;
		}
		int nval = nxCalc::sq(order);
		for (int i = 0; i < nval; ++i) {
			pLit->mSHLWk.data[i].set(pR[i], pG[i], pB[i], 0.0f);
		}
		for (int i = 0; i < order; ++i) {
			pLit->mSHLWk.data[i].w = pWgtDiff[i];
			if (pWgtRefl) {
				pLit->mSHLWk.data[i + D_GEX_MAX_SH_ORDER].w = pWgtRefl[i];
			}
		}
	}
}

void gexSHL(GEX_LIT* pLit, GEX_SHL_MODE mode, int order, float* pR, float* pG, float* pB) {
	float wgt[D_GEX_MAX_SH_ORDER];
	for (int i = 0; i < D_GEX_MAX_SH_ORDER; ++i) {
		wgt[i] = 1.0f;
	}
	gexSHLW(pLit, mode, order, pR, pG, pB, wgt, wgt);
}

const char* gexLitName(const GEX_LIT* pLit) {
	const char* pName = nullptr;
	if (pLit) {
		pName = pLit->mpName;
	}
	return pName;
}

GEX_LIT* gexLitFind(const char* pName) {
	return gexLstFind(GWK.mpLitLstHead, pName);
}


struct GEX_TEX {
	GEX_TEX* mpPrev;
	GEX_TEX* mpNext;
	char* mpName;
	char* mpPath;
	uint32_t mNameHash;
	uint32_t mPathHash;
	ID3D11ShaderResourceView* mpSRV;
	int mWidth;
	int mHeight;
	int mLvlNum;
	int mFormat;
};

GEX_TEX* gexTexCreate(const sxTextureData& tex, bool compact) {
	if (!GWK.mpDev) return nullptr;
	bool hdrFlg = tex.is_hdr();
	bool halfFlg = true;
	bool byteFlg = compact && !hdrFlg;
	GEX_TEX* pTex = gexTypeAlloc<GEX_TEX>(D_GEX_TEX_TAG);
	sxTextureData::Pyramid* pPmd = tex.get_pyramid();
	if (pTex && pPmd) {
		UINT fmtBits = 0;
		bool has10bit = false;
		HRESULT hres = GWK.mpDev->CheckFormatSupport(DXGI_FORMAT_R10G10B10A2_UNORM, &fmtBits);
		if (SUCCEEDED(hres)) {
			has10bit = !!(fmtBits & D3D11_FORMAT_SUPPORT_TEXTURE2D);
		}
		bool alphaFlg = true;
		sxTextureData::PlaneInfo* pAlphaInfo = tex.find_plane_info("a");
		if (pAlphaInfo) {
			if (pAlphaInfo->is_const() && pAlphaInfo->mMinVal == 1.0f) {
				alphaFlg = false;
			}
		}
		pTex->mpName = nxCore::str_dup(tex.get_name());
		if (pTex->mpName) {
			pTex->mNameHash = nxCore::str_hash32(pTex->mpName);
		}
		pTex->mpPath = nxCore::str_dup(tex.get_base_path());
		if (pTex->mpPath) {
			pTex->mPathHash = nxCore::str_hash32(pTex->mpPath);
		}
		int w = pPmd->mBaseWidth;
		int h = pPmd->mBaseHeight;
		int nlvl = pPmd->mLvlNum;
		pTex->mWidth = w;
		pTex->mHeight = h;
		pTex->mLvlNum = nlvl;
		int pixelSize = byteFlg ? sizeof(uint32_t) : (halfFlg ? sizeof(xt_half4) : sizeof(xt_float4));

		DXGI_FORMAT tfmt = DXGI_FORMAT_R32G32B32A32_FLOAT;
		D3D11_SUBRESOURCE_DATA* pSub = gexTypeAlloc<D3D11_SUBRESOURCE_DATA>(XD_TMP_MEM_TAG, nlvl);
		for (int i = 0; i < nlvl; ++i) {
			int lvlW = 0;
			int lvlH = 0;
			pPmd->get_lvl_dims(i, &lvlW, &lvlH);
			cxColor* pClr = pPmd->get_lvl(i);
			pSub[i].SysMemPitch = lvlW*pixelSize;
			void* pLvlMem = (halfFlg || byteFlg) ? nxCore::mem_alloc(lvlW*lvlH*pixelSize, XD_TMP_MEM_TAG) : pClr;
			pSub[i].pSysMem = pLvlMem;
			if (byteFlg) {
				if (alphaFlg || !has10bit) {
					for (int j = 0; j < lvlW*lvlH; ++j) {
						uxVal32 iclr;
						for (int k = 0; k < 4; ++k) {
							iclr.b[k] = uint8_t(pClr[j].ch[k] * 255.0f);
						}
						((uint32_t*)pLvlMem)[j] = iclr.u;
					}
					tfmt = DXGI_FORMAT_R8G8B8A8_UNORM;
				} else {
					for (int j = 0; j < lvlW*lvlH; ++j) {
						uint32_t iclr = 3 << 30;
						for (int k = 0; k < 3; ++k) {
							iclr |= uint32_t(pClr[j].ch[k] * 0x3FF) << (k * 10);
						}
						((uint32_t*)pLvlMem)[j] = iclr;
					}
					tfmt = DXGI_FORMAT_R10G10B10A2_UNORM;
				}
			} else if (halfFlg) {
				for (int j = 0; j < lvlW*lvlH; ++j) {
					gexEncodeHalf4(&((xt_half4*)pLvlMem)[j], *((xt_float4*)&pClr[j]));
				}
				tfmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
			}
		}

		D3D11_TEXTURE2D_DESC dsc;
		::ZeroMemory(&dsc, sizeof(dsc));
		dsc.ArraySize = 1;
		dsc.Format = tfmt;
		dsc.Width = w;
		dsc.Height = h;
		dsc.MipLevels = nlvl;
		dsc.SampleDesc.Count = 1;
		dsc.SampleDesc.Quality = 0;
		dsc.Usage = D3D11_USAGE_DEFAULT;
		dsc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		pTex->mFormat = dsc.Format;

		ID3D11Texture2D* pTex2D = nullptr;
		hres = GWK.mpDev->CreateTexture2D(&dsc, pSub, &pTex2D);
		if (SUCCEEDED(hres)) {
			D3D11_SHADER_RESOURCE_VIEW_DESC dscSRV;
			::ZeroMemory(&dscSRV, sizeof(dscSRV));
			dscSRV.Format = dsc.Format;
			dscSRV.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
			dscSRV.Texture2D.MipLevels = nlvl;
			ID3D11ShaderResourceView* pSRV = nullptr;
			hres = GWK.mpDev->CreateShaderResourceView(pTex2D, &dscSRV, &pSRV);
			if (SUCCEEDED(hres)) {
				pTex->mpSRV = pSRV;
			}
			pTex2D->Release();
		}

		if (byteFlg || halfFlg) {
			for (int i = 0; i < nlvl; ++i) {
				nxCore::mem_free((void*)pSub[i].pSysMem);
			}
		}
		nxCore::mem_free(pSub);

		gexLstLink(pTex, &GWK.mpTexLstHead, &GWK.mpTexLstTail);
		++GWK.mTexCount;
	}
	nxCore::mem_free(pPmd);
	return pTex;
}

GEX_TEX* gexTexCreateConst(const cxColor& clr, const char* pName) {
	GEX_TEX* pTex = gexTypeAlloc<GEX_TEX>(D_GEX_TEX_TAG);
	if (pTex) {
		pTex->mpName = nxCore::str_dup(pName);
		if (pTex->mpName) {
			pTex->mNameHash = nxCore::str_hash32(pTex->mpName);
		}
		pTex->mWidth = 1;
		pTex->mHeight = 1;
		pTex->mLvlNum = 1;
		D3D11_SUBRESOURCE_DATA sub;
		sub.pSysMem = &clr;
		sub.SysMemPitch = sizeof(xt_float4);
		D3D11_TEXTURE2D_DESC dsc;
		::ZeroMemory(&dsc, sizeof(dsc));
		dsc.ArraySize = 1;
		dsc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		dsc.Width = pTex->mWidth;
		dsc.Height = pTex->mHeight;
		dsc.MipLevels = pTex->mLvlNum;
		dsc.SampleDesc.Count = 1;
		dsc.SampleDesc.Quality = 0;
		dsc.Usage = D3D11_USAGE_DEFAULT;
		dsc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		ID3D11Texture2D* pTex2D = nullptr;
		HRESULT hres = GWK.mpDev->CreateTexture2D(&dsc, &sub, &pTex2D);
		if (SUCCEEDED(hres)) {
			D3D11_SHADER_RESOURCE_VIEW_DESC dscSRV;
			::ZeroMemory(&dscSRV, sizeof(dscSRV));
			dscSRV.Format = dsc.Format;
			dscSRV.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
			dscSRV.Texture2D.MipLevels = 1;
			ID3D11ShaderResourceView* pSRV = nullptr;
			hres = GWK.mpDev->CreateShaderResourceView(pTex2D, &dscSRV, &pSRV);
			if (SUCCEEDED(hres)) {
				pTex->mpSRV = pSRV;
			}
			pTex2D->Release();
		}
		gexLstLink(pTex, &GWK.mpTexLstHead, &GWK.mpTexLstTail);
		++GWK.mTexCount;
	}
	return pTex;
}

void gexTexDestroy(GEX_TEX* pTex) {
	if (pTex) {
		if (pTex->mpSRV) {
			pTex->mpSRV->Release();
		}
		if (pTex->mpName) {
			nxCore::mem_free(pTex->mpName);
		}
		if (pTex->mpPath) {
			nxCore::mem_free(pTex->mpPath);
		}
		gexLstUnlink(pTex, &GWK.mpTexLstHead, &GWK.mpTexLstTail);
		--GWK.mTexCount;
		nxCore::mem_free(pTex);
	}
}

const char* gexTexName(const GEX_TEX* pTex) {
	const char* pName = nullptr;
	if (pTex) {
		pName = pTex->mpName;
	}
	return pName;
}

GEX_TEX* gexTexFind(const char* pName, const char* pPath) {
	return gexLstFind(GWK.mpTexLstHead, pName, pPath);
}


struct GEX_MTL {
	const char* mpName;
	const char* mpPath;
	GEX_OBJ* mpObj;
	GEX_POL* mpPol;
	StructuredBuffer<MTL_CTX> mCtxBuf;
	MTL_CTX mCtxWk;
	GEX_TEX* mpBaseTex;
	GEX_TEX* mpSpecTex;
	GEX_TEX* mpBumpTex;
	GEX_TEX* mpReflTex;
	GEX_SORT_MODE mSortMode;
	float mSortBiasAbs;
	float mSortBiasRel;
	bool mSortPrims;
	bool mDblSided;
	bool mHidden;
	bool mCastShadows;
	bool mReceiveShadows;
	bool mAlphaToCoverage;
	bool mBlend;
	GEX_UV_MODE mUVMode;
	GEX_TESS_MODE mTessMode;

	void set_defaults() {
		::memset(&mCtxWk, 0, sizeof(MTL_CTX));
		mpBaseTex = GWK.mpDefTexWhite;
		mpSpecTex = GWK.mpDefTexWhite;
		mpReflTex = GWK.mpDefTexBlack;
		mCtxWk.baseColor.fill(1.0f);
		mCtxWk.specColor.fill(1.0f);
		float defRough = 0.5f;
		mCtxWk.diffRoughness.fill(defRough);
		mCtxWk.diffRoughnessTexRate = 1.0f;
		mCtxWk.specRoughness.fill(defRough);
		mCtxWk.specRoughnessTexRate = 1.0f;
		mCtxWk.specRoughnessMin = 1.0e-6f;
		mCtxWk.IOR.fill(1.4f);
		mCtxWk.vclrGain.fill(1.0f);
		mCtxWk.shadowDensity = 1.0f;
		mCtxWk.shadowAlphaThreshold = 0.5f;
		mCtxWk.reflColor.fill(1.0f);
		mCtxWk.reflLvl = -1.0f;
		mCtxWk.shDiffClr.fill(1.0f);
		mCtxWk.shDiffDtl = 1.5f;
		mCtxWk.shReflClr.fill(1.0f);
		mCtxWk.shReflDtl = 8.0f;
		mCtxWk.shReflFrRate = 1.0f;
		mSortMode = GEX_SORT_MODE::NEAR_POS;
		mSortBiasAbs = 0.0f;
		mSortBiasRel = 0.0f;
		mSortPrims = false;
		mDblSided = false;
		mHidden = false;
		mCastShadows = false;
		mReceiveShadows = false;
		mAlphaToCoverage = false;
		mBlend = true;
		mUVMode = GEX_UV_MODE::WRAP;
		mTessMode = GEX_TESS_MODE::NONE;
	}

	void update() {
		mCtxBuf.copy_data(&mCtxWk);
	}

	cxVec calc_sort_pos(const GEX_CAM& cam, cxAABB& bbox) const {
		cxVec pos;
		switch (mSortMode) {
			case GEX_SORT_MODE::NEAR_POS:
			default:
				pos = bbox.closest_pnt(cam.mPos);
				break;
			case GEX_SORT_MODE::CENTER:
				pos = bbox.get_center();
				break;
			case GEX_SORT_MODE::FAR_POS:
				pos = bbox.closest_pnt(cam.mPos, false) - cam.mPos;
				pos = cam.mPos + pos.get_normalized() * ((pos.length() + bbox.get_size_vec().mag()) * 2.0f);
				pos = bbox.closest_pnt(pos);
				break;
		}
		return pos;
	}

	cxVec calc_sort_pos(const GEX_CAM& cam, const cxVec vtx[3]) const {
		cxVec pos;
		float dist[3];
		switch (mSortMode) {
			case GEX_SORT_MODE::NEAR_POS:
			default:
				for (int i = 0; i < 3; ++i) {
					dist[i] = gexCalcViewDist(&cam, vtx[0]);
				}
				if (dist[0] < dist[1] && dist[0] < dist[2]) {
					pos = dist[0];
				} else if (dist[1] < dist[0] && dist[1] < dist[2]) {
					pos = dist[1];
				} else {
					pos = dist[2];
				}
				break;
			case GEX_SORT_MODE::CENTER:
				pos = vtx[0];
				pos.add(vtx[1]);
				pos.add(vtx[2]);
				pos.scl(1.0f / 3.0f);
				break;
			case GEX_SORT_MODE::FAR_POS:
				for (int i = 0; i < 3; ++i) {
					dist[i] = gexCalcViewDist(&cam, vtx[0]);
				}
				if (dist[0] > dist[1] && dist[0] > dist[2]) {
					pos = dist[0];
				} else if (dist[1] > dist[0] && dist[1] > dist[2]) {
					pos = dist[1];
				} else{
					pos = dist[2];
				}
				break;
		}
		return pos;
	}

	float calc_sort_bias(cxAABB& bbox) const {
		return bbox.get_bounding_radius()*mSortBiasRel + mSortBiasAbs;
	}
};

struct GEX_SORT_PRIM {
	float key;
	int idx;
};

struct GEX_BAT {
	const char* mpName;
	GEX_OBJ* mpObj;
	cxAABB mGeoBBox;
	cxAABB mWorldBBox;
	cxSphere* mpSkinSphs;
	uint16_t* mpSkinIds;
	ID3D11Buffer* mpDynIB16;
	ID3D11Buffer* mpDynIB32;
	uint16_t* mpIdx16;
	uint32_t* mpIdx32;
	GEX_SORT_PRIM* mpSortWk;
	int mMtlId;
	int mMaxWghtNum;
	int mMinIdx;
	int mMaxIdx;
	int mIdxOrg;
	int mTriNum;
	int mSkinNodesNum;

	bool ck_tri_idx(int idx) const { return (uint32_t)idx < (uint32_t)mTriNum; }
	bool is_idx16() const { return (mMaxIdx - mMinIdx) < (1 << 16); }
	int get_tri_num() const { return mTriNum; }
	int get_idx_num() const { return get_tri_num() * 3; }

	GEX_MTL* get_mtl() const { return gexObjMaterial(mpObj, mMtlId); }

	void copy_skin_info(sxGeometryData::Group* pGrp) {
		if (!pGrp) return;
		int nn = pGrp->get_skin_nodes_num();
		if (nn > 0) {
			mSkinNodesNum = nn;
			cxSphere* pSph = pGrp->get_skin_spheres();
			if (pSph) {
				mpSkinSphs = gexTypeAlloc<cxSphere>(XD_FOURCC('G', 'B', 'S', 'P'), nn);
				::memcpy(mpSkinSphs, pSph, nn * sizeof(cxSphere));
			}
			uint16_t* pIdx = pGrp->get_skin_ids();
			if (pIdx) {
				mpSkinIds = gexTypeAlloc<uint16_t>(XD_FOURCC('G', 'B', 'S', 'I'), nn);
				::memcpy(mpSkinIds, pIdx, nn * sizeof(uint16_t));
			}
		}
	}

	void update_world_bbox(const cxMtx* pMtx);
	void sort(const GEX_CAM& cam);

	void cleanup() {
		if (mpSkinIds) {
			nxCore::mem_free(mpSkinIds);
			mpSkinIds = nullptr;
		}
		if (mpSkinSphs) {
			nxCore::mem_free(mpSkinSphs);
			mpSkinSphs = nullptr;
		}
		if (mpIdx16) {
			nxCore::mem_free(mpIdx16);
			mpIdx16 = nullptr;
		}
		if (mpIdx32) {
			nxCore::mem_free(mpIdx32);
			mpIdx32 = nullptr;
		}
		if (mpSortWk) {
			nxCore::mem_free(mpSortWk);
			mpSortWk = nullptr;
		}
		if (mpDynIB16) {
			mpDynIB16->Release();
			mpDynIB16 = nullptr;
		}
		if (mpDynIB32) {
			mpDynIB32->Release();
			mpDynIB32 = nullptr;
		}
	}
};

struct GEX_OBJ {
	const char* mpName;
	const char* mpPath;
	uint32_t mNameHash;
	uint32_t mPathHash;
	GEX_OBJ* mpPrev;
	GEX_OBJ* mpNext;
	cxAABB mGeoBBox;
	cxAABB mWorldBBox;
	sxStrList* mpStrLst;
	StructuredBuffer<OBJ_CTX> mObjCtx;
	StructuredBuffer<SKIN_CTX> mSkinCtx;
	StructuredBuffer<XFORM_CTX> mXformCtx;
	XFORM_CTX* mpXform;
	cxSphere* mpXformSpheres;
	cxMtx* mpRestIW;
	int32_t* mpSkinNameIds;
	GEX_MTL* mpMtl;
	GEX_BAT* mpBat;
	ID3D11Buffer* mpVB;
	ID3D11Buffer* mpIB16;
	ID3D11Buffer* mpIB32;
	cxVec* mpGeoPts;
	int mXformNum;
	int mVtxNum;
	int mMtlNum;
	int mBatNum;
	int mTriNum;
	int mIdx16Num;
	int mIdx32Num;
	int mMaxSkinWgtNum;

	bool ck_mtl_idx(int idx) const { return (uint32_t)idx < (uint32_t)mMtlNum; }

	void update_world_bbox(const cxMtx* pMtx) {
		if (!pMtx) return;
		if (mpXformSpheres) {
			int n = mXformNum;
			cxSphere* pSph = mpXformSpheres;
			cxAABB bbox = mGeoBBox;
			for (int i = 0; i < n; ++i) {
				cxSphere sph = pSph[i];
				cxVec spos = gexWMtxCalcPnt(*((&mpXform->mtx) + i), sph.get_center());
				cxVec rvec(sph.get_radius());
				cxAABB sbb(spos - rvec, spos + rvec);
				if (i == 0) {
					bbox = sbb;
				} else {
					bbox.merge(sbb);
				}
			}
			mWorldBBox = bbox;
		} else {
			mWorldBBox = mGeoBBox;
			mWorldBBox.transform(*pMtx);
		}
		if (mpBat) {
			for (int i = 0; i < mBatNum; ++i) {
				mpBat[i].update_world_bbox(pMtx);
			}
		}
	}
};

static int primcmp(const void* pA, const void* pB) {
	GEX_SORT_PRIM* pPrim1 = (GEX_SORT_PRIM*)pA;
	GEX_SORT_PRIM* pPrim2 = (GEX_SORT_PRIM*)pB;
	float k1 = pPrim1->key;
	float k2 = pPrim2->key;
	if (k1 > k2) return 1;
	if (k1 < k2) return -1;
	return 0;
}

void GEX_BAT::sort(const GEX_CAM& cam) {
	if (!mpSortWk) return;
	if (!(mpIdx16 || mpIdx32)) return;
	GEX_MTL* pMtl = get_mtl();
	if (!pMtl) return;
	int ntri = get_tri_num();
	for (int i = 0; i < ntri; ++i) {
		mpSortWk[i].idx = i;
	}
	int ipnt[3];
	cxVec vtx[3];
	xt_wmtx wm;
	if (mSkinNodesNum <= 1) {
		wm = mpSkinIds ? *((&mpObj->mpXform->mtx) + mpSkinIds[0]) : mpObj->mpXform->mtx;
	}
	for (int i = 0; i < ntri; ++i) {
		if (mpIdx16) {
			for (int j = 0; j < 3; ++j) {
				ipnt[j] = mpIdx16[(i * 3) + j];
			}
		} else {
			for (int j = 0; j < 3; ++j) {
				ipnt[j] = mpIdx32[(i * 3) + j];
			}
		}
		for (int j = 0; j < 3; ++j) {
			vtx[j] = mpObj->mpGeoPts[ipnt[j] + mMinIdx];
		}
		if (mSkinNodesNum <= 1) {
			for (int j = 0; j < 3; ++j) {
				vtx[j] = gexWMtxCalcPnt(wm, vtx[j]);
			}
		}
		cxVec spos = pMtl->calc_sort_pos(cam, vtx);
		mpSortWk[i].key = gexCalcViewDist(&cam, spos);
	}

	::qsort(mpSortWk, ntri, sizeof(GEX_SORT_PRIM), primcmp);

	D3D11_MAPPED_SUBRESOURCE map;
	if (mpDynIB16) {
		HRESULT hres = GWK.mpCtx->Map(mpDynIB16, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hres)) {
			uint16_t* pDst = (uint16_t*)map.pData;
			for (int i = 0; i < ntri; ++i) {
				int itri = mpSortWk[i].idx;
				::memcpy(pDst + (itri * 3), mpIdx16 + (itri * 3), sizeof(uint16_t) * 3);
			}
			GWK.mpCtx->Unmap(mpDynIB16, 0);
		}
	} else if (mpDynIB32) {
		HRESULT hres = GWK.mpCtx->Map(mpDynIB32, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hres)) {
			uint16_t* pDst = (uint16_t*)map.pData;
			for (int i = 0; i < ntri; ++i) {
				int itri = mpSortWk[i].idx;
				::memcpy(pDst + (itri * 3), mpIdx32 + (itri * 3), sizeof(uint32_t) * 3);
			}
			GWK.mpCtx->Unmap(mpDynIB32, 0);
		}
	}
}

void GEX_BAT::update_world_bbox(const cxMtx* pMtx) {
	if (mMaxWghtNum > 0) {
		if (mpSkinSphs && mpSkinIds && mpObj->mpXform) {
			cxAABB bbox = mGeoBBox;
			for (int i = 0; i < mSkinNodesNum; ++i) {
				cxSphere sph = mpSkinSphs[i];
				cxVec spos = gexWMtxCalcPnt(*((&mpObj->mpXform->mtx) + mpSkinIds[i]), sph.get_center());
				cxVec rvec(sph.get_radius());
				cxAABB sbb(spos - rvec, spos + rvec);
				if (i == 0) {
					bbox = sbb;
				} else {
					bbox.merge(sbb);
				}
			}
			mWorldBBox = bbox;
		} else {
			mWorldBBox = mpObj->mWorldBBox;
		}
	} else {
		if (pMtx) {
			mWorldBBox = mGeoBBox;
			mWorldBBox.transform(*pMtx);
		} else {
			mWorldBBox = mpObj->mWorldBBox;
		}
	}
}

GEX_OBJ* gexObjCreate(const sxGeometryData& geo, const char* pBatGrpPrefix, const char* pSortMtlsAttr) {
	HRESULT hres;
	GEX_OBJ* pObj = gexTypeAlloc<GEX_OBJ>(D_GEX_OBJ_TAG);
	pObj->mGeoBBox = geo.mBBox;
	pObj->mWorldBBox = pObj->mGeoBBox;
	sxStrList* pStr = geo.get_str_list();
	if (pStr) {
		size_t slSize = pStr->mSize;
		pObj->mpStrLst = reinterpret_cast<sxStrList*>(nxCore::mem_alloc(slSize, XD_FOURCC('G', 'O', 'S', 'L')));
		::memcpy(pObj->mpStrLst, pStr, slSize);
		pObj->mpName = pObj->mpStrLst->get_str(geo.mNameId);
		pObj->mpPath = pObj->mpStrLst->get_str(geo.mPathId);
		if (pObj->mpName) {
			pObj->mNameHash = nxCore::str_hash32(pObj->mpName);
		}
		if (pObj->mpPath) {
			pObj->mPathHash = nxCore::str_hash32(pObj->mpPath);
		}
	}

	int nvtx = geo.get_pnt_num();
	pObj->mVtxNum = nvtx;
	GEX_VTX* pVtx = gexTypeAlloc<GEX_VTX>(XD_TMP_MEM_TAG, nvtx);
	bool vtxAlphaFlg = false;
	bool hasTng = geo.has_pnt_attr("tangentu");
	cxVec* pTmpTng = hasTng ? nullptr : geo.calc_tangents();
	for (int i = 0; i < nvtx; ++i) {
		cxVec pos = geo.get_pnt(i);
		cxVec nrm = geo.get_pnt_normal(i);
		cxVec tng = pTmpTng ? pTmpTng[i] : geo.get_pnt_tangent(i);
		cxColor clr = geo.get_pnt_color(i);
		xt_texcoord tex = geo.get_pnt_texcoord(i);
		tex.flip_v();
		xt_texcoord tex2 = geo.get_pnt_texcoord2(i);
		tex2.flip_v();
		pVtx[i].mPos = pos;
		pVtx[i].mPID = i;
		pVtx[i].set_nrm_tng(nrm, tng);
		pVtx[i].set_clr(clr);
		pVtx[i].set_tex(tex, tex2);
		if (clr.a < 1.0f) {
			vtxAlphaFlg = true;
		}
	}
	if (pTmpTng) {
		nxCore::mem_free(pTmpTng);
		pTmpTng = nullptr;
	}
	struct {
		D3D11_BUFFER_DESC dsc;
		D3D11_SUBRESOURCE_DATA dat;
	} infoVB;
	::ZeroMemory(&infoVB, sizeof(infoVB));
	infoVB.dsc.ByteWidth = sizeof(GEX_VTX) * nvtx;
	infoVB.dsc.Usage = D3D11_USAGE_DEFAULT;
	infoVB.dsc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	infoVB.dat.pSysMem = pVtx;
	hres = GWK.mpDev->CreateBuffer(&infoVB.dsc, &infoVB.dat, &pObj->mpVB);
	nxCore::mem_free(pVtx);
	pVtx = nullptr;

	OBJ_CTX octx;
	pObj->mObjCtx.alloc(1);
	octx.vtxNum = nvtx;
	int nxform = 1;
	if (geo.has_skin()) {
		nxform = geo.get_skin_nodes_num();
		int maxSkinNum = geo.mMaxSkinWgtNum;
		if (maxSkinNum > 8) {
			nxCore::dbg_msg("too many weights in geo '%s'\n", geo.get_name());
			maxSkinNum = 8;
		}
		int skinNum = nvtx;
		if (maxSkinNum > 4) {
			skinNum *= 2;
		}
		pObj->mSkinCtx.alloc(skinNum);
		SKIN_CTX* pSkinData = (SKIN_CTX*)nxCore::mem_alloc(skinNum * sizeof(SKIN_CTX), XD_TMP_MEM_TAG);
		if (pSkinData) {
			for (int i = 0; i < nvtx; ++i) {
				pSkinData[i].jnt.fill(0);
				pSkinData[i].wgt.fill(0.0f);
				if (maxSkinNum > 4) {
					pSkinData[i + nvtx].jnt.fill(0);
					pSkinData[i + nvtx].wgt.fill(0.0f);
				}
				int nwgt = geo.get_pnt_wgt_num(i);
				if (nwgt > 8) {
					nxCore::dbg_msg("limiting #weights for pnt %d\n", i);
					nwgt = 8;
				}
				for (int j = 0; j < nwgt; ++j) {
					int skinJnt = geo.get_pnt_skin_jnt(i, j);
					float skinWgt = geo.get_pnt_skin_wgt(i, j);
					if (j < 4) {
						pSkinData[i].jnt.set_at(j, skinJnt);
						pSkinData[i].wgt.set_at(j, skinWgt);
					} else {
						pSkinData[i + nvtx].jnt.set_at(j, skinJnt);
						pSkinData[i + nvtx].wgt.set_at(j, skinWgt);
					}
				}
			}
			pObj->mSkinCtx.copy_data(pSkinData);
			nxCore::mem_free(pSkinData);
			pSkinData = nullptr;
		}
		cxSphere* pSph = geo.get_skin_sph_top();
		if (pSph) {
			pObj->mpXformSpheres = gexTypeCpy<cxSphere>(pSph, XD_FOURCC('G', 'O', 'S', 'P'), nxform);
		}
		pObj->mMaxSkinWgtNum = geo.mMaxSkinWgtNum;
		octx.xformMode = geo.mMaxSkinWgtNum;
	} else {
		octx.xformMode = 0;
	}
	pObj->mObjCtx.copy_data(&octx);
	pObj->mXformNum = nxform;
	pObj->mXformCtx.alloc(nxform);
	pObj->mpXform = gexTypeAlloc<XFORM_CTX>(XD_FOURCC('G', 'X', 'F', 'M'), nxform);
	for (int i = 0; i < nxform; ++i) {
		pObj->mpXform[i].mtx.identity();
	}
	pObj->mXformCtx.copy_data(pObj->mpXform);
	if (geo.has_skin_nodes()) {
		int32_t* pSkinNameIds = geo.get_skin_node_name_ids();
		if (pSkinNameIds) {
			pObj->mpSkinNameIds = gexTypeCpy<int32_t>(pSkinNameIds, XD_FOURCC('G', 'O', 'S', 'N'), nxform);
		}
	}

	int nmtl = geo.get_mtl_num();
	pObj->mMtlNum = nmtl > 0 ? nmtl : 1;
	pObj->mpMtl = gexTypeAlloc<GEX_MTL>(D_GEX_MTL_TAG, pObj->mMtlNum);
	if (nmtl > 0) {
		for (int i = 0; i < pObj->mMtlNum; ++i) {
			sxGeometryData::Group mtl = geo.get_mtl_grp(i);
			if (pObj->mpStrLst) {
				sxGeometryData::GrpInfo* pMtlInfo = mtl.get_info();
				if (pMtlInfo) {
					pObj->mpMtl[i].mpName = pObj->mpStrLst->get_str(pMtlInfo->mNameId);
					pObj->mpMtl[i].mpPath = pObj->mpStrLst->get_str(pMtlInfo->mPathId);
				}
			}
		}
	} else {
		pObj->mpMtl[0].mpName = "<default>";
	}
	for (int i = 0; i < pObj->mMtlNum; ++i) {
		pObj->mpMtl[i].mpObj = pObj;
		pObj->mpMtl[i].mCtxBuf.alloc(1);
		pObj->mpMtl[i].set_defaults();
		if (vtxAlphaFlg && nmtl == 0) {
			gexMtlAlpha(&pObj->mpMtl[i], true);
		}
		pObj->mpMtl[i].update();
	}

	int sortMtlsCnt = 0;
	if (pSortMtlsAttr) {
		int sortMtlsAttrId = geo.find_glb_attr(pSortMtlsAttr);
		if (sortMtlsAttrId >= 0) {
			char* pSortMtlBuf = nxCore::str_dup(geo.get_attr_val_s(sortMtlsAttrId, sxGeometryData::eAttrClass::GLOBAL, 0));
			if (pSortMtlBuf) {
				char* pSortMtlNames = pSortMtlBuf;
				char* pNextMtlName = nullptr;
				for (char* pMtlName = ::strtok_s(pSortMtlNames, ";", &pNextMtlName); pMtlName; pMtlName = ::strtok_s(nullptr, ";", &pNextMtlName)) {
					GEX_MTL* pMtl = gexFindMaterial(pObj, pMtlName);
					if (pMtl) {
						//nxCore::dbg_msg("%d: %s\n", sortMtlsCnt, pMtlName);
						pMtl->mSortPrims = true;
						++sortMtlsCnt;
					}
				}
				nxCore::mem_free(pSortMtlBuf);
				pSortMtlBuf = nullptr;
			}
			if (sortMtlsCnt > 0) {
				pObj->mpGeoPts = gexTypeAlloc<cxVec>(XD_FOURCC('G', 'p', 't', 's'), nvtx);
				if (pObj->mpGeoPts) {
					::memcpy(pObj->mpGeoPts, geo.get_pnt_top(), sizeof(cxVec) * nvtx);
				}
			}
		}
	}

	int nbat = pObj->mMtlNum;
	int batGrpCount = 0;
	int* pBatGrpIdx = nullptr;
	if (pBatGrpPrefix) {
		for (int i = 0; i < geo.get_pol_grp_num(); ++i) {
			sxGeometryData::Group polGrp = geo.get_pol_grp(i);
			if (polGrp.is_valid()) {
				const char* pPolGrpName = polGrp.get_name();
				if (nxCore::str_starts_with(pPolGrpName, pBatGrpPrefix)) {
					++batGrpCount;
				}
			}
		}
		if (batGrpCount > 0) {
			nbat = batGrpCount;
			pBatGrpIdx = reinterpret_cast<int*>(nxCore::mem_alloc(batGrpCount*sizeof(int), XD_TMP_MEM_TAG));
			batGrpCount = 0;
			for (int i = 0; i < geo.get_pol_grp_num(); ++i) {
				sxGeometryData::Group polGrp = geo.get_pol_grp(i);
				if (polGrp.is_valid()) {
					const char* pPolGrpName = polGrp.get_name();
					if (nxCore::str_starts_with(pPolGrpName, pBatGrpPrefix)) {
						pBatGrpIdx[batGrpCount] = i;
						++batGrpCount;
					}
				}
			}
		}
	}
	pObj->mBatNum = nbat;
	pObj->mpBat = gexTypeAlloc<GEX_BAT>(XD_FOURCC('G', 'B', 'A', 'T'), nbat);
	int ntri = 0;
	int nidx16 = 0;
	int nidx32 = 0;
	for (int i = 0; i < nbat; ++i) {
		GEX_BAT* pBat = &pObj->mpBat[i];
		int npol = 0;
		int nwgt = 0;
		pBat->mpObj = pObj;
		if (batGrpCount > 0) {
			int batGrpIdx = pBatGrpIdx[i];
			sxGeometryData::Group batGrp = geo.get_pol_grp(batGrpIdx);
			sxGeometryData::GrpInfo* pBatInfo = batGrp.get_info();
			pBat->mpName = pObj->mpStrLst->get_str(pBatInfo->mNameId);
			pBat->mGeoBBox = pBatInfo->mBBox;
			pBat->mWorldBBox = pBat->mGeoBBox;
			pBat->mMtlId = geo.get_pol(batGrp.get_idx(0)).get_mtl_id();
			if (pBat->mMtlId < 0) {
				pBat->mMtlId = 0;
			}
			npol = batGrp.get_idx_num();
			if (geo.has_skin()) {
				nwgt = batGrp.get_max_wgt_num();
				pBat->copy_skin_info(&batGrp);
			}
		} else {
			if (nmtl > 0) {
				sxGeometryData::Group mtlGrp = geo.get_mtl_grp(i);
				sxGeometryData::GrpInfo* pMtlInfo = mtlGrp.get_info();
				pBat->mpName = pObj->mpStrLst->get_str(pMtlInfo->mNameId);
				pBat->mGeoBBox = pMtlInfo->mBBox;
				pBat->mWorldBBox = pBat->mGeoBBox;
				pBat->mMtlId = i;
				npol = mtlGrp.get_idx_num();
				if (geo.has_skin()) {
					nwgt = mtlGrp.get_max_wgt_num();
					pBat->copy_skin_info(&mtlGrp);
				}
			} else {
				pBat->mGeoBBox = pObj->mGeoBBox;
				pBat->mWorldBBox = pBat->mGeoBBox;
				pBat->mpName = "<all>";
				pBat->mMtlId = 0;
				npol = geo.get_pol_num();
				if (geo.has_skin()) {
					nwgt = geo.mMaxSkinWgtNum;
				}
			}
		}
		pBat->mMaxWghtNum = nwgt;
		pBat->mTriNum = 0;
		int batVtxIdx = 0;
		for (int j = 0; j < npol; ++j) {
			int polIdx = 0;
			if (batGrpCount > 0) {
				int batGrpIdx = pBatGrpIdx[i];
				sxGeometryData::Group batGrp = geo.get_pol_grp(batGrpIdx);
				polIdx = batGrp.get_idx(j);
			} else {
				if (nmtl > 0) {
					polIdx = geo.get_mtl_grp(i).get_idx(j);
				} else {
					polIdx = j;
				}
			}
			sxGeometryData::Polygon pol = geo.get_pol(polIdx);
			bool polFlg = pol.is_tri();
			if (polFlg) {
				for (int k = 0; k < 3; ++k) {
					int vtxId = pol.get_vtx_pnt_id(k);
					if (batVtxIdx > 0) {
						pBat->mMinIdx = nxCalc::min(pBat->mMinIdx, vtxId);
						pBat->mMaxIdx = nxCalc::max(pBat->mMaxIdx, vtxId);
					} else {
						pBat->mMinIdx = vtxId;
						pBat->mMaxIdx = vtxId;
					}
					++batVtxIdx;
				}
				++pBat->mTriNum;
			}
		}
		if (pBat->is_idx16()) {
			pBat->mIdxOrg = nidx16;
			nidx16 += pBat->get_idx_num();
		} else {
			pBat->mIdxOrg = nidx32;
			nidx32 += pBat->get_idx_num();
		}
		ntri += pBat->mTriNum;
	}
	pObj->mTriNum = ntri;
	pObj->mIdx16Num = nidx16;
	pObj->mIdx32Num = nidx32;
	uint16_t* pIdx16 = nullptr;
	if (pObj->mIdx16Num > 0) {
		pIdx16 = gexTypeAlloc<uint16_t>(XD_TMP_MEM_TAG, pObj->mIdx16Num);
	}
	uint32_t* pIdx32 = nullptr;
	if (pObj->mIdx32Num > 0) {
		pIdx32 = gexTypeAlloc<uint32_t>(XD_TMP_MEM_TAG, pObj->mIdx32Num);
	}
	nidx16 = 0;
	nidx32 = 0;
	for (int i = 0; i < nbat; ++i) {
		GEX_BAT* pBat = &pObj->mpBat[i];
		int npol = 0;
		if (batGrpCount > 0) {
			int batGrpIdx = pBatGrpIdx[i];
			sxGeometryData::Group batGrp = geo.get_pol_grp(batGrpIdx);
			npol = batGrp.get_idx_num();
		} else {
			if (nmtl > 0) {
				npol = geo.get_mtl_grp(i).get_idx_num();
			} else {
				npol = geo.get_pol_num();
			}
		}
		if (pBat->get_mtl()->mSortPrims) {
			pBat->mpSortWk = gexTypeAlloc<GEX_SORT_PRIM>(XD_FOURCC('s', 'o', 'r', 't'), npol);
			if (pBat->is_idx16()) {
				pBat->mpIdx16 = gexTypeAlloc<uint16_t>(XD_FOURCC('i', 'x', '1', '6'), npol * 3);
			} else {
				pBat->mpIdx32 = gexTypeAlloc<uint32_t>(XD_FOURCC('i', 'x', '3', '2'), npol * 3);
			}
		}
		int i16 = 0;
		int i32 = 0;
		for (int j = 0; j < npol; ++j) {
			int polIdx = 0;
			if (batGrpCount > 0) {
				int batGrpIdx = pBatGrpIdx[i];
				sxGeometryData::Group batGrp = geo.get_pol_grp(batGrpIdx);
				polIdx = batGrp.get_idx(j);
			} else {
				if (nmtl > 0) {
					polIdx = geo.get_mtl_grp(i).get_idx(j);
				} else {
					polIdx = j;
				}
			}
			sxGeometryData::Polygon pol = geo.get_pol(polIdx);
			if (pol.is_tri()) {
				if (pBat->is_idx16()) {
					if (pIdx16) {
						for (int k = 0; k < 3; ++k) {
							pIdx16[nidx16 + k] = (uint16_t)(pol.get_vtx_pnt_id(k) - pBat->mMinIdx);
						}
					}
					if (pBat->mpIdx16) {
						::memcpy(pBat->mpIdx16 + i16, pIdx16 + nidx16, sizeof(uint16_t) * 3);
						i16 += 3;
					}
					nidx16 += 3;
				} else {
					if (pIdx32) {
						for (int k = 0; k < 3; ++k) {
							pIdx32[nidx32 + k] = (uint32_t)(pol.get_vtx_pnt_id(k) - pBat->mMinIdx);
						}
					}
					if (pBat->mpIdx32) {
						::memcpy(pBat->mpIdx32 + i32, pIdx32 + nidx32, sizeof(uint32_t) * 3);
						i32 += 3;
					}
					nidx32 += 3;
				}
			}
		}
		if (pBat->mpIdx16 || pBat->mpIdx32) {
			D3D11_BUFFER_DESC dscDynIB;
			::ZeroMemory(&dscDynIB, sizeof(dscDynIB));
			dscDynIB.Usage = D3D11_USAGE_DYNAMIC;
			dscDynIB.BindFlags = D3D11_BIND_INDEX_BUFFER;
			dscDynIB.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			if (pBat->mpIdx16) {
				dscDynIB.ByteWidth = pBat->get_tri_num() * 3 * sizeof(uint16_t);
				hres = GWK.mpDev->CreateBuffer(&dscDynIB, nullptr, &pBat->mpDynIB16);
			} else {
				dscDynIB.ByteWidth = pBat->get_tri_num() * 3 * sizeof(uint32_t);
				hres = GWK.mpDev->CreateBuffer(&dscDynIB, nullptr, &pBat->mpDynIB32);
			}
		}
	}
	if (pIdx16 || pIdx32) {
		struct {
			D3D11_BUFFER_DESC dsc;
			D3D11_SUBRESOURCE_DATA dat;
		} infoIB;
		::ZeroMemory(&infoIB, sizeof(infoIB));
		infoIB.dsc.Usage = D3D11_USAGE_DEFAULT;
		infoIB.dsc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		if (pIdx16) {
			infoIB.dsc.ByteWidth = pObj->mIdx16Num * sizeof(uint16_t);
			infoIB.dat.pSysMem = pIdx16;
			hres = GWK.mpDev->CreateBuffer(&infoIB.dsc, &infoIB.dat, &pObj->mpIB16);
		}
		if (pIdx32) {
			infoIB.dsc.ByteWidth = pObj->mIdx32Num * sizeof(uint32_t);
			infoIB.dat.pSysMem = pIdx32;
			hres = GWK.mpDev->CreateBuffer(&infoIB.dsc, &infoIB.dat, &pObj->mpIB32);
		}
	}
	if (pIdx16) {
		nxCore::mem_free(pIdx16);
		pIdx16 = nullptr;
	}
	if (pIdx32) {
		nxCore::mem_free(pIdx32);
		pIdx32 = nullptr;
	}
	if (pBatGrpIdx) {
		nxCore::mem_free(pBatGrpIdx);
		pBatGrpIdx = nullptr;
	}

	gexLstLink(pObj, &GWK.mpObjLstHead, &GWK.mpObjLstTail);
	++GWK.mObjCount;

	return pObj;
}

void gexObjDestroy(GEX_OBJ* pObj) {
	if (!pObj) return;
	if (pObj->mpIB32) {
		pObj->mpIB32->Release();
	}
	if (pObj->mpIB16) {
		pObj->mpIB16->Release();
	}
	if (pObj->mpRestIW) {
		nxCore::mem_free(pObj->mpRestIW);
	}
	if (pObj->mpBat) {
		for (int i = 0; i < pObj->mBatNum; ++i) {
			pObj->mpBat[i].cleanup();
		}
		nxCore::mem_free(pObj->mpBat);
	}
	if (pObj->mpGeoPts) {
		nxCore::mem_free(pObj->mpGeoPts);
	}
	if (pObj->mpMtl) {
		for (int i = 0; i < pObj->mMtlNum; ++i) {
			pObj->mpMtl[i].mCtxBuf.release();
		}
		nxCore::mem_free(pObj->mpMtl);
	}
	if (pObj->mpSkinNameIds) {
		nxCore::mem_free(pObj->mpSkinNameIds);
	}
	if (pObj->mpXform) {
		nxCore::mem_free(pObj->mpXform);
	}
	pObj->mXformCtx.release();
	if (pObj->mMaxSkinWgtNum > 0) {
		if (pObj->mpXformSpheres) {
			nxCore::mem_free(pObj->mpXformSpheres);
		}
		pObj->mSkinCtx.release();
	}
	pObj->mObjCtx.release();
	if (pObj->mpVB) {
		pObj->mpVB->Release();
	}
	if (pObj->mpStrLst) {
		nxCore::mem_free(pObj->mpStrLst);
	}
	gexLstUnlink(pObj, &GWK.mpObjLstHead, &GWK.mpObjLstTail);
	--GWK.mObjCount;
	nxCore::mem_free(pObj);
}

void gexObjSkinInit(GEX_OBJ* pObj, const cxMtx* pRestIW) {
	if (!pObj || !pRestIW) return;
	if (pObj->mMaxSkinWgtNum > 0) {
		if (pObj->mpRestIW) {
			nxCore::mem_free(pObj->mpRestIW);
			pObj->mpRestIW = nullptr;
		}
		int n = pObj->mXformNum;
		pObj->mpRestIW = gexTypeCpy<cxMtx>(pRestIW, XD_FOURCC('G', 'O', 'I', 'W'), n);
	}
}

void gexObjTransform(GEX_OBJ* pObj, const cxMtx* pMtx) {
	if (!pObj || !pMtx) return;
	if (pObj->mMaxSkinWgtNum > 0) {
		if (pObj->mpRestIW) {
			gexMtxAryMulWM(&pObj->mpXform->mtx, pObj->mpRestIW, pMtx, pObj->mXformNum);
		}
	} else {
		gexStoreWMtx(&pObj->mpXform[0].mtx, pMtx[0]);
	}
	pObj->update_world_bbox(pMtx);
	pObj->mXformCtx.copy_data(pObj->mpXform);
}

void gexBatDraw(GEX_BAT* pBat, GEX_LIT* pLit) {
	if (!pBat) return;
	ID3D11DeviceContext* pCtx = GWK.mpCtx;
	if (!pCtx) return;
	GEX_CAM* pCam = GWK.mpScnCam;
	if (!pCam) return;
	GEX_OBJ* pObj = pBat->mpObj;
	if (!pObj) return;
	GEX_MTL* pMtl = pBat->get_mtl();
	if (!pMtl) return;
	if (!pLit) pLit = GWK.mpDefLit;

	if (pMtl->mDblSided) {
		GWK.mpCtx->RSSetState(GWK.mpDblSidedRS);
	} else {
		GWK.mpCtx->RSSetState(GWK.mpOneSidedRS);
	}

	bool tessFlg = false;
	if (pMtl->mTessMode != GEX_TESS_MODE::NONE) {
		if (pMtl->mCtxWk.tessFactor > 0.0f) {
			tessFlg = true;
		}
	}

	pCtx->VSSetShader(GWK.mpObjVS, nullptr, 0);
	pCtx->PSSetShader(GWK.mpMtlPS, nullptr, 0);
	pCtx->GSSetShader(nullptr, nullptr, 0);
	if (tessFlg) {
		pCtx->HSSetShader(GWK.mpPNTriHS, nullptr, 0);
		pCtx->DSSetShader(GWK.mpPNTriDS, nullptr, 0);
	} else {
		pCtx->HSSetShader(nullptr, nullptr, 0);
		pCtx->DSSetShader(nullptr, nullptr, 0);
	}

	pCtx->VSSetShaderResources(VSCTX_g_cam, 1, &pCam->mCtxBuf.mpSRV);
	pCtx->VSSetShaderResources(VSCTX_g_obj, 1, &pObj->mObjCtx.mpSRV);
	pCtx->VSSetShaderResources(VSCTX_g_xform, 1, &pObj->mXformCtx.mpSRV);
	if (pBat->mMaxWghtNum > 0) {
		pCtx->VSSetShaderResources(VSCTX_g_skin, 1, &pObj->mSkinCtx.mpSRV);
	}

	pCtx->PSSetShaderResources(PSCTX_g_cam, 1, &pCam->mCtxBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_mtl, 1, &pMtl->mCtxBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_lit, 1, &pLit->mCtxBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_shl, 1, &pLit->mSHLBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_fog, 1, &pLit->mFogBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_glb, 1, &GWK.mGlbBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_sdw, 1, &GWK.mSdwBuf.mpSRV);

	if (pMtl->mUVMode == GEX_UV_MODE::WRAP) {
		pCtx->PSSetSamplers(0, 1, &GWK.mpSmpStateLinWrap);
	} else {
		pCtx->PSSetSamplers(0, 1, &GWK.mpSmpStateLinClamp);
	}
	pCtx->PSSetSamplers(1, 1, &GWK.mpSmpStatePnt);
	ID3D11ShaderResourceView* pTexRV;
	pTexRV = pMtl->mpBaseTex ? pMtl->mpBaseTex->mpSRV : nullptr;
	pCtx->PSSetShaderResources(PSTEX_g_texBase, 1, &pTexRV);
	pTexRV = pMtl->mpSpecTex ? pMtl->mpSpecTex->mpSRV : nullptr;
	pCtx->PSSetShaderResources(PSTEX_g_texSpec, 1, &pTexRV);
	pTexRV = pMtl->mpBumpTex ? pMtl->mpBumpTex->mpSRV : nullptr;
	pCtx->PSSetShaderResources(PSTEX_g_texBump, 1, &pTexRV);
	pTexRV = pMtl->mpReflTex ? pMtl->mpReflTex->mpSRV : nullptr;
	pCtx->PSSetShaderResources(PSTEX_g_texRefl, 1, &pTexRV);
	pCtx->PSSetShaderResources(PSTEX_g_texSMap, 1, &GWK.mpSMRrsrcView);

	if (tessFlg) {
		pCtx->HSSetShaderResources(HSCTX_g_cam, 1, &pCam->mCtxBuf.mpSRV);
		pCtx->HSSetShaderResources(HSCTX_g_mtl, 1, &pMtl->mCtxBuf.mpSRV);
		pCtx->DSSetShaderResources(DSCTX_g_cam, 1, &pCam->mCtxBuf.mpSRV);
	}

	pCtx->IASetInputLayout(GWK.mpObjVtxLayout);
	UINT stride = sizeof(GEX_VTX);
	UINT offs = 0;
	pCtx->IASetVertexBuffers(0, 1, &pObj->mpVB, &stride, &offs);
	int idxOrg = pBat->mIdxOrg;
	if (pBat->is_idx16()) {
		if (pBat->mpDynIB16) {
			pCtx->IASetIndexBuffer(pBat->mpDynIB16, DXGI_FORMAT_R16_UINT, 0);
			idxOrg = 0;
		} else {
			pCtx->IASetIndexBuffer(pObj->mpIB16, DXGI_FORMAT_R16_UINT, 0);
		}
	} else {
		if (pBat->mpDynIB32) {
			pCtx->IASetIndexBuffer(pBat->mpDynIB32, DXGI_FORMAT_R32_UINT, 0);
			idxOrg = 0;
		} else {
			pCtx->IASetIndexBuffer(pObj->mpIB32, DXGI_FORMAT_R32_UINT, 0);
		}
	}
	if (tessFlg) {
		pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
	} else {
		pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
	pCtx->DrawIndexed(pBat->mTriNum*3, idxOrg, pBat->mMinIdx);
	GWK.mObjTriDrawCnt += pBat->mTriNum;
}

void gexBatDLFuncOpaq(GEX_DISP_ENTRY& ent) {
	if (!GWK.mpCtx) return;
	GWK.mpCtx->OMSetBlendState(GWK.mpBlendOpaq, nullptr, 0xFFFFFFFF);
	GEX_BAT* pBat = (GEX_BAT*)ent.mpData;
	gexBatDraw(pBat, ent.mpLit);
}

void gexBatDLFuncSemi(GEX_DISP_ENTRY& ent) {
	if (!GWK.mpCtx) return;
	GEX_BAT* pBat = (GEX_BAT*)ent.mpData;
	GEX_MTL* pMtl = pBat->get_mtl();
	if (pMtl && pMtl->mAlphaToCoverage) {
		if (pMtl->mBlend) {
			GWK.mpCtx->OMSetBlendState(GWK.mpBlendSemiBlendCov, nullptr, 0xFFFFFFFF);
		} else {
			GWK.mpCtx->OMSetBlendState(GWK.mpBlendSemiCov, nullptr, 0xFFFFFFFF);
		}
	} else {
		GWK.mpCtx->OMSetBlendState(GWK.mpBlendSemi, nullptr, 0xFFFFFFFF);
	}
	gexBatDraw(pBat, ent.mpLit);
}

void gexBatDLFuncCast(GEX_DISP_ENTRY& ent) {
	ID3D11DeviceContext* pCtx = GWK.mpCtx;
	if (!pCtx) return;
	GEX_BAT* pBat = (GEX_BAT*)ent.mpData;
	if (!pBat) return;
	GEX_OBJ* pObj = pBat->mpObj;
	if (!pObj) return;
	GEX_MTL* pMtl = pBat->get_mtl();
	if (!pMtl) return;

	pCtx->OMSetBlendState(GWK.mpBlendOpaq, nullptr, 0xFFFFFFFF);

	if (pMtl->mDblSided) {
		pCtx->RSSetState(GWK.mpDblSidedRS);
	} else {
		pCtx->RSSetState(GWK.mpOneSidedRS);
	}

	pCtx->VSSetShader(GWK.mpSdwVS, nullptr, 0);
	pCtx->PSSetShader(GWK.mpSdwPS, nullptr, 0);
	pCtx->GSSetShader(nullptr, nullptr, 0);
	pCtx->HSSetShader(nullptr, nullptr, 0);
	pCtx->DSSetShader(nullptr, nullptr, 0);

	pCtx->VSSetShaderResources(VSCTX_g_obj, 1, &pObj->mObjCtx.mpSRV);
	pCtx->VSSetShaderResources(VSCTX_g_xform, 1, &pObj->mXformCtx.mpSRV);
	if (pBat->mMaxWghtNum > 0) {
		pCtx->VSSetShaderResources(VSCTX_g_skin, 1, &pObj->mSkinCtx.mpSRV);
	}
	pCtx->VSSetShaderResources(VSCTX_g_sdw, 1, &GWK.mSdwBuf.mpSRV);

	pCtx->PSSetShaderResources(PSCTX_g_mtl, 1, &pMtl->mCtxBuf.mpSRV);

	if (pMtl->mUVMode == GEX_UV_MODE::WRAP) {
		pCtx->PSSetSamplers(0, 1, &GWK.mpSmpStateLinWrap);
	} else {
		pCtx->PSSetSamplers(0, 1, &GWK.mpSmpStateLinClamp);
	}
	ID3D11ShaderResourceView* pTexRV;
	pTexRV = pMtl->mpBaseTex ? pMtl->mpBaseTex->mpSRV : nullptr;
	pCtx->PSSetShaderResources(PSTEX_g_texBase, 1, &pTexRV);

	pCtx->IASetInputLayout(GWK.mpObjVtxLayout);
	UINT stride = sizeof(GEX_VTX);
	UINT offs = 0;
	pCtx->IASetVertexBuffers(0, 1, &pObj->mpVB, &stride, &offs);
	if (pBat->is_idx16()) {
		pCtx->IASetIndexBuffer(pObj->mpIB16, DXGI_FORMAT_R16_UINT, 0);
	} else {
		pCtx->IASetIndexBuffer(pObj->mpIB32, DXGI_FORMAT_R32_UINT, 0);
	}
	pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCtx->DrawIndexed(pBat->mTriNum * 3, pBat->mIdxOrg, pBat->mMinIdx);
}

bool gexCullBBox(const cxFrustum& fst, const cxAABB& box) {
	bool cullFlg = fst.cull(box);
	if (!cullFlg && GWK.mPreciseCulling) {
		cullFlg = !fst.overlaps(box);
	}
	return cullFlg;
}

void gexObjDisp(GEX_OBJ* pObj, GEX_LIT* pLit) {
	if (!pObj) return;
	if (!pLit) pLit = GWK.mpDefLit;
	GEX_CAM* pCam = GWK.mpScnCam;
	if (!pCam) return;

	GWK.mObjTriCnt += pObj->mTriNum;

	GEX_DISP_ENTRY ent;
	::memset(&ent, 0, sizeof(ent));

	int nbat = pObj->mBatNum;

	for (int i = 0; i < nbat; ++i) {
		GEX_BAT* pBat = &pObj->mpBat[i];
		GEX_MTL* pMtl = pBat->get_mtl();
		if (pMtl) {
			if (pMtl->mCastShadows) {
				if (GWK.mShadowCastCnt > 0) {
					GWK.mShadowCastersBBox.merge(pBat->mWorldBBox);
				} else {
					GWK.mShadowCastersBBox = pBat->mWorldBBox;
				}
				ent.mpFunc = gexBatDLFuncCast;
				ent.mpData = pBat;
				ent.mpLit = pLit;
				ent.mSortKey = D_GEX_LYR_CAST << D_GEX_SORT_BITS;
				gexDispListEntry(ent);
				++GWK.mShadowCastCnt;
			}
		}
	}

	bool cullFlg = gexCullBBox(pCam->mFrustum, pObj->mWorldBBox);
	if (cullFlg) {
		++GWK.mObjCullCnt;
		return;
	}

	for (int i = 0; i < nbat; ++i) {
		GEX_BAT* pBat = &pObj->mpBat[i];
		GEX_MTL* pMtl = pBat->get_mtl();
		if (pMtl && !pMtl->mHidden) {
			cullFlg = gexCullBBox(pCam->mFrustum, pBat->mWorldBBox);
			if (!cullFlg) {
				if (pMtl->mReceiveShadows) {
					if (GWK.mShadowRecvCnt > 0) {
						GWK.mShadowReceiversBBox.merge(pBat->mWorldBBox);
					} else {
						GWK.mShadowReceiversBBox = pBat->mWorldBBox;
					}
					++GWK.mShadowRecvCnt;
				}
				bool alphaFlg = !!pMtl->mCtxWk.enableAlpha;
				if (alphaFlg) {
					ent.mpFunc = gexBatDLFuncSemi;
				} else {
					ent.mpFunc = gexBatDLFuncOpaq;
				}
				pBat->sort(*pCam);
				cxVec sortPos = pMtl->calc_sort_pos(*pCam, pBat->mWorldBBox);
				float sortBias = pMtl->calc_sort_bias(pBat->mWorldBBox);
				uint32_t layer = alphaFlg ? D_GEX_LYR_SEMI : D_GEX_LYR_OPAQ;
				uint32_t zkey = gexCalcSortKey(pCam, sortPos, sortBias, alphaFlg);
				zkey |= layer << D_GEX_SORT_BITS;
				ent.mSortKey = zkey;
				ent.mpData = pBat;
				ent.mpLit = pLit;
				gexDispListEntry(ent);
			} else {
				++GWK.mBatCullCnt;
			}
		}
	}
}

int gexObjMtlNum(const GEX_OBJ* pObj) {
	int n = 0;
	if (pObj) {
		n = pObj->mMtlNum;
	}
	return n;
}

cxAABB gexObjBBox(const GEX_OBJ* pObj) {
	cxAABB box;
	box.init();
	if (pObj) {
		box = pObj->mWorldBBox;
	}
	return box;
}

const char* gexObjName(const GEX_OBJ* pObj) {
	const char* pName = nullptr;
	if (pObj) {
		pName = pObj->mpName;
	}
	return pName;
}

GEX_OBJ* gexObjFind(const char* pName, const char* pPath) {
	return gexLstFind(GWK.mpObjLstHead, pName, pPath);
}

GEX_MTL* gexObjMaterial(const GEX_OBJ* pObj, int idx) {
	GEX_MTL* pMtl = nullptr;
	if (pObj && pObj->ck_mtl_idx(idx)) {
		pMtl = &pObj->mpMtl[idx];
	}
	return pMtl;
}

GEX_MTL* gexFindMaterial(const GEX_OBJ* pObj, const char* pName) {
	GEX_MTL* pMtl = nullptr;
	if (pObj && pName) {
		for (int i = 0; i < pObj->mMtlNum; ++i) {
			GEX_MTL* pCkMtl = &pObj->mpMtl[i];
			if (nxCore::str_eq(pCkMtl->mpName, pName)) {
				pMtl = pCkMtl;
				break;
			}
		}
	}
	return pMtl;
}

void gexUpdateMaterials(GEX_OBJ* pObj) {
	if (pObj) {
		for (int i = 0; i < pObj->mMtlNum; ++i) {
			gexMtlUpdate(gexObjMaterial(pObj, i));
		}
	}
}


struct GEX_POL {
	char* mpName;
	GEX_POL* mpPrev;
	GEX_POL* mpNext;
	cxAABB mGeoBBox;
	cxAABB mWorldBBox;
	cxMtx mWorldMtx;
	StructuredBuffer<OBJ_CTX> mObjCtx;
	StructuredBuffer<XFORM_CTX> mXformCtx;
	ID3D11Buffer* mpVB;
	GEX_VTX* mpMappedVB;
	GEX_MTL* mpMtl;
	int mVtxAllocNum;
	int mVtxUseNum;
	GEX_POL_TYPE mType;
	uint32_t mNameHash;
	char mNameBuf[32];

	void update_xform_ctx() {
		XFORM_CTX xctx;
		gexStoreWMtx(&xctx.mtx, mWorldMtx);
		mXformCtx.copy_data(&xctx);
	}
};

GEX_POL* gexPolCreate(int maxVtxNum, const char* pName) {
	GEX_POL* pPol = gexTypeAlloc<GEX_POL>(D_GEX_POL_TAG);
	if (pPol) {
		OBJ_CTX octx;
		pPol->mObjCtx.alloc(1);
		octx.xformMode = 0;
		pPol->mObjCtx.copy_data(&octx);
		pPol->mXformCtx.alloc(1);
		pPol->mWorldMtx.identity();
		pPol->update_xform_ctx();

		pPol->mpMtl = gexTypeAlloc<GEX_MTL>(D_GEX_MTL_TAG);
		if (pPol->mpMtl) {
			pPol->mpMtl->mpPol = pPol;
			pPol->mpMtl->mpName = "<pol_mtl>";
			pPol->mpMtl->mCtxBuf.alloc(1);
			pPol->mpMtl->set_defaults();
			pPol->mpMtl->update();
		}

		D3D11_BUFFER_DESC dsc;
		::ZeroMemory(&dsc, sizeof(dsc));
		dsc.ByteWidth = maxVtxNum * sizeof(GEX_VTX);
		dsc.Usage = D3D11_USAGE_DYNAMIC;
		dsc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		dsc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		HRESULT hres = GWK.mpDev->CreateBuffer(&dsc, nullptr, &pPol->mpVB);
		if (SUCCEEDED(hres)) {
			pPol->mVtxAllocNum = maxVtxNum;
		}

		if (pName) {
			size_t nameSize = ::strlen(pName) + 1;
			if (nameSize > sizeof(pPol->mNameBuf)) {
				pPol->mpName = nxCore::str_dup(pName);
			} else {
				::memcpy(pPol->mNameBuf, pName, nameSize);
				pPol->mpName = pPol->mNameBuf;
			}
		} else {
			::sprintf_s(pPol->mNameBuf, sizeof(pPol->mNameBuf), "<$pol@%p>", pPol);
			pPol->mpName = pPol->mNameBuf;
		}
		pPol->mNameHash = nxCore::str_hash32(pPol->mpName);

		gexLstLink(pPol, &GWK.mpPolLstHead, &GWK.mpPolLstTail);
		++GWK.mPolCount;
	}
	return pPol;
}

void gexPolDestroy(GEX_POL* pPol) {
	if (!pPol) return;
	if (pPol->mpName != pPol->mNameBuf) {
		nxCore::mem_free(pPol->mpName);
	}
	gexPolEditEnd(pPol);
	if (pPol->mpMtl) {
		pPol->mpMtl->mCtxBuf.release();
		nxCore::mem_free(pPol->mpMtl);
	}
	pPol->mXformCtx.release();
	pPol->mObjCtx.release();
	if (pPol->mpVB) {
		pPol->mpVB->Release();
	}
	gexLstUnlink(pPol, &GWK.mpPolLstHead, &GWK.mpPolLstTail);
	--GWK.mPolCount;
	nxCore::mem_free(pPol);
}

void gexPolEditBegin(GEX_POL* pPol) {
	if (!pPol) return;
	pPol->mpMappedVB = nullptr;
	pPol->mVtxUseNum = 0;
	if (pPol->mpVB) {
		D3D11_MAPPED_SUBRESOURCE map;
		HRESULT hres = GWK.mpCtx->Map(pPol->mpVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hres)) {
			pPol->mpMappedVB = (GEX_VTX*)map.pData;
		}
	}
}

void gexPolEditEnd(GEX_POL* pPol) {
	if (!pPol) return;
	if (!pPol->mpMappedVB) return;
	GWK.mpCtx->Unmap(pPol->mpVB, 0);
	pPol->mpMappedVB = nullptr;
	pPol->mWorldBBox = pPol->mGeoBBox;
}

void gexPolAddVertex(GEX_POL* pPol, const cxVec& pos, const cxVec& nrm, const cxVec& tng, const cxColor& clr, const xt_texcoord& uv, const xt_texcoord& uv2) {
	if (!pPol) return;
	if (!pPol->mpMappedVB) return;
	if (pPol->mVtxUseNum < pPol->mVtxAllocNum) {
		GEX_VTX v;
		if (0 == pPol->mVtxUseNum) {
			pPol->mGeoBBox.set(pos);
		} else {
			pPol->mGeoBBox.add_pnt(pos);
		}
		v.mPos = pos;
		v.mPID = pPol->mVtxUseNum;
		v.set_nrm_tng(nrm, tng);
		v.set_clr(clr);
		v.set_tex(uv, uv2);
		GEX_VTX* pDst = &pPol->mpMappedVB[pPol->mVtxUseNum];
		::memcpy(pDst, &v, sizeof(GEX_VTX));
		++pPol->mVtxUseNum;
	}
}

void gexPolTransform(GEX_POL* pPol, const cxMtx& mtx) {
	if (!pPol) return;
	pPol->mWorldMtx = mtx;
	pPol->mWorldBBox = pPol->mGeoBBox;
	pPol->mWorldBBox.transform(mtx);
	pPol->update_xform_ctx();
}

D3D11_PRIMITIVE_TOPOLOGY gexPolTopology(const GEX_POL* pPol) {
	D3D11_PRIMITIVE_TOPOLOGY topo;
	switch (pPol->mType) {
		case GEX_POL_TYPE::TRILIST:
		default:
			topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			break;
		case GEX_POL_TYPE::TRISTRIP:
			topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			break;
		case GEX_POL_TYPE::SEGLIST:
			topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			break;
		case GEX_POL_TYPE::SEGSTRIP:
			topo = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
			break;
	}
	return topo;
}

void gexPolDLFuncCast(GEX_DISP_ENTRY& ent) {
	ID3D11DeviceContext* pCtx = GWK.mpCtx;
	if (!pCtx) return;
	GEX_POL* pPol = (GEX_POL*)ent.mpData;
	if (!pPol) return;
	GEX_MTL* pMtl = gexPolMaterial(pPol);
	if (!pMtl) return;

	int nvtx = pPol->mVtxUseNum;
	if (nvtx <= 0) return;

	pCtx->OMSetBlendState(GWK.mpBlendOpaq, nullptr, 0xFFFFFFFF);

	if (pMtl->mDblSided) {
		GWK.mpCtx->RSSetState(GWK.mpDblSidedRS);
	} else {
		GWK.mpCtx->RSSetState(GWK.mpOneSidedRS);
	}

	pCtx->VSSetShader(GWK.mpSdwVS, nullptr, 0);
	pCtx->PSSetShader(GWK.mpSdwPS, nullptr, 0);
	pCtx->GSSetShader(nullptr, nullptr, 0);
	pCtx->HSSetShader(nullptr, nullptr, 0);
	pCtx->DSSetShader(nullptr, nullptr, 0);

	pCtx->VSSetShaderResources(VSCTX_g_obj, 1, &pPol->mObjCtx.mpSRV);
	pCtx->VSSetShaderResources(VSCTX_g_xform, 1, &pPol->mXformCtx.mpSRV);
	pCtx->VSSetShaderResources(VSCTX_g_sdw, 1, &GWK.mSdwBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_mtl, 1, &pMtl->mCtxBuf.mpSRV);

	if (pMtl->mUVMode == GEX_UV_MODE::WRAP) {
		pCtx->PSSetSamplers(0, 1, &GWK.mpSmpStateLinWrap);
	} else {
		pCtx->PSSetSamplers(0, 1, &GWK.mpSmpStateLinClamp);
	}
	ID3D11ShaderResourceView* pTexRV;
	pTexRV = pMtl->mpBaseTex ? pMtl->mpBaseTex->mpSRV : nullptr;
	pCtx->PSSetShaderResources(PSTEX_g_texBase, 1, &pTexRV);
	pCtx->IASetInputLayout(GWK.mpObjVtxLayout);
	UINT stride = sizeof(GEX_VTX);
	UINT offs = 0;
	pCtx->IASetVertexBuffers(0, 1, &pPol->mpVB, &stride, &offs);
	pCtx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	pCtx->IASetPrimitiveTopology(gexPolTopology(pPol));
	pCtx->Draw(nvtx, 0);
}

void gexPolDraw(GEX_POL* pPol, GEX_LIT* pLit) {
	if (!pPol) return;
	ID3D11DeviceContext* pCtx = GWK.mpCtx;
	if (!pCtx) return;
	GEX_CAM* pCam = GWK.mpScnCam;
	if (!pCam) return;
	GEX_MTL* pMtl = gexPolMaterial(pPol);
	if (!pMtl) return;
	if (!pLit) pLit = GWK.mpDefLit;

	int nvtx = pPol->mVtxUseNum;
	if (nvtx <= 0) return;

	if (pMtl->mDblSided) {
		GWK.mpCtx->RSSetState(GWK.mpDblSidedRS);
	} else {
		GWK.mpCtx->RSSetState(GWK.mpOneSidedRS);
	}

	bool tessFlg = false;
	if (pMtl->mTessMode != GEX_TESS_MODE::NONE) {
		if (pMtl->mCtxWk.tessFactor > 0.0f) {
			if (pPol->mType == GEX_POL_TYPE::TRILIST) {
				tessFlg = true;
			}
		}
	}

	pCtx->VSSetShader(GWK.mpObjVS, nullptr, 0);
	pCtx->PSSetShader(GWK.mpMtlPS, nullptr, 0);
	pCtx->GSSetShader(nullptr, nullptr, 0);
	if (tessFlg) {
		pCtx->HSSetShader(GWK.mpPNTriHS, nullptr, 0);
		pCtx->DSSetShader(GWK.mpPNTriDS, nullptr, 0);
	} else {
		pCtx->HSSetShader(nullptr, nullptr, 0);
		pCtx->DSSetShader(nullptr, nullptr, 0);
	}

	pCtx->VSSetShaderResources(VSCTX_g_cam, 1, &pCam->mCtxBuf.mpSRV);
	pCtx->VSSetShaderResources(VSCTX_g_obj, 1, &pPol->mObjCtx.mpSRV);
	pCtx->VSSetShaderResources(VSCTX_g_xform, 1, &pPol->mXformCtx.mpSRV);

	pCtx->PSSetShaderResources(PSCTX_g_cam, 1, &pCam->mCtxBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_mtl, 1, &pMtl->mCtxBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_lit, 1, &pLit->mCtxBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_shl, 1, &pLit->mSHLBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_fog, 1, &pLit->mFogBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_glb, 1, &GWK.mGlbBuf.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_sdw, 1, &GWK.mSdwBuf.mpSRV);

	if (pMtl->mUVMode == GEX_UV_MODE::WRAP) {
		pCtx->PSSetSamplers(0, 1, &GWK.mpSmpStateLinWrap);
	} else {
		pCtx->PSSetSamplers(0, 1, &GWK.mpSmpStateLinClamp);
	}
	pCtx->PSSetSamplers(1, 1, &GWK.mpSmpStatePnt);
	ID3D11ShaderResourceView* pTexRV;
	pTexRV = pMtl->mpBaseTex ? pMtl->mpBaseTex->mpSRV : nullptr;
	pCtx->PSSetShaderResources(PSTEX_g_texBase, 1, &pTexRV);
	pTexRV = pMtl->mpSpecTex ? pMtl->mpSpecTex->mpSRV : nullptr;
	pCtx->PSSetShaderResources(PSTEX_g_texSpec, 1, &pTexRV);
	pCtx->PSSetShaderResources(PSTEX_g_texSMap, 1, &GWK.mpSMRrsrcView);

	if (tessFlg) {
		pCtx->HSSetShaderResources(HSCTX_g_cam, 1, &pCam->mCtxBuf.mpSRV);
		pCtx->HSSetShaderResources(HSCTX_g_mtl, 1, &pMtl->mCtxBuf.mpSRV);
		pCtx->DSSetShaderResources(DSCTX_g_cam, 1, &pCam->mCtxBuf.mpSRV);
	}

	pCtx->IASetInputLayout(GWK.mpObjVtxLayout);
	UINT stride = sizeof(GEX_VTX);
	UINT offs = 0;
	pCtx->IASetVertexBuffers(0, 1, &pPol->mpVB, &stride, &offs);
	pCtx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	if (tessFlg) {
		pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
	} else {
		pCtx->IASetPrimitiveTopology(gexPolTopology(pPol));
	}
	pCtx->Draw(nvtx, 0);
}

void gexPolDLFuncOpaq(GEX_DISP_ENTRY& ent) {
	if (!GWK.mpCtx) return;
	GWK.mpCtx->OMSetBlendState(GWK.mpBlendOpaq, nullptr, 0xFFFFFFFF);
	GEX_POL* pPol = (GEX_POL*)ent.mpData;
	gexPolDraw(pPol, ent.mpLit);
}

void gexPolDLFuncSemi(GEX_DISP_ENTRY& ent) {
	GWK.mpCtx->OMSetBlendState(GWK.mpBlendSemi, nullptr, 0xFFFFFFFF);
	GEX_POL* pPol = (GEX_POL*)ent.mpData;
	gexPolDraw(pPol, ent.mpLit);
}

void gexPolDisp(GEX_POL* pPol, GEX_POL_TYPE type, GEX_LIT* pLit) {
	if (!pPol) return;
	if (!pPol->mpVB) return;
	if (!pLit) pLit = GWK.mpDefLit;
	GEX_CAM* pCam = GWK.mpScnCam;
	if (!pCam) return;
	GEX_MTL* pMtl = gexPolMaterial(pPol);
	if (!pMtl) return;

	pPol->mType = type;

	GEX_DISP_ENTRY ent;
	::memset(&ent, 0, sizeof(ent));

	if (pMtl->mCastShadows) {
		if (GWK.mShadowCastCnt > 0) {
			GWK.mShadowCastersBBox.merge(pPol->mWorldBBox);
		} else {
			GWK.mShadowCastersBBox = pPol->mWorldBBox;
		}
		ent.mpFunc = gexPolDLFuncCast;
		ent.mpData = pPol;
		ent.mpLit = pLit;
		ent.mSortKey = D_GEX_LYR_CAST << D_GEX_SORT_BITS;
		gexDispListEntry(ent);
		++GWK.mShadowCastCnt;
	}

	if (pMtl->mHidden) {
		return;
	}

	bool cullFlg = gexCullBBox(pCam->mFrustum, pPol->mWorldBBox);
	if (cullFlg) {
		++GWK.mPolCullCnt;
		return;
	}

	if (pMtl->mReceiveShadows) {
		if (GWK.mShadowRecvCnt > 0) {
			GWK.mShadowReceiversBBox.merge(pPol->mWorldBBox);
		} else {
			GWK.mShadowReceiversBBox = pPol->mWorldBBox;
		}
		++GWK.mShadowRecvCnt;
	}
	bool alphaFlg = !!pMtl->mCtxWk.enableAlpha;
	if (alphaFlg) {
		ent.mpFunc = gexPolDLFuncSemi;
	} else {
		ent.mpFunc = gexPolDLFuncOpaq;
	}
	cxVec sortPos = pMtl->calc_sort_pos(*pCam, pPol->mWorldBBox);
	float sortBias = pMtl->calc_sort_bias(pPol->mWorldBBox);
	uint32_t layer = alphaFlg ? D_GEX_LYR_SEMI : D_GEX_LYR_OPAQ;
	uint32_t zkey = gexCalcSortKey(pCam, sortPos, sortBias, alphaFlg);
	zkey |= layer << D_GEX_SORT_BITS;
	ent.mSortKey = zkey;
	ent.mpData = pPol;
	ent.mpLit = pLit;
	gexDispListEntry(ent);
}

void gexPolSortMode(GEX_POL* pPol, GEX_SORT_MODE mode) {
	if (!pPol) return;
	GEX_MTL* pMtl = gexPolMaterial(pPol);
	gexMtlSortMode(pMtl, mode);
}

void gexPolSortBias(GEX_POL* pPol, float absBias, float relBias) {
	if (!pPol) return;
	GEX_MTL* pMtl = gexPolMaterial(pPol);
	gexMtlSortBias(pMtl, absBias, relBias);
}

GEX_MTL* gexPolMaterial(const GEX_POL* pPol) {
	GEX_MTL* pMtl = nullptr;
	if (pPol) {
		pMtl = pPol->mpMtl;
	}
	return pMtl;
}

const char* gexPolName(const GEX_POL* pPol) {
	const char* pName = nullptr;
	if (pPol) {
		pName = pPol->mpName;
	}
	return pName;
}

GEX_POL* gexPolFind(const char* pName) {
	return gexLstFind(GWK.mpPolLstHead, pName);
}



const char* gexMtlName(GEX_MTL* pMtl) {
	const char* pName = nullptr;
	if (pMtl) {
		pName = pMtl->mpName;
	}
	return pName;
}

void gexMtlUpdate(GEX_MTL* pMtl) {
	if (!pMtl) return;
	pMtl->mCtxBuf.copy_data(&pMtl->mCtxWk);
}

void gexMtlDoubleSided(GEX_MTL* pMtl, bool dsidedFlg) {
	if (!pMtl) return;
	pMtl->mDblSided = dsidedFlg;
}

void gexDoubleSidedAll(GEX_OBJ* pObj, bool dsidedFlg) {
	if (pObj) {
		for (int i = 0; i < pObj->mMtlNum; ++i) {
			pObj->mpMtl[i].mDblSided = dsidedFlg;
		}
	}
}

void gexMtlHide(GEX_MTL* pMtl, bool hideFlg) {
	if (!pMtl) return;
	pMtl->mHidden = hideFlg;
}

void gexMtlBaseColor(GEX_MTL* pMtl, const cxColor& clr) {
	if (!pMtl) return;
	gexStoreRGB(&pMtl->mCtxWk.baseColor, clr);
}

void gexMtlBaseTexture(GEX_MTL* pMtl, GEX_TEX* pTex) {
	if (!pMtl) return;
	if (!pTex) {
		pTex = GWK.mpDefTexWhite;
	}
	pMtl->mpBaseTex = pTex;
}

void gexMtlSpecularColor(GEX_MTL* pMtl, const cxColor& clr) {
	if (!pMtl) return;
	gexStoreRGB(&pMtl->mCtxWk.specColor, clr);
}

void gexMtlSpecularTexture(GEX_MTL* pMtl, GEX_TEX* pTex) {
	if (!pMtl) return;
	if (!pTex) {
		pTex = GWK.mpDefTexWhite;
	}
	pMtl->mpSpecTex = pTex;
}

void gexMtlReflectionLevel(GEX_MTL* pMtl, float lvl) {
	if (!pMtl) return;
	pMtl->mCtxWk.reflLvl = lvl;
}

void gexMtlReflectionColor(GEX_MTL* pMtl, const cxColor& clr) {
	if (!pMtl) return;
	gexStoreRGB(&pMtl->mCtxWk.reflColor, clr);
}

void gexMtlReflectionTexture(GEX_MTL* pMtl, GEX_TEX* pTex) {
	if (!pMtl) return;
	if (!pTex) {
		pTex = GWK.mpDefTexBlack;
	}
	pMtl->mpReflTex = pTex;
}

void gexMtlBumpFactor(GEX_MTL* pMtl, float val) {
	if (!pMtl) return;
	pMtl->mCtxWk.bumpFactor = val;
}

void gexMtlBumpTexture(GEX_MTL* pMtl, GEX_TEX* pTex) {
	if (!pMtl) return;
	pMtl->mpBumpTex = pTex;
}

void gexMtlTangentMode(GEX_MTL* pMtl, GEX_TANGENT_MODE mode) {
	if (!pMtl) return;
	pMtl->mCtxWk.tangentMode = (int)mode;
}

void gexMtlTangentOptions(GEX_MTL* pMtl, bool flipTangent, bool flipBitangent) {
	if (!pMtl) return;
	pMtl->mCtxWk.flipTangent = flipTangent;
	pMtl->mCtxWk.flipBitangent = flipBitangent;
}

void gexMtlTesselationMode(GEX_MTL* pMtl, GEX_TESS_MODE mode) {
	if (!pMtl) return;
	if (mode != GEX_TESS_MODE::PNTRI) {
		mode = GEX_TESS_MODE::NONE;
	}
	pMtl->mTessMode = mode;
}

void gexMtlTesselationFactor(GEX_MTL* pMtl, float factor) {
	if (!pMtl) return;
	pMtl->mCtxWk.tessFactor = factor;
}

void gexMtlAlpha(GEX_MTL* pMtl, bool enable) {
	if (!pMtl) return;
	pMtl->mCtxWk.enableAlpha = enable;
}

void gexMtlAlphaBlend(GEX_MTL* pMtl, bool enable) {
	if (!pMtl) return;
	pMtl->mBlend = enable;
}

void gexMtlAlphaToCoverage(GEX_MTL* pMtl, bool enable) {
	if (!pMtl) return;
	pMtl->mAlphaToCoverage = enable;
}

void gexMtlRoughness(GEX_MTL* pMtl, float rough) {
	if (!pMtl) return;
	rough = nxCalc::saturate(rough);
	pMtl->mCtxWk.diffRoughness.fill(rough);
	pMtl->mCtxWk.specRoughness.fill(rough);
}

void gexMtlDiffuseRoughness(GEX_MTL* pMtl, const cxColor& rgb) {
	if (!pMtl) return;
	pMtl->mCtxWk.diffRoughness.set(rgb.r, rgb.g, rgb.b);
}

void gexMtlDiffuseRoughnessTexRate(GEX_MTL* pMtl, float rate) {
	if (!pMtl) return;
	pMtl->mCtxWk.diffRoughnessTexRate = nxCalc::saturate(rate);
}

void gexMtlSpecularRoughness(GEX_MTL* pMtl, const cxColor& rgb) {
	if (!pMtl) return;
	pMtl->mCtxWk.specRoughness.set(rgb.r, rgb.g, rgb.b);
}

void gexMtlSpecularRoughnessTexRate(GEX_MTL* pMtl, float rate) {
	if (!pMtl) return;
	pMtl->mCtxWk.specRoughnessTexRate = nxCalc::saturate(rate);
}

void gexMtlSpecularRoughnessMin(GEX_MTL* pMtl, float min) {
	if (!pMtl) return;
	pMtl->mCtxWk.specRoughnessMin = nxCalc::saturate(min);
}

void gexMtlIOR(GEX_MTL* pMtl, const cxVec& ior) {
	if (!pMtl) return;
	gexStoreVec(&pMtl->mCtxWk.IOR, ior);
}

void gexMtlViewFresnel(GEX_MTL* pMtl, float gain, float bias) {
	if (!pMtl) return;
	pMtl->mCtxWk.viewFrGain = gain;
	pMtl->mCtxWk.viewFrBias = bias;
}

void gexMtlReflFresnel(GEX_MTL* pMtl, float gain, float bias) {
	if (!pMtl) return;
	pMtl->mCtxWk.reflFrGain = gain;
	pMtl->mCtxWk.reflFrBias = bias;
}

void gexMtlReflDownFadeRate(GEX_MTL* pMtl, float rate) {
	if (!pMtl) return;
	pMtl->mCtxWk.reflDownFadeRate = nxCalc::saturate(rate);
}

void gexMtlDiffMode(GEX_MTL* pMtl, GEX_DIFF_MODE mode) {
	if (!pMtl) return;
	pMtl->mCtxWk.diffMode = (int)mode;
}

void gexMtlSpecMode(GEX_MTL* pMtl, GEX_SPEC_MODE mode) {
	if (!pMtl) return;
	pMtl->mCtxWk.specMode = (int)mode;
}

void gexMtlBumpMode(GEX_MTL* pMtl, GEX_BUMP_MODE mode) {
	if (!pMtl) return;
	pMtl->mCtxWk.bumpMode = (int)mode;
}

void gexMtlSpecFresnelMode(GEX_MTL* pMtl, int mode) {
	if (!pMtl) return;
	pMtl->mCtxWk.specFresnelMode = mode;
}

void gexMtlDiffFresnelMode(GEX_MTL* pMtl, int mode) {
	if (!pMtl) return;
	pMtl->mCtxWk.diffFresnelMode = mode;
}

void gexMtlUVMode(GEX_MTL* pMtl, GEX_UV_MODE mode) {
	if (!pMtl) return;
	pMtl->mUVMode = mode;
}

void gexMtlSortMode(GEX_MTL* pMtl, GEX_SORT_MODE mode) {
	if (!pMtl) return;
	pMtl->mSortMode = mode;
}

void gexMtlSortBias(GEX_MTL* pMtl, float absBias, float relBias) {
	if (!pMtl) return;
	pMtl->mSortBiasAbs = absBias;
	pMtl->mSortBiasRel = relBias;
}

void gexMtlShadowMode(GEX_MTL* pMtl, bool castShadows, bool receiveShadows) {
	if (!pMtl) return;
	pMtl->mCastShadows = castShadows;
	pMtl->mReceiveShadows = receiveShadows;
	pMtl->mCtxWk.receiveShadows = receiveShadows;
}

void gexMtlShadowDensity(GEX_MTL* pMtl, float density) {
	if (!pMtl) return;
	pMtl->mCtxWk.shadowDensity = nxCalc::saturate(density);
}

void gexMtlSelfShadowFactor(GEX_MTL* pMtl, float factor) {
	if (!pMtl) return;
	pMtl->mCtxWk.selfShadowFactor = factor;
}

void gexMtlShadowCulling(GEX_MTL* pMtl, bool enable) {
	if (!pMtl) return;
	pMtl->mCtxWk.cullShadows = enable ? 1 : 0;
}

void gexMtlShadowAlphaThreshold(GEX_MTL* pMtl, float val) {
	if (!pMtl) return;
	pMtl->mCtxWk.shadowAlphaThreshold = val;
}

void gexMtlSHDiffuseColor(GEX_MTL* pMtl, const cxColor& clr) {
	if (!pMtl) return;
	gexStoreRGB(&pMtl->mCtxWk.shDiffClr, clr);
}

void gexMtlSHDiffuseDetail(GEX_MTL* pMtl, float dtl) {
	if (!pMtl) return;
	pMtl->mCtxWk.shDiffDtl = dtl;
}

void gexMtlSHReflectionColor(GEX_MTL* pMtl, const cxColor& clr) {
	if (!pMtl) return;
	gexStoreRGB(&pMtl->mCtxWk.shReflClr, clr);
}

void gexMtlSHReflectionDetail(GEX_MTL* pMtl, float dtl) {
	if (!pMtl) return;
	pMtl->mCtxWk.shReflDtl = dtl;
}

void gexMtlSHReflectionFresnelRate(GEX_MTL* pMtl, float rate) {
	if (!pMtl) return;
	pMtl->mCtxWk.shReflFrRate = nxCalc::saturate(rate);
}


static const float s_shadowBias[4][4] = {
	{ 0.5f,  0.0f, 0.0f, 0.0f },
	{ 0.0f, -0.5f, 0.0f, 0.0f },
	{ 0.0f,  0.0f, 1.0f, 0.0f },
	{ 0.5f,  0.5f, 0.0f, 1.0f }
};

void gexCalcShadowMtxUni() {
	GEX_CAM* pCam = GWK.mpScnCam;
	if (!pCam) return;
	cxVec ldir = GWK.mShadowDir.get_normalized();
	cxMtx mtxView = gexLoadTMtx(pCam->mCtxWk.view);
	cxMtx mtx = gexLoadTMtx(pCam->mCtxWk.invView);
	float dist = nxVec::dist(pCam->mPos, pCam->mTgt);
	float ldist = 40.0f;////////
	cxVec v;
	v.set(-mtxView.m[0][2], -mtxView.m[1][2], -mtxView.m[2][2]);
	cxVec tgt = mtx.get_translation() + v*dist;
	cxVec pos = tgt - ldir*ldist;
	cxMtx vm;
	vm.mk_view(pos, tgt, nxVec::get_axis(exAxis::PLUS_Y));
	float vsize = GWK.mShadowViewSize;
	float vdist = 50.0f;//////
	mtx.identity();
	mtx.m[0][0] = 1.0f / vsize;
	mtx.m[1][1] = 1.0f / vsize;
	mtx.m[2][2] = 1.0f / (1.0f - (ldist + vdist));
	mtx.m[3][2] = mtx.m[2][2];
	mtx = vm * mtx;
	GWK.mShadowViewProj = mtx;
	mtx.from_mem(&s_shadowBias[0][0]);
	GWK.mShadowMtx = GWK.mShadowViewProj *mtx;
}

void gexCalcShadowMtxPersp() {
	GEX_CAM* pCam = GWK.mpScnCam;
	if (!pCam) return;
	cxVec ldir = GWK.mShadowDir.get_normalized();

	cxMtx mtxView = gexLoadTMtx(pCam->mCtxWk.view);
	cxMtx mtxProj = gexLoadTMtx(pCam->mCtxWk.proj);
	cxMtx mtxInvView = gexLoadTMtx(pCam->mCtxWk.invView);
	cxMtx mtxInvProj = gexLoadTMtx(pCam->mCtxWk.invProj);
	cxMtx mtxViewProj = gexLoadTMtx(pCam->mCtxWk.viewProj);
	cxMtx mtxInvViewProj = gexLoadTMtx(pCam->mCtxWk.invViewProj);

	float persp = 0.5f;
	float vdist = 1000.0f;
	vdist *= mtxProj.m[1][1] * persp;
	float cosVL = mtxInvView.get_row_vec(2).dot(ldir);
	float reduce = ::fabsf(cosVL);
	if (reduce > 0.7f) {
		vdist *= 0.5f;
	}
	reduce *= 0.8f;
	xt_float4 projPos;
	projPos.set(0.0f, 0.0f, 0.0f, 1.0f);
	projPos = mtxInvProj.apply(projPos);
	float nearDist = -nxCalc::div0(projPos.z, projPos.w);
	float dnear = nearDist;
	float dfar = dnear + (vdist - nearDist) * (1.0f - reduce);
	cxVec vdir = mtxInvView.get_row_vec(2).neg_val().get_normalized();
	cxVec pos = mtxInvView.get_translation() + vdir*dnear;
	projPos.set(pos.x, pos.y, pos.z, 1.0f);
	projPos = mtxViewProj.apply(projPos);
	float znear = nxCalc::max(0.0f, nxCalc::div0(projPos.z, projPos.w));
	pos = mtxInvView.get_translation() + vdir*dfar;
	projPos.set(pos.x, pos.y, pos.z, 1.0f);
	projPos = mtxViewProj.apply(projPos);
	float zfar = nxCalc::min(nxCalc::div0(projPos.z, projPos.w), 1.0f);

	xt_float4 box[8] = {};
	box[0].set(-1.0f, -1.0f, znear, 1.0f);
	box[1].set(1.0f, -1.0f, znear, 1.0f);
	box[2].set(-1.0f, 1.0f, znear, 1.0f);
	box[3].set(1.0f, 1.0f, znear, 1.0f);

	box[4].set(-1.0f, -1.0f, zfar, 1.0f);
	box[5].set(1.0f, -1.0f, zfar, 1.0f);
	box[6].set(-1.0f, 1.0f, zfar, 1.0f);
	box[7].set(1.0f, 1.0f, zfar, 1.0f);

	int i;
	for (i = 0; i < 8; ++i) {
		box[i] = mtxInvViewProj.apply(box[i]);
		box[i].scl(nxCalc::rcp0(box[i].w));
	}

	cxVec up = nxVec::cross(vdir, ldir);
	up = nxVec::cross(ldir, up).get_normalized();
	pos = mtxInvView.get_translation();
	cxVec tgt = pos - ldir;
	cxMtx vm;
	vm.mk_view(pos, tgt, up);

	float ymin = FLT_MAX;
	float ymax = -ymin;
	for (i = 0; i < 8; ++i) {
		float ty = box[i].x*vm.m[0][1] + box[i].y*vm.m[1][1] + box[i].z*vm.m[2][1] + vm.m[3][1];
		ymin = nxCalc::min(ymin, ty);
		ymax = nxCalc::max(ymax, ty);
	}

	float sy = nxCalc::max( 1e-8f, ::sqrtf(1.0f - nxCalc::sq(-vdir.dot(ldir))) );
	float t = (dnear + ::sqrtf(dnear*dfar)) / sy;
	float y = (ymax - ymin) + t;

	cxMtx cm;
	cm.identity();
	cm.m[0][0] = -1;
	cm.m[1][1] = (y + t) / (y - t);
	cm.m[1][3] = 1.0f;
	cm.m[3][1] = (-2.0f*y*t) / (y - t);
	cm.m[3][3] = 0.0f;

	pos += up * (ymin - t);
	tgt = pos - ldir;
	vm.mk_view(pos, tgt, up);

	cxMtx tm = vm * cm;
	cxVec vmin(FLT_MAX);
	cxVec vmax(-FLT_MAX);
	for (i = 0; i < 8; ++i) {
		xt_float4 tqv = tm.apply(box[i]);
		cxVec tv;
		tv.from_mem(tqv);
		tv.scl(nxCalc::rcp0(tqv.w));
		vmin = nxVec::min(vmin, tv);
		vmax = nxVec::max(vmax, tv);
	}

	float zmin = vmin.z;
	cxVec offs = ldir * 40.0f;
	for (i = 0; i < 8; ++i) {
		xt_float4 tqv;
		tqv.x = box[i].x - offs.x;
		tqv.y = box[i].y - offs.y;
		tqv.z = box[i].z - offs.z;
		tqv.w = 1.0f;
		tqv = tm.apply(tqv);
		zmin = nxCalc::min(zmin, nxCalc::div0(tqv.z, tqv.w));
	}
	vmin.z = zmin;

	cxMtx fm;
	fm.identity();
	cxVec vd = vmax - vmin;
	vd.x = nxCalc::rcp0(vd.x);
	vd.y = nxCalc::rcp0(vd.y);
	vd.z = nxCalc::rcp0(vd.z);
	cxVec vs = cxVec(2.0, 2.0f, 1.0f) * vd;
	cxVec vt = cxVec(vmin.x + vmax.x, vmin.y + vmax.y, vmin.z).neg_val() * vd;
	fm.m[0][0] = vs.x;
	fm.m[1][1] = vs.y;
	fm.m[2][2] = vs.z;
	fm.m[3][0] = vt.x;
	fm.m[3][1] = vt.y;
	fm.m[3][2] = vt.z;
	fm = cm * fm;

	GWK.mShadowViewProj = vm * fm;
	tm.from_mem(&s_shadowBias[0][0]);
	GWK.mShadowMtx = GWK.mShadowViewProj * tm;
}

void gexCalcShadowMtx() {
	if (GWK.mShadowProj == GEX_SHADOW_PROJ::PERSPECTIVE) {
		gexCalcShadowMtxPersp();
	} else {
		gexCalcShadowMtxUni();
	}
}

void gexShadowCastStart(GEX_DISP_ENTRY& ent) {
	gexCalcShadowMtx();
	SDW_CTX sdw;
	::memset(&sdw, 0, sizeof(sdw));
	gexStoreTMtx(&sdw.viewProj, GWK.mShadowViewProj);
	gexStoreTMtx(&sdw.xform, GWK.mShadowMtx);
	gexStoreVec(&sdw.dir, GWK.mShadowDir);
	gexStoreRGBA(&sdw.color, GWK.mShadowColor);
	sdw.color.w *= GWK.mShadowDensity;
	sdw.fade.set(GWK.mShadowFadeStart, nxCalc::rcp0(GWK.mShadowFadeEnd - GWK.mShadowFadeStart));
	sdw.litIdx = GWK.mShadowLightIdx;
	sdw.specValSel = GWK.mShadowSpecValSel;
	sdw.size = (float)GWK.mShadowMapSize;
	sdw.invSize = nxCalc::rcp0(sdw.size);
	GWK.mSdwBuf.copy_data(&sdw);
	if (GWK.mpSMTargetView) {
		GWK.mpCtx->OMSetRenderTargets(1, &GWK.mpSMTargetView, GWK.mpSMDepthView);
		D3D11_VIEWPORT vp;
		::memset(&vp, 0, sizeof(vp));
		vp.Width = (FLOAT)GWK.mShadowMapSize;
		vp.Height = (FLOAT)GWK.mShadowMapSize;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		GWK.mpCtx->RSSetViewports(1, &vp);
		float c[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		GWK.mpCtx->ClearRenderTargetView(GWK.mpSMTargetView, c);
		GWK.mpCtx->ClearDepthStencilView(GWK.mpSMDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
}

void gexShadowCastEnd(GEX_DISP_ENTRY& ent) {
	GWK.mpCtx->OMSetRenderTargets(1, &GWK.mpTargetView, GWK.mpDepthView);
	D3D11_VIEWPORT vp;
	::memset(&vp, 0, sizeof(vp));
	vp.Width = (FLOAT)GWK.mWidth;
	vp.Height = (FLOAT)GWK.mHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	GWK.mpCtx->RSSetViewports(1, &vp);
}

static void gexBgDraw() {
	GEX_CAM* pCam = GWK.mpScnCam;
	if (!pCam) return;
	bool flg = false;
	switch (GWK.mBgMode) {
		case GEX_BG_MODE::PANO:
			if (GWK.mpBgPanoTex) {
				flg = true;
			}
			break;
	}
	if (!flg) return;

	cxMtx wm;
	wm.mk_scl(pCam->mFar * 0.9f / 2.0f);
	cxVec pos = pCam->mPos.neg_val();
	wm.set_translation(pos);
	XFORM_CTX xform;
	gexStoreWMtx(&xform.mtx, wm);
	GWK.mEnvXformCtx.copy_data(&xform);

	ID3D11DeviceContext* pCtx = GWK.mpCtx;
	pCtx->OMSetBlendState(GWK.mpBlendOpaq, nullptr, 0xFFFFFFFF);
	pCtx->RSSetState(GWK.mpOneSidedRS);
	pCtx->VSSetShader(GWK.mpEnvVS, nullptr, 0);
	pCtx->PSSetShader(GWK.mpEnvPS, nullptr, 0);
	pCtx->GSSetShader(nullptr, nullptr, 0);
	pCtx->HSSetShader(nullptr, nullptr, 0);
	pCtx->DSSetShader(nullptr, nullptr, 0);
	pCtx->VSSetShaderResources(VSCTX_g_cam, 1, &pCam->mCtxBuf.mpSRV);
	pCtx->VSSetShaderResources(VSCTX_g_xform, 1, &GWK.mEnvXformCtx.mpSRV);
	pCtx->PSSetShaderResources(PSCTX_g_glb, 1, &GWK.mGlbBuf.mpSRV);

	pCtx->PSSetSamplers(0, 1, &GWK.mpSmpStateLinWrap);
	pCtx->PSSetShaderResources(PSTEX_g_texPano, 1, &GWK.mpBgPanoTex->mpSRV);

	pCtx->IASetInputLayout(GWK.mpEnvVtxLayout);
	UINT stride = sizeof(xt_float3);
	UINT offs = 0;
	pCtx->IASetVertexBuffers(0, 1, &GWK.mpEnvVB, &stride, &offs);
	pCtx->IASetIndexBuffer(GWK.mpEnvIB, DXGI_FORMAT_R16_UINT, 0);
	pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCtx->DrawIndexed(12*3, 0, 0);
}
