// Author: Sergey Chaban <sergey.chaban@gmail.com>

#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX
#define _WIN32_WINNT 0x0500
#include <Windows.h>
#include <tchar.h>
#include <Shlwapi.h>
#include <wincodec.h>
#include <mmsystem.h>

#include "crossdata.hpp"
#include "wutil.hpp"

#define GET_WAVEOUT_FN(_name) *(PROC*)&mWaveOut.##_name = (PROC)::GetProcAddress(mhWinMM, "waveOut" #_name)
#define GET_MIDIOUT_FN(_name) *(PROC*)&mMidiOut.##_name = (PROC)::GetProcAddress(mhWinMM, "midiOut" #_name)

static struct WU_WK {
	HMODULE mhSHLW;
	HRESULT (WINAPI *mpSHCreateStreamOnFileA)(const char* pPath, DWORD mode, IStream** ppStream);
	HRESULT (WINAPI *mpSHCreateStreamOnFileW)(const wchar_t* pPath, DWORD mode, IStream** ppStream);

	HMODULE mhCodecs;
	HRESULT (WINAPI *mpWICConvertBitmapSource)(REFWICPixelFormatGUID guid, IWICBitmapSource* pSrc, IWICBitmapSource** ppDst);

	HMODULE mhWinMM;

	struct {
		UINT (WINAPI *GetNumDevs)();
		MMRESULT (WINAPI *GetDevCapsA)(UINT_PTR, WAVEOUTCAPSA*, UINT);
		MMRESULT (WINAPI *SetVolume)(HWAVEOUT, DWORD);
		MMRESULT (WINAPI *Open)(HWAVEOUT*, UINT, const WAVEFORMATEX*, DWORD_PTR, DWORD_PTR, DWORD);
		MMRESULT (WINAPI *Close)(HWAVEOUT);
		MMRESULT (WINAPI *PrepareHeader)(HWAVEOUT, WAVEHDR*, UINT);
		MMRESULT (WINAPI *UnprepareHeader)(HWAVEOUT, WAVEHDR*, UINT);
		MMRESULT (WINAPI *Write)(HWAVEOUT, WAVEHDR*, UINT);
		MMRESULT (WINAPI *Reset)(HWAVEOUT);
		MMRESULT (WINAPI *GetPosition)(HWAVEOUT, MMTIME*, UINT);
	} mWaveOut;
	bool mWaveOutOk;

	struct {
		UINT (WINAPI *GetNumDevs)();
		MMRESULT (WINAPI *GetDevCapsA)(UINT_PTR, MIDIOUTCAPSA*, UINT);
		MMRESULT (WINAPI *Open)(HMIDIOUT*, UINT, DWORD_PTR, DWORD_PTR, DWORD);
		MMRESULT (WINAPI *Close)(HMIDIOUT);
		MMRESULT (WINAPI *PrepareHeader)(HMIDIOUT, MIDIHDR*, UINT);
		MMRESULT (WINAPI *UnprepareHeader)(HMIDIOUT, MIDIHDR*, UINT);
		MMRESULT (WINAPI *ShortMsg)(HMIDIOUT, DWORD);
		MMRESULT (WINAPI *LongMsg)(HMIDIOUT, MIDIHDR*, UINT);
		MMRESULT (WINAPI *Reset)(HMIDIOUT);
	} mMidiOut;
	bool mMidiOutOk;

	struct {
		MCIERROR (WINAPI *SendStringA)(const char*, char*, UINT, HWND);
	} mMCI;

