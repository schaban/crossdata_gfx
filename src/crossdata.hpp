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
#	if defined(_MSC_VER)
#		define XD_NOINLINE __declspec(noinline)
#	elif defined(__GNUC__)
#		define XD_NOINLINE __attribute__((noinline))
#	else
#		define XD_NOINLINE
#	endif
#endif

#ifndef XD_FORCEINLINE
#	if defined(_MSC_VER)
#		define XD_FORCEINLINE __forceinline
#	elif defined(__GNUC__)
#		define XD_FORCEINLINE __inline__ __attribute__((__always_inline__))
#	else
#		define XD_FORCEINLINE inline
#	endif
#endif

#ifndef XD_USE_LA
#	define XD_USE_LA 1
#endif

#define XD_MAX_PATH 4096

#define XD_FOURCC(c1, c2, c3, c4) ((((uint8_t)(c4))<<24)|(((uint8_t)(c3))<<16)|(((uint8_t)(c2))<<8)|((uint8_t)(c1)))
#define XD_INCR_PTR(_ptr, _inc) ( &((uint8_t*)(_ptr))[_inc] )
#define XD_ARY_LEN(_arr) (sizeof((_arr)) / sizeof((_arr)[0]))
#define XD_ALIGN(_x, _n) ( ((uintptr_t)(_x) + ((_n) - 1)) & (~((_n) - 1)) )

#define XD_BIT_ARY_SIZE(_storage_t, _nbits) ((((int)(_nbits)-1)/(sizeof(_storage_t)*8))+1)
#define XD_BIT_ARY_DECL(_storage_t, _name, _nbits) _storage_t _name[XD_BIT_ARY_SIZE(_storage_t, _nbits)]
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


struct sxRNG { uint64_t s[2]; };

namespace nxCore {

void* mem_alloc(size_t size, uint32_t tag = XD_DEF_MEM_TAG, int alignment = 0x10);
void* mem_realloc(void* pMem, size_t newSize, int alignment = 0x10);
void* mem_resize(void* pMem, float factor, int alignment = 0x10);
void mem_free(void* pMem);
size_t mem_size(void* pMem);
uint32_t mem_tag(void* pMem);
void mem_dbg();
void dbg_msg(const char* fmt, ...);
void* bin_load(const char* pPath, size_t* pSize = nullptr, bool appendPath = false, bool unpack = false);
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

inline int clz32(uint32_t x) {
	if (x == 0) return 32;
	int n = 0;
	int s = 16;
	while (s) {
		uint32_t m = ((1U << s) - 1) << (32 - s);
		if ((x & m) == 0) {
			n += s;
			x <<= s;
		}
		s >>= 1;
	}
	return n;
}

inline int ctz32(uint32_t x) {
	if (x == 0) return 32;
	int n = 0;
	int s = 16;
	while (s) {
		uint32_t m = (1U << s) - 1;
		if ((x & m) == 0) {
			n += s;
			x >>= s;
		}
		s >>= 1;
	}
	return n;
}

void rng_seed(sxRNG* pState, uint64_t seed);
inline void rng_seed(uint64_t seed) { rng_seed(nullptr, seed); }
uint64_t rng_next(sxRNG* pState = nullptr);
float rng_f01(sxRNG* pState = nullptr);

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

template<typename T> T ipow(T x, int n) {
	float wx = x;
	int wn = n;
	if (n < 0) {
		wx = rcp0(x);
		wn = -n;
	}
	float res = 1.0f;
	do {
		if ((wn & 1) != 0) res *= wx;
		wx *= wx;
		wn >>= 1;
	} while (wn > 0);
	return res;
}

inline float saturate(float x) { return clamp(x, 0.0f, 1.0f); }

inline float div0(float x, float y) { return y != 0.0f ? x / y : 0.0f; }

inline float rcp0(float x) { return div0(1.0f, x); }

inline float hypot(float x, float y) {
	float m = max(::fabsf(x), ::fabsf(y));
	float im = rcp0(m);
	return ::sqrtf(sq(x*im) + sq(y*im)) * m;
}

inline float cb_root(float x) {
	float a = ::fabsf(x);
	float r = ::expf(::logf(a) * (1.0f / 3.0f));
	return x < 0.0f ? -r : r;
}

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

template<typename FN> float panorama_scan(FN& fn, int w, int h) {
	float da = (float)((2.0*XD_PI / w) * (XD_PI / h));
	float iw = 1.0f / (float)w;
	float ih = 1.0f / (float)h;
	float sum = 0.0f;
	for (int y = 0; y < h; ++y) {
		float v = 1.0f - (y + 0.5f)*ih;
		float dw = da * ::sinf(XD_PI * v);
		float ay = (v - 1.0f) * XD_PI;
		float sy = ::sinf(ay);
		float cy = ::cosf(ay);
		float ax0 = iw * XD_PI;
		float rsx = ::sinf(ax0);
		float rcx = ::cosf(ax0);
		float rax = 2.0f * sq(rsx);
		float rbx = ::sinf(ax0 * 2.0f);
		for (int x = 0; x < w; ++x) {
			float sx = rsx;
			float cx = rcx;
			float dx = cx * sy;
			float dy = cy;
			float dz = sx * sy;
			float isx = rsx - (rax*rsx - rbx*rcx);
			float icx = rcx - (rax*rcx + rbx*rsx);
			rsx = isx;
			rcx = icx;
			fn(x, y, dx, dy, dz, dw);
			sum += dw;
		}
	}
	return (XD_PI * 4.0f) * rcp0(sum);
}

} // nxCalc

