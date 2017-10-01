/*
 * Author: Sergey Chaban <sergey.chaban@gmail.com>
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <malloc.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>

#ifndef XD_INLINE
#	define XD_INLINE inline
#endif

#ifndef XD_NOINLINE
#	define XD_NOINLINE
#endif

#define XD_MAX_PATH 4096

#define XD_FOURCC(c1, c2, c3, c4) ((((uint8_t)(c4))<<24)|(((uint8_t)(c3))<<16)|(((uint8_t)(c2))<<8)|((uint8_t)(c1)))
#define XD_INCR_PTR(_ptr, _inc) ( &((uint8_t*)(_ptr))[_inc] )
#define XD_ARY_LEN(_arr) (sizeof((_arr)) / sizeof((_arr)[0]))
#define XD_ALIGN(_x, _n) ( ((uintptr_t)(_x) + ((_n) - 1)) & (~((_n) - 1)) )

#define XD_BIT_ARY_SIZE(_storage_t, _nbits) ((((int)(_nbits)-1)/(sizeof(_storage_t)*8))+1)
#define XD_BIT_ARY_DECL(_storage_t, _name, _nbits) _name[XD_BIT_ARY_SIZE(storage_t, _nbits)]
#define XD_BIT_ARY_IDX(_storage_t, _no) ((_no) / (sizeof(_storage_t)*8))
#define XD_BIT_ARY_MASK(_storage_t, _no) (1 << ((_no)&((sizeof(_storage_t)*8) - 1)))
#define XD_BIT_ARY_ST(_storage_t, _ary, _no) (_ary)[XD_BIT_ARY_IDX(_storage_t, _no)] |= XD_BIT_ARY_MASK(_storage_t, _no)
#define XD_BIT_ARY_CL(_storage_t, _ary, _no) (_ary)[XD_BIT_ARY_IDX(_storage_t, _no)] &= ~XD_BIT_ARY_MASK(_storage_t, _no)
#define XD_BIT_ARY_SW(_storage_t, _ary, _no) (_ary)[XD_BIT_ARY_IDX(_storage_t, _no)] ^= XD_BIT_ARY_MASK(_storage_t, _no)
#define XD_BIT_ARY_CK(_storage_t, _ary, _no) (!!((_ary)[XD_BIT_ARY_IDX(_storage_t, _no)] & XD_BIT_ARY_MASK(_storage_t, _no)))

#ifndef M_PI
#	define M_PI 3.14159265358979323846
#endif
#define XD_PI ((float)M_PI)

#define XD_DEG2RAD(_deg) ( (_deg) * (XD_PI / 180.0f) )
#define XD_RAD2DEG(_rad) ( (_rad) * (180.0f / XD_PI) )

#define XD_DEF_MEM_TAG XD_FOURCC('X','M','E','M')
#define XD_DAT_MEM_TAG XD_FOURCC('X','D','A','T')
#define XD_TMP_MEM_TAG XD_FOURCC('X','T','M','P')

#ifdef _MSC_VER
#	define XD_SPRINTF ::sprintf_s
#	define XD_SPRINTF_BUF(_buf, _bufsize)  (_buf), (_bufsize)
#else
#	define XD_SPRINTF ::sprintf
#	define XD_SPRINTF_BUF(_buf, _bufsize)  (_buf)
#endif

typedef void* XD_FHANDLE;

struct sxSysIfc {
	void* (*fn_malloc)(size_t);
	void (*fn_free)(void*);
	XD_FHANDLE (*fn_fopen)(const char*);
	void (*fn_fclose)(XD_FHANDLE);
	size_t (*fn_fsize)(XD_FHANDLE);
	size_t (*fn_fread)(XD_FHANDLE, void*, size_t);
	void (*fn_dbgmsg)(const char*);
};

namespace nxSys {

void init(sxSysIfc* pIfc);

void* malloc(size_t);
void free(void*);
XD_FHANDLE fopen(const char*);
void fclose(XD_FHANDLE);
size_t fsize(XD_FHANDLE);
size_t fread(XD_FHANDLE, void*, size_t);
void dbgmsg(const char*);

FILE* fopen_w_txt(const char* fpath);
FILE* fopen_w_bin(const char* fpath);

} // nxSys


namespace nxCore {

void* mem_alloc(size_t size, uint32_t tag = XD_DEF_MEM_TAG, int alignment = 0x10);
void* mem_realloc(void* pMem, size_t newSize, int alignment = 0x10);
void* mem_resize(void* pMem, float factor, int alignment = 0x10);
void mem_free(void* pMem);
size_t mem_size(void* pMem);
uint32_t mem_tag(void* pMem);
void mem_dbg();
void dbg_msg(const char* fmt, ...);
void* bin_load(const char* pPath, size_t* pSize = nullptr, bool appendPath = false);
void bin_unload(void* pMem);
void bin_save(const char* pPath, const void* pMem, size_t size);
uint32_t str_hash32(const char* pStr);
uint16_t str_hash16(const char* pStr);
bool str_eq(const char* pStrA, const char* pStrB);
bool str_starts_with(const char* pStr, const char* pPrefix);
bool str_ends_with(const char* pStr, const char* pPostfix);
char* str_dup(const char* pSrc);

inline bool is_pow2(uint32_t val) {
	return (val & (val - 1)) == 0;
}

inline uintptr_t align_pad(uintptr_t x, int a) {
	return ((x + (a - 1)) / a) * a;
}

uint16_t float_to_half(float x);
float half_to_float(uint16_t h);
float f32_set_bits(uint32_t x);
uint32_t f32_get_bits(float x);
inline uint32_t f32_get_exp_bits(float x) { return (f32_get_bits(x) >> 23) & 0xFF; }
inline int f32_get_exp(float x) { return (int)f32_get_exp_bits(x) - 127; }
inline float f32_mk_nan() { return f32_set_bits(0xFFC00000); }

uint32_t fetch_bits32(uint8_t* pTop, uint32_t org, uint32_t len);

} // nxCore

namespace nxCalc {

template<typename T> inline T min(T x, T y) { return (x < y ? x : y); }
template<typename T> inline T min(T x, T y, T z) { return min(x, min(y, z)); }
template<typename T> inline T min(T x, T y, T z, T w) { return min(x, min(y, z, w)); }
template<typename T> inline T max(T x, T y) { return (x > y ? x : y); }
template<typename T> inline T max(T x, T y, T z) { return max(x, max(y, z)); }
template<typename T> inline T max(T x, T y, T z, T w) { return max(x, max(y, z, w)); }

template<typename T> inline T clamp(T x, T lo, T hi) { return max(min(x, hi), lo); }

template<typename T> inline T sq(T x) { return x*x; }
template<typename T> inline T cb(T x) { return x*x*x; }

inline float saturate(float x) { return clamp(x, 0.0f, 1.0f); }

inline float div0(float x, float y) { return y != 0.0f ? x / y : 0.0f; }

inline float rcp0(float x) { return div0(1.0f, x); }

inline float lerp(float a, float b, float t) { return a + (b - a)*t; }

inline float sinc(float x) {
	if (::fabsf(x) < 1.0e-4f) {
		return 1.0f;
	}
	return (::sinf(x) / x);
}

inline float ease(float t, float e = 1.0f) {
	float x = 0.5f - 0.5f*::cosf(t*XD_PI);
	if (e != 1.0f) {
		x = ::powf(x, e);
	}
	return x;
}

float hermite(float p0, float m0, float p1, float m1, float t);

float fit(float val, float oldMin, float oldMax, float newMin, float newMax);

float calc_fovy(float focal, float aperture, float aspect);

} // nxCalc

typedef int32_t xt_int;
typedef float xt_float;

struct xt_float2 {
	float x, y;

	void set(float xval, float yval) { x = xval; y = yval; }
	void fill(float val) { x = val; y = val; }
	void scl(float s) { x *= s; y *= s; }

	operator float* () { return reinterpret_cast<float*>(this); }
	operator const float* () const { return reinterpret_cast<const float*>(this); }
};

struct xt_float3 {
	float x, y, z;

	void set(float xval, float yval, float zval) { x = xval; y = yval; z = zval; }
	void fill(float val) { x = val; y = val; z = val; }
	void scl(float s) { x *= s; y *= s; z *= s; }

	float get_at(int i) const {
		float res = nxCore::f32_mk_nan();
		switch (i) {
			case 0: res = x; break;
			case 1: res = y; break;
			case 2: res = z; break;
		}
		return res;
	}

	void set_at(int i, float val) {
		switch (i) {
			case 0: x = val; break;
			case 1: y = val; break;
			case 2: z = val; break;
		}
	}

	operator float* () { return reinterpret_cast<float*>(this); }
	operator const float* () const { return reinterpret_cast<const float*>(this); }
};

struct xt_float4 {
	float x, y, z, w;

	void set(float xval, float yval, float zval, float wval) { x = xval; y = yval; z = zval; w = wval; }
	void fill(float val) { x = val; y = val; z = val; w = val; }
	void scl(float s) { x *= s; y *= s; z *= s; w *= s; }

	float get_at(int i) const {
		float res = nxCore::f32_mk_nan();
		switch (i) {
			case 0: res = x; break;
			case 1: res = y; break;
			case 2: res = z; break;
			case 3: res = w; break;
		}
		return res;
	}

	void set_at(int at, float val) {
		switch (at) {
			case 0: x = val; break;
			case 1: y = val; break;
			case 2: z = val; break;
			case 3: w = val; break;
			default: break;
		}
	}

	operator float* () { return reinterpret_cast<float*>(this); }
	operator const float* () const { return reinterpret_cast<const float*>(this); }
};

struct xt_mtx {
	float m[4][4];

	void identity();

	operator float* () { return reinterpret_cast<float*>(this); }
	operator const float* () const { return reinterpret_cast<const float*>(this); }
};

struct xt_wmtx {
	float m[3][4];

	void identity();

	operator float* () { return reinterpret_cast<float*>(this); }
	operator const float* () const { return reinterpret_cast<const float*>(this); }
};

struct xt_int2 {
	int32_t x, y;

	void set(int32_t xval, int32_t yval) { x = xval; y = yval; }
	void fill(int32_t val) { x = val; y = val; }
};

struct xt_int3 {
	int32_t x, y, z;

	void set(int32_t xval, int32_t yval, int32_t zval) { x = xval; y = yval; z = zval; }
	void fill(int32_t val) { x = val; y = val; z = val; }
};

struct xt_int4 {
	int32_t x, y, z, w;

	void set(int32_t xval, int32_t yval, int32_t zval, int32_t wval) { x = xval; y = yval; z = zval; w = wval; }
	void fill(int32_t val) { x = val; y = val; z = val; w = val; }

	void set_at(int at, int32_t val) {
		switch (at) {
			case 0: x = val; break;
			case 1: y = val; break;
			case 2: z = val; break;
			case 3: w = val; break;
			default: break;
		}
	}
};

struct xt_half {
	uint16_t x;

	void set(float f);
	float get() const;
};

struct xt_half2 {
	uint16_t x, y;

	void set(float fx, float fy);
	void set(const xt_float2& fv) { set(fv.x, fv.y); }
	xt_float2 get() const;
};

struct xt_half3 {
	uint16_t x, y, z;

	void set(float fx, float fy, float fz);
	void set(const xt_float3& fv) { set(fv.x, fv.y, fv.z); }
	xt_float3 get() const;
};

struct xt_half4 {
	uint16_t x, y, z, w;

	void set(float fx, float fy, float fz, float fw);
	void set(const xt_float4& fv) { set(fv.x, fv.y, fv.z, fv.w); }
	xt_float4 get() const;
};

struct xt_texcoord {
	float u, v;

	void set(float u, float v) { this->u = u; this->v = v; }
	void fill(float val) { u = val; v = val; }
	void flip_v() { v = 1.0f - v; }
	void negate_v() { v = -v; }
	void scroll(const xt_texcoord& step);
	void encode_half(uint16_t* pDst) const;
};

union uxVal32 {
	int32_t i;
	uint32_t u;
	float f;
	uint8_t b[4];
};

union uxVal64 {
	int64_t i;
	uint64_t u;
	double f;
	uint8_t b[8];
};


enum class exAnimChan : int8_t {
	UNKNOWN = -1,
	TX = 0,
	TY = 1,
	TZ = 2,
	RX = 3,
	RY = 4,
	RZ = 5,
	SX = 6,
	SY = 7,
	SZ = 8
};

enum class exTransformOrd : uint8_t {
	SRT = 0,
	STR = 1,
	RST = 2,
	RTS = 3,
	TSR = 4,
	TRS = 5
};

enum class exRotOrd : uint8_t {
	XYZ = 0,
	XZY = 1,
	YXZ = 2,
	YZX = 3,
	ZXY = 4,
	ZYX = 5
};

enum class exAxis : uint8_t {
	PLUS_X = 0,
	MINUS_X = 1,
	PLUS_Y = 2,
	MINUS_Y = 3,
	PLUS_Z = 4,
	MINUS_Z = 5
};


class cxVec;
class cxMtx;
class cxQuat;

class cxVec : public xt_float3 {
public:
	cxVec() {}
	cxVec(float x, float y, float z) { set(x, y, z); }
	cxVec(float s) { fill(s); }

	cxVec(const cxVec& v) = default;
	cxVec& operator=(const cxVec& v) = default;

	void zero() { fill(0.0f); }

	void parse(const char* pStr);
	void from_mem(const float* pSrc) { set(pSrc[0], pSrc[1], pSrc[2]); }
	void to_mem(float* pDst) const {
		pDst[0] = x;
		pDst[1] = y;
		pDst[2] = z;
	}

	float operator [](int i) const { return get_at(i); }

	void add(const cxVec& v) {
		x += v.x;
		y += v.y;
		z += v.z;
	}
	void sub(const cxVec& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
	}
	void mul(const cxVec& v) {
		x *= v.x;
		y *= v.y;
		z *= v.z;
	}
	void div(const cxVec& v) {
		x /= v.x;
		y /= v.y;
		z /= v.z;
	}
	void scl(float s) {
		x *= s;
		y *= s;
		z *= s;
	}
	void neg() {
		x = -x;
		y = -y;
		z = -z;
	}
	void abs() {
		x = ::fabsf(x);
		y = ::fabsf(y);
		z = ::fabsf(z);
	}

	cxVec neg_val() const {
		cxVec v = *this;
		v.neg();
		return v;
	}

	cxVec abs_val() const {
		cxVec v = *this;
		v.abs();
		return v;
	}

	cxVec& operator += (const cxVec& v) { add(v); return *this; }
	cxVec& operator -= (const cxVec& v) { sub(v); return *this; }
	cxVec& operator *= (const cxVec& v) { mul(v); return *this; }
	cxVec& operator /= (const cxVec& v) { div(v); return *this; }

	cxVec& operator *= (float s) { scl(s); return *this; }
	cxVec& operator /= (float s) { scl(1.0f / s); return *this; }

	void div0(const cxVec& v) {
		x = nxCalc::div0(x, v.x);
		y = nxCalc::div0(y, v.y);
		z = nxCalc::div0(z, v.z);
	}

	void div0(float s) { scl(s != 0.0f ? 1.0f / s : 0.0f); }

	cxVec inv_val() const {
		cxVec v(1.0f);
		v.div0(*this);
		return v;
	}
	void inv() { *this = inv_val(); }

	float min_elem(bool absFlg = false) const {
		cxVec v = *this;
		if (absFlg) {
			v.abs();
		}
		return nxCalc::min(v.x, v.y, v.z);
	}

	float min_abs_elem() const { return min_elem(true); }

	float max_elem(bool absFlg = false) const {
		cxVec v = *this;
		if (absFlg) {
			v.abs();
		}
		return nxCalc::max(v.x, v.y, v.z);
	}

	float max_abs_elem() const { return max_elem(true); }

	float mag2() const { return dot(*this); }
	float mag_fast() const { return ::sqrtf(mag2()); }
	float mag() const;
	float length() const { return mag(); }

	void normalize(const cxVec& v);
	void normalize() { normalize(*this); }
	cxVec get_normalized() const {
		cxVec n;
		n.normalize(*this);
		return n;
	}

	float azimuth() const { return ::atan2f(x, z); }
	float elevation() const { return -::atan2f(y, ::hypotf(x, z)); }

	void min(const cxVec& v) {
		x = nxCalc::min(x, v.x);
		y = nxCalc::min(y, v.y);
		z = nxCalc::min(z, v.z);
	}

	void min(const cxVec& v1, const cxVec& v2) {
		x = nxCalc::min(v1.x, v2.x);
		y = nxCalc::min(v1.y, v2.y);
		z = nxCalc::min(v1.z, v2.z);
	}

	void max(const cxVec& v) {
		x = nxCalc::max(x, v.x);
		y = nxCalc::max(y, v.y);
		z = nxCalc::max(z, v.z);
	}

	void max(const cxVec& v1, const cxVec& v2) {
		x = nxCalc::max(v1.x, v2.x);
		y = nxCalc::max(v1.y, v2.y);
		z = nxCalc::max(v1.z, v2.z);
	}

	void clamp(const cxVec& v, const cxVec& vmin, const cxVec& vmax) {
		x = nxCalc::clamp(v.x, vmin.x, vmax.x);
		y = nxCalc::clamp(v.y, vmin.y, vmax.y);
		z = nxCalc::clamp(v.z, vmin.z, vmax.z);
	}

	void clamp(const cxVec& vmin, const cxVec& vmax) {
		x = nxCalc::clamp(x, vmin.x, vmax.x);
		y = nxCalc::clamp(y, vmin.y, vmax.y);
		z = nxCalc::clamp(z, vmin.z, vmax.z);
	}

	void saturate(const cxVec& v) {
		x = nxCalc::saturate(v.x);
		y = nxCalc::saturate(v.y);
		z = nxCalc::saturate(v.z);
	}

	void saturate() {
		x = nxCalc::saturate(x);
		y = nxCalc::saturate(y);
		z = nxCalc::saturate(z);
	}

	cxVec get_clamped(const cxVec& vmin, const cxVec& vmax) const {
		cxVec v;
		v.clamp(*this, vmin, vmax);
		return v;
	}

	cxVec get_saturated() const {
		cxVec v;
		v.saturate(*this);
		return v;
	}

	float dot(const cxVec& v) const {
		return x*v.x + y*v.y + z*v.z;
	}

	void cross(const cxVec& v1, const cxVec& v2) {
		float v10 = v1.x;
		float v11 = v1.y;
		float v12 = v1.z;
		float v20 = v2.x;
		float v21 = v2.y;
		float v22 = v2.z;
		x = v11*v22 - v12*v21;
		y = v12*v20 - v10*v22;
		z = v10*v21 - v11*v20;
	}

	void lerp(const cxVec& v1, const cxVec& v2, float t) {
		x = nxCalc::lerp(v1.x, v2.x, t);
		y = nxCalc::lerp(v1.y, v2.y, t);
		z = nxCalc::lerp(v1.z, v2.z, t);
	}

	xt_float2 encode_octa() const;
	void decode_octa(const xt_float2& oct);
};

inline cxVec operator + (const cxVec& v1, const cxVec& v2) { cxVec v = v1; v.add(v2); return v; }
inline cxVec operator - (const cxVec& v1, const cxVec& v2) { cxVec v = v1; v.sub(v2); return v; }
inline cxVec operator * (const cxVec& v1, const cxVec& v2) { cxVec v = v1; v.mul(v2); return v; }
inline cxVec operator / (const cxVec& v1, const cxVec& v2) { cxVec v = v1; v.div(v2); return v; }
inline cxVec operator * (const cxVec& v0, float s) { cxVec v = v0; v.scl(s); return v; }
inline cxVec operator * (float s, const cxVec& v0) { cxVec v = v0; v.scl(s); return v; }

namespace nxVec {

inline cxVec cross(const cxVec& v1, const cxVec& v2) {
	cxVec cv;
	cv.cross(v1, v2);
	return cv;
}

inline float scalar_triple(const cxVec& v0, const cxVec& v1, const cxVec& v2) {
	return cross(v0, v1).dot(v2);
}

inline cxVec lerp(const cxVec& v1, const cxVec& v2, float t) {
	cxVec v;
	v.lerp(v1, v2, t);
	return v;
}

inline cxVec min(const cxVec& v1, const cxVec& v2) {
	cxVec v;
	v.min(v1, v2);
	return v;
}

inline cxVec max(const cxVec& v1, const cxVec& v2) {
	cxVec v;
	v.max(v1, v2);
	return v;
}

float dist2(const cxVec& pos0, const cxVec& pos1);
float dist(const cxVec& pos0, const cxVec& pos1);

cxVec get_axis(exAxis axis);
cxVec reflect(const cxVec& vec, const cxVec& nrm);

} // nxVec


class cxMtx : public xt_mtx {
protected:
	void set_rot_n(const cxVec& axis, float ang);

public:
	cxMtx() {}

	void identity_sr();

	void from_mem(const float* pSrc) { ::memcpy(this, pSrc, sizeof(cxMtx)); }
	void to_mem(float* pDst) const { ::memcpy(pDst, this, sizeof(cxMtx)); }

	void from_quat(const cxQuat& q);
	void from_quat_and_pos(const cxQuat& qrot, const cxVec& vtrans);
	cxQuat to_quat() const;

	void transpose();
	void transpose(const cxMtx& mtx);
	void transpose_sr();
	void transpose_sr(const cxMtx& mtx);
	void invert();
	void invert(const cxMtx& m);

	void mul(const cxMtx& mtx);
	void mul(const cxMtx& m1, const cxMtx& m2);

	cxVec get_translation() const {
		return cxVec(m[3][0], m[3][1], m[3][2]);
	}

	cxVec get_row_vec(int i) const {
		cxVec v(0.0f);
		switch (i) {
			case 0: v.set(m[0][0], m[0][1], m[0][2]); break;
			case 1: v.set(m[1][0], m[1][1], m[1][2]); break;
			case 2: v.set(m[2][0], m[2][1], m[2][2]); break;
			case 3: v.set(m[3][0], m[3][1], m[3][2]); break;
		}
		return v;
	}

	void set_translation(float tx, float ty, float tz) {
		m[3][0] = tx;
		m[3][1] = ty;
		m[3][2] = tz;
		m[3][3] = 1.0f;
	}

	void set_translation(const cxVec& tv) {
		set_translation(tv.x, tv.y, tv.z);
	}

	void mk_translation(const cxVec& tv) {
		identity_sr();
		set_translation(tv);
	}

	cxMtx get_transposed() const {
		cxMtx m;
		m.transpose(*this);
		return m;
	}

	cxMtx get_transposed_sr() const {
		cxMtx m;
		m.transpose_sr(*this);
		return m;
	}

	cxMtx get_inverted() const {
		cxMtx m;
		m.invert(*this);
		return m;
	}

	cxMtx get_sr() const {
		cxMtx m = *this;
		m.set_translation(cxVec(0.0f));
		return m;
	}

	void orient_zy(const cxVec& axisZ, const cxVec& axisY, bool normalizeInput = true);
	void orient_zx(const cxVec& axisZ, const cxVec& axisX, bool normalizeInput = true);

	void set_rot_frame(const cxVec& axisX, const cxVec& axisY, const cxVec& axisZ);
	void set_rot(const cxVec& axis, float ang);
	void set_rot_x(float rx);
	void set_rot_y(float ry);
	void set_rot_z(float rz);
	void set_rot(float rx, float ry, float rz, exRotOrd ord = exRotOrd::XYZ);
	void set_rot_degrees(const cxVec& r, exRotOrd ord = exRotOrd::XYZ);

	bool is_valid_rot(float tol = 1.0e-3f) const;
	cxVec get_rot(exRotOrd ord = exRotOrd::XYZ) const;
	cxVec get_rot_degrees(exRotOrd ord = exRotOrd::XYZ) const { return get_rot(ord) * XD_RAD2DEG(1.0f); }

	void mk_scl(float sx, float sy, float sz);
	void mk_scl(const cxVec& sv) { mk_scl(sv.x, sv.y, sv.z); }
	void mk_scl(float s) { mk_scl(s, s, s); }

	cxVec get_scl(cxMtx* pMtxRT = nullptr, exTransformOrd xord = exTransformOrd::SRT) const;

	void calc_xform(const cxMtx& mtxT, const cxMtx& mtxR, const cxMtx& mtxS, exTransformOrd ord = exTransformOrd::SRT);

	void mk_view(const cxVec& pos, const cxVec& tgt, const cxVec& upvec);
	void mk_proj(float fovy, float aspect, float znear, float zfar);

	cxVec calc_vec(const cxVec& v) const;
	cxVec calc_pnt(const cxVec& v) const;
	xt_float4 apply(const xt_float4& qv) const;
};

inline cxMtx operator * (const cxMtx& m1, const cxMtx& m2) { cxMtx m = m1; m.mul(m2); return m; }

namespace nxMtx {

inline cxMtx orient_zy(const cxVec& axisZ, const cxVec& axisY, bool normalizeInput = true) {
	cxMtx m;
	m.orient_zy(axisZ, axisY, normalizeInput);
	return m;
}

inline cxMtx orient_zx(const cxVec& axisZ, const cxVec& axisX, bool normalizeInput = true) {
	cxMtx m;
	m.orient_zx(axisZ, axisX, normalizeInput);
	return m;
}

inline cxMtx from_axis_angle(const cxVec& axis, float ang) {
	cxMtx m;
	m.set_rot(axis, ang);
	return m;
}

inline cxMtx mk_rot_frame(const cxVec& axisX, const cxVec& axisY, const cxVec& axisZ) {
	cxMtx m;
	m.set_rot_frame(axisX, axisY, axisZ);
	return m;
}

inline cxMtx mk_scl(const cxVec sv) {
	cxMtx m;
	m.mk_scl(sv);
	return m;
}

inline cxMtx mk_translation(const cxVec& tv) {
	cxMtx m;
	m.mk_translation(tv);
	return m;
}

cxVec wmtx_calc_vec(const xt_wmtx& m, const cxVec& v);
cxVec wmtx_calc_pnt(const xt_wmtx& m, const cxVec& v);

} // nxMtx


class cxQuat : public xt_float4 {
public:
	cxQuat() {}
	cxQuat(float xval, float yval, float zval, float wval) { set(xval, yval, zval, wval); }

	cxQuat(const cxQuat& q) = default;
	cxQuat& operator=(const cxQuat& q) = default;

	void identity() { set(0.0f, 0.0f, 0.0f, 1.0f); }

	cxVec get_axis_x() const { return cxVec(1.0f - (2.0f*y*y) - (2.0f*z*z), (2.0f*x*y) + (2.0f*w*z), (2.0f*x*z) - (2.0f*w*y)); }
	cxVec get_axis_y() const { return cxVec((2.0f*x*y) - (2.0f*w*z), 1.0f - (2.0f*x*x) - (2.0f*z*z), (2.0f*y*z) + (2.0f*w*x)); }
	cxVec get_axis_z() const { return cxVec((2.0f*x*z) + (2.0f*w*y), (2.0f*y*z) - (2.0f*w*x), 1.0f - (2.0f*x*x) - (2.0f*y*y)); }

	void from_mtx(const cxMtx& m);
	cxMtx to_mtx() const;
	float get_axis_ang(cxVec* pAxis) const;
	cxVec get_log_vec() const;
	void from_log_vec(const cxVec& lvec, bool nrmFlg = true);
	void from_vecs(const cxVec& vfrom, const cxVec& vto);

	float dot(const cxQuat& q) const { return x*q.x + y*q.y + z*q.z + w*q.w; }

	float mag2() const { return dot(*this); }
	float mag() const { return ::sqrtf(mag2()); }

	void mul(const cxQuat& qa, const cxQuat& qb) {
		float tx = qa.w*qb.x + qa.x*qb.w + qa.y*qb.z - qa.z*qb.y;
		float ty = qa.w*qb.y + qa.y*qb.w + qa.z*qb.x - qa.x*qb.z;
		float tz = qa.w*qb.z + qa.z*qb.w + qa.x*qb.y - qa.y*qb.x;
		float tw = qa.w*qb.w - qa.x*qb.x - qa.y*qb.y - qa.z*qb.z;
		set(tx, ty, tz, tw);
	}

	void mul(const cxQuat& q) { mul(*this, q); }

	void normalize() {
		float len = mag();
		if (len > 0.0f) {
			float rlen = 1.0f / len;
			x *= rlen;
			y *= rlen;
			z *= rlen;
			w *= rlen;
		}
	}

	void normalize(const cxQuat& q) {
		*this = q;
		normalize();
	}

	cxQuat get_normalized() const { cxQuat q; q.normalize(*this); return q; }

	void conjugate() {
		x = -x;
		y = -y;
		z = -z;
	}

	void conjugate(const cxQuat& q) {
		x = -q.x;
		y = -q.y;
		z = -q.z;
		w = q.w;
	}

	cxQuat get_conjugated() const { cxQuat q; q.conjugate(*this); return q; }

	void invert() {
		float lsq = mag2();
		float rlsq = nxCalc::rcp0(lsq);
		conjugate();
		x *= rlsq;
		y *= rlsq;
		z *= rlsq;
		w *= rlsq;
	}

	void invert(const cxQuat& q) {
		*this = q;
		invert();
	}

	cxQuat get_inverted() const { cxQuat q; q.invert(*this); return q; }

	void set_rot(const cxVec& axis, float ang);
	void set_rot_x(float rx);
	void set_rot_y(float ry);
	void set_rot_z(float rz);
	void set_rot(float rx, float ry, float rz, exRotOrd ord = exRotOrd::XYZ);
	void set_rot_degrees(const cxVec& r, exRotOrd ord = exRotOrd::XYZ);

	cxVec get_rot(exRotOrd ord = exRotOrd::XYZ) const;
	cxVec get_rot_degrees(exRotOrd ord = exRotOrd::XYZ) const { return get_rot(ord) * XD_RAD2DEG(1.0f); }

	void slerp(const cxQuat& q1, const cxQuat& q2, float t);

	cxVec apply(const cxVec& v) const;
};

inline cxQuat operator * (const cxQuat& q1, const cxQuat& q2) { cxQuat q = q1; q.mul(q2); return q; }

namespace nxQuat {

inline cxQuat slerp(const cxQuat& q1, const cxQuat& q2, float t) {
	cxQuat q;
	q.slerp(q1, q2, t);
	return q;
}


} // nxQuat


class cxLineSeg;
class cxPlane;
class cxSphere;
class cxAABB;
class cxCapsule;

namespace nxGeom {

inline cxVec tri_normal_cw(const cxVec& v0, const cxVec& v1, const cxVec& v2) {
	return nxVec::cross(v0 - v1, v2 - v1).get_normalized();
}

inline cxVec tri_normal_ccw(const cxVec& v0, const cxVec& v1, const cxVec& v2) {
	return nxVec::cross(v1 - v0, v2 - v0).get_normalized();
}

inline float signed_tri_area_2d(float ax, float ay, float bx, float by, float cx, float cy) {
	return (ax - cx)*(by - cy) - (ay - cy)*(bx - cx);
}

bool seg_seg_overlap_2d(float s0x0, float s0y0, float s0x1, float s0y1, float s1x0, float s1y0, float s1x1, float s1y1);
bool seg_plane_intersect(const cxVec& p0, const cxVec& p1, const cxPlane& pln, float* pT = nullptr);
bool seg_quad_intersect_cw(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& v3, cxVec* pHitPos = nullptr, cxVec* pHitNrm = nullptr);
bool seg_quad_intersect_cw_n(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& v3, const cxVec& nrm, cxVec* pHitPos = nullptr);
bool seg_quad_intersect_ccw(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& v3, cxVec* pHitPos = nullptr, cxVec* pHitNrm = nullptr);
bool seg_quad_intersect_ccw_n(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& v3, const cxVec& nrm, cxVec* pHitPos = nullptr);
bool seg_polyhedron_intersect(const cxVec& p0, const cxVec& p1, const cxPlane* pPln, int plnNum, float* pFirst = nullptr, float* pLast = nullptr);
bool seg_tri_intersect_cw_n(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& nrm, cxVec* pHitPos = nullptr);
bool seg_tri_intersect_cw(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, cxVec* pHitPos = nullptr, cxVec* pHitNrm = nullptr);
bool seg_tri_intersect_ccw_n(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& nrm, cxVec* pHitPos = nullptr);
bool seg_tri_intersect_ccw(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, cxVec* pHitPos = nullptr, cxVec* pHitNrm = nullptr);
cxVec barycentric(const cxVec& pos, const cxVec& v0, const cxVec& v1, const cxVec& v2);
float quad_dist2(const cxVec& pos, const cxVec vtx[4]);
int quad_convex_ck(const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& v3);
void update_nrm_newell(cxVec* pNrm, cxVec* pVtxI, cxVec* pVtxJ);
cxVec poly_normal_cw(cxVec* pVtx, int vtxNum);
cxVec poly_normal_ccw(cxVec* pVtx, int vtxNum);

inline float line_pnt_closest(const cxVec& lp0, const cxVec& lp1, const cxVec& pnt, cxVec* pClosestPos = nullptr, cxVec* pLineDir = nullptr) {
	cxVec dir = lp1 - lp0;
	cxVec vec = pnt - lp0;
	float t = nxCalc::div0(vec.dot(dir), dir.dot(dir));
	if (pClosestPos) {
		*pClosestPos = lp0 + dir*t;
	}
	if (pLineDir) {
		*pLineDir = dir;
	}
	return t;
}

inline cxVec seg_pnt_closest(const cxVec& sp0, const cxVec& sp1, const cxVec& pnt) {
	cxVec dir;
	float t = nxCalc::saturate(line_pnt_closest(sp0, sp1, pnt, nullptr, &dir));
	return sp0 + dir*t;
}

float seg_seg_dist2(const cxVec& s0p0, const cxVec& s0p1, const cxVec& s1p0, const cxVec& s1p1, cxVec* pBridgeP0 = nullptr, cxVec* pBridgeP1 = nullptr);

inline bool pnt_in_aabb(const cxVec& pos, const cxVec& min, const cxVec& max) {
	if (pos.x < min.x || pos.x > max.x) return false;
	if (pos.y < min.y || pos.y > max.y) return false;
	if (pos.z < min.z || pos.z > max.z) return false;
	return true;
}

inline bool pnt_in_sph(const cxVec& pos, const cxVec& sphc, float sphr, float* pSqDist = nullptr) {
	float d2 = nxVec::dist2(pos, sphc);
	float r2 = nxCalc::sq(sphr);
	if (pSqDist) {
		*pSqDist = d2;
	}
	return d2 <= r2;
}

inline bool pnt_in_cap(const cxVec& pos, const cxVec& cp0, const cxVec& cp1, float cr, float* pSqDist = nullptr) {
	float d2 = nxVec::dist2(pos, seg_pnt_closest(cp0, cp1, pos));
	float r2 = nxCalc::sq(cr);
	if (pSqDist) {
		*pSqDist = d2;
	}
	return d2 <= r2;
}

inline bool aabb_aabb_overlap(const cxVec& min0, const cxVec& max0, const cxVec& min1, const cxVec& max1) {
	if (min0.x > max1.x || max0.x < min1.x) return false;
	if (min0.y > max1.y || max0.y < min1.y) return false;
	if (min0.z > max1.z || max0.z < min1.z) return false;
	return true;
}

inline bool sph_sph_overlap(const cxVec& s0c, float s0r, const cxVec& s1c, float s1r) {
	float cdd = nxVec::dist2(s0c, s1c);
	float rsum = s0r + s1r;
	return cdd <= nxCalc::sq(rsum);
}

inline bool sph_aabb_overlap(const cxVec& sc, float sr, const cxVec& bmin, const cxVec& bmax) {
	cxVec pos = sc.get_clamped(bmin, bmax);
	float dd = nxVec::dist2(sc, pos);
	return dd <= nxCalc::sq(sr);
}

inline bool sph_cap_overlap(const cxVec& sc, float sr, const cxVec& cp0, const cxVec& cp1, float cr, cxVec* pCapAxisPos = nullptr) {
	cxVec pos = seg_pnt_closest(cp0, cp1, sc);
	bool flg = sph_sph_overlap(sc, sr, pos, cr);
	if (flg && pCapAxisPos) {
		*pCapAxisPos = pos;
	}
	return flg;
}

inline bool cap_cap_overlap(const cxVec& c0p0, const cxVec& c0p1, float cr0, const cxVec& c1p0, const cxVec& c1p1, float cr1) {
	float dist2 = seg_seg_dist2(c0p0, c0p1, c1p0, c1p1);
	float rsum2 = nxCalc::sq(cr0 + cr1);
	return dist2 <= rsum2;
}

bool cap_aabb_overlap(const cxVec& cp0, const cxVec& cp1, float cr, const cxVec& bmin, const cxVec& bmax);

} // nxGeom


class cxLineSeg {
protected:
	cxVec mPos0;
	cxVec mPos1;

public:
	cxLineSeg() {}
	cxLineSeg(const cxVec& p0, const cxVec& p1) { set(p0, p1); }

	cxVec get_pos0() const { return mPos0; }
	cxVec* get_pos0_ptr() { return &mPos0; }
	void set_pos0(const cxVec& p0) { mPos0 = p0; }
	cxVec get_pos1() const { return mPos1; }
	cxVec* get_pos1_ptr() { return &mPos1; }
	cxVec get_inner_pos(float t) const { return nxVec::lerp(mPos0, mPos1, nxCalc::saturate(t)); }
	void set_pos1(const cxVec& p1) { mPos1 = p1; }
	void set(const cxVec& p0, const cxVec& p1) { mPos0 = p0; mPos1 = p1; }
	cxVec get_center() const { return (mPos0 + mPos1) * 0.5f; }
	cxVec get_dir() const { return mPos1 - mPos0; }
	cxVec get_dir_nrm() const { return get_dir().get_normalized(); }
	float len2() const { return nxVec::dist2(mPos0, mPos1); }
	float len() const { return nxVec::dist(mPos0, mPos1); }
};

class cxPlane {
protected:
	cxVec mABC;
	float mD;

public:
	cxPlane() {}

	void calc(const cxVec& pos, const cxVec& nrm);
	cxVec get_normal() const { return mABC; }
	float get_D() const { return mD; }
	float signed_dist(const cxVec& pos) const { return pos.dot(get_normal()) - get_D(); }
	float dist(const cxVec& pos) const { return ::fabsf(signed_dist(pos)); }
	bool pnt_in_front(const cxVec& pos) const { return signed_dist(pos) >= 0.0f; }
	bool seg_intersect(const cxVec& p0, const cxVec& p1, float* pT = nullptr) const { return nxGeom::seg_plane_intersect(p0, p1, *this, pT); }
};

class cxSphere {
protected:
	xt_float4 mSph;

public:
	cxSphere() {}
	cxSphere(const cxVec& c, float r) {
		mSph.set(c.x, c.y, c.z, r);
	}
	cxSphere(float x, float y, float z, float r) {
		mSph.set(x, y, z, r);
	}

	float get_radius() const {
		return mSph.w;
	}

	void set_radius(float r) {
		mSph.w = r;
	}

	cxVec get_center() const {
		cxVec c;
		c.set(mSph.x, mSph.y, mSph.z);
		return c;
	}

	void set_center(const cxVec& c) {
		mSph.x = c.x;
		mSph.y = c.y;
		mSph.z = c.z;
	}

	bool contains(const cxVec& pos, float* pSqDist = nullptr) const { return nxGeom::pnt_in_sph(pos, get_center(), get_radius(), pSqDist); }

	bool overlaps(const cxSphere& sph) const { return nxGeom::sph_sph_overlap(get_center(), get_radius(), sph.get_center(), sph.get_radius()); }
	bool overlaps(const cxAABB& box) const;
	bool overlaps(const cxCapsule& cap, cxVec* pCapAxisPos = nullptr) const;
};

class cxAABB {
protected:
	cxVec mMin;
	cxVec mMax;

public:
	cxAABB() {}
	cxAABB(const cxVec& p0, const cxVec& p1) { set(p0, p1); }

	void init() {
		mMin.fill(FLT_MAX);
		mMax.fill(-FLT_MAX);
	}

	void set(const cxVec& pnt) {
		mMin = pnt;
		mMax = pnt;
	}

	void set(const cxVec& p0, const cxVec& p1) {
		mMin.min(p0, p1);
		mMax.max(p0, p1);
	}

	cxVec get_min_pos() const { return mMin; }
	cxVec get_max_pos() const { return mMax; }
	cxVec get_center() const { return ((mMin + mMax) * 0.5f); }
	cxVec get_size_vec() const { return mMax - mMin; }
	cxVec get_radius_vec() const { return get_size_vec() * 0.5f; }
	float get_bounding_radius() const { return get_radius_vec().mag(); }

	void add_pnt(const cxVec& p) {
		mMin.min(p);
		mMax.max(p);
	}

	void merge(const cxAABB& box) {
		mMin.min(box.mMin);
		mMax.max(box.mMax);
	}

	void transform(const cxAABB& box, const cxMtx& mtx);
	void transform(const cxMtx& mtx) { transform(*this, mtx); }

	void from_seg(const cxLineSeg& seg) { set(seg.get_pos0(), seg.get_pos1()); }
	void from_sph(const cxSphere& sph);

	cxVec closest_pnt(const cxVec& pos, bool onFace = true) const;

	bool contains(const cxVec& pos) const { return nxGeom::pnt_in_aabb(pos, mMin, mMax); }

	bool seg_ck(const cxVec& p0, const cxVec& p1) const;
	bool seg_ck(const cxLineSeg& seg) const { return seg_ck(seg.get_pos0(), seg.get_pos1()); }

	bool overlaps(const cxSphere& sph) const { return sph.overlaps(*this); }
	bool overlaps(const cxAABB& box) const { return nxGeom::aabb_aabb_overlap(mMin, mMax, box.mMin, box.mMax); }
};

class cxCapsule {
protected:
	cxVec mPos0;
	cxVec mPos1;
	float mRadius;

public:
	cxCapsule() {}
	cxCapsule(const cxVec& p0, const cxVec& p1, float radius) { set(p0, p1, radius); }

	void set(const cxVec& p0, const cxVec& p1, float radius) {
		mPos0 = p0;
		mPos1 = p1;
		mRadius = radius;
	}

	cxVec get_pos0() const { return mPos0; }
	cxVec get_pos1() const { return mPos1; }
	float get_radius() const { return mRadius; }
	cxVec get_center() const { return (mPos0 + mPos1) * 0.5f; }
	float get_bounding_radius() const { return nxVec::dist(mPos0, mPos1)*0.5f + mRadius; }

	bool contains(const cxVec& pos, float* pSqDist = nullptr) const { return nxGeom::pnt_in_cap(pos, get_pos0(), get_pos1(), get_radius(), pSqDist); }

	bool overlaps(const cxSphere& sph, cxVec* pAxisPos = nullptr) const { return sph.overlaps(*this, pAxisPos); }
	bool overlaps(const cxAABB& box) const;
	bool overlaps(const cxCapsule& cap) const;
};

class cxFrustum {
protected:
	cxVec mPnt[8];
	cxVec mNrm[6];
	cxPlane mPlane[6];

	void calc_normals();
	void calc_planes();

public:
	cxFrustum() {}

	void init(const cxMtx& mtx, float fovy, float aspect, float znear, float zfar);

	cxVec get_center() const;
	cxPlane get_near_plane() const { return mPlane[0]; }
	cxPlane get_far_plane() const { return mPlane[5]; }
	cxPlane get_left_plane() const { return mPlane[1]; }
	cxPlane get_right_plane() const { return mPlane[3]; }
	cxPlane get_top_plane() const { return mPlane[2]; }
	cxPlane get_bottom_plane() const { return mPlane[4]; }

	bool cull(const cxSphere& sph) const;
	bool overlaps(const cxSphere& sph) const;
	bool cull(const cxAABB& box) const;
	bool overlaps(const cxAABB& box) const;
};


class cxColor {
public:
	float r, g, b, a;

public:
	cxColor() {}
	cxColor(float val) { set(val); }
	cxColor(float r, float g, float b, float a = 1.0f) { set(r, g, b, a); }

	void set(float r, float g, float b, float a = 1.0f) {
		this->r = r;
		this->g = g;
		this->b = b;
		this->a = a;
	}
	void set(float val) { set(val, val, val); }

	void zero() {
		r = 0.0f;
		g = 0.0f;
		b = 0.0f;
		a = 0.0f;
	}

	float luma() const { return r*0.299f + g*0.587f + b*0.114f; }
	float luminance() const { return r*0.212671f + g*0.71516f + b*0.072169f; }

	void scl(float s) {
		r *= s;
		g *= s;
		b *= s;
		a *= s;
	}

	void scl_rgb(float s) {
		r *= s;
		g *= s;
		b *= s;
	}

	void scl_rgb(float sr, float sg, float sb) {
		r *= sr;
		g *= sg;
		b *= sb;
	}

	void add(const cxColor& c) {
		r += c.r;
		g += c.g;
		b += c.b;
		a += c.a;
	}

	void add_rgb(const cxColor& c) {
		r += c.r;
		g += c.g;
		b += c.b;
	}
};


struct sxStrList {
	uint32_t mSize;
	uint32_t mNum;
	uint32_t mOffs[1];

	bool ck_idx(int idx) const { return (uint32_t)idx < mNum; }
	uint16_t* get_hash_top() const { return (uint16_t*)&mOffs[mNum]; }
	const char* get_str(int idx) const { return ck_idx(idx) ? reinterpret_cast<const char*>(this) + mOffs[idx] : nullptr; }
	bool is_sorted() const;
	int find_str(const char* pStr) const;
	int find_str_any(const char** ppStrs, int n) const;
};

struct sxVecList {
	struct Entry {
		uint32_t mOrg : 30;
		uint32_t mLen : 2;
	};

	uint32_t mSize;
	uint32_t mNum;
	Entry mTbl[1];

	bool ck_idx(int idx) const { return (uint32_t)idx < mNum; }
	const float* get_ptr(int idx) const { return ck_idx(idx) ? reinterpret_cast<const float*>(&mTbl[mNum]) + mTbl[idx].mOrg : nullptr; }
	int get_elems(float* pDst, int idx, int num) const;
	xt_float2 get_f2(int idx);
	xt_float3 get_f3(int idx);
	xt_float4 get_f4(int idx);
};

struct sxData {
	uint32_t mKind;
	uint32_t mFlags;
	uint32_t mFileSize;
	uint32_t mHeadSize;
	uint32_t mOffsStr;
	int16_t mNameId;
	int16_t mPathId;
	uint32_t mFilePathLen;
	uint32_t mReserved;

	sxStrList* get_str_list() const { return mOffsStr ? (sxStrList*)XD_INCR_PTR(this, mOffsStr) : nullptr; }
	const char* get_str(int id) const { sxStrList* pStrLst = get_str_list(); return pStrLst ? pStrLst->get_str(id) : nullptr; }
	int find_str(const char* pStr) const { return pStr && mOffsStr ? get_str_list()->find_str(pStr) : -1; }
	bool has_file_path() const { return !!mFilePathLen; }
	const char* get_file_path() const { return has_file_path() ? (const char*)XD_INCR_PTR(this, mFileSize) : nullptr; }
	const char* get_name() const { return get_str(mNameId); }
	const char* get_base_path() const { return get_str(mPathId); }

	template<typename T> bool is() const { return mKind == T::KIND; }
	template<typename T> T* as() const { return is<T>() ? (T*)this : nullptr; }
};


struct sxValuesData : public sxData {
	uint32_t mOffsVec;
	uint32_t mOffsGrp;

	enum class eValType {
		UNKNOWN = 0,
		FLOAT = 1,
		VEC2 = 2,
		VEC3 = 3,
		VEC4 = 4,
		INT = 5,
		STRING = 6
	};

	struct ValInfo {
		int16_t mNameId;
		int8_t mType; /* ValType */
		int8_t mReserved;
		union {
			float f;
			int32_t i;
		} mValId;

		eValType get_type() const { return (eValType)mType; }
	};

	struct GrpInfo {
		int16_t mNameId;
		int16_t mPathId;
		int16_t mTypeId;
		int16_t mReserved;
		int32_t mValNum;
		ValInfo mVals[1];
	};

	struct GrpList {
		uint32_t mNum;
		uint32_t mOffs[1];
	};

	class Group {
	private:
		const sxValuesData* mpVals;
		int32_t mGrpId;

		Group() {}

		friend struct sxValuesData;

	public:
		bool is_valid() const { return mpVals && mpVals->ck_grp_idx(mGrpId); }
		bool ck_val_idx(int idx) const { return (uint32_t)idx < (uint32_t)get_val_num(); }
		const char* get_name() const { return is_valid() ? mpVals->get_str(get_info()->mNameId) : nullptr; }
		const char* get_path() const { return is_valid() ? mpVals->get_str(get_info()->mPathId) : nullptr; }
		const char* get_type() const { return is_valid() ? mpVals->get_str(get_info()->mTypeId) : nullptr; }
		const GrpInfo* get_info() const { return is_valid() ? mpVals->get_grp_info(mGrpId) : nullptr; }
		int get_val_num() const { return is_valid() ? get_info()->mValNum : 0; }
		int find_val_idx(const char* pName) const;
		int find_val_idx_any(const char** ppNames, int n) const;
		const char* get_val_name(int idx) const { return ck_val_idx(idx) ? mpVals->get_str(get_info()->mVals[idx].mNameId) : nullptr; }
		eValType get_val_type(int idx) const { return ck_val_idx(idx) ? get_info()->mVals[idx].get_type() : eValType::UNKNOWN; }
		const ValInfo* get_val_info(int idx) const { return ck_val_idx(idx) ? &get_info()->mVals[idx] : nullptr; }
		int get_val_i(int idx) const;
		float get_val_f(int idx) const;
		xt_float2 get_val_f2(int idx) const;
		xt_float3 get_val_f3(int idx) const;
		xt_float4 get_val_f4(int idx) const;
		const char* get_val_s(int idx) const;
		float get_float(const char* pName, const float defVal = 0.0f) const;
		float get_float_any(const char** ppNames, int n, const float defVal = 0.0f) const;
		int get_int(const char* pName, const int defVal = 0) const;
		int get_int_any(const char** ppNames, int n, const int defVal = 0) const;
		cxVec get_vec(const char* pName, const cxVec& defVal = cxVec(0.0f)) const;
		cxVec get_vec_any(const char** ppNames, int n, const cxVec& defVal = cxVec(0.0f)) const;
		cxColor get_rgb(const char* pName, const cxColor& defVal = cxColor(0.0f)) const;
		cxColor get_rgb_any(const char** ppNames, int n, const cxColor& defVal = cxColor(0.0f)) const;
		const char* get_str(const char* pName, const char* pDefVal = "") const;
		const char* get_str_any(const char** ppNames, int n, const char* pDefVal = "") const;
	};

	bool ck_grp_idx(int idx) const { return (uint32_t)idx < (uint32_t)get_grp_num(); }
	int find_grp_idx(const char* pName, const char* pPath = nullptr, int startIdx = 0) const;
	sxVecList* get_vec_list() const { return mOffsVec ? reinterpret_cast<sxVecList*>(XD_INCR_PTR(this, mOffsVec)) : nullptr; }
	GrpList* get_grp_list() const { return mOffsGrp ? reinterpret_cast<GrpList*>(XD_INCR_PTR(this, mOffsGrp)) : nullptr; }
	int get_grp_num() const { GrpList* pGrpLst = get_grp_list(); return pGrpLst ? pGrpLst->mNum : 0; };
	GrpInfo* get_grp_info(int idx) const { return ck_grp_idx(idx) ? reinterpret_cast<GrpInfo*>(XD_INCR_PTR(this, get_grp_list()->mOffs[idx])) : nullptr; }
	Group get_grp(int idx) const;
	Group find_grp(const char* pName, const char* pPath = nullptr) const { return get_grp(find_grp_idx(pName, pPath)); }

	static const char* get_val_type_str(eValType typ);

	static const uint32_t KIND = XD_FOURCC('X', 'V', 'A', 'L');
};