	void load_libs() {
		mhSHLW = ::LoadLibraryW(L"SHLWAPI.dll");
		if (mhSHLW) {
			*(PROC*)&mpSHCreateStreamOnFileA = ::GetProcAddress(mhSHLW, "SHCreateStreamOnFileA");
			*(PROC*)&mpSHCreateStreamOnFileW = ::GetProcAddress(mhSHLW, "SHCreateStreamOnFileW");
		}
		mhCodecs = ::LoadLibraryW(L"WindowsCodecs.dll");
		if (mhCodecs) {
			*(PROC*)&mpWICConvertBitmapSource = ::GetProcAddress(mhCodecs, "WICConvertBitmapSource");
		}
		mWaveOutOk = false;
		mhWinMM = ::LoadLibraryW(L"WINMM.dll");
		if (mhWinMM) {
			GET_WAVEOUT_FN(GetNumDevs);
			GET_WAVEOUT_FN(GetDevCapsA);
			GET_WAVEOUT_FN(SetVolume);
			GET_WAVEOUT_FN(Open);
			GET_WAVEOUT_FN(Close);
			GET_WAVEOUT_FN(PrepareHeader);
			GET_WAVEOUT_FN(UnprepareHeader);
			GET_WAVEOUT_FN(Write);
			GET_WAVEOUT_FN(Reset);
			GET_WAVEOUT_FN(GetPosition);
			mWaveOutOk = true;
			for (int i = 0; i < sizeof(mWaveOut) / sizeof(PROC); ++i) {
				if (!((PROC*)&mWaveOut)[i]) {
					ZeroMemory(&mWaveOut, sizeof(mWaveOut));
					mWaveOutOk = false;
					break;
				}
			}

			GET_MIDIOUT_FN(GetNumDevs);
			GET_MIDIOUT_FN(GetDevCapsA);
			GET_MIDIOUT_FN(Open);
			GET_MIDIOUT_FN(Close);
			GET_MIDIOUT_FN(PrepareHeader);
			GET_MIDIOUT_FN(UnprepareHeader);
			GET_MIDIOUT_FN(ShortMsg);
			GET_MIDIOUT_FN(LongMsg);
			GET_MIDIOUT_FN(Reset);
			mMidiOutOk = true;
			for (int i = 0; i < sizeof(mMidiOut) / sizeof(PROC); ++i) {
				if (!((PROC*)&mMidiOut)[i]) {
					ZeroMemory(&mMidiOut, sizeof(mMidiOut));
					mMidiOutOk = false;
					break;
				}
			}

			*(PROC*)&mMCI.SendStringA = ::GetProcAddress(mhWinMM, "mciSendStringA");
		}
	}

	void unload_libs() {
		if (mhSHLW) {
			mpSHCreateStreamOnFileA = nullptr;
			mpSHCreateStreamOnFileW = nullptr;
			::FreeLibrary(mhSHLW);
			mhSHLW = NULL;
		}
		if (mhCodecs) {
			mpWICConvertBitmapSource = nullptr;
			::FreeLibrary(mhCodecs);
			mhCodecs = NULL;
		}
		if (mhWinMM) {
			ZeroMemory(&mWaveOut, sizeof(mWaveOut));
			ZeroMemory(&mMidiOut, sizeof(mMidiOut));
			ZeroMemory(&mMCI, sizeof(mMCI));
			::FreeLibrary(mhWinMM);
			mhWinMM = NULL;
		}
	}
} WUW = {};

void wuInit() {
	::CoInitialize(NULL);
	WUW.load_libs();
}

void wuReset() {
	WUW.unload_libs();
	::CoUninitialize();
}

static void wuDefDbgMsg(const char* pMsg) {
	::OutputDebugStringA(pMsg);
}

void wuDefaultSysIfc(sxSysIfc* pIfc) {
	if (pIfc) {
		ZeroMemory(pIfc, sizeof(sxSysIfc));
		pIfc->fn_dbgmsg = wuDefDbgMsg;
	}
}

static bool s_conFlg = false;

void wuConAttach(int x, int y, int w, int h) {
	if (s_conFlg) return;
	::AllocConsole();
	::AttachConsole(::GetCurrentProcessId());
	FILE* pConFile;
	::freopen_s(&pConFile, "CON", "w", stdout);
	::freopen_s(&pConFile, "CON", "w", stderr);
	::SetWindowPos(::GetConsoleWindow(), nullptr, x, y, w, h, 0);
	s_conFlg = true;
}

void wuConClose() {
	if (s_conFlg) {
		::FreeConsole();
	}
	s_conFlg = false;
}

xt_int2 wuConGetLocation() {
	HANDLE hstd = ::GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO sbi;
	::GetConsoleScreenBufferInfo(hstd, &sbi);
	xt_int2 loc;
	loc.x = sbi.dwCursorPosition.X;
	loc.y = sbi.dwCursorPosition.Y;
	return loc;
}