namespace nxLA {

template<typename T> inline T* mtx_alloc(int M, int N) {
	int a = nxCalc::min(int(sizeof(T) * 4), 0x10);
	return reinterpret_cast<T*>(nxCore::mem_alloc(M * N * sizeof(T), XD_FOURCC('L', 'A', 'm', 'x'), a));
}

template<typename DST_T, typename SRC_T> inline void mtx_cpy(DST_T* pDst, const SRC_T* pSrc, int M, int N) {
	int n = M * N;
	for (int i = 0; i < n; ++i) {
		pDst[i] = DST_T(pSrc[i]);
	}
}

template<typename DST_T, typename SRC_T> inline void vec_cpy(DST_T* pDst, const SRC_T* pSrc, int N) {
	for (int i = 0; i < N; ++i) {
		pDst[i] = DST_T(pSrc[i]);
	}
}

// https://blogs.msdn.microsoft.com/nativeconcurrency/2014/09/04/raking-through-the-parallelism-tool-shed-the-curious-case-of-matrix-matrix-multiplication/
template<typename DST_T, typename SRC_L_T, typename SRC_R_T>
XD_FORCEINLINE
#if 0
void mul_mm(DST_T* pDst, const SRC_L_T* pSrc1, const SRC_R_T* pSrc2, int M, int N, int P) {
	const int nres = M * P;
	for (int i = 0; i < nres; ++i) {
		pDst[i] = 0;
	}
	for (int i = 0; i < M; ++i) {
		int ra = i * N;
		int rr = i * P;
		for (int j = 0; j < N; ++j) {
			int rb = j * P;
			DST_T s = DST_T(pSrc1[ra + j]);
			for (int k = 0; k < P; ++k) {
				pDst[rr + k] += DST_T(pSrc2[rb + k]) * s;
			}
		}
	}
}
#else
void mul_mm(DST_T* pDst, const SRC_L_T* pSrc1, const SRC_R_T* pSrc2, int M, int N, int P) {
	for (int i = 0; i < M; ++i) {
		int ra = i * N;
		int rr = i * P;
		DST_T s = DST_T(pSrc1[ra]);
		for (int k = 0; k < P; ++k) {
			pDst[rr + k] = DST_T(pSrc2[k]) * s;
		}
	}
	for (int i = 0; i < M; ++i) {
		int ra = i * N;
		int rr = i * P;
		for (int j = 1; j < N; ++j) {
			int rb = j * P;
			DST_T s = DST_T(pSrc1[ra + j]);
			for (int k = 0; k < P; ++k) {
				pDst[rr + k] += DST_T(pSrc2[rb + k]) * s;
			}
		}
	}
}
#endif

template<typename DST_VEC_T, typename SRC_VEC_T, typename MTX_T>
inline void mul_vm(DST_VEC_T* pDstVec, const SRC_VEC_T* pSrcVec, const MTX_T* pMtx, int M, int N) {
	mul_mm(pDstVec, pSrcVec, pMtx, 1, M, N);
}

template<typename DST_VEC_T, typename MTX_T, typename SRC_VEC_T>
inline void mul_mv(DST_VEC_T* pDstVec, const MTX_T* pMtx, const SRC_VEC_T* pSrcVec, int M, int N) {
#if 0
	mul_mm(pDstVec, pMtx, pSrcVec, M, N, 1);
#else
	for (int i = 0; i < M; ++i) {
		pDstVec[i] = DST_VEC_T(0);
	}
	for (int i = 0; i < M; ++i) {
		int r = i * N;
		for (int j = 0; j < N; ++j) {
			pDstVec[i] = DST_VEC_T(pDstVec[i] + pMtx[r + j]*pSrcVec[j]);
		}
	}
#endif
}

template<typename T> inline void identity(T* pMtx, int N) {
	for (int i = 0; i < N * N; ++i) {
		pMtx[i] = 0;
	}
	for (int i = 0; i < N; ++i) {
		pMtx[(i * N) + i] = 1;
	}
}

template<typename T>
inline
bool transpose(T* pMtx, int M, int N, uint32_t* pFlgWk = nullptr) {
	if (M == N) {
		for (int i = 0; i < N - 1; ++i) {
			for (int j = i + 1; j < N; ++j) {
				int ij = i*N + j;
				int ji = j*N + i;
				T t = pMtx[ij];
				pMtx[ij] = pMtx[ji];
				pMtx[ji] = t;
			}
		}
	} else {
		XD_BIT_ARY_DECL(uint32_t, flg, 64*32);
		uint32_t* pFlg = flg;
		size_t flgSize = XD_BIT_ARY_SIZE(uint32_t, M*N);
		size_t maxSize = XD_ARY_LEN(flg);
		if (flgSize > maxSize) {
			pFlg = pFlgWk;
		}
		if (!pFlg) {
			nxSys::dbgmsg("LA::transpose - insufficient memory.\n");
			return false;
		}
		for (size_t i = 0; i < flgSize; ++i) {
			pFlg[i] = 0;
		}
		int q = M*N - 1;
		for (int i = 1; i < q;) {
			int i0 = i;
			int org = i;
			T t = pMtx[org];
			do {
				int i1 = (i*M) % q;
				T t1 = pMtx[i1];
				pMtx[i1] = t;
				t = t1;
				XD_BIT_ARY_ST(uint32_t, pFlg, i);
				i = i1;
			} while (i != org);
			for (i = i0 + 1; i < q && XD_BIT_ARY_CK(uint32_t, pFlg, i); ++i) {}
		}
	}
	return true;
}

template<typename T>
XD_FORCEINLINE
bool inv_gj(T* pMtx, int N, int* pWk /* [N*3] */) {
	int* pPiv = pWk;
	int* pCol = pWk + N;
	int* pRow = pWk + N*2;
	int ir = 0;
	int ic = 0;
	for (int i = 0; i < N; ++i) {
		pPiv[i] = 0;
	}
	for (int i = 0; i < N; ++i) {
		T amax = 0;
		for (int j = 0; j < N; ++j) {
			if (pPiv[j] != 1) {
				int rj = j * N;
				for (int k = 0; k < N; ++k) {
					if (0 == pPiv[k]) {
						T a = pMtx[rj + k];
						if (a < 0) a = -a;
						if (a >= amax) {
							amax = a;
							ir = j;
							ic = k;
						}
					}
				}
			}
		}
		++pPiv[ic];
		if (ir != ic) {
			int rr = ir * N;
			int rc = ic * N;
			for (int j = 0; j < N; ++j) {
				T t = pMtx[rr + j];
				pMtx[rr + j] = pMtx[rc + j];
				pMtx[rc + j] = t;
			}
		}
		pRow[i] = ir;
		pCol[i] = ic;
		int rc = ic * N;
		T piv = pMtx[rc + ic];
		if (piv == 0) return false; /* singular */
		T ipiv = 1 / piv;
		pMtx[rc + ic] = 1;
		for (int j = 0; j < N; ++j) {
			pMtx[rc + j] *= ipiv;
		}
		for (int j = 0; j < N; ++j) {
			if (j != ic) {
				int rj = j * N;
				T d = pMtx[rj + ic];
				pMtx[rj + ic] = 0;
				T* pDst = &pMtx[rj];
				T* pSrc = &pMtx[rc];
				for (int k = 0; k < N; ++k) {
					*pDst++ -= *pSrc++ * d;
				}
			}
		}
	}
	for (int i = N; --i >= 0;) {
		ir = pRow[i];
		ic = pCol[i];
		if (ir != ic) {
			for (int j = 0; j < N; ++j) {
				int rj = j * N;
				T t = pMtx[rj + ir];
				pMtx[rj + ir] = pMtx[rj + ic];
				pMtx[rj + ic] = t;
			}
		}
	}
	return true;
}

template<typename T>
XD_FORCEINLINE
bool lu_decomp(T* pMtx, int N, T* pWk /* [N] */, int* pIdx /* [N] */, T* pDetSgn = nullptr) {
	T dsgn = 1;
	T* pScl = pWk;
	for (int i = 0; i < N; ++i) {
		T scl = 0;
		int ri = i * N;
		for (int j = 0; j < N; ++j) {
			T a = pMtx[ri + j];
			if (a < 0) a = -a;
			if (a > scl) scl = a;
		}
		if (scl == 0) {
			if (pDetSgn) {
				*pDetSgn = 0;
			}
			return false;
		}
		pScl[i] = scl;
	}
	for (int i = 0; i < N; ++i) {
		pScl[i] = 1 / pScl[i];
	}
	int imax = 0;
	for (int k = 0; k < N; ++k) {
		int rk = k * N;
		T amax = 0;
		for (int i = k; i < N; ++i) {
			T a = pMtx[(i * N) + k];
			if (a < 0) a = -a;
			a *= pScl[i];
			if (amax <= a) {
				amax = a;
				imax = i;
			}
		}
		if (k != imax) {
			int rm = imax * N;
			for (int j = 0; j < N; ++j) {
				T t = pMtx[rm + j];
				pMtx[rm + j] = pMtx[rk + j];
				pMtx[rk + j] = t;
			}
			dsgn = -dsgn;
			pScl[imax] = pScl[k];
		}
		if (pIdx) {
			pIdx[k] = imax;
		}
		if (pMtx[rk + k] == 0) {
			pMtx[rk + k] = T(1.0e-16);
		}
		for (int i = k + 1; i < N; ++i) {
			int ri = i * N;
			T s = pMtx[ri + k] / pMtx[rk + k];
			pMtx[ri + k] = s;
			T* pDst = &pMtx[ri + k + 1];
			T* pSrc = &pMtx[rk + k + 1];
			for (int j = k + 1; j < N; ++j) {
				*pDst++ -= *pSrc++ * s;
			}
		}
	}
	if (pDetSgn) {
		*pDetSgn = dsgn;
	}
	return true;
}

template<typename T>
XD_FORCEINLINE
void lu_solve(T* pAns, const T* pLU, const T* pRHS, const int* pIdx, int N) {
	if (pAns != pRHS) {
		for (int i = 0; i < N; ++i) {
			pAns[i] = pRHS[i];
		}
	}
	T s = 0;
	int ii = -1;
	for (int i = 0; i < N; ++i) {
		int ri = i * N;
		int idx = pIdx[i];
		s = pAns[idx];
		pAns[idx] = pAns[i];
		if (ii < 0) {
			if (s != 0) {
				ii = i;
			}
		} else {
			for (int j = ii; j < i; ++j) {
				s -= pLU[ri + j] * pAns[j];
			}
		}
		pAns[i] = s;
	}
	for (int i = N; --i >= 0;) {
		int ri = i * N;
		s = pAns[i];
		for (int j = i + 1; j < N; ++j) {
			s -= pLU[ri + j] * pAns[j];
		}
		pAns[i] = s / pLU[ri + i];
	}
}

template<typename T>
inline
bool symm_ldlt_decomp(T* pMtx, int N, T* pDet /* [N] */, int* pIdx /* [N] */) {
	const T zthr = T(1.0e-16);
	const T aprm = T(1 + ::sqrt(17.0)) / 8;
	int eposi = 0;
	int enega = 0;
	pIdx[N - 1] = N - 1;
	int i = 0;
	while (i < N - 1) {
		int ri = i * N;
		int i1 = i + 1;
		int i2 = i + 2;
		T aii = pMtx[ri + i];
		if (aii < 0) aii = -aii;
		pIdx[i] = i;
		T lambda = pMtx[ri + i1];
		int j = i1;
		for (int k = i2; k < N; ++k) {
			T aik = pMtx[ri + k];
			if (aik < 0) aik = -aik;
			if (aik > lambda) {
				j = k;
				lambda = aik;
			}
		}
		int rj = j * N;
		bool flg = true;
		if (aii < aprm*lambda) {
			T sigma = lambda;
			for (int k = i1; k < j; ++k) {
				T akj = pMtx[k*N + j];
				if (akj < 0) akj = -akj;
				if (akj > sigma) sigma = akj;
			}
			for (int k = j + 1; k < N; ++k) {
				T ajk = pMtx[rj + k];
				if (ajk < 0) ajk = -ajk;
				if (ajk > sigma) sigma = ajk;
			}
			if (aii*sigma < lambda) {
				T ajj = pMtx[rj + j];
				if (ajj < 0) ajj = -ajj;
				if (ajj > aprm*sigma) {
					for (int k = j + 1; k < N; ++k) {
						T t = pMtx[rj + k];
						pMtx[rj + k] = pMtx[ri + k];
						pMtx[ri + k] = t;
					}
					for (int k = i1; k < j; ++k) {
						int rk = k*N;
						T t = pMtx[ri + k];
						pMtx[ri + k] = pMtx[rk + j];
						pMtx[rk + j] = t;
					}
					T t = pMtx[ri + i];
					pMtx[ri + i] = pMtx[rj + j];
					pMtx[rj + j] = t;
					pIdx[i] = j;
				} else {
					int ri1 = i1*N;
					if (j > i1) {
						for (int k = j + 1; k < N; ++k) {
							T t = pMtx[rj + k];
							pMtx[rj + k] = pMtx[ri1 + k];
							pMtx[ri1 + k] = t;
						}
						for (int k = i2; k < j; ++k) {
							int rk = k*N;
							T t = pMtx[ri1 + k];
							pMtx[ri1 + k] = pMtx[rk + j];
							pMtx[rk + j] = t;
						}
						T t = pMtx[ri + i];
						pMtx[ri + i] = pMtx[rj + j];
						pMtx[rj + j] = t;
						t = pMtx[ri + j];
						pMtx[ri + j] = pMtx[ri + i1];
						pMtx[ri + i1] = t;
					}
					T t = pMtx[ri + i1];
					T det = pMtx[ri + i]*pMtx[ri1 + i1] - t*t;
					T idet = T(1) / det;
					aii = pMtx[ri + i] * idet;
					T aii1 = pMtx[ri + i1] * idet;
					T ai1i1 = pMtx[ri1 + i1] * idet;
					pIdx[i] = j;
					pIdx[i1] = -1;
					pDet[i] = 1;
					pDet[i1] = det;
					for (int k = i2; k < N; ++k) {
						T u = aii1*pMtx[ri1 + k] - ai1i1*pMtx[ri + k];
						T v = aii1*pMtx[ri + k] - aii*pMtx[ri1 + k];
						int rk = k*N;
						T* pDst = &pMtx[rk + k];
						T* pSrc = &pMtx[ri + k];
						for (int m = k; m < N; ++m) {
							*pDst++ += *pSrc++ * u;
						}
						pDst = &pMtx[rk + k];
						pSrc = &pMtx[ri1 + k];
						for (int m = k; m < N; ++m) {
							*pDst++ += *pSrc++ * v;
						}
						pMtx[ri + k] = u;
						pMtx[ri1 + k] = v;
					}
					i = i2;
					++eposi;
					++enega;
					flg = false;
				}
			}
		}
		if (flg) {
			aii = pMtx[ri + i];
			if (aii < 0) aii = -aii;
			if (aii > zthr) {
				aii = pMtx[ri + i];
				if (aii > 0) {
					++eposi;
				} else {
					++enega;
				}
				pDet[i] = aii;
			}
			T s = -T(1) / aii;
			for (int k = i1; k < N; ++k) {
				int rk = k*N;
				T t = pMtx[ri + k] * s;
				T* pDst = &pMtx[rk + k];
				T* pSrc = &pMtx[ri + k];
				for (int m = k; m < N; ++m) {
					*pDst++ += *pSrc++ * t;
				}
				pMtx[ri + k] = t;
			}
			i = i1;
		}
	}
	if (i == N - 1) {
		int ri = i*N;
		T aii = pMtx[ri + i];
		if (aii < 0) aii = -aii;
		if (aii > zthr) {
			aii = pMtx[ri + i];
			if (aii > 0) {
				++eposi;
			} else {
				++enega;
			}
		}
		pDet[i] = pMtx[i*N + i];
	}
	return (eposi + enega) == N;
}

template<typename T>
inline
void symm_ldlt_solve(T* pAns, const T* pMtx, const T* pRHS, const T* pDet /* [N] */, const int* pIdx /* [N] */, int N) {
	if (pAns != pRHS) {
		for (int i = 0; i < N; ++i) {
			pAns[i] = pRHS[i];
		}
	}
	int i = 0;
	while (i < N - 1) {
		int ri = i*N;
		int idx = pIdx[i];
		T tp = pAns[idx];
		if (pIdx[i + 1] >= 0) {
			pAns[idx] = pAns[i];
			pAns[i] = tp / pMtx[ri + i];
			for (int j = i + 1; j < N; ++j) {
				pAns[j] += pMtx[ri + j] * tp;
			}
			i = i + 1;
		} else {
			int i1 = i + 1;
			int ri1 = i1*N;
			T t = pAns[i];
			pAns[idx] = pAns[i1];
			T idet = T(1) / pDet[i1];
			pAns[i] = (pMtx[ri1 + i1]*t - pMtx[ri + i1]*tp) * idet;
			pAns[i1] = (pMtx[ri + i]*tp - pMtx[ri + i1]*t) * idet;
			for (int j = i + 2; j < N; ++j) {
				pAns[j] += pMtx[ri + j] * t;
			}
			for (int j = i + 2; j < N; ++j) {
				pAns[j] += pMtx[ri1 + j] * tp;
			}
			i = i + 2;
		}
	}
	if (i == N - 1) {
		pAns[i] /= pMtx[i*N + i];
		i = N - 2;
	} else {
		i = N - 3;
	}
	while (i >= 0) {
		int ii = pIdx[i] >= 0 ? i : i - 1;
		for (int j = ii; j <= i; ++j) {
			int rj = j*N;
			T s = pAns[j];
			for (int k = i + 1; k < N; ++k) {
				s += pMtx[rj + k] * pAns[k];
			}
			pAns[j] = s;
		}
		int idx = pIdx[ii];
		T t = pAns[i];
		pAns[i] = pAns[idx];
		pAns[idx] = t;
		i = ii - 1;
	}
}

} // nxLA

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

	void fill(float val) {
		for (int i = 0; i < 4; ++i) {
			(*this)[i] = val;
		}
	}

	void scl(float s) {
		for (int i = 0; i < 4; ++i) {
			(*this)[i] *= s;
		}
	}

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
	float elevation() const { return -::atan2f(y, nxCalc::hypot(x, z)); }

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

