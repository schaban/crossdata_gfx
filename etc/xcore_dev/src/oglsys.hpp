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

#ifndef OGLSYS_ES
#	define OGLSYS_ES 0
#endif

#include <cstdint>

#if defined(ANDROID)
#	define OGLSYS_ANDROID
#	undef OGLSYS_ES
#	define OGLSYS_ES 1
#	define GL_GLEXT_PROTOTYPES
#	include <EGL/egl.h>
#	include <EGL/eglext.h>
#	include <GLES/gl.h>
#	include <GLES2/gl2.h>
#	include <GLES2/gl2ext.h>
#	include <GLES3/gl3.h>
#	include <GLES3/gl31.h>
#	include <GLES3/gl32.h>
#	include <android_native_app_glue.h>
android_app* oglsys_get_app();
#elif defined(X11)
#	define OGLSYS_X11
#	if OGLSYS_ES
#		if OGLSYS_USE_DYNAMICGLES
#			define DYNAMICGLES_NO_NAMESPACE
#			define DYNAMICEGL_NO_NAMESPACE
#			include <DynamicGles.h>
#		else
#			define GL_GLEXT_PROTOTYPES
#			include <EGL/egl.h>
#			include <EGL/eglext.h>
#			include <GLES2/gl2.h>
#			include <GLES2/gl2ext.h>
#			include <GLES3/gl3.h>
#			include <GLES3/gl31.h>
#			include <GLES3/gl32.h>
#		endif
#	else
#		include <GL/glcorearb.h>
#		include <GL/glext.h>
#		define OGL_FN(_type, _name) extern PFNGL##_type##PROC gl##_name;
#		undef OGL_FN_CORE
#		define OGL_FN_CORE
#		undef OGL_FN_EXTRA
#		define OGL_FN_EXTRA
#		include "oglsys.inc"
#		undef OGL_FN
#	endif
#elif defined(VIVANTE_FB)
#	define OGLSYS_VIVANTE_FB
#	undef OGLSYS_ES
#	define OGLSYS_ES 1
#	include <EGL/egl.h>
#	include <EGL/eglext.h>
#	include <EGL/eglvivante.h>
#	include <GLES/gl.h>
#	include <GLES/glext.h>
#	include <GLES2/gl2.h>
#	include <GLES2/gl2ext.h>
#	include <GLES3/gl3.h>
#elif defined(__APPLE__)
#	define OGLSYS_APPLE
#	define OGLSYS_MACOS
#	include <OpenGL/gl3.h>
#else
#	define OGLSYS_WINDOWS
#	undef _WIN32_WINNT
#	define _WIN32_WINNT 0x0500
#	if OGLSYS_ES
#		define DYNAMICGLES_NO_NAMESPACE
#		define DYNAMICEGL_NO_NAMESPACE
#		include <DynamicGles.h>
#	else
#		define WIN32_LEAN_AND_MEAN 1
#		undef NOMINMAX
#		define NOMINMAX
#		include <Windows.h>
#		include <tchar.h>
#		include <GL/glcorearb.h>
#		include <GL/glext.h>
#		include <GL/wglext.h>
#		define OGL_FN(_type, _name) extern PFNGL##_type##PROC gl##_name;
#		undef OGL_FN_CORE
#		define OGL_FN_CORE
#		undef OGL_FN_EXTRA
#		define OGL_FN_EXTRA
#		include "oglsys.inc"
#		undef OGL_FN
#endif
#endif

struct OGLSysIfc {
	void* (*mem_alloc)(size_t size, const char* pTag);
	void (*mem_free)(void* p);
	void (*dbg_msg)(const char* pFmt, ...);
	const char* (*load_glsl)(const char* pPath, size_t* pSize);
	void (*unload_glsl)(const char* pCode);

	OGLSysIfc() {
		mem_alloc = nullptr;
		mem_free = nullptr;
		dbg_msg = nullptr;
		load_glsl = nullptr;
		unload_glsl = nullptr;
	}
};

struct OGLSysCfg {
	int x;
	int y;
	int width;
	int height;
	int msaa;
	OGLSysIfc ifc;
	bool reduceRes;
	bool hideSysIcons;
	bool withoutCtx;
};

struct OGLSysMouseState {
	enum BTN {
		BTN_LEFT = 0,
		BTN_MIDDLE,
		BTN_RIGHT,
		BTN_X,
		BTN_Y
	};

	uint32_t mBtnOld;
	uint32_t mBtnNow;
	float mNowX;
	float mNowY;
	float mOldX;
	float mOldY;
	int32_t mWheel;