void wuConLocate(int x, int y) {
	HANDLE hstd = ::GetStdHandle(STD_OUTPUT_HANDLE);
	COORD c;
	c.X = (SHORT)x;
	c.Y = (SHORT)y;
	::SetConsoleCursorPosition(hstd, c);
}

void wuConTextColor(const cxColor& c) {
	HANDLE hstd = ::GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	::GetConsoleScreenBufferInfo(hstd, &info);
	WORD attr = 0;
	if (c.r >= 0.5f) attr |= FOREGROUND_RED;
	if (c.g >= 0.5f) attr |= FOREGROUND_GREEN;
	if (c.b >= 0.5f) attr |= FOREGROUND_BLUE;
	if (c.luma() > 0.5f) attr |= FOREGROUND_INTENSITY;
	::SetConsoleTextAttribute(hstd, attr);
}

void wuConTextRGB(float r, float g, float b) {
	wuConTextColor(cxColor(r, g, b));
}


bool wuCreateDir(const char* pPath) {
	if (!pPath) return false;
	return ::CreateDirectoryA(pPath, nullptr) != 0;
}

bool wuSetCurDirToExePath() {
	LPTSTR pCmd;
	TCHAR path[MAX_PATH];
	TCHAR fullPath[MAX_PATH];
	::memset(path, 0, sizeof(path));
	pCmd = GetCommandLine();
	while (*pCmd == '"' || *pCmd == ' ') ++pCmd;
	LPTSTR pOrg = pCmd;
	while (*pCmd && !::iswspace(*pCmd)) ++pCmd;
	LPTSTR pEnd = pCmd;
	while (pEnd >= pOrg && *pEnd != '\\') --pEnd;
#if defined(_MSC_VER)
	_tcsncpy_s(path, sizeof(path) / sizeof(path[0]), pOrg, pEnd - pOrg);
#else
	_tcsncpy(path, pOrg, pEnd - pOrg);
#endif
	GetFullPathName(path, sizeof(fullPath) / sizeof(fullPath[0]), fullPath, NULL);
	BOOL res = SetCurrentDirectory(fullPath);
	if (!res) {
		nxCore::dbg_msg("!cwd\n");
	}
	return !!res;
}

static WU_IMAGE* wuImgLoad(const void* pPath, bool wflg, const IID& decoderClsId, bool linearize = true, uint32_t alphaKey = 0) {
	WU_IMAGE* pImg = nullptr;
	HRESULT hres = S_OK;
	IStream* pStrm = nullptr;
	if (WUW.mpWICConvertBitmapSource) {
		if (wflg) {
			if (WUW.mpSHCreateStreamOnFileW) {
				hres = WUW.mpSHCreateStreamOnFileW((const wchar_t*)pPath, STGM_READ, &pStrm);
				if (FAILED(hres)) {
					pStrm = nullptr;
				}
			}
		} else {
			if (WUW.mpSHCreateStreamOnFileA) {
				hres = WUW.mpSHCreateStreamOnFileA((const char*)pPath, STGM_READ, &pStrm);
				if (FAILED(hres)) {
					pStrm = nullptr;
				}
			}
		}
	}
	if (pStrm) {
		IWICBitmapDecoder* pDec = nullptr;
		hres = ::CoCreateInstance(decoderClsId, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICBitmapDecoder), (void**)&pDec);
		if (SUCCEEDED(hres)) {
			hres = pDec->Initialize(pStrm, WICDecodeMetadataCacheOnLoad);
			if (SUCCEEDED(hres)) {
				IWICBitmapFrameDecode* pFrm = nullptr;
				hres = pDec->GetFrame(0, &pFrm);
				if (SUCCEEDED(hres)) {
					if (SUCCEEDED(hres)) {
						IWICBitmapSource* pBmp = nullptr;
						hres = WUW.mpWICConvertBitmapSource(GUID_WICPixelFormat32bppRGBA, pFrm, &pBmp);
						if (SUCCEEDED(hres)) {
							UINT w = 0;
							UINT h = 0;
							hres = pBmp->GetSize(&w, &h);
							if (SUCCEEDED(hres)) {
								pImg = WU_IMAGE::alloc(w, h);
								if (pImg) {
									uxVal32* pTmpPix = reinterpret_cast<uxVal32*>(nxCore::mem_alloc(w*h*sizeof(uxVal32)));
									if (pTmpPix) {
										cxColor* pClr = pImg->get_pixels();
										WICRect rect;
										rect.X = 0;
										rect.Y = 0;
										rect.Width = w;
										rect.Height = h;
										UINT stride = UINT(w * sizeof(uxVal32));
										hres = pBmp->CopyPixels(&rect, stride, (UINT)pImg->calc_data_size(), (BYTE*)pTmpPix);
										if (SUCCEEDED(hres)) {
											if (alphaKey) {
												uint32_t rgbk = alphaKey & 0xFFFFFF;
												for (UINT i = 0; i < w*h; ++i) {
													if ((pTmpPix[i].u & 0xFFFFFF) == rgbk) {
														pTmpPix[i].u = 0;
													}
												}
											}
											for (UINT i = 0; i < w*h; ++i) {
												cxColor* pClrDst = &pClr[i];
												uxVal32* pClrSrc = &pTmpPix[i];
												for (int j = 0; j < 4; ++j) {
													pClrDst->ch[j] = float(pClrSrc->b[j]);
												}
											}
											for (UINT i = 0; i < w*h; ++i) {
												pClr[i].scl(1.0f / 255);
											}
											if (linearize) {
												for (UINT i = 0; i < w*h; ++i) {
													cxColor* pWk = &pClr[i];
													for (int j = 0; j < 4; ++j) {
														pWk->ch[j] = ::powf(pWk->ch[j], 2.2f);
													}
												}
											}
										}
										nxCore::mem_free(pTmpPix);
									}
								}
							}
							pBmp->Release();
							pBmp = nullptr;
						}
						pFrm->Release();
						pFrm = nullptr;
					}
				}
			}
			pDec->Release();
			pDec = nullptr;
		}
		pStrm->Release();
		pStrm = nullptr;
	}
	return pImg;
}