inline cxVec from_mem(const float* pSrc) {
	cxVec v;
	v.from_mem(pSrc);
	return v;
}

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

inline cxVec rcp0(const cxVec& v) {
	return cxVec(nxCalc::rcp0(v.x), nxCalc::rcp0(v.y), nxCalc::rcp0(v.z));
}

float dist2(const cxVec& pos0, const cxVec& pos1);
float dist(const cxVec& pos0, const cxVec& pos1);

cxVec get_axis(exAxis axis);
cxVec from_polar_uv(float u, float v);
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
	void invert_fast();
	void invert_lu();
	void invert_lu_hi();

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

	void from_upvec(const cxVec& n);

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

inline cxMtx from_quat_and_pos(const cxQuat& rot, const cxVec& pos) {
	cxMtx m;
	m.from_quat_and_pos(rot, pos);
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

	void zero() { fill(0.0f); }
	void identity() { set(0.0f, 0.0f, 0.0f, 1.0f); }

	cxVec get_axis_x() const { return cxVec(1.0f - (2.0f*y*y) - (2.0f*z*z), (2.0f*x*y) + (2.0f*w*z), (2.0f*x*z) - (2.0f*w*y)); }
	cxVec get_axis_y() const { return cxVec((2.0f*x*y) - (2.0f*w*z), 1.0f - (2.0f*x*x) - (2.0f*z*z), (2.0f*y*z) + (2.0f*w*x)); }
	cxVec get_axis_z() const { return cxVec((2.0f*x*z) + (2.0f*w*y), (2.0f*y*z) - (2.0f*w*x), 1.0f - (2.0f*x*x) - (2.0f*y*y)); }

	void from_mem(const float* pSrc) { set(pSrc[0], pSrc[1], pSrc[2], pSrc[3]); }

	void from_mtx(const cxMtx& m);
	cxMtx to_mtx() const;
	float get_axis_ang(cxVec* pAxis) const;
	cxVec get_log_vec() const;
	void from_log_vec(const cxVec& lvec, bool nrmFlg = true);
	void from_vecs(const cxVec& vfrom, const cxVec& vto);

	float get_real_part() const { return w; }
	cxVec get_imag_part() const { return cxVec(x, y, z); }
	float get_scalar_part() const { return get_real_part(); }
	cxVec get_vector_part() const { return get_imag_part(); }
	void from_parts(const cxVec& imag, float real) { set(imag.x, imag.y, imag.z, real); }

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

	void add(const cxQuat& qa, const cxQuat& qb) {
		for (int i = 0; i < 4; ++i) {
			(*this)[i] = qa[i] + qb[i];
		}
	}

	void add(const cxQuat& q) {
		for (int i = 0; i < 4; ++i) {
			(*this)[i] += q[i];
		}
	}

	void scale(float s) { scl(s); }

	void scale(const cxQuat& q, float s) {
		*this = q;
		scale(s);
	}

	cxQuat get_scaled(float s) const {
		cxQuat q;
		q.scale(*this, s);
		return q;
	}

	void normalize() {
		float len = mag();
		if (len > 0.0f) {
			float rlen = 1.0f / len;
			scale(rlen);
		}
	}

	void normalize(const cxQuat& q) {
		*this = q;
		normalize();
	}

	cxQuat get_normalized() const {
		cxQuat q;
		q.normalize(*this);
		return q;
	}

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

	cxQuat get_conjugate() const {
		cxQuat q;
		q.conjugate(*this);
		return q;
	}

	void invert() {
		float lsq = mag2();
		float rlsq = nxCalc::rcp0(lsq);
		conjugate();
		scale(rlsq);
	}

	void invert(const cxQuat& q) {
		*this = q;
		invert();
	}

	cxQuat get_inverted() const {
		cxQuat q;
		q.invert(*this);
		return q;
	}

	void negate() {
		x = -x;
		y = -y;
		z = -z;
		w = -w;
	}

	void negate(const cxQuat& q) {
		x = -q.x;
		y = -q.y;
		z = -q.z;
		w = -q.w;
	}

	cxQuat get_negated() const {
		cxQuat q;
		q.negate(*this);
		return q;
	}

	void set_rot(const cxVec& axis, float ang);
	void set_rot_x(float rx);
	void set_rot_y(float ry);
	void set_rot_z(float rz);
	void set_rot(float rx, float ry, float rz, exRotOrd ord = exRotOrd::XYZ);
	void set_rot_degrees(float dx, float dy, float dz, exRotOrd ord = exRotOrd::XYZ);
	void set_rot_degrees(const cxVec& r, exRotOrd ord = exRotOrd::XYZ);

	cxVec get_rot(exRotOrd ord = exRotOrd::XYZ) const;
	cxVec get_rot_degrees(exRotOrd ord = exRotOrd::XYZ) const { return get_rot(ord) * XD_RAD2DEG(1.0f); }

	void slerp(const cxQuat& q1, const cxQuat& q2, float t);

	void lerp(const cxQuat& q1, const cxQuat& q2, float t) {
		const float* p1 = q1;
		const float* p2 = q2;
		float* p = *this;
		for (int i = 0; i < 4; ++i) {
			p[i] = p1[i] + (p2[i] - p1[i])*t;
		}
	}

	cxVec apply(const cxVec& v) const;

	cxQuat get_closest_x() const;
	cxQuat get_closest_y() const;
	cxQuat get_closest_z() const;
	cxQuat get_closest_xy() const;
	cxQuat get_closest_yx() const;
	cxQuat get_closest_xz() const;
	cxQuat get_closest_zx() const;
	cxQuat get_closest_yz() const;
	cxQuat get_closest_zy() const;
};