struct sxRigData : public sxData {
	uint32_t mNodeNum;
	uint32_t mLvlNum;
	uint32_t mOffsNode;
	uint32_t mOffsWMtx;
	uint32_t mOffsIMtx;
	uint32_t mOffsLMtx;
	uint32_t mOffsLPos;
	uint32_t mOffsLRot;
	uint32_t mOffsLScl;
	uint32_t mOffsInfo;

	enum class eInfoKind {
		LIST  = XD_FOURCC('L', 'I', 'S', 'T'),
		LIMBS = XD_FOURCC('L', 'I', 'M', 'B')
	};

	enum class eLimbType {
		LEG_L,
		LEG_R,
		ARM_L,
		ARM_R
	};

	struct Node {
		int16_t mSelfIdx;
		int16_t mParentIdx;
		int16_t mNameId;
		int16_t mPathId;
		int16_t mTypeId;
		int16_t mLvl;
		uint16_t mAttr;
		uint8_t mRotOrd;
		uint8_t mXfmOrd;

		exRotOrd get_rot_order() const { return (exRotOrd)mRotOrd; }
		exTransformOrd get_xform_order() const { return (exTransformOrd)mXfmOrd; }
		bool is_hrc_top() const { return mParentIdx < 0; }
	};

	struct Info {
		uint32_t mKind;
		uint32_t mOffs;
		uint32_t mNum;
		uint32_t mReserved;

