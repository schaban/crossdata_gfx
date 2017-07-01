#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX
#define _WIN32_WINNT 0x0500
#include <tchar.h>
#include <windows.h>
#include <stdlib.h>
#include <intrin.h>

#include "crossdata.hpp"
#include "gex.hpp"

#include "util.hpp"
#include "test.hpp"

#define TEST_IFC(_tname) static TEST_IFC ifc_##_tname = {_tname##_init, _tname##_loop, _tname##_end }

TEST_IFC(test1);
TEST_IFC(test2);
TEST_IFC(test3);
TEST_IFC(test4);

TEST_IFC(test_obst);
TEST_IFC(test_car);

static TEST_IFC* s_pTest = &ifc_test3;


static void tst_dbg_msg(const char* pMsg) {
	::OutputDebugStringA(pMsg);
}

void cpu_serialize() {
	int dummy[4];
	__cpuid(dummy, 0);
}

int64_t get_timestamp() {
	LARGE_INTEGER ctr;
	cpu_serialize();
	::QueryPerformanceCounter(&ctr);
	cpu_serialize();
	return ctr.QuadPart;
}

void sys_init() {
	sxSysIfc ifc;
	::ZeroMemory(&ifc, sizeof(ifc));
	ifc.fn_dbgmsg = tst_dbg_msg;
	nxSys::init(&ifc);
}

static bool s_consoleFlg = false;

void attach_console() {
	::AllocConsole();
	::AttachConsole(::GetCurrentProcessId());
	FILE* pConFile;
	::freopen_s(&pConFile, "CON", "w", stdout);
	::freopen_s(&pConFile, "CON", "w", stderr);
	::SetWindowPos(::GetConsoleWindow(), nullptr, 1040, 0, 800, 800, 0);
	s_consoleFlg = true;
}

void close_console() {
	if (s_consoleFlg) {
		::FreeConsole();
		s_consoleFlg = false;
	}
}

struct sProgArgs {
	int mTestNo;
	int mTestMode;
	int mMSAA; // 0:Off, 1:Normal, 2:High

	sProgArgs()
	: mTestNo(0), mTestMode(0), mMSAA(1)
	{}

	void parse(const char* pCmd);
};

void sProgArgs::parse(const char* pCmd) {
	if (!pCmd) return;
	size_t len = ::strlen(pCmd);
	char* pBuf = nxCore::str_dup(pCmd);
	char* pNext;
	char name[256];
	char val[256];
	for (char* pTok = ::strtok_s(pBuf, ";", &pNext); pTok; pTok = ::strtok_s(nullptr, ";", &pNext)) {
		::sscanf_s(pTok, "%[_.a-z0-9] = %[ \\:!$-_./a-zA-Z0-9]", name, (uint32_t)sizeof(name), val, (uint32_t)sizeof(val));
		if (nxCore::str_eq(name, "test")) {
			if (nxCore::str_eq(val, "obst")) {
				mTestNo = -1;
			} else if (nxCore::str_eq(val, "car")) {
				mTestNo = -2;
			} else {
				mTestNo = ::atoi(val);
			}
		} else if (nxCore::str_eq(name, "mode")) {
			mTestMode = ::atoi(val);
		} else if (nxCore::str_eq(name, "msaa")) {
			mMSAA = ::atoi(val);
		}
	}
	nxCore::mem_free(pBuf);
}

static sProgArgs s_args;

int get_test_mode() {
	return s_args.mTestMode;
}

static struct sMouseState {
	enum class eBtn {
		LEFT,
		MIDDLE,
		RIGHT,
		X,
		Y
	};

	int32_t mOrgX;
	int32_t mOrgY;
	uint32_t mBtnOld;
	uint32_t mBtnNow;
	int32_t mNowX;
	int32_t mNowY;
	int32_t mOldX;
	int32_t mOldY;
	int32_t mWheel;
	TRACKBALL mTrackball;
	float mWheelVal;

	sMouseState() {
		reset();
		mTrackball.init();
	}

	void reset() {
		::memset(this, 0, sizeof(*this));
		mWheelVal = 0.5f;
	}
	bool ck_now(eBtn id) const { return !!(mBtnNow & (1 << (uint32_t)id)); }
	bool ck_old(eBtn id) const { return !!(mBtnOld & (1 << (uint32_t)id)); }
	bool ck_trg(eBtn id) const { return !!((mBtnNow & (mBtnNow ^ mBtnOld)) & (1 << (uint32_t)id)); }
	bool ck_chg(eBtn id) const { return !!((mBtnNow ^ mBtnOld) & (1 << (uint32_t)id)); }
} s_mouse;

TRACKBALL* get_trackball() {
	return &s_mouse.mTrackball;
}

float get_wheel_val() {
	return s_mouse.mWheelVal;
}