inline cxQuat operator * (const cxQuat& q1, const cxQuat& q2) { cxQuat q = q1; q.mul(q2); return q; }
inline cxQuat operator + (const cxQuat& q1, const cxQuat& q2) { cxQuat q = q1; q.add(q2); return q; }
inline cxQuat operator * (const cxQuat& q, const float s) { cxQuat r = q; r.scl(s); return r; }

namespace nxQuat {

inline cxQuat identity() {
	cxQuat q;
	q.identity();
	return q;
}

inline cxQuat zero() {
	cxQuat q;
	q.zero();
	return q;
}
	
inline cxQuat slerp(const cxQuat& q1, const cxQuat& q2, float t) {
	cxQuat q;
	q.slerp(q1, q2, t);
	return q;
}

inline cxQuat from_radians(float rx, float ry, float rz, exRotOrd ord = exRotOrd::XYZ) {
	cxQuat q;
	q.set_rot(rx, ry, rz, ord);
	return q;
}

inline cxQuat from_degrees(float dx, float dy, float dz, exRotOrd ord = exRotOrd::XYZ) {
	cxQuat q;
	q.set_rot_degrees(dx, dy, dz, ord);
	return q;
}

cxQuat log(const cxQuat& q);
cxQuat exp(const cxQuat& q);
cxQuat pow(const cxQuat& q, float x);

float arc_dist(const cxQuat& a, const cxQuat& b);

} // nxQuat