		eInfoKind get_kind() const { return (eInfoKind)mKind; }
	};

	struct LimbInfo {
		int32_t mTopCtrl;
		int32_t mEndCtrl;
		int32_t mExtCtrl;
		int32_t mTop;
		int32_t mRot;
		int32_t mEnd;
		int32_t mExt;
		uint8_t mLimbId; /* type:4, idx:4 */
		uint8_t mAxis;
		uint8_t mUp;
		uint8_t mExtComp;

		eLimbType get_type() const { return (eLimbType)(mLimbId & 0xF); }
		int get_idx() const { return (mLimbId >> 4) & 0xF; }
		exAxis get_axis() const { return (exAxis)mAxis; }
		exAxis get_up_axis() const { return (exAxis)mUp; }
	};

	struct IKChain {
		class AdjustFunc {
		public:
			AdjustFunc() {}
			virtual ~AdjustFunc() {}
			virtual cxVec operator()(const sxRigData& rig, const IKChain& chain, const cxVec& pos) { return pos; }
		};

		struct Solution {
			cxMtx mTop;
			cxMtx mRot;
			cxMtx mEnd;
			cxMtx mExt;
		};

		int mTopCtrl;
		int mEndCtrl;
		int mExtCtrl;
		int mTop;
		int mRot;
		int mEnd;
		int mExt;
		exAxis mAxis;
		exAxis mUp;
		bool mExtCompensate;

