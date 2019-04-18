// Author: Sergey Chaban <sergey.chaban@gmail.com>

#define WU_DEF_GAMMA (2.2f)

class WU_IMAGE {
protected:
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mReserve0;
	uint32_t mReserve1;
	cxColor mPixels[1];

	WU_IMAGE() {}

public:
	size_t calc_data_size() const { return mWidth * mHeight * sizeof(cxColor); }
	uint32_t get_width() const { return mWidth; }
	uint32_t get_height() const { return mHeight; }
	cxColor* get_pixels() { return mPixels; }
	const cxColor* get_pixels() const { return mPixels; }
	void to_nonlinear(float gamma = WU_DEF_GAMMA);
	void to_linear(float gamma = WU_DEF_GAMMA);

	void save_dds128(const char* pPath) const;
	void save_dds64(const char* pPath) const;
	void save_dds32(const char* pPath, float gamma = WU_DEF_GAMMA) const;
	void save_sgi(const char* pPath, float gamma = WU_DEF_GAMMA) const;
	void save_png(const char* pPath, float gamma = WU_DEF_GAMMA) const;

	static WU_IMAGE* alloc(uint32_t w, uint32_t h);
	static WU_IMAGE* dup(const WU_IMAGE* pSrc);
	static WU_IMAGE* upscale(const WU_IMAGE* pSrc, int xscl, int yscl, bool filt = true);
	static WU_IMAGE* load_png(const char* pPath, float gamma = WU_DEF_GAMMA);
	static WU_IMAGE* load_jpg(const char* pPath, float gamma = WU_DEF_GAMMA);
	static WU_IMAGE* load_tif(const char* pPath, float gamma = WU_DEF_GAMMA);
};

void wuInit();
void wuReset();

void wuDefaultSysIfc(sxSysIfc* pIfc);

void wuConAttach(int x, int y, int w, int h);
void wuConClose();
xt_int2 wuConGetLocation();
void wuConLocate(int x, int y);
void wuConTextColor(const cxColor& c);
void wuConTextRGB(float r, float g, float b);

bool wuCreateDir(const char* pPath);
bool wuSetCurDirToExePath();

WU_IMAGE* wuImgAlloc(uint32_t w, uint32_t h);
WU_IMAGE* wuImgDup(const WU_IMAGE* pSrc);
void wuImgFree(WU_IMAGE* pImg);
WU_IMAGE* wuImgLoadPNG(const char* pPath, float gamma = WU_DEF_GAMMA);
WU_IMAGE* wuImgLoadJPG(const char* pPath, float gamma = WU_DEF_GAMMA);
WU_IMAGE* wuImgLoadTIF(const char* pPath, float gamma = WU_DEF_GAMMA);
WU_IMAGE* wuImgLoadAlphaKeyPNG(const char* pPath, float gamma = WU_DEF_GAMMA, uint32_t aclr = 0xFF80FF);
WU_IMAGE* wuImgLoadAlphaKeyGIF(const char* pPath, float gamma = WU_DEF_GAMMA, uint32_t aclr = 0xFF80FF);

struct WU_WAVEOUT;

typedef void (*WU_WAVEOUT_CBFN)(WU_WAVEOUT* pWaveOut, void* pUsrData);

struct WU_WAVEOUT_CB {
	WU_WAVEOUT_CBFN open;
	WU_WAVEOUT_CBFN done;
	WU_WAVEOUT_CBFN close;
};

struct WU_MIDIOUT;

typedef void (*WU_MIDIOUT_CBFN)(WU_MIDIOUT* pMidiOut, void* pUsrData);

struct WU_MIDIOUT_CB {
	WU_MIDIOUT_CBFN open;
	WU_MIDIOUT_CBFN done;
	WU_MIDIOUT_CBFN close;
};

bool wuTextCommandToMCI(const char* pCmd, char* pRet = nullptr, size_t retSize = 0);

int wuGetNumWaveOutDevices();
int wuFindMidiOutFM();
int wuFindMidiOutWT();
int wuFindMidiOutSW();
WU_WAVEOUT* wuWaveOutOpen(int freq = 44100, bool use16Bits = true, bool useStereo = true, WU_WAVEOUT_CB* pCB = nullptr, void* pUsrData = nullptr, int nblk = 1, int devId = -1);
void wuWaveOutClose(WU_WAVEOUT* pWaveOut);
bool wuWaveOutBlockInit(WU_WAVEOUT* pWaveOut, int blkIdx, void* pBuf, size_t bufSize);
bool wuWaveOutBlockReset(WU_WAVEOUT* pWaveOut, int blkIdx);
bool wuWaveOutBlockSend(WU_WAVEOUT* pWaveOut, int blkIdx);
void wuWaveOutSetVolume(WU_WAVEOUT* pWaveOut, float volL, float volR);
int wuWaveOutGetFreq(WU_WAVEOUT* pWaveOut);

int wuGetNumMidiOutDevices();
WU_MIDIOUT* wuMidiOutOpen(WU_MIDIOUT_CB* pCB = nullptr, void* pUsrData = nullptr, int devId = -1);
void wuMidiOutClose(WU_MIDIOUT* pMidiOut);
bool wuMidiOutSend(WU_MIDIOUT* pMidiOut, uint8_t* pMsg, size_t msgLen);