class cxDualQuat {
protected:
	cxQuat mReal;
	cxQuat mDual;

public:

	void set(const cxQuat& rot, const cxVec& pos) {
		mReal = rot;
		mDual = cxQuat(pos.x, pos.y, pos.z, 0.0f) * mReal * 0.5f; // RT
	}

	void set_tr(const cxQuat& rot, const cxVec& pos) {
		mReal = rot;
		mDual = mReal * cxQuat(pos.x, pos.y, pos.z, 0.0f) * 0.5f; // TR
	}

	void set_parts(const cxQuat& qreal, const cxQuat& qdual) {
		mReal = qreal;
		mDual = qdual;
	}

	cxQuat get_real_part() const { return mReal; }
	cxQuat get_dual_part() const { return mDual; }

	void normalize() {
		float s = nxCalc::rcp0(mReal.mag());
		mReal.scl(s);
		mDual.scl(s);
	}

	void mul(const cxDualQuat& dq);

	cxVec get_pos() const {
		return (mDual.get_scaled(2.0f) * mReal.get_conjugate()).get_vector_part();
	}

	cxMtx to_mtx() const {
		cxMtx m = mReal.to_mtx();
		m.set_translation(get_pos());
		return m;
	}

	cxMtx get_inv_mtx() const {
		cxDualQuat idq;
		idq.mReal = mReal.get_conjugate();
		idq.mDual = mDual.get_conjugate();
		return idq.to_mtx();
	}