static WU_IMAGE* wuImgLoadA(const char* pPath, const IID& decoderClsId, bool linearize = true, uint32_t alphaKey = 0) {
	return wuImgLoad(pPath, false, decoderClsId, linearize, alphaKey);
}

static WU_IMAGE* wuImgLoadW(const wchar_t* pPath, const IID& decoderClsId, bool linearize = true, uint32_t alphaKey = 0) {
	return wuImgLoad(pPath, true, decoderClsId, linearize, alphaKey);
}

void WU_IMAGE::to_nonlinear(float gamma) {
	if (gamma <= 0.0f || gamma == 1.0f) return;
	float igamma = 1.0f / gamma;
	for (uint32_t i = 0; i < mWidth * mHeight; ++i) {
		for (int j = 0; j < 3; ++j) {
			if (mPixels[i].ch[j] <= 0.0f) {
				mPixels[i].ch[j] = 0.0f;
			} else {
				mPixels[i].ch[j] = ::powf(mPixels[i].ch[j], igamma);
			}
		}
	}
}

void WU_IMAGE::to_linear(float gamma) {
	if (gamma <= 0.0f || gamma == 1.0f) return;
	for (uint32_t i = 0; i < mWidth * mHeight; ++i) {
		mPixels[i].to_linear(gamma);
	}
}

void WU_IMAGE::save_dds128(const char* pPath) const {
	nxTexture::save_dds128(pPath, mPixels, mWidth, mHeight);
}

void WU_IMAGE::save_dds64(const char* pPath) const {
	nxTexture::save_dds64(pPath, mPixels, mWidth, mHeight);
}

void WU_IMAGE::save_dds32(const char* pPath, float gamma) const {
	nxTexture::save_dds32_bgra8(pPath, mPixels, mWidth, mHeight, gamma);
}

void WU_IMAGE::save_sgi(const char* pPath, float gamma) const {
	nxTexture::save_sgi(pPath, mPixels, mWidth, mHeight, gamma);
}

WU_IMAGE* WU_IMAGE::alloc(uint32_t w, uint32_t h) {
	WU_IMAGE* pImg = nullptr;
	if (w > 0 && h > 0) {
		size_t size = sizeof(WU_IMAGE) + ((w * h * sizeof(cxColor)) - 1);
		pImg = reinterpret_cast<WU_IMAGE*>(nxCore::mem_alloc(size, XD_FOURCC('W', 'I', 'm', 'g')));
		if (pImg) {
			pImg->mWidth = w;
			pImg->mHeight = h;
		}
	}
	return pImg;
}