		void set(LimbInfo* pInfo);
	};

	bool ck_node_idx(int idx) const { return (uint32_t)idx < mNodeNum; }
	int get_nodes_num() const { return mNodeNum; }
	int get_levels_num() const { return mLvlNum; }
	Node* get_node_top() const { return mOffsNode ? reinterpret_cast<Node*>(XD_INCR_PTR(this, mOffsNode)) : nullptr; }
	Node* get_node_ptr(int idx) const;
	const char* get_node_name(int idx) const;
	const char* get_node_path(int idx) const;
	const char* get_node_type(int idx) const;
	int find_node(const char* pName, const char* pPath = nullptr) const;
	exRotOrd get_rot_order(int idx) const { return ck_node_idx(idx) ? get_node_ptr(idx)->get_rot_order() : exRotOrd::XYZ; }
	exTransformOrd get_xform_order(int idx) const { return ck_node_idx(idx) ? get_node_ptr(idx)->get_xform_order() : exTransformOrd::SRT; }
	int get_parent_idx(int idx) const { return ck_node_idx(idx) ? get_node_ptr(idx)->mParentIdx : -1; }
	cxMtx get_wmtx(int idx) const;
	cxMtx* get_wmtx_ptr(int idx) const;
	cxMtx get_imtx(int idx) const;
	cxMtx* get_imtx_ptr(int idx) const;
	cxMtx get_lmtx(int idx) const;
	cxMtx* get_lmtx_ptr(int idx) const;
	cxMtx calc_lmtx(int idx) const;
	cxVec calc_parent_offs(int idx) const;
	float calc_parent_dist(int idx) const { return calc_parent_offs(idx).mag(); }
	cxVec get_lpos(int idx) const;
	cxVec get_lscl(int idx) const;
	cxVec get_lrot(int idx, bool inRadians = false) const;
	cxQuat calc_lquat(int idx) const;
	cxMtx calc_wmtx(int idx, const cxMtx* pMtxLocal, cxMtx* pParentWMtx = nullptr) const;