	cxVec calc_pnt(const cxVec& pos) const;

	cxVec calc_vec(const cxVec& vec) const {
		return mReal.apply(vec);
	}

	void interpolate(const cxDualQuat& dqA, const cxDualQuat& dqB, float t);
};

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
	union {
		struct { float r, g, b, a; };
		float ch[4];
	};

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

	void set(const cxVec& v) {
		set(v.x, v.y, v.z);
	}

	void zero() {
		for (int i = 0; i < 4; ++i) {
			ch[i] = 0.0f;
		}
	}

	void zero_rgb() {
		for (int i = 0; i < 3; ++i) {
			ch[i] = 0.0f;
		}
		a = 1.0f;
	}

	float luma() const { return r*0.299f + g*0.587f + b*0.114f; }
	float luminance() const { return r*0.212671f + g*0.71516f + b*0.072169f; }
	float average() const { return (r + g + b) / 3.0f; }
	float min() const { return nxCalc::min(r, g, b); }
	float max() const { return nxCalc::max(r, g, b); }

	void clip_neg() {
		for (int i = 0; i < 4; ++i) {
			ch[i] = nxCalc::max(ch[i], 0.0f);
		}
	}

	void scl(float s) {
		for (int i = 0; i < 4; ++i) {
			ch[i] *= s;
		}
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
		for (int i = 0; i < 4; ++i) {
			ch[i] += c.ch[i];
		}
	}

	void add_rgb(const cxColor& c) {
		r += c.r;
		g += c.g;
		b += c.b;
	}

	cxVec YCgCo() const;
	void from_YCgCo(const cxVec& ygo);
	cxVec TMI() const;
	void from_TMI(const cxVec& tmi);
	cxVec XYZ(cxMtx* pRGB2XYZ = nullptr) const;
	void from_XYZ(const cxVec& xyz, cxMtx* pXYZ2RGB = nullptr);
	cxVec Lab(cxMtx* pRGB2XYZ = nullptr) const;
	void from_Lab(const cxVec& lab, cxMtx* pRGB2XYZ = nullptr, cxMtx* pXYZ2RGB = nullptr);
	cxVec Lch(cxMtx* pRGB2XYZ = nullptr) const;
	void from_Lch(const cxVec& lch, cxMtx* pRGB2XYZ = nullptr, cxMtx* pXYZ2RGB = nullptr);

	uint32_t encode_rgbe() const;
	void decode_rgbe(uint32_t rgbe);
	uint32_t encode_bgre() const;
	void decode_bgre(uint32_t bgre);
	uint32_t encode_rgbi() const;
	void decode_rgbi(uint32_t rgbi);
	uint32_t encode_bgri() const;
	void decode_bgri(uint32_t bgri);
	uint32_t encode_rgba8() const;
	void decode_rgba8(uint32_t rgba);
	uint32_t encode_bgra8() const;
	void decode_bgra8(uint32_t bgra);
};

namespace nxColor {

void init_XYZ_transform(cxMtx* pRGB2XYZ, cxMtx* pXYZ2RGB, cxVec* pPrims = nullptr, cxVec* pWhite = nullptr);
void init_XYZ_transform_xy_w(cxMtx* pRGB2XYZ, cxMtx* pXYZ2RGB, float wx = 0.31271f, float wy = 0.32902f);
void init_XYZ_transform_xy(cxMtx* pRGB2XYZ, cxMtx* pXYZ2RGB, float rx = 0.64f, float ry = 0.33f, float gx = 0.3f, float gy = 0.6f, float bx = 0.15f, float by = 0.06f, float wx = 0.31271f, float wy = 0.32902f);
cxVec XYZ_to_Lab(const cxVec& xyz, cxMtx* pRGB2XYZ = nullptr);
cxVec Lab_to_XYZ(const cxVec& lab, cxMtx* pRGB2XYZ = nullptr);
cxVec Lab_to_Lch(const cxVec& lab);
cxVec Lch_to_Lab(const cxVec& lch);
float Lch_perceived_lightness(const cxVec& lch);

float approx_CMF_x31(float w);
float approx_CMF_y31(float w);
float approx_CMF_z31(float w);
float approx_CMF_x64(float w);
float approx_CMF_y64(float w);
float approx_CMF_z64(float w);

inline cxColor lerp(const cxColor& cA, const cxColor& cB, float t) {
	cxColor c;
	for (int i = 0; i < 4; ++i) {
		c.ch[i] = nxCalc::lerp(cA.ch[i], cB.ch[i], t);
	}
	return c;
}

} // nxColor