WU_IMAGE* WU_IMAGE::dup(const WU_IMAGE* pSrc) {
	WU_IMAGE* pImg = nullptr;
	if (pSrc) {
		uint32_t w = pSrc->get_width();
		uint32_t h = pSrc->get_height();
		pImg = alloc(w, h);
		if (pImg) {
			::memcpy(pImg->mPixels, pSrc->mPixels, w * h * sizeof(cxColor));
		}
	}
	return pImg;
}

WU_IMAGE* WU_IMAGE::upscale(const WU_IMAGE* pSrc, int xscl, int yscl, bool filt) {
	WU_IMAGE* pImg = nullptr;
	if (pSrc) {
		int sw = pSrc->get_width();
		int sh = pSrc->get_height();
		int w = sw * xscl;
		int h = sh * yscl;
		if (w > 0 && h > 0) {
			pImg = alloc(w, h);
			if (pImg) {
				nxTexture::upscale(pSrc->mPixels, sw, sh, xscl, yscl, filt, pImg->mPixels);
			}
		}
	}
	return pImg;
}

WU_IMAGE* WU_IMAGE::load_png(const char* pPath) {
	return wuImgLoadA(pPath, CLSID_WICPngDecoder);
}

WU_IMAGE* WU_IMAGE::load_jpg(const char* pPath) {
	return wuImgLoadA(pPath, CLSID_WICJpegDecoder);
}

WU_IMAGE* WU_IMAGE::load_tif(const char* pPath) {
	return wuImgLoadA(pPath, CLSID_WICTiffDecoder);
}

XD_NOINLINE WU_IMAGE* wuImgAlloc(uint32_t w, uint32_t h) {
	return WU_IMAGE::alloc(w, h);
}

XD_NOINLINE WU_IMAGE* wuImgDup(const WU_IMAGE* pSrc) {
	return WU_IMAGE::dup(pSrc);
}

XD_NOINLINE void wuImgFree(WU_IMAGE* pImg) {
	if (pImg) {
		nxCore::mem_free(pImg);
	}
}

XD_NOINLINE WU_IMAGE* wuImgLoadPNG(const char* pPath) {
	return WU_IMAGE::load_png(pPath);
}

XD_NOINLINE WU_IMAGE* wuImgLoadJPG(const char* pPath) {
	return WU_IMAGE::load_jpg(pPath);
}

XD_NOINLINE WU_IMAGE* wuImgLoadAlphaKeyPNG(const char* pPath, uint32_t aclr) {
	return wuImgLoadA(pPath, CLSID_WICPngDecoder, true, aclr);
}

XD_NOINLINE WU_IMAGE* wuImgLoadAlphaKeyGIF(const char* pPath, uint32_t aclr) {
	return wuImgLoadA(pPath, CLSID_WICGifDecoder, true, aclr);
}


XD_NOINLINE bool wuTextCommandToMCI(const char* pCmd, char* pRet, size_t retSize) {
	if (!WUW.mMCI.SendStringA || !pCmd) return false;
	return WUW.mMCI.SendStringA(pCmd, pRet, (UINT)retSize, nullptr) == 0;
}

struct WU_WAVEOUT {
	HWAVEOUT h;
	WU_WAVEOUT_CB cb;
	void* pUsrData;
	int smpSize;
	int freq;
	int attr;
	int nblk;

	bool is_16bits() const { return !!(attr & 1); }
	bool is_stereo() const { return !!(attr & 2); }

	bool supports_volume() const { return !!(attr & 4); }
	bool supports_panning() const { return !!(attr & 8); }

	WAVEHDR* get_wavehdr(int idx) {
		if (idx >= 0 && idx < nblk) {
			return reinterpret_cast<WAVEHDR*>(this + 1) + idx;
		}
		return nullptr;
	}
};

XD_NOINLINE int wuGetNumWaveOutDevices() {
	int n = 0;
	if (WUW.mWaveOutOk) {
		n = int(WUW.mWaveOut.GetNumDevs());
	}
	return n;
}

