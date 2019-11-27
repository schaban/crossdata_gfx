/*
 * OpenGL system interface
 * Author: Sergey Chaban <sergey.chaban@gmail.com>
 *
 * Copyright 2019 Sergey Chaban
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "oglsys.hpp"

#if defined(OGLSYS_ANDROID)
#	include <android/window.h>
#elif defined(OGLSYS_X11)
#	include <unistd.h>
#	include <X11/Xlib.h>
#	include <X11/Xutil.h>
#endif


#if !OGLSYS_ES
#	if defined(OGLSYS_WINDOWS)
#		define GET_GL_FN(_name) *(PROC*)&_name = (PROC)get_func_ptr(#_name)
#		define GET_WGL_FN(_name) *(PROC*)&mWGL._name = (PROC)get_func_ptr(#_name)
#		define OGL_FN(_type, _name) PFNGL##_type##PROC gl##_name;
#		undef OGL_FN_CORE
#		define OGL_FN_CORE
#		undef OGL_FN_EXTRA
#		define OGL_FN_EXTRA
#		include "oglsys.inc"
#		undef OGL_FN
#	endif
#endif

enum KBD_PUNCT {
	KBD_PUNCT_SPACE = 0,
	KBD_PUNCT_PLUS,
	KBD_PUNCT_MINUS,
	KBD_PUNCT_COMMA,
	KBD_PUNCT_PERIOD,
	KBD_PUNCT_SLASH,
	KBD_PUNCT_COLON,
	_KBD_PUNCT_NUM_
};

static struct KBD_PUNCT_XLAT {
	int code, idx;
} s_kbdPunctTbl[] = {
	{ ' ', KBD_PUNCT_SPACE },
	{ '+', KBD_PUNCT_PLUS },
	{ '-', KBD_PUNCT_PLUS },
	{ ',', KBD_PUNCT_COMMA },
	{ '.', KBD_PUNCT_PERIOD },
};

enum KBD_CTRL {
	KBD_CTRL_UP = 0,
	KBD_CTRL_DOWN,
	KBD_CTRL_LEFT,
	KBD_CTRL_RIGHT,
	KBD_CTRL_TAB,
	KBD_CTRL_BACK,
	KBD_CTRL_LSHIFT,
	KBD_CTRL_LCTRL,
	KBD_CTRL_RSHIFT,
	KBD_CTRL_RCTRL,
	_KBD_CTRL_NUM_
};

struct KBD_STATE {
	bool digit[10];
	bool alpha['z' - 'a' + 1];
	bool func[12];
	bool punct[_KBD_PUNCT_NUM_];
	bool ctrl[_KBD_CTRL_NUM_];
};

static struct OGLSysGlb {
#if defined(OGLSYS_WINDOWS)
	HINSTANCE mhInstance;
	ATOM mClassAtom;
	HWND mhWnd;
	HDC mhDC;
#elif defined(OGLSYS_ANDROID)
	ANativeWindow* mpNativeWnd;
#elif defined(OGLSYS_X11)
	Display* mpXDisplay;
	Window mXWnd;
#elif defined(OGLSYS_VIVANTE_FB)
	EGLNativeDisplayType mVivDisp;
	EGLNativeWindowType mVivWnd;
#endif

	uint64_t mFrameCnt;

	OGLSysIfc mIfc;
	int mWndOrgX;
	int mWndOrgY;
	int mWndW;
	int mWndH;
	int mWidth;
	int mHeight;

	bool mReduceRes;
	bool mHideSysIcons;
	bool mWithoutCtx;

	GLint mDefFBO;
	GLint mMaxTexSize;

	struct Exts {
#if OGLSYS_ES
		PFNGLGENQUERIESEXTPROC pfnGenQueries;
		PFNGLDELETEQUERIESEXTPROC pfnDeleteQueries;
		PFNGLQUERYCOUNTEREXTPROC pfnQueryCounter;
		PFNGLGETQUERYOBJECTIVEXTPROC pfnGetQueryObjectiv;
		PFNGLGETQUERYOBJECTUIVEXTPROC pfnGetQueryObjectuiv;
		PFNGLGETQUERYOBJECTUI64VEXTPROC pfnGetQueryObjectui64v;
		PFNGLGETPROGRAMBINARYOESPROC pfnGetProgramBinary;
		PFNGLDISCARDFRAMEBUFFEREXTPROC pfnDiscardFramebuffer;
		PFNGLGENVERTEXARRAYSOESPROC pfnGenVertexArrays;
		PFNGLDELETEVERTEXARRAYSOESPROC pfnDeleteVertexArrays;
		PFNGLBINDVERTEXARRAYOESPROC pfnBindVertexArray;
	#ifdef OGLSYS_VIVANTE_FB
		void* pfnDrawElementsBaseVertex;
	#else
		PFNGLDRAWELEMENTSBASEVERTEXOESPROC pfnDrawElementsBaseVertex;
	#endif
#endif
		bool bindlessTex;
		bool ASTC_LDR;
		bool S3TC;
		bool DXT1;
		bool disjointTimer;
		bool derivs;
		bool vtxHalf;
		bool idxUInt;
		bool progBin;
		bool discardFB;
	} mExts;

	OGLSys::InputHandler mInpHandler;
	void* mpInpHandlerWk;
	float mInpSclX;
	float mInpSclY;

	OGLSysMouseState mMouseState;

	KBD_STATE mKbdState;

	void* mem_alloc(size_t size) {
		return mIfc.mem_alloc ? mIfc.mem_alloc(size) : malloc(size);
	}

	void mem_free(void* p) {
		if (mIfc.mem_free) {
			mIfc.mem_free(p);
		} else {
			free(p);
		}
	}

	void dbg_msg(const char* pFmt, ...) {
		if (mIfc.dbg_msg) {
			char buf[1024];
			va_list lst;
			va_start(lst, pFmt);
#ifdef _MSC_VER
			vsprintf_s(buf, sizeof(buf), pFmt, lst);
#else
			vsprintf(buf, pFmt, lst);
#endif
			mIfc.dbg_msg("%s", buf);
			va_end(lst);
		}
	}

	const char* load_glsl(const char* pPath, size_t* pSize) {
		if (mIfc.load_glsl) {
			return mIfc.load_glsl(pPath, pSize);
		} else {
			const char* pCode = nullptr;
			size_t size = 0;
			FILE* pFile = nullptr;
			const char* pMode = "rb";
#if defined(_MSC_VER)
			fopen_s(&pFile, pPath, pMode);
#else
			pFile = fopen(pPath, pMode);
#endif
			if (pFile) {
				if (fseek(pFile, 0, SEEK_END) == 0) {
					size = ftell(pFile);
				}
				fseek(pFile, 0, SEEK_SET);
				if (size) {
					pCode = (const char*)mem_alloc(size);
					if (pCode) {
						fread((void*)pCode, 1, size, pFile);
					}
				}
				fclose(pFile);
			}
			if (pSize) {
				*pSize = size;
			}
			return pCode;
		}
	}

	void unload_glsl(const char* pCode) {
		if (mIfc.unload_glsl) {
			mIfc.unload_glsl(pCode);
		} else {
			mem_free((void*)pCode);
		}
	}

#if OGLSYS_ES
	struct EGL {
		EGLDisplay display;
		EGLSurface surface;
		EGLContext context;
		EGLConfig config;

		void reset() {
			display = EGL_NO_DISPLAY;
			surface = EGL_NO_SURFACE;
			context = EGL_NO_CONTEXT;
		}
	} mEGL;
#elif defined(OGLSYS_WINDOWS)
	struct WGL {
		HMODULE hLib;
		PROC(APIENTRY *wglGetProcAddress)(LPCSTR);
		HGLRC(APIENTRY *wglCreateContext)(HDC);
		BOOL(APIENTRY *wglDeleteContext)(HGLRC);
		BOOL(APIENTRY *wglMakeCurrent)(HDC, HGLRC);
		HGLRC(WINAPI *wglCreateContextAttribsARB)(HDC, HGLRC, const int*);
		const char* (APIENTRY *wglGetExtensionsStringEXT)();
		BOOL(APIENTRY *wglSwapIntervalEXT)(int);
		int (APIENTRY *wglGetSwapIntervalEXT)();
		HGLRC hCtx;
		HGLRC hExtCtx;
		bool ok;

		void init_ctx(HDC hDC) {
			if (hDC && wglCreateContext && wglDeleteContext && wglMakeCurrent) {
				PIXELFORMATDESCRIPTOR fmt;
				ZeroMemory(&fmt, sizeof(fmt));
				fmt.nVersion = 1;
				fmt.nSize = sizeof(fmt);
				fmt.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_GENERIC_ACCELERATED | PFD_DOUBLEBUFFER;
				fmt.iPixelType = PFD_TYPE_RGBA;
				fmt.cColorBits = 32;
				fmt.cDepthBits = 24;
				fmt.cStencilBits = 8;
				int ifmt = ChoosePixelFormat(hDC, &fmt);
				if (0 == ifmt) {
					return;
				}
				if (!SetPixelFormat(hDC, ifmt, &fmt)) {
					return;
				}
				hCtx = wglCreateContext(hDC);
				if (hCtx) {
					BOOL flg = wglMakeCurrent(hDC, hCtx);
					if (!flg) {
						wglDeleteContext(hCtx);
						hCtx = nullptr;
					}
				}
			}
		}

		void init_ext_ctx(HDC hDC) {
			if (hDC && hCtx && wglCreateContextAttribsARB) {
				int attribsCompat[] = {
					WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
					WGL_CONTEXT_MINOR_VERSION_ARB, 0,
					WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
					0
				};
				hExtCtx = wglCreateContextAttribsARB(hDC, hCtx, attribsCompat);
				if (!hExtCtx) {
					int attribsCore[] = {
						WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
						WGL_CONTEXT_MINOR_VERSION_ARB, 0,
						WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
						0
					};
					hExtCtx = wglCreateContextAttribsARB(hDC, hCtx, attribsCore);
				}
				if (hExtCtx) {
					BOOL flg = wglMakeCurrent(hDC, hExtCtx);
					if (!flg) {
						wglDeleteContext(hExtCtx);
						hExtCtx = nullptr;
						wglMakeCurrent(hDC, hCtx);
					}
				}
			}
		}

		void reset_ctx() {
			if (wglDeleteContext) {
				if (hExtCtx) {
					wglDeleteContext(hExtCtx);
					hExtCtx = nullptr;
				}
				if (hCtx) {
					wglDeleteContext(hCtx);
					hCtx = nullptr;
				}
			}
		}

		bool valid_ctx() const { return (!!hCtx); }

		void set_swap_interval(int i) {
			if (wglSwapIntervalEXT) {
				wglSwapIntervalEXT(i);
			}
		}
	} mWGL;
#endif

#if OGLSYS_ES
	bool valid_display() const { return mEGL.display != EGL_NO_DISPLAY; }
	bool valid_surface() const { return mEGL.surface != EGL_NO_SURFACE; }
	bool valid_context() const { return mEGL.context != EGL_NO_CONTEXT; }
	bool valid_ogl() const { return valid_display() && valid_surface() && valid_context(); }
#elif defined(OGLSYS_WINDOWS)
	bool valid_ogl() const { return mWGL.ok; }

	void* get_func_ptr(const char* pName) {
		void* p = (void*)(mWGL.wglGetProcAddress ? mWGL.wglGetProcAddress(pName) : nullptr);
		if (!p) {
			if (mWGL.hLib) {
				p = (void*)GetProcAddress(mWGL.hLib, pName);
			}
		}
		return p;
	}
#elif defined(OGLSYS_APPLE)
	bool valid_ogl() const { return true; }
#endif

	void stop_ogl() {
#if OGLSYS_ES
		if (valid_display()) {
			eglMakeCurrent(mEGL.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			eglTerminate(mEGL.display);
		}
#elif defined(OGLSYS_WINDOWS)
		if (valid_ogl()) {
			mWGL.wglMakeCurrent(mhDC, nullptr);
		}
#endif
	}

	void init_wnd();
	void init_ogl();
	void reset_wnd();
	void reset_ogl();

	void handle_ogl_ext(const GLubyte* pStr, const int lenStr);

	void update_mouse_pos(const float x, const float y) {
		mMouseState.mOldX = mMouseState.mNowX;
		mMouseState.mOldY = mMouseState.mNowY;
		mMouseState.mNowX = x * mInpSclX;
		mMouseState.mNowY = y * mInpSclY;
	}

	void update_mouse_pos(const int x, const int y) {
		update_mouse_pos(float(x), float(y));
	}

	void set_mouse_pos(const float x, const float y) {
		mMouseState.mNowX = x * mInpSclX;
		mMouseState.mNowY = y * mInpSclY;
		mMouseState.mOldX = mMouseState.mNowX;
		mMouseState.mOldY = mMouseState.mNowY;
	}

	void set_mouse_pos(const int x, const int y) {
		set_mouse_pos(float(x), float(y));
	}

	void send_input(OGLSysInput* pInp) {
		if (pInp && mInpHandler) {
			if (pInp->act != OGLSysInput::ACT_NONE) {
				if (pInp->pressure > 1.0f) {
					pInp->pressure = 1.0f;
				}
				pInp->relX = pInp->absX * mInpSclX;
				pInp->relY = pInp->absY * mInpSclY;
				mInpHandler(*pInp, mpInpHandlerWk);
			}
		}
	}

} GLG;

#if defined(OGLSYS_WINDOWS)
static bool wnd_mouse_msg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static UINT mouseMsg[] = {
		WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK,
		WM_MBUTTONDOWN, WM_MBUTTONUP, WM_MBUTTONDBLCLK,
		WM_RBUTTONDOWN, WM_RBUTTONUP, WM_RBUTTONDBLCLK,
		WM_XBUTTONDOWN, WM_XBUTTONUP, WM_XBUTTONDBLCLK,
		WM_MOUSEWHEEL, WM_MOUSEMOVE
	};
	bool mouseFlg = false;
	for (size_t i = 0; i < sizeof(mouseMsg) / sizeof(mouseMsg[0]); ++i) {
		if (mouseMsg[i] == msg) {
			mouseFlg = true;
			break;
		}
	}
	if (mouseFlg) {
		static uint32_t mouseMask[] = {
			MK_LBUTTON, MK_MBUTTON, MK_RBUTTON, MK_XBUTTON1, MK_XBUTTON2
		};
		uint32_t mask = 0;
		uint32_t btnMask = LOWORD(wParam);
		for (size_t i = 0; i < sizeof(mouseMask) / sizeof(mouseMask[0]); ++i) {
			if (btnMask & mouseMask[i]) {
				mask |= 1 << i;
			}
		}
		GLG.mMouseState.mBtnOld = GLG.mMouseState.mBtnNow;
		GLG.mMouseState.mBtnNow = mask;

		int32_t posX = (int32_t)LOWORD(lParam);
		int32_t posY = (int32_t)HIWORD(lParam);
		int32_t wheel = 0;
		if (msg == WM_MOUSEWHEEL) {
			wheel = (int16_t)HIWORD(wParam);
		}
		GLG.update_mouse_pos(posX, posY);
		GLG.mMouseState.mWheel = wheel;

		OGLSysInput inp;
		inp.device = OGLSysInput::MOUSE;
		inp.sysDevId = 0;
		inp.pressure = 1.0f;
		inp.act = OGLSysInput::ACT_NONE;
		inp.id = -1;
		switch (msg) {
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_LBUTTONDBLCLK:
				inp.id = 0;
				break;
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			case WM_MBUTTONDBLCLK:
				inp.id = 1;
				break;
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_RBUTTONDBLCLK:
				inp.id = 2;
				break;
			case WM_XBUTTONDOWN:
			case WM_XBUTTONUP:
			case WM_XBUTTONDBLCLK:
				inp.id = 3;
				break;
		}
		inp.absX = float(LOWORD(lParam));
		inp.absY = float(HIWORD(lParam));
		switch (msg) {
			case WM_MOUSEMOVE:
				if (GLG.mMouseState.ck_now(OGLSysMouseState::BTN_LEFT)) {
					inp.id = 0;
					inp.act = OGLSysInput::ACT_DRAG;
				} else {
					inp.act = OGLSysInput::ACT_HOVER;
				}
				break;
			case WM_LBUTTONDOWN:
			case WM_MBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_XBUTTONDOWN:
				inp.act = OGLSysInput::ACT_DOWN;
				break;
			case WM_LBUTTONUP:
			case WM_MBUTTONUP:
			case WM_RBUTTONUP:
			case WM_XBUTTONUP:
				inp.act = OGLSysInput::ACT_UP;
				break;
		}
		GLG.send_input(&inp);
	}
	return mouseFlg;
}

static bool wnd_kbd_msg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	bool res = false;
	if (msg == WM_KEYDOWN || msg == WM_KEYUP) {
		bool* pState = nullptr;
		int idx = -1;
		bool repFlg = msg == WM_KEYDOWN ? !!((lParam >> 30) & 1) : false;
		if (!repFlg) {
			int code = int(wParam);
			if (code >= '0' && code <= '9') {
				pState = GLG.mKbdState.digit;
				idx = code - '0';
			} else if (code >= 'A' && code <= 'Z') {
				pState = GLG.mKbdState.alpha;
				idx = code - 'A';
			} else if (code >= VK_F1 && code <= VK_F12) {
				pState = GLG.mKbdState.func;
				idx = code - VK_F1;
			} else {
				int scan = (lParam >> 16) & 0xFF;
				bool extFlg = !!((lParam >> 24) & 1);
				if (code == VK_SHIFT) {
					code = scan == 0x2A ? VK_LSHIFT : VK_RSHIFT;
				} else if (code == VK_CONTROL) {
					code = extFlg ? VK_RCONTROL : VK_LCONTROL;
				}
				switch (code) {
					case VK_SPACE:
						pState = GLG.mKbdState.punct;
						idx = KBD_PUNCT_SPACE;
						break;
					case VK_OEM_PLUS:
						pState = GLG.mKbdState.punct;
						idx = KBD_PUNCT_PLUS;
						break;
					case  VK_OEM_MINUS:
						pState = GLG.mKbdState.punct;
						idx = KBD_PUNCT_MINUS;
						break;
					case VK_OEM_COMMA:
						pState = GLG.mKbdState.punct;
						idx = KBD_PUNCT_COMMA;
						break;
					case VK_OEM_PERIOD:
						pState = GLG.mKbdState.punct;
						idx = KBD_PUNCT_PERIOD;
						break;
					case VK_UP:
						pState = GLG.mKbdState.ctrl;
						idx = KBD_CTRL_UP;
						break;
					case VK_DOWN:
						pState = GLG.mKbdState.ctrl;
						idx = KBD_CTRL_DOWN;
						break;
					case VK_LEFT:
						pState = GLG.mKbdState.ctrl;
						idx = KBD_CTRL_LEFT;
						break;
					case VK_RIGHT:
						pState = GLG.mKbdState.ctrl;
						idx = KBD_CTRL_RIGHT;
						break;
					case VK_TAB:
						pState = GLG.mKbdState.ctrl;
						idx = KBD_CTRL_TAB;
						break;
					case VK_BACK:
						pState = GLG.mKbdState.ctrl;
						idx = KBD_CTRL_BACK;
						break;
					case VK_LSHIFT:
						pState = GLG.mKbdState.ctrl;
						idx = KBD_CTRL_LSHIFT;
						break;
					case VK_LCONTROL:
						pState = GLG.mKbdState.ctrl;
						idx = KBD_CTRL_LCTRL;
						break;
					case VK_RSHIFT:
						pState = GLG.mKbdState.ctrl;
						idx = KBD_CTRL_RSHIFT;
						break;
					case VK_RCONTROL:
						pState = GLG.mKbdState.ctrl;
						idx = KBD_CTRL_RCTRL;
						break;
				}
			}
		}
		if (pState && idx >= 0) {
			pState[idx] = (msg == WM_KEYDOWN);
			res = true;
		}
	}
	return res;
}

static LRESULT CALLBACK wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	LRESULT res = 0;
	bool mouseFlg = wnd_mouse_msg(hWnd, msg, wParam, lParam);
	bool kbdFlg = wnd_kbd_msg(hWnd, msg, wParam, lParam);
	if (!mouseFlg && !kbdFlg) {
		switch (msg) {
			case WM_DESTROY:
				PostQuitMessage(0);
				break;
			case WM_SYSCOMMAND:
				if (!(wParam == SC_SCREENSAVE || wParam == SC_MONITORPOWER)) {
					res = DefWindowProc(hWnd, msg, wParam, lParam);
				}
				break;
			default:
				res = DefWindowProc(hWnd, msg, wParam, lParam);
				break;
		}
	}
	return res;
}

static const TCHAR* s_oglClassName = OGLSYS_ES ? _T("SysGLES") : _T("SysOGL");
#elif defined(OGLSYS_ANDROID)

static ANativeWindow* s_pNativeWnd = nullptr;

static void app_cmd(android_app* pApp, int32_t cmd) {
	switch (cmd) {
		case APP_CMD_INIT_WINDOW:
			s_pNativeWnd = pApp->window;
			break;
		case APP_CMD_TERM_WINDOW:
			break;
		case APP_CMD_STOP:
			ANativeActivity_finish(pApp->activity);
			break;
		default:
			break;
	}
}
#elif defined(OGLSYS_X11)
static int wait_MapNotify(Display* pDisp, XEvent* pEvt, char* pArg) {
	return ((pEvt->type == MapNotify) && (pEvt->xmap.window == (Window)pArg)) ? 1 : 0;
}
#endif

void OGLSysGlb::init_wnd() {
#if defined(OGLSYS_WINDOWS)
	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(WNDCLASSEX));
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_VREDRAW | CS_HREDRAW;
	wc.hInstance = mhInstance;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = s_oglClassName;
	wc.lpfnWndProc = wnd_proc;
	wc.cbWndExtra = 0x10;
	mClassAtom = RegisterClassEx(&wc);

	RECT rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = mWidth;
	rect.bottom = mHeight;
	int style = WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_GROUP;
	AdjustWindowRect(&rect, style, FALSE);
	mWndW = rect.right - rect.left;
	mWndH = rect.bottom - rect.top;
	char tmpTitle[100];
#if defined(_MSC_VER)
	sprintf_s(tmpTitle, sizeof(tmpTitle),
#else
	sprintf(tmpTitle,
#endif
		"%s: build %s", mWithoutCtx ? "SysWnd" : (OGLSYS_ES ? "OGL(ES)" : "OGL"), __DATE__);
	size_t tlen = ::strlen(tmpTitle);
	TCHAR title[100];
	ZeroMemory(title, sizeof(title));
	for (size_t i = 0; i < tlen; ++i) {
		title[i] = (TCHAR)tmpTitle[i];
	}
	mhWnd = CreateWindowEx(0, s_oglClassName, title, style, mWndOrgX, mWndOrgY, mWndW, mWndH, NULL, NULL, mhInstance, NULL);
	if (mhWnd) {
		ShowWindow(mhWnd, SW_SHOW);
		UpdateWindow(mhWnd);
		mhDC = GetDC(mhWnd);
	}
#elif defined(OGLSYS_ANDROID)
	android_app* pApp = oglsys_get_app();
	if (!pApp) return;

	pApp->onAppCmd = app_cmd;

	while (true) {
		int fd;
		int events;
		android_poll_source* pPollSrc;
		while (true) {
			int id = ALooper_pollAll(0, &fd, &events, (void**)&pPollSrc);
			if (id < 0) break;
			if (pPollSrc) {
				pPollSrc->process(pApp, pPollSrc);
			}
			if (pApp->destroyRequested) return;
			if (s_pNativeWnd) break;
		}
		if (s_pNativeWnd) break;
	}

	mpNativeWnd = s_pNativeWnd;
	s_pNativeWnd = nullptr;

	if (mHideSysIcons) {
		ANativeActivity_setWindowFlags(pApp->activity, AWINDOW_FLAG_FULLSCREEN, 0);
	}

	mWidth = ANativeWindow_getWidth(mpNativeWnd);
	mHeight = ANativeWindow_getHeight(mpNativeWnd);
	mWndW = mWidth;
	mWndH = mHeight;
	mInpSclX = mWidth > 0.0f ? 1.0f / float(mWidth) : 1.0f;
	mInpSclY = mHeight > 0.0f ? 1.0f / float(mHeight) : 1.0f;
	dbg_msg("Full resolution: %d x %d\n", mWidth, mHeight);
	if (mReduceRes) {
		float rscl = (mHeight > 720 ? 720.0f : 360.0f) / float(mHeight);
		if (rscl < 1.0f) {
			mWidth = int(float(mWidth) * rscl);
			mHeight = int(float(mHeight) * rscl);
			ANativeWindow_setBuffersGeometry(mpNativeWnd, mWidth, mHeight, ANativeWindow_getFormat(mpNativeWnd));
			dbg_msg("Using reduced resolution: %d x %d\n", mWidth, mHeight);
		}
	}
#elif defined(OGLSYS_X11)
	mpXDisplay = XOpenDisplay(0);
	if (!mpXDisplay) {
		return;
	}
	int defScr = XDefaultScreen(mpXDisplay);
	int defDepth = DefaultDepth(mpXDisplay, defScr);
	XVisualInfo vi;
	XMatchVisualInfo(mpXDisplay, defScr, defDepth, TrueColor, &vi);
	Window rootWnd = RootWindow(mpXDisplay, defScr);
	XSetWindowAttributes wattrs;
	wattrs.colormap = XCreateColormap(mpXDisplay, rootWnd, vi.visual, AllocNone);
	wattrs.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask | KeyPressMask;
	mWndW = mWidth;
	mWndH = mHeight;
	mXWnd = XCreateWindow(mpXDisplay, rootWnd, 0, 0, mWndW, mWndH, 0, vi.depth, InputOutput, vi.visual, CWEventMask | CWColormap, &wattrs);
	char title[100];
	sprintf(title, mWithoutCtx ? "X11SysWnd" : "X11 %s: build %s", OGLSYS_ES ? "OGL(ES)" : "OGL", __DATE__);
	XMapWindow(mpXDisplay, mXWnd);
	XStoreName(mpXDisplay, mXWnd, title);
	Atom del = XInternAtom(mpXDisplay, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(mpXDisplay, mXWnd, &del, 1);
	XEvent evt;
	XIfEvent(mpXDisplay, &evt, wait_MapNotify, (char*)mXWnd);
	XSelectInput(mpXDisplay, mXWnd, ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask | PointerMotionMask);
#elif defined(OGLSYS_VIVANTE_FB)
	mVivDisp = fbGetDisplayByIndex(0);
	int vivW = 0;
	int vivH = 0;
	fbGetDisplayGeometry(mVivDisp, &vivW, &vivH);
	dbg_msg("Vivante display: %d x %d\n", vivW, vivH);
	int vivX = 0;
	int vivY = 0;
	if (mReduceRes) {
		const float vivReduce = 0.5f;
		int fullVivW = vivW;
		int fullVivH = vivH;
		vivW = int(float(fullVivW) * vivReduce);
		vivH = int(float(fullVivH) * vivReduce);
		vivX = (fullVivW - vivW) / 2;
		vivY = (fullVivH - vivH) / 2;
		dbg_msg("Reduced resolution: %d x %d\n", vivW, vivH);
	}
	mWidth = vivW;
	mHeight = vivH;
	mWndW = mWidth;
	mWndH = mHeight;
	mVivWnd = fbCreateWindow(mVivDisp, vivX, vivY, mWidth, mHeight);
#endif

#if !defined(OGLSYS_ANDROID)
	mInpSclX = mWidth > 0.0f ? 1.0f / float(mWidth) : 1.0f;
	mInpSclY = mHeight > 0.0f ? 1.0f / float(mHeight) : 1.0f;
#endif
}

void OGLSysGlb::reset_wnd() {
#if defined(OGLSYS_WINDOWS)
	if (mhDC) {
		ReleaseDC(mhWnd, mhDC);
		mhDC = NULL;
	}
	UnregisterClass(s_oglClassName, mhInstance);
#elif defined(OGLSYS_X11)
	if (mpXDisplay) {
		XDestroyWindow(mpXDisplay, mXWnd);
		XCloseDisplay(mpXDisplay);
		mpXDisplay = nullptr;
	}
#elif defined(OGLSYS_VIVANTE_FB)
	fbDestroyWindow(mVivWnd);
	fbDestroyDisplay(mVivDisp);
#endif
}

#if defined(OGLSYS_VIVANTE_FB)
#	define OGLSYS_ES_ALPHA_SIZE 0
#	define OGLSYS_ES_DEPTH_SIZE 16
#else
#	define OGLSYS_ES_ALPHA_SIZE 8
#	define OGLSYS_ES_DEPTH_SIZE 24

#endif

void OGLSysGlb::init_ogl() {
	if (mWithoutCtx) {
		return;
	}
#if OGLSYS_ES
#	if defined(OGLSYS_ANDROID)
	mEGL.display = eglGetDisplay((EGLNativeDisplayType)0);
#	elif defined(OGLSYS_WINDOWS)
	if (mhDC) {
		mEGL.display = eglGetDisplay(mhDC);
	}
#	elif defined(OGLSYS_X11)
	if (mpXDisplay) {
		mEGL.display = eglGetDisplay((EGLNativeDisplayType)mpXDisplay);
	}
#	elif defined(OGLSYS_VIVANTE_FB)
	mEGL.display = eglGetDisplay(mVivDisp);
#	endif
	if (!valid_display()) return;
	int verMaj = 0;
	int verMin = 0;
	bool flg = !!eglInitialize(mEGL.display, &verMaj, &verMin);
	if (!flg) return;
	dbg_msg("EGL %d.%d\n", verMaj, verMin);
	flg = !!eglBindAPI(EGL_OPENGL_ES_API);
	if (flg != EGL_TRUE) return;

	static EGLint cfgAttrs[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, OGLSYS_ES_ALPHA_SIZE,
		EGL_DEPTH_SIZE, OGLSYS_ES_DEPTH_SIZE,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
		EGL_NONE
	};
	bool useGLES2 = false;
	EGLint ncfg = 0;
	flg = !!eglChooseConfig(mEGL.display, cfgAttrs, &mEGL.config, 1, &ncfg);
	if (!flg) {
		static EGLint cfgAttrs2[] = {
			EGL_RED_SIZE, 8,
			EGL_GREEN_SIZE, 8,
			EGL_BLUE_SIZE, 8,
			EGL_ALPHA_SIZE, OGLSYS_ES_ALPHA_SIZE,
			EGL_DEPTH_SIZE, OGLSYS_ES_DEPTH_SIZE,
			EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
			EGL_NONE
		};
		useGLES2 = true;
		flg = !!eglChooseConfig(mEGL.display, cfgAttrs2, &mEGL.config, 1, &ncfg);
	}
	if (flg) flg = ncfg == 1;
	if (!flg) return;
	EGLNativeWindowType hwnd =
#if defined(OGLSYS_ANDROID)
		mpNativeWnd
#elif defined(OGLSYS_WINDOWS)
		mhWnd
#elif defined(OGLSYS_X11)
		(EGLNativeWindowType)mXWnd
#elif defined(OGLSYS_VIVANTE_FB)
		mVivWnd;
#else
		(EGLNativeWindowType)0
#endif
		;
	mEGL.surface = eglCreateWindowSurface(mEGL.display, mEGL.config, hwnd, nullptr);
	if (!valid_surface()) return;

	static EGLint ctxAttrs[] = {
		EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
		EGL_CONTEXT_MINOR_VERSION_KHR, 1,
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};
	static EGLint ctxAttrs2[] = {
			EGL_CONTEXT_CLIENT_VERSION, 2,
			EGL_NONE
	};
	mEGL.context = eglCreateContext(mEGL.display, mEGL.config, nullptr, useGLES2 ? ctxAttrs2 : ctxAttrs);
	if (!valid_context() && !useGLES2) {
		static EGLint ctxAttrs3[] = {
			EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
			EGL_CONTEXT_MINOR_VERSION_KHR, 0,
			EGL_CONTEXT_CLIENT_VERSION, 3,
			EGL_NONE
		};
		mEGL.context = eglCreateContext(mEGL.display, mEGL.config, nullptr, ctxAttrs3);
	}
	if (!valid_context()) return;
	eglMakeCurrent(mEGL.display, mEGL.surface, mEGL.surface, mEGL.context);
	eglSwapInterval(mEGL.display, 1);
#elif defined(OGLSYS_WINDOWS)
	mWGL.ok = false;
	mWGL.hLib = LoadLibraryW(L"opengl32.dll");
	if (!mWGL.hLib) return;
	GET_WGL_FN(wglGetProcAddress);
	if (!mWGL.wglGetProcAddress) return;
	GET_WGL_FN(wglCreateContext);
	if (!mWGL.wglCreateContext) return;
	GET_WGL_FN(wglDeleteContext);
	if (!mWGL.wglDeleteContext) return;
	GET_WGL_FN(wglMakeCurrent);
	if (!mWGL.wglMakeCurrent) return;

	mWGL.init_ctx(mhDC);
	if (!mWGL.valid_ctx()) return;

	GET_WGL_FN(wglCreateContextAttribsARB);

	GET_WGL_FN(wglGetExtensionsStringEXT);
	GET_WGL_FN(wglSwapIntervalEXT);
	GET_WGL_FN(wglGetSwapIntervalEXT);

	mWGL.init_ext_ctx(mhDC);

#	define OGL_FN(_type, _name) GET_GL_FN(gl##_name);
#	undef OGL_FN_CORE
#	define OGL_FN_CORE
#	undef OGL_FN_EXTRA
#	define OGL_FN_EXTRA
#	include "oglsys.inc"
#	undef OGL_FN

	int allCnt = 0;
	int okCnt = 0;
#	define OGL_FN(_type, _name) if (gl##_name) { ++okCnt; } else { dbg_msg("missing OGL core function #%d: %s\n", allCnt, "gl"#_name); } ++allCnt;
#	undef OGL_FN_CORE
#	define OGL_FN_CORE
#	undef OGL_FN_EXTRA
#	include "oglsys.inc"
#	undef OGL_FN
	dbg_msg("OGL core functions: %d/%d\n", okCnt, allCnt);
	if (allCnt != okCnt) {
		mWGL.reset_ctx();
		return;
	}

	allCnt = 0;
	okCnt = 0;
#	define OGL_FN(_type, _name) if (gl##_name) { ++okCnt; } else { dbg_msg("missing OGL extra function #%d: %s\n", allCnt, "gl"#_name); } ++allCnt;
#	undef OGL_FN_CORE
#	undef OGL_FN_EXTRA
#	define OGL_FN_EXTRA
#	include "oglsys.inc"
#	undef OGL_FN
	dbg_msg("OGL extra functions: %d/%d\n", okCnt, allCnt);

	mWGL.set_swap_interval(1);
	mWGL.ok = true;
#endif

	dbg_msg("OpenGL version: %s\n", glGetString(GL_VERSION));
	dbg_msg("%s\n", glGetString(GL_VENDOR));
	dbg_msg("%s\n\n", glGetString(GL_RENDERER));

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mDefFBO);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mMaxTexSize);

	const GLubyte* pExts = glGetString(GL_EXTENSIONS);
	if (pExts) {
		int iext = 0;
		int iwk = 0;
		bool extsDone = false;
		while (!extsDone) {
			GLubyte extc = pExts[iwk];
			extsDone = extc == 0;
			if (extsDone || extc == ' ') {
				int extLen = iwk - iext;
				const GLubyte* pExtStr = &pExts[iext];
				iext = iwk + 1;
				if (extLen > 1) {
					handle_ogl_ext(pExtStr, extLen);
				}
			}
			++iwk;
		}
	}

#if OGLSYS_ES
	mExts.pfnGenQueries = (PFNGLGENQUERIESEXTPROC)eglGetProcAddress("glGenQueriesEXT");
	mExts.pfnDeleteQueries = (PFNGLDELETEQUERIESEXTPROC)eglGetProcAddress("glDeleteQueriesEXT");
	mExts.pfnQueryCounter = (PFNGLQUERYCOUNTEREXTPROC)eglGetProcAddress("glQueryCounterEXT");
	mExts.pfnGetQueryObjectiv = (PFNGLGETQUERYOBJECTIVEXTPROC)eglGetProcAddress("glGetQueryObjectivEXT");
	mExts.pfnGetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVEXTPROC)eglGetProcAddress("glGetQueryObjectuivEXT");
	mExts.pfnGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VEXTPROC)eglGetProcAddress("glGetQueryObjectui64vEXT");
	mExts.pfnGetProgramBinary = (PFNGLGETPROGRAMBINARYOESPROC)eglGetProcAddress("glGetProgramBinaryOES");
	mExts.pfnDiscardFramebuffer = (PFNGLDISCARDFRAMEBUFFEREXTPROC)eglGetProcAddress("glDiscardFramebufferEXT");
	mExts.pfnGenVertexArrays = (PFNGLGENVERTEXARRAYSOESPROC)eglGetProcAddress("glGenVertexArraysOES");
	mExts.pfnDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSOESPROC)eglGetProcAddress("glDeleteVertexArraysOES");
	mExts.pfnBindVertexArray = (PFNGLBINDVERTEXARRAYOESPROC)eglGetProcAddress("glBindVertexArrayOES");
	#ifdef OGLSYS_VIVANTE_FB
	mExts.pfnDrawElementsBaseVertex = nullptr;
	#else
	mExts.pfnDrawElementsBaseVertex = (PFNGLDRAWELEMENTSBASEVERTEXOESPROC)eglGetProcAddress("glDrawElementsBaseVertexOES");
	#endif
#endif
	
#if defined(OGLSYS_VIVANTE_FB)
	if (mReduceRes) {
		int vivW = 0;
		int vivH = 0;
		fbGetDisplayGeometry(mVivDisp, &vivW, &vivH);
		EGLNativeWindowType vivWnd = fbCreateWindow(mVivDisp, 0, 0, vivW, vivH);
		EGLSurface vivSurf = eglCreateWindowSurface(mEGL.display, mEGL.config, vivWnd, nullptr);
		eglMakeCurrent(mEGL.display, vivSurf, vivSurf, mEGL.context);
		glViewport(0, 0, vivW, vivH);
		glScissor(0, 0, vivW, vivH);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		for (int i = 0; i < 8; ++i) {
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			eglSwapBuffers(mEGL.display, vivSurf);
		}
		eglDestroySurface(mEGL.display, vivSurf);
		eglMakeCurrent(mEGL.display, mEGL.surface, mEGL.surface, mEGL.context);
		fbDestroyWindow(vivWnd);
		glViewport(0, 0, mWidth, mHeight);
		glScissor(0, 0, mWidth, mHeight);
	}
#endif
}

void OGLSysGlb::reset_ogl() {
	if (mWithoutCtx) {
		return;
	}
#if OGLSYS_ES
	mEGL.reset();
#elif defined(OGLSYS_WINDOWS)
	mWGL.reset_ctx();
	if (mWGL.hLib) {
		FreeLibrary(mWGL.hLib);
	}
	ZeroMemory(&mWGL, sizeof(mWGL));
#endif
}

void OGLSysGlb::handle_ogl_ext(const GLubyte* pStr, const int lenStr) {
	char buf[128];
	if (lenStr < sizeof(buf) - 1) {
		memset(buf, 0, sizeof(buf));
		memcpy(buf, pStr, lenStr);
		if (strcmp(buf, "GL_KHR_texture_compression_astc_ldr") == 0) {
			mExts.ASTC_LDR = true;
		} else if (strcmp(buf, "GL_EXT_texture_compression_dxt1") == 0) {
			mExts.DXT1 = true;
		} else if (strcmp(buf, "GL_EXT_texture_compression_s3tc") == 0) {
			mExts.S3TC = true;
		} else if (strcmp(buf, "GL_EXT_disjoint_timer_query") == 0) {
			mExts.disjointTimer = true;
		} else if (strcmp(buf, "GL_ARB_bindless_texture") == 0) {
			mExts.bindlessTex = true;
		} else if (strcmp(buf, "GL_OES_standard_derivatives") == 0) {
			mExts.derivs = true;
		} else if (strcmp(buf, "GL_OES_vertex_half_float") == 0) {
			mExts.vtxHalf = true;
		} else if (strcmp(buf, "GL_OES_element_index_uint") == 0) {
			mExts.idxUInt = true;
		} else if (strcmp(buf, "GL_OES_get_program_binary") == 0) {
			mExts.progBin = true;
		} else if (strcmp(buf, "GL_EXT_discard_framebuffer") == 0) {
			mExts.discardFB = true;
		}
	}
}

#if defined(OGLSYS_X11)
static bool xbtn_xlat(const XEvent& evt, OGLSysMouseState::BTN* pBtn) {
	bool res = true;
	switch (evt.xbutton.button) {
		case 1: *pBtn = OGLSysMouseState::BTN_LEFT; break;
		case 2: *pBtn = OGLSysMouseState::BTN_MIDDLE; break;
		case 3: *pBtn = OGLSysMouseState::BTN_RIGHT; break;
		default: res = false; break;
	}
	return res;
}
#endif

namespace OGLSys {

	bool s_initFlg = false;

	void init(const OGLSysCfg& cfg) {
		if (s_initFlg) return;
		memset(&GLG, 0, sizeof(GLG));
#ifdef OGLSYS_WINDOWS
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)wnd_proc, &GLG.mhInstance);
#endif
		GLG.mIfc = cfg.ifc;
		GLG.mWndOrgX = cfg.x;
		GLG.mWndOrgY = cfg.y;
		GLG.mWidth = cfg.width;
		GLG.mHeight = cfg.height;
		GLG.mReduceRes = cfg.reduceRes;
		GLG.mHideSysIcons = cfg.hideSysIcons;
		GLG.mWithoutCtx = cfg.withoutCtx;
		GLG.init_wnd();
		GLG.init_ogl();
		s_initFlg = true;
	}

	void reset() {
		if (!s_initFlg) return;
		GLG.reset_ogl();
		GLG.reset_wnd();
		s_initFlg = false;
	}

	void stop() {
		GLG.stop_ogl();
	}

	void swap() {
#if OGLSYS_ES
		if (GLG.mExts.pfnDiscardFramebuffer && GLG.mExts.discardFB) {
			static GLenum exts[] = { GL_DEPTH_EXT, GL_STENCIL_EXT };
			GLG.mExts.pfnDiscardFramebuffer(GL_FRAMEBUFFER, sizeof(exts) / sizeof(exts[0]), exts);
		}
		eglSwapBuffers(GLG.mEGL.display, GLG.mEGL.surface);
#elif defined(OGLSYS_WINDOWS)
		::SwapBuffers(GLG.mhDC);
#endif
	}

	void loop(void(*pLoop)()) {
#if defined(OGLSYS_WINDOWS)
		MSG msg;
		bool done = false;
		while (!done) {
			if (PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE)) {
				if (GetMessage(&msg, NULL, 0, 0)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				} else {
					done = true;
					break;
				}
			} else {
				if (pLoop) {
					pLoop();
				}
				++GLG.mFrameCnt;
			}
		}
#elif defined(OGLSYS_ANDROID)
		android_app* pApp = oglsys_get_app();
		if (!pApp) return;
		while (true) {
			int fd;
			int events;
			android_poll_source* pPollSrc;
			while (true) {
				int id = ALooper_pollAll(0, &fd, &events, (void**)&pPollSrc);
				if (id < 0) break;
				if (id == LOOPER_ID_INPUT) {
					if (pApp->inputQueue && AInputQueue_hasEvents(pApp->inputQueue)) {
						AInputEvent* pInpEvt;
						while (true) {
							pInpEvt = nullptr;
							int32_t inpRes = AInputQueue_getEvent(pApp->inputQueue, &pInpEvt);
							if (inpRes < 0) break;
							if (!pInpEvt) break;
							int32_t inpEvtType = AInputEvent_getType(pInpEvt);
							int32_t evtHandled = 0;
							OGLSysInput inp;
							inp.act = OGLSysInput::ACT_NONE;
							if (inpEvtType == AINPUT_EVENT_TYPE_MOTION) {
								inp.device = OGLSysInput::TOUCH;
								inp.sysDevId = AInputEvent_getDeviceId(pInpEvt);
								int32_t act = AMotionEvent_getAction(pInpEvt);
								size_t ntouch = AMotionEvent_getPointerCount(pInpEvt);
								if (act == AMOTION_EVENT_ACTION_MOVE) {
									inp.act = OGLSysInput::ACT_DRAG;
									for (int i = 0; i < ntouch; ++i) {
										inp.id = AMotionEvent_getPointerId(pInpEvt, i);
										inp.absX = AMotionEvent_getX(pInpEvt, i);
										inp.absY = AMotionEvent_getY(pInpEvt, i);
										inp.pressure = AMotionEvent_getPressure(pInpEvt, i);
										if (inp.id == 0) {
											GLG.update_mouse_pos(inp.absX, inp.absY);
										}
										GLG.send_input(&inp);
									}
								} else if (act == AMOTION_EVENT_ACTION_DOWN || act == AMOTION_EVENT_ACTION_UP) {
									inp.act = act == AMOTION_EVENT_ACTION_DOWN ? OGLSysInput::ACT_DOWN : OGLSysInput::ACT_UP;
									inp.id = AMotionEvent_getPointerId(pInpEvt, 0);
									inp.absX = AMotionEvent_getX(pInpEvt, 0);
									inp.absY = AMotionEvent_getY(pInpEvt, 0);
									inp.pressure = AMotionEvent_getPressure(pInpEvt, 0);
									if (inp.id == 0) {
										GLG.set_mouse_pos(inp.absX, inp.absY);
										uint32_t mask = 0;
										if (inp.act == OGLSysInput::ACT_DOWN) {
											mask = 1 << OGLSysMouseState::BTN_LEFT;
										}
										GLG.mMouseState.mBtnOld = GLG.mMouseState.mBtnNow;
										GLG.mMouseState.mBtnNow = mask;
									}
									GLG.send_input(&inp);
								}
								evtHandled = 1;
							}
							AInputQueue_finishEvent(pApp->inputQueue, pInpEvt, evtHandled);
						}
					}
				}
				if (pPollSrc) {
					pPollSrc->process(pApp, pPollSrc);
				}
			}
			if (pApp->destroyRequested) break;
			if (pLoop) {
				pLoop();
			}
			++GLG.mFrameCnt;
		}
#elif defined(OGLSYS_X11)
		XEvent evt;
		bool done = false;
		while (!done) {
			int kcode = 0;
			KeySym ksym;
			KeySym* pKeySym = nullptr;
			Atom* pAtom = nullptr;
			int cnt = 0;
			bool* pKbdState = nullptr;
			int kbdIdx = -1;
			OGLSysMouseState::BTN btn;
			while (XPending(GLG.mpXDisplay)) {
				XNextEvent(GLG.mpXDisplay, &evt);
				switch (evt.type) {
					case ButtonPress:
						if (xbtn_xlat(evt, &btn)) {
							send_mouse_down(btn, float(evt.xbutton.x), float(evt.xbutton.y), true);
						}
						break;
					case ButtonRelease:
						if (xbtn_xlat(evt, &btn)) {
							send_mouse_up(btn, float(evt.xbutton.x), float(evt.xbutton.y), true);
						}
						break;
					case MotionNotify:
						send_mouse_move(float(evt.xbutton.x), float(evt.xbutton.y));
						break;
					case KeyPress:
					case KeyRelease:
						pKeySym = XGetKeyboardMapping(GLG.mpXDisplay, evt.xkey.keycode, 1, &cnt);
						ksym = *pKeySym;
						XFree(pKeySym);
						kcode = ksym & 0xFF;
						if (kcode == 0x1B) { /* ESC */
							done = true;
						}
						pKbdState = nullptr;
						kbdIdx = -1;
						if (kcode >= 'a' && kcode <= 'z') {
							pKbdState = GLG.mKbdState.alpha;
							kbdIdx = kcode - 'a';
						} else if (kcode >= 190 && kcode <= 201) {
							pKbdState = GLG.mKbdState.func;
							kbdIdx = kcode - 190;
						} else {
							switch (kcode) {
								case 8:
									pKbdState = GLG.mKbdState.ctrl;
									kbdIdx = KBD_CTRL_BACK;
									break;
								case 9:
									pKbdState = GLG.mKbdState.ctrl;
									kbdIdx = KBD_CTRL_TAB;
									break;
								case 81:
									pKbdState = GLG.mKbdState.ctrl;
									kbdIdx = KBD_CTRL_LEFT;
									break;
								case 82:
									pKbdState = GLG.mKbdState.ctrl;
									kbdIdx = KBD_CTRL_UP;
									break;
								case 83:
									pKbdState = GLG.mKbdState.ctrl;
									kbdIdx = KBD_CTRL_RIGHT;
									break;
								case 84:
									pKbdState = GLG.mKbdState.ctrl;
									kbdIdx = KBD_CTRL_DOWN;
									break;
								case 225:
									pKbdState = GLG.mKbdState.ctrl;
									kbdIdx = KBD_CTRL_LSHIFT;
									break;
								case 226:
									pKbdState = GLG.mKbdState.ctrl;
									kbdIdx = KBD_CTRL_RSHIFT;
									break;
								case 227:
									pKbdState = GLG.mKbdState.ctrl;
									kbdIdx = KBD_CTRL_LCTRL;
									break;
								case 228:
									pKbdState = GLG.mKbdState.ctrl;
									kbdIdx = KBD_CTRL_RCTRL;
									break;

								case 20:
									pKbdState = GLG.mKbdState.punct;
									kbdIdx = KBD_PUNCT_SPACE;
									break;

								default:
									break;
							}
						}
						if (pKbdState && kbdIdx >= 0) {
							pKbdState[kbdIdx] = (evt.type == KeyPress);
						}
						break;
					case ClientMessage:
						if (XGetWMProtocols(GLG.mpXDisplay, GLG.mXWnd, &pAtom, &cnt)) {
							if (evt.xclient.data.l[0] == *pAtom) {
								done = true;
							}
						}
						break;
				}
			}
			if (pLoop) {
				pLoop();
			}
			++GLG.mFrameCnt;
		}