	void calc_ik_chain_local(IKChain::Solution* pSolution, const IKChain& chain, cxMtx* pMtx, IKChain::AdjustFunc* pAdjFunc = nullptr) const;
	void copy_ik_chain_solution(cxMtx* pDstMtx, const IKChain& chain, const IKChain::Solution& solution);

	bool has_info_list() const;
	Info* find_info(eInfoKind kind) const;
	LimbInfo* find_limb(eLimbType type, int idx = 0) const;

	static const uint32_t KIND = XD_FOURCC('X', 'R', 'I', 'G');
};

struct sxGeometryData : public sxData {
	cxAABB mBBox;
	uint32_t mPntNum;
	uint32_t mPolNum;
	uint32_t mMtlNum;
	uint32_t mGlbAttrNum;
	uint32_t mPntAttrNum;
	uint32_t mPolAttrNum;
	uint32_t mPntGrpNum;
	uint32_t mPolGrpNum;
	uint32_t mSkinNodeNum;
	uint16_t mMaxSkinWgtNum;
	uint16_t mMaxVtxPerPol;

	uint32_t mPntOffs;
	uint32_t mPolOffs;
	uint32_t mMtlOffs;
	uint32_t mGlbAttrOffs;
	uint32_t mPntAttrOffs;
	uint32_t mPolAttrOffs;
	uint32_t mPntGrpOffs;
	uint32_t mPolGrpOffs;
	uint32_t mSkinNodesOffs;
	uint32_t mSkinOffs;
	uint32_t mBVHOffs;