static void CALLBACK wave_out_proc(HWAVEOUT hwo, UINT msg, DWORD_PTR self, DWORD_PTR param1, DWORD_PTR param2) {
	WU_WAVEOUT* pWO = (WU_WAVEOUT*)self;
	if (pWO) {
		switch (msg) {
			case WOM_OPEN:
				if (pWO->cb.open) {
					pWO->cb.open(pWO, pWO->pUsrData);
				}
				break;
			case WOM_DONE:
				if (pWO->cb.done) {
					pWO->cb.done(pWO, pWO->pUsrData);
				}
				break;
			case WOM_CLOSE:
				if (pWO->cb.close) {
					pWO->cb.close(pWO, pWO->pUsrData);
				}
				break;
		}
	}
}

XD_NOINLINE WU_WAVEOUT* wuWaveOutOpen(int freq, bool use16Bits, bool useStereo, WU_WAVEOUT_CB* pCB, void* pUsrData, int nblk, int devId) {
	WU_WAVEOUT* pWO = nullptr;
	if (WUW.mWaveOutOk) {
		UINT idev = WAVE_MAPPER;
		if (devId >= 0 && devId < wuGetNumWaveOutDevices()) {
			idev = (UINT)devId;
		}
		nblk = nxCalc::min(1, nblk);
		size_t size = sizeof(WU_WAVEOUT) + (sizeof(WAVEHDR) * nblk);
		pWO = (WU_WAVEOUT*)nxCore::mem_alloc(size, XD_FOURCC('W', 'U', 'W', 'O'));
		if (pWO) {
			ZeroMemory(pWO, size);
			if (pCB) {
				pWO->cb = *pCB;
			}
			pWO->pUsrData = pUsrData;
			int nch = useStereo ? 2 : 1;
			int nbytes = use16Bits ? 2 : 1;
			int smpSize = nch * nbytes;
			WAVEFORMATEX fmt;
			ZeroMemory(&fmt, sizeof(fmt));
			fmt.cbSize = sizeof(fmt);
			fmt.nSamplesPerSec = freq;
			fmt.nChannels = nch;
			fmt.wBitsPerSample = nbytes * 8;
			fmt.nBlockAlign = smpSize;
			fmt.nAvgBytesPerSec = freq * smpSize;
			fmt.wFormatTag = WAVE_FORMAT_PCM;
			MMRESULT mmres = WUW.mWaveOut.Open(&pWO->h, idev, &fmt, (DWORD_PTR)wave_out_proc, (DWORD_PTR)pWO, CALLBACK_FUNCTION);
			if (MMSYSERR_NOERROR == mmres) {
				pWO->smpSize = smpSize;
				pWO->freq = freq;
				pWO->nblk = nblk;
				if (use16Bits) pWO->attr |= 1;
				if (useStereo) pWO->attr |= 2;
				WAVEOUTCAPSA caps;
				mmres = WUW.mWaveOut.GetDevCapsA((UINT_PTR)pWO->h, &caps, (UINT)sizeof(caps));
				if (MMSYSERR_NOERROR == mmres) {
					if (caps.dwSupport & WAVECAPS_VOLUME) {
						pWO->attr |= 4;
					}
					if (caps.dwSupport & WAVECAPS_LRVOLUME) {
						pWO->attr |= 8;
					}
				}
			} else {
				nxCore::mem_free(pWO);
				pWO = nullptr;
			}
		}
	}
	return pWO;
}

XD_NOINLINE void wuWaveOutClose(WU_WAVEOUT* pWaveOut) {
	if (WUW.mWaveOutOk && pWaveOut && pWaveOut->h) {
		WUW.mWaveOut.Reset(pWaveOut->h);
		while (WUW.mWaveOut.Close(pWaveOut->h) == WAVERR_STILLPLAYING) {
			::Sleep(10);
		}
		nxCore::mem_free(pWaveOut);
	}
}