void wnd_msg_func(const GEX_WNDMSG& msg) {
	static UINT mouseMsg[] = {
		WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK,
		WM_MBUTTONDOWN, WM_MBUTTONUP, WM_MBUTTONDBLCLK,
		WM_RBUTTONDOWN, WM_RBUTTONUP, WM_RBUTTONDBLCLK,
		WM_XBUTTONDOWN, WM_XBUTTONUP, WM_XBUTTONDBLCLK,
		WM_MOUSEWHEEL, WM_MOUSEMOVE
	};
	HWND hWnd = (HWND)msg.mHandle;
	sMouseState* pMouse = &s_mouse;
	bool mouseFlg = false;
	for (int i = 0; i < XD_ARY_LEN(mouseMsg); ++i) {
		if (mouseMsg[i] == msg.mCode) {
			mouseFlg = true;
			break;
		}
	}
	if (mouseFlg) {
		int32_t posX = (int32_t)LOWORD(msg.mParams[1]);
		int32_t posY = (int32_t)HIWORD(msg.mParams[1]);
		pMouse->mWheel = 0;
		if (WM_MOUSEWHEEL == msg.mCode) {
			POINT wpt;
			wpt.x = posX;
			wpt.y = posY;
			::ScreenToClient(hWnd, &wpt);
			pMouse->mWheel = (int16_t)HIWORD(msg.mParams[0]);
		}
		uint32_t btnMask = LOWORD(msg.mParams[0]);
		static uint32_t mouseMask[] = {
			MK_LBUTTON, MK_MBUTTON, MK_RBUTTON, MK_XBUTTON1, MK_XBUTTON2
		};
		uint32_t mask = 0;
		for (int i = 0; i < XD_ARY_LEN(mouseMask); ++i) {
			if (btnMask & mouseMask[i]) {
				mask |= 1 << i;
			}
		}
		pMouse->mBtnOld = pMouse->mBtnNow;
		pMouse->mBtnNow = mask;
		pMouse->mOldX = pMouse->mNowX;
		pMouse->mOldY = pMouse->mNowY;
		pMouse->mNowX = posX;
		pMouse->mNowY = posY;

		if (pMouse->ck_now(sMouseState::eBtn::LEFT)) {
			if (!pMouse->ck_old(sMouseState::eBtn::LEFT)) {
				pMouse->mOrgX = pMouse->mOldX;
				pMouse->mOrgY = pMouse->mOldY;
			}
			int dx = pMouse->mNowX - pMouse->mOldX;
			int dy = pMouse->mNowY - pMouse->mOldY;
			if (dx || dy) {
				float sx = 1.0f / (gexGetDispWidth() - 1);
				float sy = 1.0f / (gexGetDispHeight() - 1);
				float x0 = (pMouse->mOldX - pMouse->mOrgX) * sx;
				float y0 = (pMouse->mOrgY - pMouse->mOldY) * sy;
				float x1 = (pMouse->mNowX - pMouse->mOrgX) * sx;
				float y1 = (pMouse->mOrgY - pMouse->mNowY) * sy;
				pMouse->mTrackball.update(x0, y0, x1, y1);
			}
		}

		if (pMouse->mWheel) {
			float dw = 0.01f;
			if (pMouse->mWheel < 0) {
				pMouse->mWheelVal += dw;
			} else {
				pMouse->mWheelVal -= dw;
			}
			pMouse->mWheelVal = nxCalc::saturate(pMouse->mWheelVal);
		}
	}
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
	sys_init();
	attach_console();

	s_args.parse(pCmdLine);

	TEST_IFC* pTest = &ifc_test1;
	int w = 1024;
	int h = 768;
	int smSize = 1024;
	switch (s_args.mTestNo) {
		case 2:
			pTest = &ifc_test2;
			w = 1600;
			h = 900;
			break;
		case 3:
			pTest = &ifc_test3;
			//w = 1600;
			//h = 900;
			smSize = 1024 * 2;
			break;
		case 4:
			pTest = &ifc_test4;
			//w = 1600;
			//h = 900;
			smSize = 1024 * 4;
			break;
		case -1:
			pTest = &ifc_test_obst;
			break;
		case -2:
			w = 1600;
			h = 900;
			pTest = &ifc_test_car;
			smSize = 1024 * 4;
			break;
	}
	s_pTest = pTest;

	GEX_CONFIG cfg;
	::memset(&cfg, 0, sizeof(cfg));
	cfg.mSysHandle = hInstance;
	cfg.mShadowMapSize = smSize;
	cfg.mMSAA = s_args.mMSAA;
	cfg.mWidth = w;
	cfg.mHeight = h;
	cfg.mpMsgFunc = wnd_msg_func;

	gexInit(cfg);
	util_init();

	s_pTest->pInit();
	gexLoop(s_pTest->pLoop);
	s_pTest->pEnd();

	util_reset();
	gexReset();

	close_console();

	nxCore::mem_dbg();//////////////////////
	return 0;
}