	enum class eAttrClass {
		GLOBAL,
		POINT,
		POLYGON
	};

	struct AttrInfo {
		uint32_t mDataOffs;
		int32_t mNameId;
		uint16_t mElemNum;
		uint8_t mType;
		uint8_t mReserved;

		bool is_int() const { return mType == 0; }
		bool is_float() const { return mType == 1; }
		bool is_string() const { return mType == 2; }
		bool is_float2() const { return is_float() && mElemNum == 2; }
		bool is_float3() const { return is_float() && mElemNum == 3; }
	};

	class Polygon {
	private:
		const sxGeometryData* mpGeom;
		int32_t mPolId;

		Polygon() {}

		friend struct sxGeometryData;

		uint8_t* get_vtx_lst() const;
		int get_mtl_id_size() const { return mpGeom->get_mtl_num() < (1 << 7) ? 1 : 2; }

	public:
		const sxGeometryData* get_geo() const { return mpGeom; }
		int get_id() const { return mPolId; }
		bool is_valid() const { return (mpGeom && mpGeom->ck_pol_idx(mPolId)); }
		bool ck_vtx_idx(int idx) const { return (uint32_t)idx < (uint32_t)get_vtx_num(); }
		int get_vtx_pnt_id(int vtxIdx) const;
		int get_vtx_num() const;
		int get_mtl_id() const;
		cxVec get_vtx_pos(int vtxIdx) const { return is_valid() ? mpGeom->get_pnt(get_vtx_pnt_id(vtxIdx)) : cxVec(0.0f); };
		cxVec calc_centroid() const;
		cxAABB calc_bbox() const;
		cxVec calc_normal_cw() const;
		cxVec calc_normal_ccw() const;
		cxVec calc_normal() const { return calc_normal_cw(); }
		bool is_tri() const { return get_vtx_num() == 3; }
		bool is_quad() const { return get_vtx_num() == 4; }
		bool is_planar(float eps = 1.0e-6f) const;
		bool intersect(const cxLineSeg& seg, cxVec* pHitPos = nullptr, cxVec* pHitNrm = nullptr) const;
		bool contains_xz(const cxVec& pos) const;
	};

	struct GrpInfo {
		int16_t mNameId;
		int16_t mPathId;
		cxAABB mBBox;
		int32_t mMinIdx;
		int32_t mMaxIdx;
		uint16_t mMaxWgtNum;
		uint16_t mSkinNodeNum;
		uint32_t mIdxNum;
	};

	class Group {
	private:
		const sxGeometryData* mpGeom;
		GrpInfo* mpInfo;

		Group() {}

		friend struct sxGeometryData;

		void* get_idx_top() const;
		int get_idx_elem_size() const;

	public:
		bool is_valid() const { return (mpGeom && mpInfo); }
		const char* get_name() const { return is_valid() ? mpGeom->get_str(mpInfo->mNameId) : nullptr; }
		const char* get_path() const { return is_valid() ? mpGeom->get_str(mpInfo->mPathId) : nullptr; }
		GrpInfo* get_info() const { return mpInfo; }
		cxAABB get_bbox() const {
			if (is_valid()) {
				return mpInfo->mBBox;
			} else {
				cxAABB bbox;
				bbox.init();
				return bbox;
			}
		}
		int get_max_wgt_num() const { return is_valid() ? mpInfo->mMaxWgtNum : 0; }
		bool ck_idx(int at) const { return is_valid() ? (uint32_t)at < mpInfo->mIdxNum : false; }
		uint32_t get_idx_num() const { return is_valid() ? mpInfo->mIdxNum : 0; }
		int get_min_idx() const { return is_valid() ? mpInfo->mMinIdx : -1; }
		int get_max_idx() const { return is_valid() ? mpInfo->mMaxIdx : -1; }
		int get_rel_idx(int at) const;
		int get_idx(int at) const;
		bool contains(int idx) const;
		int get_skin_nodes_num() const { return is_valid() ? mpInfo->mSkinNodeNum : 0; }
		cxSphere* get_skin_spheres() const;
		uint16_t* get_skin_ids() const;
	};

	class HitFunc {
	public:
		HitFunc() {}
		virtual ~HitFunc() {}
		virtual bool operator()(const Polygon& pol, const cxVec& hitPos, const cxVec& hitNrm, float hitDist) { return false; }
	};

	class RangeFunc {
	public:
		RangeFunc() {}
		virtual ~RangeFunc() {}
		virtual bool operator()(const Polygon& pol) { return false; }
	};

	struct BVH {
		struct Node {
			cxAABB mBBox;
			int32_t mLeft;
			int32_t mRight;

			bool is_leaf() const { return mRight < 0; }
			int get_pol_id() const { return mLeft; }
		};

		uint32_t mNodesNum;
		uint32_t mReserved0;
		uint32_t mReserved1;
		uint32_t mReserved2;
	};