	bool ck_now(BTN id) const { return !!(mBtnNow & (1 << (uint32_t)id)); }
	bool ck_old(BTN id) const { return !!(mBtnOld & (1 << (uint32_t)id)); }
	bool ck_trg(BTN id) const { return !!((mBtnNow & (mBtnNow ^ mBtnOld)) & (1 << (uint32_t)id)); }
	bool ck_chg(BTN id) const { return !!((mBtnNow ^ mBtnOld) & (1 << (uint32_t)id)); }
};

struct OGLSysInput {
	enum ACT {
		ACT_NONE,
		ACT_DRAG,
		ACT_HOVER,
		ACT_DOWN,
		ACT_UP
	};

	enum DEVICE {
		MOUSE,
		TOUCH
	};

	int device;
	int act;
	int id;
	int sysDevId;
	float absX;
	float absY;
	float relX;
	float relY;
	float pressure;
};

struct OGLSysParamsBuf {
	const GLchar* mpName;
	GLuint mProgId;
	GLuint mBlockIdx;
	GLuint mBufHandle;
	GLuint mBindingIdx;
	GLint mSize;
	int mNumFields;
	const GLchar** mpFieldNames;
	GLuint* mpFieldIndices;
	GLint* mpFieldOffsets;
	GLint* mpFieldTypes;
	void* mpStorage;
	bool mUpdateFlg;

	void init(GLuint progId, const GLchar* pName, GLuint binding, const GLchar** ppFieldNames, const int numFields);
	void reset();
	int find_field(const char* pName);
	bool ck_field_idx(const int idx) const { return idx >= 0 && idx < mNumFields; }
	void set(const int idx, const float x);
	void set(const char* pName, const float x) { set(find_field(pName), x); }
	void set(const int idx, const float x, const float y);
	void set(const char* pName, const float x, const float y) { set(find_field(pName), x, y); };
	void set(const int idx, const float x, const float y, const float z);
	void set(const char* pName, const float x, const float y, const float z) { set(find_field(pName), x, y, z); };
	void set(const int idx, const float x, const float y, const float z, const float w);
	void set(const char* pName, const float x, const float y, const float z, const float w) { set(find_field(pName), x, y, z, w); };
	void send();
	void bind();
};

namespace OGLSys {

	typedef void (*InputHandler)(const OGLSysInput& inp, void* pWk);

	void init(const OGLSysCfg& cfg);
	void reset();
	void stop();
	void swap();
	void loop(void (*pLoop)(void*), void* pLoopCtx = nullptr);
	bool valid();

	void* get_window();
	void* get_display();
	void* get_instance();

	void bind_def_framebuf();

	int get_width();
	int get_height();
	uint64_t get_frame_count();

	void* get_proc_addr(const char* pName);

	GLuint compile_shader_str(const char* pSrc, size_t srcSize, GLenum kind);
	GLuint compile_shader_file(const char* pSrcPath, GLenum kind);
	GLuint link_prog(const GLuint* pSIDs, const int nSIDs);
	GLuint link_draw_prog(GLuint sidVert, GLuint sidFrag);
	void* get_prog_bin(GLuint pid, size_t* pSize, GLenum* pFmt);
	void free_prog_bin(void* pBin);

	void enable_msaa(const bool flg);

	GLuint create_timestamp();
	void delete_timestamp(GLuint handle);
	void put_timestamp(GLuint handle);
	GLuint64 get_timestamp(GLuint handle);
	void wait_timestamp(GLuint handle);

	GLuint gen_vao();
	void bind_vao(const GLuint vao);
	void del_vao(const GLuint vao);

	void draw_tris_base_vtx(const int ntris, const GLenum idxType, const intptr_t ibOrg, const int baseVtx);

	GLuint get_black_tex();
	GLuint get_white_tex();

	bool ext_ck_bindless_texs();
	bool ext_ck_derivatives();
	bool ext_ck_vtx_half();
	bool ext_ck_idx_uint();
	bool ext_ck_ASTC_LDR();
	bool ext_ck_S3TC();
	bool ext_ck_DXT1();
	bool ext_ck_prog_bin();
	bool ext_ck_shader_bin();
	bool ext_ck_spv();
	bool ext_ck_vao();
	bool ext_ck_vtx_base();

	bool get_key_state(const char code);
	bool get_key_state(const char* pName);

	void set_key_state(const char* pName, const bool state);

	void set_input_handler(InputHandler handler, void* pWk);

	OGLSysMouseState get_mouse_state();
	void send_mouse_down(OGLSysMouseState::BTN btn, float absX, float absY, bool updateFlg);
	void send_mouse_up(OGLSysMouseState::BTN btn, float absX, float absY, bool updateFlg);
	void send_mouse_move(float absX, float absY);
}