XD_NOINLINE bool wuWaveOutBlockInit(WU_WAVEOUT* pWaveOut, int blkIdx, void* pBuf, size_t bufSize) {
	if (!pWaveOut) return false;
	if (!pWaveOut->h) return false;
	if (!pBuf) return false;
	if (!bufSize) return false;
	WAVEHDR* pHdr = pWaveOut->get_wavehdr(blkIdx);
	if (!pHdr) return false;
	if (pHdr->dwFlags & WHDR_PREPARED) {
		while (WUW.mWaveOut.UnprepareHeader(pWaveOut->h, pHdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
			::Sleep(10);
		}
	}
	pHdr->lpData = (LPSTR)pBuf;
	pHdr->dwBufferLength = (DWORD)bufSize;
	MMRESULT mmres = WUW.mWaveOut.PrepareHeader(pWaveOut->h, pHdr, sizeof(WAVEHDR));
	if (MMSYSERR_NOERROR != mmres) return false;
	return true;
}

XD_NOINLINE bool wuWaveOutBlockReset(WU_WAVEOUT* pWaveOut, int blkIdx) {
	if (!pWaveOut) return false;
	if (!pWaveOut->h) return false;
	WAVEHDR* pHdr = pWaveOut->get_wavehdr(blkIdx);
	if (!pHdr) return false;
	if (pHdr->dwFlags & WHDR_PREPARED) {
		while (WUW.mWaveOut.UnprepareHeader(pWaveOut->h, pHdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
			::Sleep(10);
		}
	}
	return true;
}

XD_NOINLINE bool wuWaveOutBlockSend(WU_WAVEOUT* pWaveOut, int blkIdx) {
	if (!pWaveOut) return false;
	if (!pWaveOut->h) return false;
	WAVEHDR* pHdr = pWaveOut->get_wavehdr(blkIdx);
	if (!pHdr) return false;
	if (!(pHdr->dwFlags & WHDR_PREPARED)) return false;
	MMRESULT mmres = WUW.mWaveOut.Write(pWaveOut->h, pHdr, sizeof(WAVEHDR));
	if (MMSYSERR_NOERROR != mmres) return false;
	return true;
}

XD_NOINLINE void wuWaveOutSetVolume(WU_WAVEOUT* pWaveOut, float volL, float volR) {
	if (!pWaveOut || !pWaveOut->h) return;
	if (!pWaveOut->supports_volume()) return;
	float l = nxCalc::saturate(volL);
	float r = nxCalc::saturate(volR);
	const float scl = float(0xFFFF);
	if (pWaveOut->supports_panning()) {
		DWORD dl = (DWORD)(l * scl);
		DWORD dr = (DWORD)(r * scl);
		WUW.mWaveOut.SetVolume(pWaveOut->h, dl | (dr << 16));
	} else {
		DWORD d = (DWORD)((l + r) * 0.5f * scl);
		WUW.mWaveOut.SetVolume(pWaveOut->h, d | (d << 16));
	}
}

XD_NOINLINE int wuWaveOutGetFreq(WU_WAVEOUT* pWaveOut) {
	return pWaveOut ? pWaveOut->freq : 0;
}


struct WU_MIDIOUT {
	HMIDIOUT h;
	WU_MIDIOUT_CB cb;
	void* pUsrData;
	int kind;
	int attr;
};

XD_NOINLINE int wuGetNumMidiOutDevices() {
	int n = 0;
	if (WUW.mMidiOutOk) {
		n = int(WUW.mMidiOut.GetNumDevs());
	}
	return n;
}

static int wuFindMidiOutTech(int kind) {
	if (WUW.mMidiOutOk) {
		int n = wuGetNumMidiOutDevices();
		for (int i = 0; i < n; ++i) {
			MIDIOUTCAPSA caps;
			MMRESULT mmres = WUW.mMidiOut.GetDevCapsA((UINT_PTR)i, &caps, (UINT)sizeof(caps));
			if (MMSYSERR_NOERROR == mmres) {
				if (caps.wTechnology == kind) {
					return i;
				}
			}
		}
	}
	return -1;
}

XD_NOINLINE int wuFindMidiOutFM() {
	return wuFindMidiOutTech(MOD_FMSYNTH);
}

XD_NOINLINE int wuFindMidiOutWT() {
	return wuFindMidiOutTech(MOD_WAVETABLE);
}

XD_NOINLINE int wuFindMidiOutSW() {
	return wuFindMidiOutTech(MOD_SWSYNTH);
}

static void CALLBACK midi_out_proc(HMIDIOUT hmo, UINT msg, DWORD_PTR self, DWORD_PTR param1, DWORD_PTR param2) {
	WU_MIDIOUT* pMO = (WU_MIDIOUT*)self;
	if (pMO) {
		switch (msg) {
		case MOM_OPEN:
			if (pMO->cb.open) {
				pMO->cb.open(pMO, pMO->pUsrData);
			}
			break;
		case MOM_DONE:
			if (pMO->cb.done) {
				pMO->cb.done(pMO, pMO->pUsrData);
			}
			break;
		case MOM_CLOSE:
			if (pMO->cb.close) {
				pMO->cb.close(pMO, pMO->pUsrData);
			}
			break;
		}
	}
}

XD_NOINLINE WU_MIDIOUT* wuMidiOutOpen(WU_MIDIOUT_CB* pCB, void* pUsrData, int devId) {
	WU_MIDIOUT* pMO = nullptr;
	if (WUW.mMidiOutOk) {
		UINT idev = MIDI_MAPPER;
		if (devId >= 0 && devId < wuGetNumMidiOutDevices()) {
			idev = (UINT)devId;
		}
		size_t size = sizeof(WU_MIDIOUT);
		pMO = (WU_MIDIOUT*)nxCore::mem_alloc(size, XD_FOURCC('W', 'U', 'M', 'O'));
		if (pMO) {
			ZeroMemory(pMO, size);
			if (pCB) {
				pMO->cb = *pCB;
			}
			pMO->pUsrData = pUsrData;
			MMRESULT mmres = WUW.mMidiOut.Open(&pMO->h, idev, (DWORD_PTR)midi_out_proc, (DWORD_PTR)pMO, CALLBACK_FUNCTION);
			if (MMSYSERR_NOERROR == mmres) {
				MIDIOUTCAPSA caps;
				mmres = WUW.mMidiOut.GetDevCapsA((UINT_PTR)pMO->h, &caps, (UINT)sizeof(caps));
				if (MMSYSERR_NOERROR == mmres) {
					pMO->kind = caps.wTechnology;
					if (caps.dwSupport & MIDICAPS_VOLUME) pMO->attr |= 1;
					if (caps.dwSupport & MIDICAPS_LRVOLUME) pMO->attr |= 2;
				}
			} else {
				nxCore::mem_free(pMO);
				pMO = nullptr;
			}
		}
	}
	return pMO;
}

XD_NOINLINE void wuMidiOutClose(WU_MIDIOUT* pMidiOut) {
	if (WUW.mMidiOutOk && pMidiOut && pMidiOut->h) {
		WUW.mMidiOut.Reset(pMidiOut->h);
		WUW.mMidiOut.Close(pMidiOut->h);
		nxCore::mem_free(pMidiOut);
	}
}

XD_NOINLINE bool wuMidiOutSend(WU_MIDIOUT* pMidiOut, uint8_t* pMsg, size_t msgLen) {
	if (!WUW.mMidiOutOk) return false;
	if (!pMidiOut || !pMidiOut->h || !pMsg) return false;
	MMRESULT mmres = MMSYSERR_NOERROR;
	if (0xF0 == *pMsg) {
		MIDIHDR mh;
		::memset(&mh, 0, sizeof(mh));
		mh.lpData = (LPSTR)pMsg;
		mh.dwBufferLength = (DWORD)msgLen;
		mmres = WUW.mMidiOut.PrepareHeader(pMidiOut->h, &mh, sizeof(mh));
		if (MMSYSERR_NOERROR == mmres) {
			mmres = WUW.mMidiOut.LongMsg(pMidiOut->h, &mh, sizeof(mh));
			if (MMSYSERR_NOERROR == mmres) {
				while (WUW.mMidiOut.UnprepareHeader(pMidiOut->h, &mh, sizeof(mh)) == MIDIERR_STILLPLAYING) {
					::Sleep(1);
				}
			}
		}
	} else {
		uxVal32 msgVal;
		msgVal.u = 0;
		int n = nxCalc::min(int(msgLen), 3);
		for (int i = 0; i < n; ++i) {
			msgVal.b[i] = pMsg[i];
		}
		mmres = WUW.mMidiOut.ShortMsg(pMidiOut->h, (DWORD)msgVal.u);
	}
	return (MMSYSERR_NOERROR == mmres);
}