	int get_pnt_num() const { return mPntNum; }
	int get_pol_num() const { return mPolNum; }
	int get_mtl_num() const { return mMtlNum; }
	int get_pnt_grp_num() const { return mPntGrpNum; }
	int get_pol_grp_num() const { return mPolGrpNum; }
	bool is_same_pol_size() const { return !!(mFlags & 1); }
	bool is_same_pol_mtl() const { return !!(mFlags & 2); }
	bool has_skin_spheres() const { return !!(mFlags & 4); }
	bool all_quads_planar_convex() const { return !!(mFlags & 8); }
	bool is_all_tris() const { return is_same_pol_size() && mMaxVtxPerPol == 3; }
	bool has_skin() const { return mSkinOffs != 0; }
	bool has_skin_nodes() const { return mSkinNodesOffs != 0; }
	bool has_BVH() const { return mBVHOffs != 0; }
	bool ck_BVH_node_idx(int idx) const { return has_BVH() ? (uint32_t)idx < get_BVH()->mNodesNum : false; }
	bool ck_pnt_idx(int idx) const { return (uint32_t)idx < mPntNum; }
	bool ck_pol_idx(int idx) const { return (uint32_t)idx < mPolNum; }
	bool ck_mtl_idx(int idx) const { return (uint32_t)idx < mMtlNum; }
	bool ck_pnt_grp_idx(int idx) const { return (uint32_t)idx < mPntGrpNum; }
	bool ck_pol_grp_idx(int idx) const { return (uint32_t)idx < mPolGrpNum; }
	bool ck_skin_idx(int idx) const { return (uint32_t)idx < mSkinNodeNum; }
	int get_vtx_idx_size() const;
	cxVec* get_pnt_top() const { return mPntOffs ? reinterpret_cast<cxVec*>(XD_INCR_PTR(this, mPntOffs)) : nullptr; }
	cxVec get_pnt(int idx) const { return ck_pnt_idx(idx) ? get_pnt_top()[idx] : cxVec(0.0f); }
	Polygon get_pol(int idx) const;
	cxSphere* get_skin_sph_top() const { return mSkinNodesOffs ? reinterpret_cast<cxSphere*>(XD_INCR_PTR(this, mSkinNodesOffs)) : nullptr; }
	int get_skin_nodes_num() const { return mSkinNodeNum; }
	int32_t* get_skin_node_name_ids() const;
	const char* get_skin_node_name(int idx) const;
	int find_skin_node(const char* pName) const;
	uint32_t get_attr_info_offs(eAttrClass cls) const;
	uint32_t get_attr_info_num(eAttrClass cls) const;
	uint32_t get_attr_item_num(eAttrClass cls) const;
	int find_attr(const char* pName, eAttrClass cls) const;
	int find_glb_attr(const char* pName) const { return find_attr(pName, eAttrClass::GLOBAL); }
	int find_pnt_attr(const char* pName) const { return find_attr(pName, eAttrClass::POINT); }
	int find_pol_attr(const char* pName) const { return find_attr(pName, eAttrClass::POLYGON); }
	AttrInfo* get_attr_info(int attrIdx, eAttrClass cls) const;
	AttrInfo* get_glb_attr_info(int attrIdx) const { return get_attr_info(attrIdx, eAttrClass::GLOBAL); }
	AttrInfo* get_pnt_attr_info(int attrIdx) const { return get_attr_info(attrIdx, eAttrClass::POINT); }
	AttrInfo* get_pol_attr_info(int attrIdx) const { return get_attr_info(attrIdx, eAttrClass::POLYGON); }
	float* get_attr_data_f(int attrIdx, eAttrClass cls, int itemIdx, int minElem = 1) const;
	float* get_glb_attr_data_f(int attrIdx, int minElem = 1) const { return get_attr_data_f(attrIdx, eAttrClass::GLOBAL, 0, minElem); }
	float* get_pnt_attr_data_f(int attrIdx, int itemIdx, int minElem = 1) const { return get_attr_data_f(attrIdx, eAttrClass::POINT, itemIdx, minElem); }
	float* get_pol_attr_data_f(int attrIdx, int itemIdx, int minElem = 1) const { return get_attr_data_f(attrIdx, eAttrClass::POLYGON, itemIdx, minElem); }
	float get_pnt_attr_val_f(int attrIdx, int pntIdx) const;
	xt_float3 get_pnt_attr_val_f3(int attrIdx, int pntIdx) const;
	cxVec get_pnt_normal(int pntIdx) const;
	cxVec get_pnt_tangent(int pntIdx) const;
	cxVec get_pnt_bitangent(int pntIdx) const;
	cxVec calc_pnt_bitangent(int pntIdx) const;
	cxColor get_pnt_color(int pntIdx, bool useAlpha = true) const;
	xt_texcoord get_pnt_texcoord(int pntIdx) const;
	xt_texcoord get_pnt_texcoord2(int pntIdx) const;
	int get_pnt_wgt_num(int pntIdx) const;
	int get_pnt_skin_jnt(int pntIdx, int wgtIdx) const;
	float get_pnt_skin_wgt(int pntIdx, int wgtIdx) const;
	int find_mtl_grp_idx(const char* pName, const char* pPath = nullptr) const;
	Group find_mtl_grp(const char* pName, const char* pPath = nullptr) const { return get_mtl_grp(find_mtl_grp_idx(pName, pPath)); }
	GrpInfo* get_mtl_info(int idx) const;
	Group get_mtl_grp(int idx) const;
	GrpInfo* get_pnt_grp_info(int idx) const;
	Group get_pnt_grp(int idx) const;
	GrpInfo* get_pol_grp_info(int idx) const;
	Group get_pol_grp(int idx) const;
	void hit_query_nobvh(const cxLineSeg& seg, HitFunc& fun) const;
	void hit_query(const cxLineSeg& seg, HitFunc& fun) const;
	void range_query_nobvh(const cxAABB& box, RangeFunc& fun) const;
	void range_query(const cxAABB& box, RangeFunc& fun) const;
	BVH* get_BVH() const { return has_BVH() ? reinterpret_cast<BVH*>(XD_INCR_PTR(this, mBVHOffs)) : nullptr; }
	BVH::Node* get_BVH_node(int nodeId) const { return ck_BVH_node_idx(nodeId) ? &reinterpret_cast<BVH::Node*>(get_BVH() + 1)[nodeId] : nullptr; }
	cxAABB calc_world_bbox(cxMtx* pMtxW, int* pIdxMap = nullptr) const;

	static const uint32_t KIND = XD_FOURCC('X', 'G', 'E', 'O');
};

struct sxDDSHead {
	uint32_t mMagic;
	uint32_t mSize;
	uint32_t mFlags;
	uint32_t mHeight;
	uint32_t mWidth;
	uint32_t mPitchLin;
	uint32_t mDepth;
	uint32_t mMipMapCount;
	uint32_t mReserved1[11];
	struct PixelFormat {
		uint32_t mSize;
		uint32_t mFlags;
		uint32_t mFourCC;
		uint32_t mBitCount;
		uint32_t mMaskR;
		uint32_t mMaskG;
		uint32_t mMaskB;
		uint32_t mMaskA;
	} mFormat;
	uint32_t mCaps1;
	uint32_t mCaps2;
	uint32_t mReserved2[3];
};

namespace nxTexture {

sxDDSHead* alloc_dds128(int w, int h, uint32_t* pSize);

} // nxTexture

struct sxTextureData : public sxData {
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mPlaneNum;
	uint32_t mPlaneOffs;

	struct PlaneInfo {
		uint32_t mDataOffs;
		int16_t mNameId;
		uint8_t mTrailingZeroes;
		int8_t mFormat;
		float mMinVal;
		float mMaxVal;
		float mValOffs;
		uint32_t mBitCount;
		uint32_t mReserved0;
		uint32_t mReserved1;

		bool is_const() const { return mFormat == 0; }
		bool is_compressed() const { return mFormat == 1; }
	};

	class Plane {
	private:
		const sxTextureData* mpTex;
		int32_t mPlaneId;

		Plane() {}

		void expand(float* pDst, int pixelStride) const;

		friend struct sxTextureData;
	public:
		bool is_valid() const { return mPlaneId >= 0; }
		void get_data(float* pDst, int pixelStride = 1) const;
		float* get_data() const;
	};

	class DDS {
	private:
		sxDDSHead* mpHead;
		uint32_t mSize;

		friend struct sxTextureData;

	public:
		bool is_valid() const { return mpHead && mSize; }
		void release();
		sxDDSHead* get_data_ptr() const { return mpHead; }
		uint32_t get_data_size() const { return mSize; }
		int get_width() const { return is_valid() ? mpHead->mWidth : 0; }
		int get_height() const { return is_valid() ? mpHead->mHeight : 0; }
		void save(const char* pOutPath) const;
	};

	struct Pyramid {
		int32_t mBaseWidth;
		int32_t mBaseHeight;
		int32_t mLvlNum;
		int32_t mLvlOffs[1];

		bool ck_lvl_idx(int idx) const { return (uint32_t)idx < (uint32_t)mLvlNum; }
		cxColor* get_lvl(int idx) const { return ck_lvl_idx(idx) ? reinterpret_cast<cxColor*>(XD_INCR_PTR(this, mLvlOffs[idx])) : nullptr; }
		void get_lvl_dims(int idx, int* pWidth, int* pHeight) const;
		DDS get_lvl_dds(int idx) const;
	};

	bool ck_plane_idx(int idx) const { return (uint32_t)idx < mPlaneNum; }
	int get_width() const { return mWidth; }
	int get_height() const { return mHeight; }
	int calc_mip_num() const;
	int find_plane_idx(const char* pName) const;
	Plane find_plane(const char* pName) const;
	PlaneInfo* get_plane_info(int idx) const { return ck_plane_idx(idx) ? reinterpret_cast<PlaneInfo*>(XD_INCR_PTR(this, mPlaneOffs)) + idx : nullptr; }
	Plane get_plane(int idx) const;
	void get_rgba(float* pDst) const;
	cxColor* get_rgba() const;
	DDS get_dds() const;
	Pyramid* get_pyramid() const;

	static const uint32_t KIND = XD_FOURCC('X', 'T', 'E', 'X');
};


struct sxKeyframesData : public sxData {
	float mFPS;
	int32_t mMinFrame;
	int32_t mMaxFrame;
	uint32_t mFCurveNum;
	uint32_t mFCurveOffs;
	uint32_t mNodeInfoNum;
	uint32_t mNodeInfoOffs;

	enum class eFunc {
		CONSTANT = 0,
		LINEAR = 1,
		CUBIC = 2
	};

	struct NodeInfo {
		int16_t mPathId;
		int16_t mNameId;
		int16_t mTypeId;
		uint8_t mRotOrd;
		uint8_t mXfmOrd;

		exRotOrd get_rot_order() const { return (exRotOrd)mRotOrd; }
		exTransformOrd get_xform_order() const { return (exTransformOrd)mXfmOrd; }
	};

	struct FCurveInfo {
		int16_t mNodePathId;
		int16_t mNodeNameId;
		int16_t mChanNameId;
		uint16_t mKeyNum;
		float mMinVal;
		float mMaxVal;
		uint32_t mValOffs;
		uint32_t mLSlopeOffs;
		uint32_t mRSlopeOffs;
		uint32_t mFnoOffs;
		uint32_t mFuncOffs;
		int8_t mCmnFunc;
		uint8_t mReserved8;
		uint16_t mReserved16;
		uint32_t mReserved32[2];

		bool is_const() const { return mKeyNum == 0; }
		bool has_fno_lst() const { return mFnoOffs != 0; }
		eFunc get_common_func() const { return mCmnFunc < 0 ? eFunc::LINEAR : (eFunc)mCmnFunc; }
	};

	class FCurve {
	private:
		const sxKeyframesData* mpKfr;
		int32_t mFcvId;

		FCurve() {}

		friend struct sxKeyframesData;

	public:
		bool is_valid() const { return mpKfr && mpKfr->ck_fcv_idx(mFcvId); }
		FCurveInfo* get_info() const { return is_valid() ? mpKfr->get_fcv_info(mFcvId) : nullptr; }
		const char* get_node_name() const { return is_valid() ? mpKfr->get_fcv_node_name(mFcvId) : nullptr; }
		const char* get_node_path() const { return is_valid() ? mpKfr->get_fcv_node_path(mFcvId) : nullptr; }
		const char* get_chan_name() const { return is_valid() ? mpKfr->get_fcv_chan_name(mFcvId) : nullptr; }
		bool is_const() const { return is_valid() ? get_info()->is_const() : true; }
		int get_key_num() const { return is_valid() ? get_info()->mKeyNum : 0; }
		bool ck_key_idx(int fno) const { return is_valid() ? (uint32_t)fno < (uint32_t)get_key_num() : false; }
		int find_key_idx(int fno) const;
		int get_fno(int idx) const;
		float eval(float frm, bool extrapolate = false) const;
	};

	struct RigLink {
		struct Val {
			xt_float3 f3;
			int32_t fcvId[3];

			cxVec get_vec() const { return cxVec(f3.x, f3.y, f3.z); }
			void set_vec(const cxVec& v) { f3.set(v.x, v.y, v.z); }
		};

		struct Node {
			int16_t mRigNodeId;
			int16_t mKfrNodeId;
			uint32_t mPosValOffs;
			uint32_t mRotValOffs;
			uint32_t mSclValOffs;
			exRotOrd mRotOrd;
			exTransformOrd mXformOrd;
			bool mUseSlerp;

			Val* get_pos_val() { return mPosValOffs ? (Val*)XD_INCR_PTR(this, mPosValOffs) : nullptr; }
			Val* get_rot_val() { return mRotValOffs ? (Val*)XD_INCR_PTR(this, mRotValOffs) : nullptr; }
			Val* get_scl_val() { return mSclValOffs ? (Val*)XD_INCR_PTR(this, mSclValOffs) : nullptr; }
		};

		int32_t mNodeNum;
		int32_t mRigNodeNum;
		uint32_t mRigMapOffs;
		Node mNodes[1];