#else
		while (true) {
			if (pLoop) {
				pLoop();
			}
			++GLG.mFrameCnt;
		}
#endif
	}

	bool valid() {
		return GLG.valid_ogl();
	}

	void* get_window() {
#if defined(OGLSYS_ANDROID)
		return GLG.mpNativeWnd;
#elif defined(OGLSYS_WINDOWS)
		return GLG.mhWnd;
#elif defined(OGLSYS_X11)
		return (void*)GLG.mXWnd;
#else
		return nullptr;
#endif
	}

	void* get_display() {
#if defined(OGLSYS_X11)
		return GLG.mpXDisplay;
#else
		return nullptr;
#endif
	}

	void* get_instance() {
#if defined(OGLSYS_WINDOWS)
		return GLG.mhInstance;
#else
		return nullptr;
#endif
	}

	void bind_def_framebuf() {
		if (valid()) {
			glBindFramebuffer(GL_FRAMEBUFFER, GLG.mDefFBO);
		}
	}

	int get_width() {
		return GLG.mWidth;
	}

	int get_height() {
		return GLG.mHeight;
	}

	uint64_t get_frame_count() {
		return GLG.mFrameCnt;
	}

	GLuint compile_shader_str(const char* pSrc, size_t srcSize, GLenum kind) {
		GLuint sid = 0;
		if (valid() && pSrc && srcSize > 0) {
			sid = glCreateShader(kind);
			if (sid) {
				GLint len[1] = { (GLint)srcSize };
				glShaderSource(sid, 1, (const GLchar* const*)&pSrc, len);
				glCompileShader(sid);
				GLint status = 0;
				glGetShaderiv(sid, GL_COMPILE_STATUS, &status);
				if (!status) {
					GLint infoLen = 0;
					glGetShaderiv(sid, GL_INFO_LOG_LENGTH, &infoLen);
					if (infoLen > 0) {
						char* pInfo = (char*)GLG.mem_alloc(infoLen);
						if (pInfo) {
							glGetShaderInfoLog(sid, infoLen, &infoLen, pInfo);
							const int infoBlkSize = 512;
							char infoBlk[infoBlkSize + 1];
							infoBlk[infoBlkSize] = 0;
							int nblk = infoLen / infoBlkSize;
							for (int i = 0; i < nblk; ++i) {
								::memcpy(infoBlk, &pInfo[infoBlkSize * i], infoBlkSize);
								GLG.dbg_msg("%s", infoBlk);
							}
							int endSize = infoLen % infoBlkSize;
							if (endSize) {
								::memcpy(infoBlk, &pInfo[infoBlkSize * nblk], endSize);
								infoBlk[endSize] = 0;
								GLG.dbg_msg("%s", infoBlk);
							}
							GLG.mem_free(pInfo);
							pInfo = nullptr;
						}
					}
					glDeleteShader(sid);
					sid = 0;
				}
			}
		}
		return sid;
	}

	GLuint compile_shader_file(const char* pSrcPath, GLenum kind) {
		GLuint sid = 0;
		if (valid()) {
			size_t size = 0;
			const char* pSrc = GLG.load_glsl(pSrcPath, &size);
			sid = compile_shader_str(pSrc, size, kind);
			GLG.unload_glsl(pSrc);
			pSrc = nullptr;
		}
		return sid;
	}

	GLuint link_draw_prog(GLuint sidVert, GLuint sidFrag) {
		GLuint pid = 0;
		if (sidVert && sidFrag) {
			GLuint sids[] = { sidVert, sidFrag };
			pid = link_prog(sids, 2);
		}
		return pid;
	}

	GLuint link_prog(const GLuint* pSIDs, const int nSIDs) {
		GLuint pid = 0;
		if (pSIDs && nSIDs > 0) {
			pid = glCreateProgram();
			if (pid) {
				for (int i = 0; i < nSIDs; ++i) {
					GLuint sid = pSIDs[i];
					if (sid) {
						glAttachShader(pid, sid);
					}
				}
#if !OGLSYS_ES
				glProgramParameteri(pid, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
#endif
				glLinkProgram(pid);
				GLint status = 0;
				glGetProgramiv(pid, GL_LINK_STATUS, &status);
				if (!status) {
					GLint infoLen = 0;
					glGetProgramiv(pid, GL_INFO_LOG_LENGTH, &infoLen);
					if (infoLen > 0) {
						char* pInfo = (char*)GLG.mem_alloc(infoLen);
						if (pInfo) {
							glGetProgramInfoLog(pid, infoLen, &infoLen, pInfo);
							GLG.dbg_msg("%s", pInfo);
							GLG.mem_free(pInfo);
							pInfo = nullptr;
						}
						for (int i = 0; i < nSIDs; ++i) {
							GLuint sid = pSIDs[i];
							if (sid) {
								glDetachShader(pid, sid);
							}
						}
						glDeleteProgram(pid);
						pid = 0;
					}
				}
			}
		}
		return pid;
	}

	void* get_prog_bin(GLuint pid, size_t* pSize, GLenum* pFmt) {
		void* pBin = nullptr;
		GLenum fmt = 0;
		size_t size = 0;
		if (pid != 0) {
#if OGLSYS_ES
			if (GLG.mExts.progBin && GLG.mExts.pfnGetProgramBinary) {
				GLsizei len = 0;
				glGetProgramiv(pid, GL_PROGRAM_BINARY_LENGTH_OES, &len);
				if (len > 0) {
					size = (size_t)len;
					pBin = GLG.mem_alloc(size);
					if (pBin) {
						GLG.mExts.pfnGetProgramBinary(pid, len, &len, &fmt, pBin);
					} else {
						size = 0;
					}
				}
			}
#elif defined(OGLSYS_APPLE)
			// TODO
#else
			if (glGetProgramBinary) {
				GLsizei len = 0;
				glGetProgramiv(pid, GL_PROGRAM_BINARY_LENGTH, &len);
				if (len > 0) {
					size = (size_t)len;
					pBin = GLG.mem_alloc(size);
					if (pBin) {
						glGetProgramBinary(pid, len, &len, &fmt, pBin);
					} else {
						size = 0;
					}
				}
			}
#endif
		}
		if (pSize) {
			*pSize = size;
		}
		if (pFmt) {
			*pFmt = fmt;
		}
		return pBin;
	}

	void free_prog_bin(void* pBin) {
		if (pBin) {
			GLG.mem_free(pBin);
		}
	}

	GLuint create_timestamp() {
		GLuint handle = 0;
#if OGLSYS_ES
		if (0) {
			if (GLG.mExts.disjointTimer && GLG.mExts.pfnGenQueries) {
				GLG.mExts.pfnGenQueries(1, &handle);
			}
		}
#elif defined(OGLSYS_APPLE)
		//glGenQueries(1, &handle);
#else
		if (glGenQueries) {
			glGenQueries(1, &handle);
		}
#endif
		return handle;
	}

	void delete_timestamp(GLuint handle) {
#if OGLSYS_ES
		if (handle != 0 && GLG.mExts.disjointTimer && GLG.mExts.pfnDeleteQueries) {
			GLG.mExts.pfnDeleteQueries(1, &handle);
		}
#elif defined(OGLSYS_APPLE)
		if (handle != 0) {
			glDeleteQueries(1, &handle);
		}
#else
		if (handle != 0 && glDeleteQueries) {
			glDeleteQueries(1, &handle);
		}
#endif
	}

	void put_timestamp(GLuint handle) {
#if OGLSYS_ES
		if (handle != 0 && GLG.mExts.disjointTimer && GLG.mExts.pfnQueryCounter) {
			GLG.mExts.pfnQueryCounter(handle, GL_TIMESTAMP_EXT);
		}
#elif defined(OGLSYS_APPLE)
		if (handle != 0) {
			glQueryCounter(handle, GL_TIMESTAMP);
		}
#else
		if (handle != 0 && glQueryCounter) {
			glQueryCounter(handle, GL_TIMESTAMP);
		}
#endif
	}

	GLuint64 get_timestamp(GLuint handle) {
		GLuint64 val = 0;
#if OGLSYS_ES
		if (handle != 0 && GLG.mExts.disjointTimer) {
			GLint err = 0;
			glGetIntegerv(GL_GPU_DISJOINT_EXT, &err);
			if (!err) {
				if (GLG.mExts.pfnGetQueryObjectui64v) {
					GLG.mExts.pfnGetQueryObjectui64v(handle, GL_QUERY_RESULT, &val);
				} else if (GLG.mExts.pfnGetQueryObjectuiv) {
					GLuint tval = 0;
					GLG.mExts.pfnGetQueryObjectuiv(handle, GL_QUERY_RESULT, &tval);
					val = tval;
				}
			}
		}
#elif defined(OGLSYS_APPLE)
		if (handle != 0) {
			glGetQueryObjectui64v(handle, GL_QUERY_RESULT, &val);
		}
#else
		if (handle != 0) {
			if (glGetQueryObjectui64v) {
				glGetQueryObjectui64v(handle, GL_QUERY_RESULT, &val);
			} else if (glGetQueryObjectuiv) {
				GLuint tval = 0;
				glGetQueryObjectuiv(handle, GL_QUERY_RESULT, &tval);
				val = tval;
			}
		}
#endif
		return val;
	}

	void wait_timestamp(GLuint handle) {
#if OGLSYS_ES
		if (handle != 0 && GLG.mExts.disjointTimer && GLG.mExts.pfnGetQueryObjectiv) {
			GLint flg = 0;
			while (!flg) {
				GLG.mExts.pfnGetQueryObjectiv(handle, GL_QUERY_RESULT_AVAILABLE, &flg);
			}
		}
#elif defined(OGLSYS_APPLE)
		if (handle != 0) {
			GLint flg = 0;
			while (!flg) {
				glGetQueryObjectiv(handle, GL_QUERY_RESULT_AVAILABLE, &flg);
			}
		}
#else
		if (handle != 0) {
			if (glGetQueryObjectiv) {
				GLint flg = 0;
				while (!flg) {
					glGetQueryObjectiv(handle, GL_QUERY_RESULT_AVAILABLE, &flg);
				}
			}
		}
#endif
	}

	GLuint gen_vao() {
		GLuint vao = 0;
#if OGLSYS_ES
		if (GLG.mExts.pfnGenVertexArrays != nullptr) {
			GLG.mExts.pfnGenVertexArrays(1, &vao);
		}
#elif defined(OGLSYS_APPLE)
#else
		if (glGenVertexArrays != nullptr) {
			glGenVertexArrays(1, &vao);
		}
#endif
		return vao;
	}

	void bind_vao(const GLuint vao) {
#if OGLSYS_ES
		if (GLG.mExts.pfnBindVertexArray != nullptr) {
			GLG.mExts.pfnBindVertexArray(vao);
		}
#elif defined(OGLSYS_APPLE)
#else
		if (glBindVertexArray != nullptr) {
			glBindVertexArray(vao);
		}
#endif
	}

	void del_vao(const GLuint vao) {
#if OGLSYS_ES
		if (GLG.mExts.pfnDeleteVertexArrays != nullptr) {
			GLG.mExts.pfnDeleteVertexArrays(1, &vao);
		}
#elif defined(OGLSYS_APPLE)
#else
		if (glDeleteVertexArrays != nullptr) {
			glDeleteVertexArrays(1, &vao);
		}
#endif
	}


	void draw_tris_base_vtx(const int ntris, const GLenum idxType, const intptr_t ibOrg, const int baseVtx) {
#if OGLSYS_ES
		#if !defined(OGLSYS_VIVANTE_FB)
		if (GLG.mExts.pfnDrawElementsBaseVertex != nullptr) {
			GLG.mExts.pfnDrawElementsBaseVertex(GL_TRIANGLES, ntris * 3, idxType, (const void*)ibOrg, baseVtx);
		}
		#endif
#elif defined(OGLSYS_APPLE)
#else
		if (glDrawElementsBaseVertex != nullptr) {
			glDrawElementsBaseVertex(GL_TRIANGLES, ntris * 3, idxType, (const void*)ibOrg, baseVtx);
		}
#endif
	}


	bool ext_ck_bindless_texs() {
		return GLG.mExts.bindlessTex;
	}

	bool ext_ck_derivatives() {
#if OGLSYS_ES
		return GLG.mExts.derivs;
#else
		return true;
#endif
	}

	bool ext_ck_vtx_half() {
#if OGLSYS_ES
		return GLG.mExts.vtxHalf;
#else
		return true;
#endif
	}

	bool ext_ck_idx_uint() {
#if OGLSYS_ES
		return GLG.mExts.idxUInt;
#else
		return true;
#endif
	}

	bool ext_ck_ASTC_LDR() {
		return GLG.mExts.ASTC_LDR;
	}

	bool ext_ck_S3TC() {
		return GLG.mExts.S3TC;
	}

	bool ext_ck_DXT1() {
		return GLG.mExts.DXT1;
	}

	bool ext_ck_prog_bin() {
#if OGLSYS_ES
		return GLG.mExts.progBin;
#elif defined(OGLSYS_APPLE)
		return GLG.mExts.progBin;
#else
		return glGetProgramBinary != nullptr;
#endif
	}

	bool ext_ck_shader_bin() {
#if OGLSYS_ES
		return false;
#elif defined(OGLSYS_APPLE)
		return false;
#else
		return glShaderBinary != nullptr;
#endif
	}

	bool ext_ck_spv() {
		bool res = false;
		if (ext_ck_shader_bin()) {
#if OGLSYS_ES
#elif defined(OGLSYS_APPLE)
#else
			GLint nfmt = 0;
			glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &nfmt);
			if (nfmt > 0) {
				GLint fmtLst[16];
				GLint* pLst = fmtLst;
				if (nfmt > sizeof(fmtLst) / sizeof(fmtLst[0])) {
					pLst = (GLint*)GLG.mem_alloc(nfmt * sizeof(GLint));
				}
				if (pLst) {
					glGetIntegerv(GL_SHADER_BINARY_FORMATS, fmtLst);
					for (int i = 0; i < nfmt; ++i) {
						if (pLst[i] == GL_SHADER_BINARY_FORMAT_SPIR_V_ARB) {
							res = true;
							break;
						}
					}
				}
				if (pLst != fmtLst) {
					GLG.mem_free(pLst);
					pLst = nullptr;
				}
			}
#endif
		}
		return res;
	}

	bool ext_ck_vao() {
		bool res = false;
#if OGLSYS_ES
		if (GLG.mExts.pfnGenVertexArrays != nullptr) {
			res = true;
		}
#elif defined(OGLSYS_APPLE)
#else
		if (glGenVertexArrays != nullptr) {
			res = true;
		}
#endif
		return res;
	}

	bool ext_ck_vtx_base() {
		bool res = false;
#if OGLSYS_ES
		if (GLG.mExts.pfnDrawElementsBaseVertex != nullptr) {
			res = true;
		}
#elif defined(OGLSYS_APPLE)
#else
		if (glDrawElementsBaseVertex != nullptr) {
			res = true;
		}
#endif
		return res;
	}


	bool get_key_state(const char code) {
		bool* pState = nullptr;
		int idx = -1;
		if (code >= '0' && code <= '9') {
			pState = GLG.mKbdState.digit;
			idx = code - '0';
		} else if (code >= 'A' && code <= 'Z') {
			pState = GLG.mKbdState.alpha;
			idx = code - 'A';
		} else if (code >= 'a' && code <= 'z') {
			pState = GLG.mKbdState.alpha;
			idx = code - 'a';
		} else {
			const int npunct = sizeof(s_kbdPunctTbl) / sizeof(s_kbdPunctTbl[0]);
			for (int i = 0; i < npunct; ++i) {
				if (s_kbdPunctTbl[i].code == code) {
					pState = GLG.mKbdState.punct;
					idx = s_kbdPunctTbl[i].idx;
				}
			}
		}
		return (pState && idx >= 0) ? pState[idx] : false;
	}

	void xlat_key_name(const char* pName, const size_t nameLen, bool** ppState, int* pIdx) {
		bool* pState = nullptr;
		int idx = -1;
		if (pName[0] == 'F') {
			if (nameLen == 2) {
				int code = pName[1];
				if ((code >= '1' && code <= '9')) {
					pState = GLG.mKbdState.func;
					idx = code - '1';
				}
			} else if (nameLen == 3) {
				int code = pName[2];
				if (pName[1] == '1' && (code >= '0' && code <= '2')) {
					pState = GLG.mKbdState.func;
					idx = (code - '0') + 9;
				}
			}
		}
		if (!pState) {
			if (strcmp(pName, "UP") == 0) {
				pState = GLG.mKbdState.ctrl;
				idx = KBD_CTRL_UP;
			} else if (strcmp(pName, "DOWN") == 0) {
				pState = GLG.mKbdState.ctrl;
				idx = KBD_CTRL_DOWN;
			} else if (strcmp(pName, "LEFT") == 0) {
				pState = GLG.mKbdState.ctrl;
				idx = KBD_CTRL_LEFT;
			} else if (strcmp(pName, "RIGHT") == 0) {
				pState = GLG.mKbdState.ctrl;
				idx = KBD_CTRL_RIGHT;
			} else if (strcmp(pName, "TAB") == 0) {
				pState = GLG.mKbdState.ctrl;
				idx = KBD_CTRL_TAB;
			} else if (strcmp(pName, "BACK") == 0) {
				pState = GLG.mKbdState.ctrl;
				idx = KBD_CTRL_BACK;
			} else if (strcmp(pName, "LSHIFT") == 0) {
				pState = GLG.mKbdState.ctrl;
				idx = KBD_CTRL_LSHIFT;
			} else if (strcmp(pName, "LCTRL") == 0) {
				pState = GLG.mKbdState.ctrl;
				idx = KBD_CTRL_LCTRL;
			} else if (strcmp(pName, "RSHIFT") == 0) {
				pState = GLG.mKbdState.ctrl;
				idx = KBD_CTRL_RSHIFT;
			} else if (strcmp(pName, "RCTRL") == 0) {
				pState = GLG.mKbdState.ctrl;
				idx = KBD_CTRL_RCTRL;
			}
		}
		if (ppState) {
			*ppState = pState;
		}
		if (pIdx) {
			*pIdx = idx;
		}
	}

	bool get_key_state(const char* pName) {
		bool res = false;
		size_t nameLen = pName ? strlen(pName) : 0;
		if (nameLen == 1) {
			res = get_key_state(pName[0]);
		} else if (nameLen > 0) {
			bool* pState = nullptr;
			int idx = -1;
			xlat_key_name(pName, nameLen, &pState, &idx);
			if (pState && idx >= 0) {
				res = pState[idx];
			}
		}
		return res;
	}

	void set_key_state(const char* pName, const bool state) {
		bool* pState = nullptr;
		int idx = -1;
		size_t nameLen = pName ? strlen(pName) : 0;
		if (nameLen > 0) {
			if (nameLen == 1) {
				int code = int(pName[0]);
				if (code >= '0' && code <= '9') {
					pState = GLG.mKbdState.digit;
					idx = code - '0';
				} else if (code >= 'A' && code <= 'Z') {
					pState = GLG.mKbdState.alpha;
					idx = code - 'A';
				} else if (code >= 'a' && code <= 'z') {
					pState = GLG.mKbdState.alpha;
					idx = code - 'a';
				} else {
					const int npunct = sizeof(s_kbdPunctTbl) / sizeof(s_kbdPunctTbl[0]);
					for (int i = 0; i < npunct; ++i) {
						if (s_kbdPunctTbl[i].code == code) {
							pState = GLG.mKbdState.punct;
							idx = s_kbdPunctTbl[i].idx;
						}
					}
				}
			} else {
				xlat_key_name(pName, nameLen, &pState, &idx);
			}
		}
		if (pState && idx >= 0) {
			pState[idx] = state;
		}
	}

	void set_input_handler(InputHandler handler, void* pWk) {
		GLG.mInpHandler = handler;
		GLG.mpInpHandlerWk = pWk;
	}

	OGLSysMouseState get_mouse_state() {
		return GLG.mMouseState;
	}

	void set_inp_btn_id(OGLSysInput* pInp, OGLSysMouseState::BTN btn) {
		switch (btn) {
			case OGLSysMouseState::BTN_LEFT:
				pInp->id = 0;
				break;
			case OGLSysMouseState::BTN_MIDDLE:
				pInp->id = 1;
				break;
			case OGLSysMouseState::BTN_RIGHT:
				pInp->id = 2;
				break;
			case OGLSysMouseState::BTN_X:
				pInp->id = 3;
				break;
			case OGLSysMouseState::BTN_Y:
				pInp->id = 4;
				break;
			default:
				pInp->id = -1;
				break;
		}
	}

	void send_mouse_down(OGLSysMouseState::BTN btn, float absX, float absY, bool updateFlg) {
		uint32_t mask = 1U << btn;
		GLG.mMouseState.mBtnOld = GLG.mMouseState.mBtnNow;
		GLG.mMouseState.mBtnNow &= ~mask;
		GLG.mMouseState.mBtnNow |= mask;
		OGLSysInput inp;
		inp.device = OGLSysInput::MOUSE;
		inp.sysDevId = 0;
		inp.pressure = 1.0f;
		inp.absX = absX;
		inp.absY = absY;
		inp.act = OGLSysInput::ACT_NONE;
		set_inp_btn_id(&inp, btn);
		if (inp.id != -1) {
			inp.act = OGLSysInput::ACT_DOWN;
		}
		if (updateFlg) {
			GLG.update_mouse_pos(absX, absY);
		} else {
			GLG.set_mouse_pos(absX, absY);
		}
		GLG.send_input(&inp);
	}

	void send_mouse_up(OGLSysMouseState::BTN btn, float absX, float absY, bool updateFlg) {
		uint32_t mask = 1U << btn;
		GLG.mMouseState.mBtnOld = GLG.mMouseState.mBtnNow;
		GLG.mMouseState.mBtnNow &= ~mask;
		OGLSysInput inp;
		inp.device = OGLSysInput::MOUSE;
		inp.sysDevId = 0;
		inp.pressure = 1.0f;
		inp.absX = absX;
		inp.absY = absY;
		inp.act = OGLSysInput::ACT_NONE;
		set_inp_btn_id(&inp, btn);
		if (inp.id != -1) {
			inp.act = OGLSysInput::ACT_UP;
		}
		if (updateFlg) {
			GLG.update_mouse_pos(absX, absY);
		} else {
			GLG.set_mouse_pos(absX, absY);
		}
		GLG.send_input(&inp);
	}

	void send_mouse_move(float absX, float absY) {
		OGLSysInput inp;
		inp.device = OGLSysInput::MOUSE;
		inp.sysDevId = 0;
		inp.pressure = 1.0f;
		inp.absX = absX;
		inp.absY = absY;
		inp.id = -1;
		if (GLG.mMouseState.ck_now(OGLSysMouseState::BTN_LEFT)) {
			inp.id = 0;
			inp.act = OGLSysInput::ACT_DRAG;
		} else {
			inp.act = OGLSysInput::ACT_HOVER;
		}
		GLG.update_mouse_pos(absX, absY);
		GLG.send_input(&inp);
	}

} // OGLSys