struct sxCompiledExpression;

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
		LIMBS = XD_FOURCC('L', 'I', 'M', 'B'),
		EXPRS = XD_FOURCC('E', 'X', 'P', 'R')
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
		uint32_t mParam;

		eInfoKind get_kind() const { return (eInfoKind)mKind; }
	};

	struct ExprInfo {
		int16_t mNodeId;
		int8_t mChanId;
		uint8_t mReserved;

		exAnimChan get_chan_id() const { return (exAnimChan)mChanId; }
		sxCompiledExpression* get_code() { return reinterpret_cast<sxCompiledExpression*>(this + 1); }
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
		uint8_t mFlags;

		eLimbType get_type() const { return (eLimbType)(mLimbId & 0xF); }
		int get_idx() const { return (mLimbId >> 4) & 0xF; }
		exAxis get_axis() const { return (exAxis)mAxis; }
		exAxis get_up_axis() const { return (exAxis)mUp; }
		bool get_ext_comp_flg() const { return !!(mFlags & 1); }
	};

	struct LimbChain {
		class AdjustFunc {
		public:
			AdjustFunc() {}
			virtual ~AdjustFunc() {}
			virtual cxVec operator()(const sxRigData& rig, const LimbChain& chain, const cxVec& pos) { return pos; }
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

	void calc_limb_local(LimbChain::Solution* pSolution, const LimbChain& chain, cxMtx* pMtx, LimbChain::AdjustFunc* pAdjFunc = nullptr) const;
	void copy_limb_solution(cxMtx* pDstMtx, const LimbChain& chain, const LimbChain::Solution& solution);

	bool has_info_list() const;
	Info* find_info(eInfoKind kind) const;
	LimbInfo* find_limb(eLimbType type, int idx = 0) const;

	int get_expr_num() const;
	int get_expr_stack_size() const;
	ExprInfo* get_expr_info(int idx) const;
	sxCompiledExpression* get_expr_code(int idx) const;

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

	class DisplayList {
	public:
		struct Batch {
			int32_t mMtlId;
			int32_t mGrpId; /* -1 for material groups */
			int32_t mMinIdx;
			int32_t mMaxIdx;
			int32_t mIdxOrg;
			int32_t mTriNum;
			int32_t mMaxWgtNum;
			int32_t mSkinNodesNum;

			bool is_idx16() const { return (mMaxIdx - mMinIdx) < (1 << 16); }
		};

	private:
		struct Data {
			uint32_t mOffsBat;
			uint32_t mOffsIdx32;
			uint32_t mOffsIdx16;
			uint32_t mMtlNum;
			uint32_t mBatNum;
			uint32_t mTriNum;
			uint32_t mIdx16Num;
			uint32_t mIdx32Num;

			Batch* get_bat_top() { return mOffsBat ? reinterpret_cast<Batch*>(XD_INCR_PTR(this, mOffsBat)) : nullptr; }
			uint16_t* get_idx16() { return mOffsIdx16 ? reinterpret_cast<uint16_t*>(XD_INCR_PTR(this, mOffsIdx16)) : nullptr; }
			uint32_t* get_idx32() { return mOffsIdx32 ? reinterpret_cast<uint32_t*>(XD_INCR_PTR(this, mOffsIdx32)) : nullptr; }
		};

		const sxGeometryData* mpGeo;
		Data* mpData;

		DisplayList() : mpGeo(nullptr), mpData(nullptr) {}

		void create(const sxGeometryData& geo, const char* pBatGrpPrefix = nullptr);

		friend struct sxGeometryData;

	public:
		~DisplayList() { destroy(); }

		void destroy();

		bool is_valid() const { return mpGeo != nullptr && mpData != nullptr; }

		int get_mtl_num() const { return mpData ? mpData->mMtlNum : 0; }
		int get_bat_num() const { return mpData ? mpData->mBatNum : 0; }
		int get_tri_num() const { return mpData ? mpData->mTriNum : 0; }

		int get_idx16_num() const { return mpData ? mpData->mIdx16Num : 0; }
		uint16_t* get_idx16_buf() { return mpData ? mpData->get_idx16() : nullptr; }
		int get_idx32_num() const { return mpData ? mpData->mIdx32Num : 0; }
		uint32_t* get_idx32_buf() { return mpData ? mpData->get_idx32() : nullptr; }

		bool bat_idx_ck(int idx) const { return mpData ? (uint32_t)idx < mpData->mBatNum : false; }
		Batch* get_bat(int idx) const { return bat_idx_ck(idx) ? mpData->get_bat_top() + idx : nullptr; }
		const char* get_bat_mtl_name(int batIdx) const;
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
	bool has_glb_attr(const char* pName) const { return find_glb_attr(pName) >= 0; }
	bool has_pnt_attr(const char* pName) const { return find_pnt_attr(pName) >= 0; }
	bool has_pol_attr(const char* pName) const { return find_pol_attr(pName) >= 0; }
	AttrInfo* get_attr_info(int attrIdx, eAttrClass cls) const;
	AttrInfo* get_glb_attr_info(int attrIdx) const { return get_attr_info(attrIdx, eAttrClass::GLOBAL); }
	AttrInfo* get_pnt_attr_info(int attrIdx) const { return get_attr_info(attrIdx, eAttrClass::POINT); }
	AttrInfo* get_pol_attr_info(int attrIdx) const { return get_attr_info(attrIdx, eAttrClass::POLYGON); }
	const char* get_attr_val_s(int attrIdx, eAttrClass cls, int itemIdx) const;
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
	const char* get_mtl_name(int idx) const;
	const char* get_mtl_path(int idx) const;
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
	void calc_tangents(cxVec* pTng, bool flip = false, const char* pAttrName = nullptr) const;
	cxVec* calc_tangents(bool flip = false, const char* pAttrName = nullptr) const;

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

	bool is_dds128() const { return mFormat.mFourCC == 0x74; }
	bool is_dds64() const { return mFormat.mFourCC == 0x71; }
	bool is_dds32() const { return mFormat.mFourCC == 0 && mFormat.mBitCount == 32; }

	void set_encoding(uint32_t code) { mReserved1[1] = code; }
	uint32_t get_encoding() const { return mReserved1[1]; }

	void set_gamma(float gamma) {
		float* pGamma = reinterpret_cast<float*>(&mReserved1[2]);
		*pGamma = gamma;
	}

	float get_gamma() const {
		const float* pGamma = reinterpret_cast<const float*>(&mReserved1[2]);
		return *pGamma;
	}

	void init(uint32_t w, uint32_t h);
	void init_dds128(uint32_t w, uint32_t h);
	void init_dds64(uint32_t w, uint32_t h);
	void init_dds32(uint32_t w, uint32_t h, bool encRGB = true);

	static const uint32_t ENC_RGBE = XD_FOURCC('R', 'G', 'B', 'E');
	static const uint32_t ENC_BGRE = XD_FOURCC('B', 'G', 'R', 'E');
	static const uint32_t ENC_RGBI = XD_FOURCC('R', 'G', 'B', 'I');
	static const uint32_t ENC_BGRI = XD_FOURCC('B', 'G', 'R', 'I');
};

namespace nxTexture {

sxDDSHead* alloc_dds128(uint32_t w, uint32_t h, uint32_t* pSize = nullptr);
void save_dds128(const char* pPath, cxColor* pClr, uint32_t w, uint32_t h);
void save_dds64(const char* pPath, cxColor* pClr, uint32_t w, uint32_t h);
void save_dds32_rgbe(const char* pPath, cxColor* pClr, uint32_t w, uint32_t h);
void save_dds32_bgre(const char* pPath, cxColor* pClr, uint32_t w, uint32_t h);
void save_dds32_rgbi(const char* pPath, cxColor* pClr, uint32_t w, uint32_t h);
void save_dds32_bgri(const char* pPath, cxColor* pClr, uint32_t w, uint32_t h);
void save_dds32_rgba8(const char* pPath, cxColor* pClr, uint32_t w, uint32_t h, float gamma = 2.2f);
void save_dds32_bgra8(const char* pPath, cxColor* pClr, uint32_t w, uint32_t h, float gamma = 2.2f);

cxColor* decode_dds(sxDDSHead* pDDS, uint32_t* pWidth, uint32_t* pHeight, float gamma = 2.2f);

void calc_resample_wgts(int oldRes, int newRes, xt_float4* pWgt, int16_t* pOrg);

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
		void free();
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
	Plane get_plane(int idx) const;
	PlaneInfo* get_plane_info(int idx) const { return ck_plane_idx(idx) ? reinterpret_cast<PlaneInfo*>(XD_INCR_PTR(this, mPlaneOffs)) + idx : nullptr; }
	PlaneInfo* find_plane_info(const char* pName) const { return get_plane_info(find_plane_idx(pName)); }
	bool is_plane_hdr(int idx) const;
	bool is_hdr() const;
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

			float get_pos_chan(int idx);
			float get_rot_chan(int idx);
			float get_scl_chan(int idx);
			float get_anim_chan(exAnimChan chan);
			bool ck_pos_chan(int idx);
			bool ck_rot_chan(int idx);
			bool ck_scl_chan(int idx);
			bool ck_anim_chan(exAnimChan chan);
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
inline constexpr int calc_consts_num(int order) { return order < 1 ? 0 : order*order - (order - 1); }
inline int calc_ary_idx(int l, int m) { return l*(l+1) + m; }
inline int band_idx_from_ary_idx(int idx) { return (int)::sqrtf((float)idx); }
inline int func_idx_from_ary_band(int idx, int l) { return idx - l*(l + 1); }

void calc_weights(float* pWgt, int order, float s, float scl = 1.0f);
void get_diff_weights(float* pWgt, int order, float scl = 1.0f);
void apply_weights(float* pDst, int order, const float* pSrc, const float* pWgt, int nelems = 1);
void calc_consts(int order, float* pConsts);
void calc_consts(int order, double* pConsts);
void eval(int order, float* pCoefs, float x, float y, float z, const float* pConsts);
void eval_ary4(int order, float* pCoefs, float x[4], float y[4], float z[4], const float* pConsts);
void eval_ary8(int order, float* pCoefs, float x[8], float y[8], float z[8], const float* pConsts);
void eval_sh3(float* pCoefs, float x, float y, float z, const float* pConsts);
void eval_sh3_ary4(float* pCoefs, float x[4], float y[4], float z[4], const float* pConsts);
void eval_sh3_ary8(float* pCoefs, float x[8], float y[8], float z[8], const float* pConsts);

} // nxSH

namespace nxDataUtil {

exAnimChan anim_chan_from_str(const char* pStr);
const char* anim_chan_to_str(exAnimChan chan);
exRotOrd rot_ord_from_str(const char* pStr);
const char* rot_ord_to_str(exRotOrd rord);
exTransformOrd xform_ord_from_str(const char* pStr);
const char* xform_ord_to_str(exTransformOrd xord);

} // nxDataUtil

struct sxPackedData {
	uint32_t mSig;
	uint32_t mAttr;
	uint32_t mPackSize;
	uint32_t mRawSize;

	int get_mode() const { return (int)(mAttr & 0xFF); }

	static const uint32_t SIG = XD_FOURCC('x', 'p', 'k', 'd');
};

namespace nxData {

sxData* load(const char* pPath);
void unload(sxData* pData);

sxPackedData* pack(const uint8_t* pSrc, uint32_t srcSize, uint32_t mode = 0);
uint8_t* unpack(sxPackedData* pPkd, uint32_t memTag = XD_TMP_MEM_TAG, uint8_t* pDstMem = nullptr, uint32_t dstMemSize = 0);

} // nxData