		int16_t* get_rig_map() const { return mRigMapOffs ? (int16_t*)XD_INCR_PTR(this, mRigMapOffs) : nullptr; }
		bool ck_node_idx(int idx) const { return (uint32_t)idx < (uint32_t)mNodeNum; }
	};

	bool has_node_info() const;
	bool ck_node_info_idx(int idx) const { return has_node_info() && ((uint32_t)idx < mNodeInfoNum); }
	int get_node_info_num() const { return has_node_info() ? mNodeInfoNum : 0; }
	int find_node_info_idx(const char* pName, const char* pPath = nullptr, int startIdx = 0) const;
	NodeInfo* get_node_info_ptr(int idx) const { return ck_node_info_idx(idx) ? reinterpret_cast<NodeInfo*>(XD_INCR_PTR(this, mNodeInfoOffs)) + idx : nullptr; }
	const char* get_node_name(int idx) const;

	float get_frame_rate() const { return mFPS; }
	int get_frame_count() const { return get_max_fno() + 1; }
	int get_max_fno() const { return mMaxFrame - mMinFrame; }
	int get_rel_fno(int fno) const { return (fno - mMinFrame) % get_frame_count(); }
	bool ck_fno(int fno) const { return (uint32_t)fno <= (uint32_t)get_max_fno(); }
	bool ck_fcv_idx(int idx) const { return (uint32_t)idx < mFCurveNum; }
	int find_fcv_idx(const char* pNodeName, const char* pChanName, const char* pNodePath = nullptr) const;
	int get_fcv_num() const { return mFCurveNum; }
	FCurveInfo* get_fcv_top() const { return mFCurveOffs ? reinterpret_cast<FCurveInfo*>XD_INCR_PTR(this, mFCurveOffs) : nullptr; }
	FCurveInfo* get_fcv_info(int idx) const { return mFCurveOffs && (ck_fcv_idx(idx)) ? get_fcv_top() + idx : nullptr; }
	FCurve get_fcv(int idx) const;
	FCurve find_fcv(const char* pNodeName, const char* pChanName, const char* pNodePath = nullptr) const { return get_fcv(find_fcv_idx(pNodeName, pChanName, pNodePath)); }
	const char* get_fcv_node_name(int idx) const { return ck_fcv_idx(idx) ? get_str(get_fcv_info(idx)->mNodeNameId) : nullptr; }
	const char* get_fcv_node_path(int idx) const { return ck_fcv_idx(idx) ? get_str(get_fcv_info(idx)->mNodePathId) : nullptr; }
	const char* get_fcv_chan_name(int idx) const { return ck_fcv_idx(idx) ? get_str(get_fcv_info(idx)->mChanNameId) : nullptr; }
	RigLink* make_rig_link(const sxRigData& rig) const;
	void eval_rig_link_node(RigLink* pLink, int nodeIdx, float frm, const sxRigData* pRig = nullptr, cxMtx* pRigLocalMtx = nullptr) const;
	void eval_rig_link(RigLink* pLink, float frm, const sxRigData* pRig = nullptr, cxMtx* pRigLocalMtx = nullptr) const;

	void dump_clip(FILE* pOut) const;
	void dump_clip(const char* pOutPath = nullptr) const;

	static const uint32_t KIND = XD_FOURCC('X', 'K', 'F', 'R');
};

struct sxCompiledExpression {
	uint32_t mSig;
	uint32_t mLen;
	uint32_t mValsNum;
	uint32_t mCodeNum;
	uint32_t mStrsNum;

	enum class eOp : uint8_t {
		NOP = 0,
		END = 1,
		NUM = 2,
		STR = 3,
		VAR = 4,
		CMP = 5,
		ADD = 6,
		SUB = 7,
		MUL = 8,
		DIV = 9,
		MOD = 10,
		NEG = 11,
		FUN = 12,
		XOR = 13,
		AND = 14,
		OR = 15
	};

	enum class eCmp : uint8_t {
		EQ = 0,
		NE = 1,
		LT = 2,
		LE = 3,
		GT = 4,
		GE = 5
	};

	struct Code {
		uint8_t mOp;
		int8_t mPrio;
		int16_t mInfo;

		eOp get_op() const { return (eOp)mOp; }
	};

	struct StrInfo {
		uint32_t mHash;
		uint16_t mOffs;
		uint16_t mLen;
	};

	struct String {
		const char* mpChars;
		uint32_t mHash;
		uint32_t mLen;

		bool is_valid() const { return mpChars != nullptr; }
	};

	template<typename T> struct TagsT {
		T* mpBitAry;
		int mNum;

		TagsT() : mpBitAry(nullptr), mNum(0) {}

		void init(void* pBits, int n) {
			mpBitAry = (T*)pBits;
			mNum = n;
		}

		void reset() {
			mpBitAry = nullptr;
			mNum = 0;
		}

		bool ck_idx(int idx) const { return (uint32_t)idx < mNum; }
		void set_num(int idx) { XD_BIT_ARY_CL(T, mpBitAry, idx); }
		bool is_num(int idx) const { return !XD_BIT_ARY_CK(T, mpBitAry, idx); }
		void set_str(int idx) { XD_BIT_ARY_ST(T, mpBitAry, idx); }
		bool is_str(int idx) const { return XD_BIT_ARY_CK(T, mpBitAry, idx); }

		static int calc_mem_size(int n) { return XD_BIT_ARY_SIZE(T, n); }
	};

	typedef TagsT<uint8_t> Tags;

	class Stack {
	public:
		union Val {
			float num;
			int32_t sptr;
		};
	protected:
		void* mpMem;
		Val* mpVals;
		Tags mTags;
		int mPtr;

	public:
		Stack() : mpMem(nullptr), mpVals(nullptr), mPtr(0) {}

		void alloc(int n);
		void free();

		int size() const { return mTags.mNum; }
		void clear() { mPtr = 0; }
		bool is_empty() const { return mPtr == 0; }
		bool is_full() const { return mPtr >= size(); }
		void push_num(float num);
		float pop_num();
		void push_str(int ptr);
		int pop_str();
	};

	class ExecIfc {
	public:
		virtual Stack* get_stack() { return nullptr; }
		virtual void set_result(float val) {}
		virtual float ch(const String& path) { return 0.0f; }
		virtual float detail(const String& path, const String& attrName, int idx) { return 0.0f; }
		virtual float var(const String& name) { return 0.0f; }
	};

	bool is_valid() const { return mSig == XD_FOURCC('C', 'E', 'X', 'P') && mLen > 0; }
	const float* get_vals_top() const { return reinterpret_cast<const float*>(this + 1); }
	bool ck_val_idx(int idx) const { return (uint32_t)idx < mValsNum; }
	float get_val(int idx) const { return ck_val_idx(idx) ? get_vals_top()[idx] : 0.0f; }
	const Code* get_code_top() const { return reinterpret_cast<const Code*>(&get_vals_top()[mValsNum]); }
	bool ck_str_idx(int idx) const { return (uint32_t)idx < mStrsNum; }
	const StrInfo* get_str_info_top() const { return reinterpret_cast<const StrInfo*>(&get_code_top()[mCodeNum]); }
	const StrInfo* get_str_info(int idx) const { return ck_str_idx(idx) ? get_str_info_top() + idx : nullptr; }
	void get_str(int idx, String* pStr) const;
	String get_str(int idx) const;
	void exec(ExecIfc& ifc) const;
	void disasm(FILE* pFile = stdout) const;
};

struct sxExprLibData : public sxData {
	uint32_t mExprNum;
	uint32_t mListOffs;

	struct ExprInfo {
		int16_t mNodeNameId;
		int16_t mNodePathId;
		int16_t mChanNameId;
		int16_t mReserved;

		const sxCompiledExpression* get_compiled_expr() const { return reinterpret_cast<const sxCompiledExpression*>(this + 1); }
	};

	class Entry {
	private:
		const sxExprLibData* mpLib;
		int32_t mExprId;

		Entry() {}

		friend struct sxExprLibData;

	public:
		bool is_valid() const { return mpLib && mpLib->ck_expr_idx(mExprId); }
		const ExprInfo* get_info() const { return is_valid() ? mpLib->get_info(mExprId) : nullptr; }
		const sxCompiledExpression* get_expr() const { return is_valid() ? mpLib->get_info(mExprId)->get_compiled_expr() : nullptr; }
		const char* get_node_name() const { return is_valid() ? mpLib->get_str(get_info()->mNodeNameId) : nullptr; }
		const char* get_node_path() const { return is_valid() ? mpLib->get_str(get_info()->mNodePathId) : nullptr; }
		const char* get_chan_name() const { return is_valid() ? mpLib->get_str(get_info()->mChanNameId) : nullptr; }
	};

	bool ck_expr_idx(int idx) const { return (uint32_t)idx < mExprNum; }
	int get_expr_num() const { return mExprNum; }
	int find_expr_idx(const char* pNodeName, const char* pChanName, const char* pNodePath = nullptr, int startIdx = 0) const;
	const ExprInfo* get_info(int idx) const { return ck_expr_idx(idx) ? (const ExprInfo*)XD_INCR_PTR(this, ((uint32_t*)XD_INCR_PTR(this, mListOffs))[idx]) : nullptr; }
	Entry get_entry(int idx) const;
	int count_rig_entries(const sxRigData& rig) const;

	static const uint32_t KIND = XD_FOURCC('X', 'C', 'E', 'L');
};

struct sxFileCatalogue : public sxData {
	uint32_t mFilesNum;
	uint32_t mListOffs;

	struct FileInfo {
		int32_t mNameId;
		int32_t mFileNameId;
	};

	bool ck_file_idx(int idx) const { return (uint32_t)idx < mFilesNum; }
	const FileInfo* get_info(int idx) const { return ck_file_idx(idx) ? &((const FileInfo*)XD_INCR_PTR(this, mListOffs))[idx] : nullptr; }
	const char* get_name(int idx) const { const FileInfo* pInfo = get_info(idx); return pInfo ? get_str(pInfo->mNameId) : nullptr; }
	const char* get_file_name(int idx) const { const FileInfo* pInfo = get_info(idx); return pInfo ? get_str(pInfo->mFileNameId) : nullptr; }
	int find_name_idx(const char* pName) const;

	static const uint32_t KIND = XD_FOURCC('F', 'C', 'A', 'T');
};


namespace nxSH {

inline int calc_coefs_num(int order) { return order < 1 ? 0 : nxCalc::sq(order); }
inline int calc_ary_idx(int l, int m) { return l*(l+1) + m; }
inline int band_idx_from_ary_idx(int idx) { return (int)::sqrtf((float)idx); }
inline int func_idx_from_ary_band(int idx, int l) { return idx - l*(l + 1); }

void calc_weights(float* pWgt, int order, float s, float scl = 1.0f);

} // nxSH

namespace nxDataUtil {

exAnimChan anim_chan_from_str(const char* pStr);
const char* anim_chan_to_str(exAnimChan chan);
exRotOrd rot_ord_from_str(const char* pStr);
const char* rot_ord_to_str(exRotOrd rord);
exTransformOrd xform_ord_from_str(const char* pStr);
const char* xform_ord_to_str(exTransformOrd xord);

} // nxDataUtil


namespace nxData {

sxData* load(const char* pPath);
void unload(sxData* pData);

} // nxData

