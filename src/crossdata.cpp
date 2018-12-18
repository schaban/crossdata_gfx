/*
 * Author: Sergey Chaban <sergey.chaban@gmail.com>
 */

#include "crossdata.hpp"

namespace nxSys {

FILE* x_fopen(const char* fpath, const char* mode) {
#if defined(_MSC_VER)
	FILE* f;
	::fopen_s(&f, fpath, mode);
	return f;
#else
	return ::fopen(fpath, mode);
#endif
}

void x_strcpy(char* pDst, size_t size, const char* pSrc) {
#if defined(_MSC_VER)
	::strcpy_s(pDst, size, pSrc);
#else
	::strcpy(pDst, pSrc);
#endif
}


void* def_malloc(size_t size) {
	return ::malloc(size);
}

void def_free(void* p) {
	if (p) {
		::free(p);
	}
}

XD_FHANDLE def_fopen(const char* fpath) {
	return (XD_FHANDLE)x_fopen(fpath, "rb");
}

void def_fclose(XD_FHANDLE fh) {
	::fclose((FILE*)fh);
}

size_t def_fsize(XD_FHANDLE fh) {
	long len = 0;
	FILE* f = (FILE*)fh;
	long old = ftell(f);
	if (0 == fseek(f, 0, SEEK_END)) {
		len = ftell(f);
	}
	fseek(f, old, SEEK_SET);
	return (size_t)len;
}

size_t def_fread(XD_FHANDLE fh, void* pDst, size_t size) {
	size_t nread = 0;
	if (fh && pDst && size) {
		FILE* f = (FILE*)fh;
		nread = fread(pDst, 1, size, f);
	}
	return nread;
}

void def_dbgmsg(const char* pMsg) {
}

static sxSysIfc s_ifc {
	def_malloc,
	def_free,
	def_fopen,
	def_fclose,
	def_fsize,
	def_fread,
	def_dbgmsg
};

void init(sxSysIfc* pIfc) {
	if (pIfc) {
		::memcpy(&s_ifc, pIfc, sizeof(sxSysIfc));
	}
}

void* malloc(size_t size) {
	void* p = nullptr;
	if (s_ifc.fn_malloc) {
		p = s_ifc.fn_malloc(size);
	} else {
		p = def_malloc(size);
	}
	return p;
}

void free(void* p) {
	if (s_ifc.fn_free) {
		s_ifc.fn_free(p);
	} else {
		def_free(p);
	}
}

XD_FHANDLE fopen(const char* fpath) {
	XD_FHANDLE fh = nullptr;
	if (s_ifc.fn_fopen) {
		fh = s_ifc.fn_fopen(fpath);
	} else {
		fh = def_fopen(fpath);
	}
	return fh;
}

void fclose(XD_FHANDLE fh) {
	if (s_ifc.fn_fclose) {
		s_ifc.fn_fclose(fh);
	} else {
		def_fclose(fh);
	}
}

size_t fsize(XD_FHANDLE fh) {
	size_t size = 0;
	if (s_ifc.fn_fsize) {
		size = s_ifc.fn_fsize(fh);
	} else {
		size = def_fsize(fh);
	}
	return size;
}

size_t fread(XD_FHANDLE fh, void* pDst, size_t size) {
	size_t nread = 0;
	if (s_ifc.fn_fread) {
		nread = s_ifc.fn_fread(fh, pDst, size);
	} else {
		nread = def_fread(fh, pDst, size);
	}
	return nread;
}

void dbgmsg(const char* pMsg) {
	if (s_ifc.fn_dbgmsg) {
		s_ifc.fn_dbgmsg(pMsg);
	} else {
		def_dbgmsg(pMsg);
	}
}

FILE* fopen_w_txt(const char* fpath) {
	return x_fopen(fpath, "w");
}

FILE* fopen_w_bin(const char* fpath) {
	return x_fopen(fpath, "wb");
}

} // nxSys


namespace nxCore {

struct sxMemInfo {
	sxMemInfo* mpPrev;
	sxMemInfo* mpNext;
	size_t mSize;
	size_t mRawSize;
	uint32_t mTag;
	uint32_t mOffs;
	uint32_t mAlgn;
};

static sxMemInfo* s_pMemHead = nullptr;
static sxMemInfo* s_pMemTail = nullptr;
static uint32_t s_allocCount = 0;
static uint64_t s_allocBytes = 0;
static uint64_t s_allocPeakBytes = 0;

sxMemInfo* mem_info_from_addr(void* pMem) {
	sxMemInfo* pMemInfo = nullptr;
	if (pMem) {
		uint8_t* pOffs = (uint8_t*)pMem - sizeof(uint32_t);
		uint32_t offs = 0;
		for (int i = 0; i < 4; ++i) {
			offs |= pOffs[i] << (i * 8);
		}
		pMemInfo = (sxMemInfo*)((uint8_t*)pMem - offs);
	}
	return pMemInfo;
}

void* mem_addr_from_info(sxMemInfo* pInfo) {
	void* p = nullptr;
	if (pInfo) {
		p = (uint8_t*)pInfo + pInfo->mOffs;
	}
	return p;
}

sxMemInfo* find_mem_info(void* pMem) {
	sxMemInfo* pMemInfo = nullptr;
	sxMemInfo* pCkInfo = mem_info_from_addr(pMem);
	sxMemInfo* pWkInfo = s_pMemHead;
	while (pWkInfo) {
		if (pWkInfo == pCkInfo) {
			pMemInfo = pWkInfo;
			pWkInfo = nullptr;
		} else {
			pWkInfo = pWkInfo->mpNext;
		}
	}
	return pMemInfo;
}

void* mem_alloc(size_t size, uint32_t tag, int alignment) {
	void* p = nullptr;
	if (alignment < 1) alignment = 0x10;
	if (size > 0) {
		size_t asize = nxCore::align_pad(size + sizeof(sxMemInfo) + alignment + sizeof(uint32_t), alignment);
		void* p0 = nxSys::malloc(asize);
		if (p0) {
			p = (void*)nxCore::align_pad((uintptr_t)((uint8_t*)p0 + sizeof(sxMemInfo) + sizeof(uint32_t)), alignment);
			uint32_t offs = (uint32_t)((uint8_t*)p - (uint8_t*)p0);
			uint8_t* pOffs = (uint8_t*)p - sizeof(uint32_t);
			pOffs[0] = offs & 0xFF;
			pOffs[1] = (offs >> 8) & 0xFF;
			pOffs[2] = (offs >> 16) & 0xFF;
			pOffs[3] = (offs >> 24) & 0xFF;
			sxMemInfo* pInfo = mem_info_from_addr(p);
			pInfo->mRawSize = asize;
			pInfo->mSize = size;
			pInfo->mTag = tag;
			pInfo->mOffs = offs;
			pInfo->mAlgn = alignment;
			pInfo->mpPrev = nullptr;
			pInfo->mpNext = nullptr;
			if (!s_pMemHead) {
				s_pMemHead = pInfo;
			} else {
				pInfo->mpPrev = s_pMemTail;
			}
			if (s_pMemTail) {
				s_pMemTail->mpNext = pInfo;
			}
			s_pMemTail = pInfo;
			s_allocBytes += pInfo->mSize;
			s_allocPeakBytes = nxCalc::max(s_allocPeakBytes, s_allocBytes);
			++s_allocCount;
		}
	}
	return p;
}

void* mem_realloc(void* pMem, size_t newSize, int alignment) {
	void* pNewMem = pMem;
	if (pMem && newSize) {
		sxMemInfo* pMemInfo = find_mem_info(pMem);
		if (pMemInfo) {
			uint32_t oldTag = pMemInfo->mTag;
			size_t oldSize = pMemInfo->mSize;
			size_t cpySize = nxCalc::min(oldSize, newSize);
			if (alignment < 1) alignment = pMemInfo->mAlgn;
			void* p = mem_alloc(newSize, oldTag, alignment);
			if (p) {
				pNewMem = p;
				::memcpy(pNewMem, pMem, cpySize);
				mem_free(pMem);
			}
		} else {
			dbg_msg("invalid realloc request: %p\n", pMem);
		}
	}
	return pNewMem;
}

void* mem_resize(void* pMem, float factor, int alignment) {
	void* pNewMem = pMem;
	if (pMem && factor > 0.0f) {
		sxMemInfo* pMemInfo = find_mem_info(pMem);
		if (pMemInfo) {
			uint32_t oldTag = pMemInfo->mTag;
			size_t oldSize = pMemInfo->mSize;
			size_t newSize = (size_t)::ceilf((float)oldSize * factor);
			size_t cpySize = nxCalc::min(oldSize, newSize);
			if (alignment < 1) alignment = pMemInfo->mAlgn;
			void* p = mem_alloc(newSize, oldTag, alignment);
			if (p) {
				pNewMem = p;
				::memcpy(pNewMem, pMem, cpySize);
				mem_free(pMem);
			}
		} else {
			dbg_msg("memory @ %p cannot be resized\n", pMem);
		}
	}
	return pNewMem;
}

void mem_free(void* pMem) {
	if (!pMem) return;
	sxMemInfo* pInfo = find_mem_info(pMem);
	if (!pInfo) {
		dbg_msg("cannot free memory @ %p\n", pMem);
		return;
	}
	sxMemInfo* pNext = pInfo->mpNext;
	if (pInfo->mpPrev) {
		pInfo->mpPrev->mpNext = pNext;
		if (pNext) {
			pNext->mpPrev = pInfo->mpPrev;
		} else {
			s_pMemTail = pInfo->mpPrev;
		}
	} else {
		s_pMemHead = pNext;
		if (pNext) {
			pNext->mpPrev = nullptr;
		} else {
			s_pMemTail = nullptr;
		}
	}
	s_allocBytes -= pInfo->mSize;
	void* p0 = (void*)((intptr_t)pMem - pInfo->mOffs);
	nxSys::free(p0);
	--s_allocCount;
}

size_t mem_size(void* pMem) {
	size_t size = 0;
	sxMemInfo* pMemInfo = find_mem_info(pMem);
	if (pMemInfo) {
		size = pMemInfo->mSize;
	}
	return size;
}

uint32_t mem_tag(void* pMem) {
	uint32_t tag = 0;
	sxMemInfo* pMemInfo = find_mem_info(pMem);
	if (pMemInfo) {
		tag = pMemInfo->mTag;
	}
	return tag;
}

void mem_dbg() {
	dbg_msg("%d allocs\n", s_allocCount);
	sxMemInfo* pInfo = s_pMemHead;
	char tag[5];
	tag[4] = 0;
	while (pInfo) {
		void* pMem = mem_addr_from_info(pInfo);
		::memcpy(tag, &pInfo->mTag, 4);
		dbg_msg("%s @ %p, size=0x%X (0x%X, 0x%X)\n", tag, pMem, pInfo->mSize, pInfo->mRawSize, pInfo->mAlgn);
		if (pInfo->mTag == XD_DAT_MEM_TAG) {
			sxData* pData = (sxData*)pMem;
			::memcpy(tag, &pData->mKind, 4);
			dbg_msg("  %s: %s\n", tag, pData->get_file_path());
		}
		pInfo = pInfo->mpNext;
	}
}

void dbg_msg(const char* fmt, ...) {
	char msg[1024 * 2];
	va_list mrk;
	va_start(mrk, fmt);
#if defined(_MSC_VER)
	::vsprintf_s(msg, sizeof(msg), fmt, mrk);
#else
	::vsprintf(msg, fmt, mrk);
#endif
	va_end(mrk);
	nxSys::dbgmsg(msg);
}

static void* bin_load_impl(const char* pPath, size_t* pSize, bool appendPath, bool unpack, uint32_t tag) {
	void* pData = nullptr;
	size_t size = 0;
	XD_FHANDLE fh = nxSys::fopen(pPath);
	if (fh) {
		size_t pathLen = ::strlen(pPath);
		size_t fsize = nxSys::fsize(fh);
		size_t memsize = fsize;
		if (appendPath) {
			memsize += pathLen + 1;
		}
		pData = mem_alloc(memsize, tag);
		if (pData) {
			size = nxSys::fread(fh, pData, fsize);
		}
		nxSys::fclose(fh);
		if (pData && unpack && fsize > sizeof(sxPackedData)) {
			sxPackedData* pPkd = (sxPackedData*)pData;
			if (pPkd->mSig == sxPackedData::SIG && pPkd->mPackSize == fsize) {
				size_t memsize0 = memsize;
				memsize = pPkd->mRawSize;
				if (appendPath) {
					memsize += pathLen + 1;
				}
				uint8_t* pUnpkd = (uint8_t*)mem_alloc(memsize, tag);
				if (nxData::unpack(pPkd, tag, pUnpkd, (uint32_t)memsize)) {
					size = pPkd->mRawSize;
					fsize = size;
					mem_free(pData);
					pData = pUnpkd;
				} else {
					mem_free(pUnpkd);
					memsize = memsize0;
				}
			}
		}
		if (pData && appendPath) {
			nxSys::x_strcpy(&((char*)pData)[fsize], pathLen + 1, pPath);
		}
	}
	if (pSize) {
		*pSize = size;
	}
	return pData;
}

void* bin_load(const char* pPath, size_t* pSize, bool appendPath, bool unpack) {
	return bin_load_impl(pPath, pSize, appendPath, unpack, XD_FOURCC('X', 'B', 'I', 'N'));
}

void bin_unload(void* pMem) {
	mem_free(pMem);
}

void bin_save(const char* pPath, const void* pMem, size_t size) {
	if (!pPath || !pMem || !size) return;
	FILE* f = nxSys::fopen_w_bin(pPath);
	if (f) {
		::fwrite(pMem, 1, size, f);
		::fclose(f);
	}
}

// http://www.isthe.com/chongo/tech/comp/fnv/index.html
uint32_t str_hash32(const char* pStr) {
	if (!pStr) return 0;
	uint32_t h = 2166136261;
	while (true) {
		uint8_t c = *pStr++;
		if (!c) break;
		h *= 16777619;
		h ^= c;
	}
	return h;
}

uint16_t str_hash16(const char* pStr) {
	uint32_t h = str_hash32(pStr);
	return (uint16_t)((h >> 16) ^ (h & 0xFFFF));
}

bool str_eq(const char* pStrA, const char* pStrB) {
	bool res = false;
	if (pStrA && pStrB) {
		res = ::strcmp(pStrA, pStrB) == 0;
	}
	return res;
}

bool str_starts_with(const char* pStr, const char* pPrefix) {
	if (pStr && pPrefix) {
		size_t len = ::strlen(pPrefix);
		for (size_t i = 0; i < len; ++i) {
			char ch = pStr[i];
			if (0 == ch || ch != pPrefix[i]) return false;
		}
		return true;
	}
	return false;
}

bool str_ends_with(const char* pStr, const char* pPostfix) {
	if (pStr && pPostfix) {
		size_t lenStr = ::strlen(pStr);
		size_t lenPost = ::strlen(pPostfix);
		if (lenStr < lenPost) return false;
		for (size_t i = 0; i < lenPost; ++i) {
			if (pStr[lenStr - lenPost + i] != pPostfix[i]) return false;
		}
		return true;
	}
	return false;
}

char* str_dup(const char* pSrc) {
	char* pDst = nullptr;
	if (pSrc) {
		size_t len = ::strlen(pSrc) + 1;
		pDst = reinterpret_cast<char*>(mem_alloc(len, XD_FOURCC('X', 'S', 'T', 'R')));
		::memcpy(pDst, pSrc, len);
	}
	return pDst;
}

uint16_t float_to_half(float x) {
	uint32_t b = f32_get_bits(x);
	uint16_t s = (uint16_t)((b >> 16) & (1 << 15));
	b &= ~(1U << 31);
	if (b > 0x477FE000U) {
		/* infinity */
		b = 0x7C00;
	} else {
		if (b < 0x38800000U) {
			uint32_t r = 0x70 + 1 - (b >> 23);
			b &= (1U << 23) - 1;
			b |= (1U << 23);
			b >>= r;
		} else {
			b += 0xC8000000U;
		}
		uint32_t a = (b >> 13) & 1;
		b += (1U << 12) - 1;
		b += a;
		b >>= 13;
		b &= (1U << 15) - 1;
	}
	return (uint16_t)(b | s);
}

float half_to_float(uint16_t h) {
	float f = 0.0f;
	if (h & ((1 << 16) - 1)) {
		int32_t e = (((h >> 10) & 0x1F) + 0x70) << 23;
		uint32_t m = (h & ((1 << 10) - 1)) << (23 - 10);
		uint32_t s = (uint32_t)(h >> 15) << 31;
		f = f32_set_bits(e | m | s);
	}
	return f;
}

float f32_set_bits(uint32_t x) {
	uxVal32 v;
	v.u = x;
	return v.f;
}

uint32_t f32_get_bits(float x) {
	uxVal32 v;
	v.f = x;
	return v.u;
}

uint32_t fetch_bits32(uint8_t* pTop, uint32_t org, uint32_t len) {
	uint64_t res = 0;
	uint32_t idx = org >> 3;
	::memcpy(&res, &pTop[idx], 5);
	res >>= org & 7;
	res &= (1ULL << len) - 1;
	return (uint32_t)res;
}

static sxRNG s_rng = { /* seed(1) -> */ 0x910A2DEC89025CC1ULL, 0xBEEB8DA1658EEC67ULL };

void rng_seed(sxRNG* pState, uint64_t seed) {
	if (!pState) {
		pState = &s_rng;
	}
	if (seed == 0) {
		seed = 1;
	}
	uint64_t st = seed;
	for (int i = 0; i < 2; ++i) {
		/* splitmix64 */
		st += 0x9E3779B97F4A7C15ULL;
		uint64_t z = st;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
		z ^= (z >> 31);
		pState->s[i] = z;
	}
}

/* xoroshiro128+ */
uint64_t rng_next(sxRNG* pState) {
	if (!pState) {
		pState = &s_rng;
	}
	uint64_t s0 = pState->s[0];
	uint64_t s1 = pState->s[1];
	uint64_t r = s0 + s1;
	s1 ^= s0;
	s0 = (s0 << 55) | (s0 >> 9);
	s0 ^= s1 ^ (s1 << 14);
	s1 = (s1 << 36) | (s1 >> 28);
	pState->s[0] = s0;
	pState->s[1] = s1;
	return r;
}

float rng_f01(sxRNG* pState) {
	uint32_t bits = (uint32_t)rng_next(pState);
	bits &= 0x7FFFFF;
	bits |= f32_get_bits(1.0f);
	float f = f32_set_bits(bits);
	return f - 1.0f;
}

} // nxCore


namespace nxCalc {

static float s_identityMtx[4][4] = {
	{ 1.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f },
};

float hermite(float p0, float m0, float p1, float m1, float t) {
	float tt = t*t;
	float ttt = tt*t;
	float tt3 = tt*3.0f;
	float tt2 = tt + tt;
	float ttt2 = tt2 * t;
	float h00 = ttt2 - tt3 + 1.0f;
	float h10 = ttt - tt2 + t;
	float h01 = -ttt2 + tt3;
	float h11 = ttt - tt;
	return (h00*p0 + h10*m0 + h01*p1 + h11*m1);
}

float fit(float val, float oldMin, float oldMax, float newMin, float newMax) {
	float rel = div0(val - oldMin, oldMax - oldMin);
	rel = saturate(rel);
	return lerp(newMin, newMax, rel);
}

float calc_fovy(float focal, float aperture, float aspect) {
	float zoom = ((2.0f * focal) / aperture) * aspect;
	float fovy = 2.0f * ::atan2f(1.0f, zoom);
	return fovy;
}

bool is_prime(int32_t x) {
	if (x <= 1) return false;
	if (is_even(x)) return x == 2;
	int n = int(::sqrt(x));
	for (int i = 3; i < n; i += 2) {
		if ((x % i) == 0) return false;
	}
	return true;
}

int32_t prime(int32_t x) {
	if (is_prime(x)) return x;
	int org = x > 1 ? (x & (~1)) - 1 : 0;
	int end = int32_t(uint32_t(-1) >> 1);
	for (int i = org; i < end; i += 2) {
		if (is_prime(i)) return i;
	}
	return x;
}

} // nxCalc


void xt_half::set(float f) {
	x = nxCore::float_to_half(f);
}

float xt_half::get() const {
	return nxCore::half_to_float(x);
}


void xt_half2::set(float fx, float fy) {
	x = nxCore::float_to_half(fx);
	y = nxCore::float_to_half(fy);
}

xt_float2 xt_half2::get() const {
	xt_float2 fv;
	fv.x = nxCore::half_to_float(x);
	fv.y = nxCore::half_to_float(y);
	return fv;
}


void xt_half3::set(float fx, float fy, float fz) {
	x = nxCore::float_to_half(fx);
	y = nxCore::float_to_half(fy);
	z = nxCore::float_to_half(fz);
}

xt_float3 xt_half3::get() const {
	xt_float3 fv;
	fv.x = nxCore::half_to_float(x);
	fv.y = nxCore::half_to_float(y);
	fv.z = nxCore::half_to_float(z);
	return fv;
}


void xt_half4::set(float fx, float fy, float fz, float fw) {
	x = nxCore::float_to_half(fx);
	y = nxCore::float_to_half(fy);
	z = nxCore::float_to_half(fz);
	w = nxCore::float_to_half(fw);
}

xt_float4 xt_half4::get() const {
	xt_float4 fv;
	fv.x = nxCore::half_to_float(x);
	fv.y = nxCore::half_to_float(y);
	fv.z = nxCore::half_to_float(z);
	fv.w = nxCore::half_to_float(w);
	return fv;
}


static inline float tex_scroll(float val, float add) {
	val += add;
	if (val < 0.0f) val += 1.0f;
	if (val > 1.0f) val -= 1.0f;
	return val;
}

void xt_texcoord::scroll(const xt_texcoord& step) {
	u = tex_scroll(u, step.u);
	v = tex_scroll(v, step.v);
}

void xt_texcoord::encode_half(uint16_t* pDst) const {
	if (pDst) {
		pDst[0] = nxCore::float_to_half(u);
		pDst[1] = nxCore::float_to_half(v);
	}
}


void xt_mtx::identity() {
	::memcpy(this, nxCalc::s_identityMtx, sizeof(*this));
}

void xt_wmtx::identity() {
	::memcpy(this, nxCalc::s_identityMtx, sizeof(*this));
}


namespace nxVec {

float dist2(const cxVec& pos0, const cxVec& pos1) { return (pos1 - pos0).mag2(); }
float dist(const cxVec& pos0, const cxVec& pos1) { return (pos1 - pos0).mag(); }

cxVec get_axis(exAxis axis) {
	int i = (int)axis;
	cxVec v(0.0f);
	float* pV = (float*)&v;
	pV[(i >> 1) & 3] = (i & 1) ? -1.0f : 1.0f;
	return v;
}

cxVec from_polar_uv(float u, float v) {
	float azimuth = u * 2.0f * XD_PI;
	float sinAzi = ::sinf(azimuth);
	float cosAzi = ::cosf(azimuth);
	float elevation = (v - 1.0f) * XD_PI;
	float sinEle = ::sinf(elevation);
	float cosEle = ::cosf(elevation);
	float nx = cosAzi*sinEle;
	float ny = cosEle;
	float nz = sinAzi*sinEle;
	return cxVec(nx, ny, nz);
}

cxVec reflect(const cxVec& vec, const cxVec& nrm) {
	return vec - nrm*vec.dot(nrm)*2.0f;
}

}// nxVec


void cxVec::parse(const char* pStr) {
	float val[3];
	::memset(val, 0, sizeof(val));
#if defined(_MSC_VER)
	::sscanf_s(pStr, "[%f,%f,%f]", &val[0], &val[1], &val[2]);
#else
	::sscanf(pStr, "[%f,%f,%f]", &val[0], &val[1], &val[2]);
#endif
	set(val[0], val[1], val[2]);
}

// Ref: Mike Day, "Vector length and normalization difficulties"
float cxVec::mag() const {
	cxVec v = *this;
	float m = v.max_abs_elem();
	float l = 0.0f;
	if (m) {
		v /= m;
		l = v.mag_fast() * m;
	}
	return l;
}

// see ref above
void cxVec::normalize(const cxVec& v) {
	cxVec n = v;
	float m = v.max_abs_elem();
	if (m > 0.0f) {
		n.scl(1.0f / m);
		n.scl(1.0f / n.mag_fast());
	}
	*this = n;
}

// Refs:
//   Meyer et al. "On floating-point normal vectors"
//   Cigolle et al. "A Survey of Efficient Representations for Independent Unit Vectors"
//   http://jcgt.org/published/0003/02/01/
xt_float2 cxVec::encode_octa() const {
	xt_float2 oct;
	cxVec av = abs_val();
	float d = nxCalc::rcp0(av.x + av.y + av.z);
	float ox = x * d;
	float oy = y * d;
	if (z < 0.0f) {
		float tx = (1.0f - ::fabsf(oy)) * (ox < 0.0f ? -1.0f : 1.0f);
		float ty = (1.0f - ::fabsf(ox)) * (oy < 0.0f ? -1.0f : 1.0f);
		ox = tx;
		oy = ty;
	}
	oct.x = ox;
	oct.y = oy;
	return oct;
}

void cxVec::decode_octa(const xt_float2& oct) {
	float ox = oct.x;
	float oy = oct.y;
	float ax = ::fabsf(ox);
	float ay = ::fabsf(oy);
	z = 1.0f - ax - ay;
	if (z < 0.0f) {
		x = (1.0f - ay) * (ox < 0.0f ? -1.0f : 1.0f);
		y = (1.0f - ax) * (oy < 0.0f ? -1.0f : 1.0f);
	} else {
		x = ox;
		y = oy;
	}
	normalize();
}


void cxMtx::identity_sr() {
	::memcpy(this, nxCalc::s_identityMtx, sizeof(float)*3*4);
}

void cxMtx::from_quat(const cxQuat& q) {
	cxVec ax = q.get_axis_x();
	cxVec ay = q.get_axis_y();
	cxVec az = q.get_axis_z();
	set_rot_frame(ax, ay, az);
}

void cxMtx::from_quat_and_pos(const cxQuat& qrot, const cxVec& vtrans) {
	from_quat(qrot);
	set_translation(vtrans);
}

namespace nxMtx {

cxVec wmtx_calc_vec(const xt_wmtx& m, const cxVec& v) {
	cxVec res;
	float x = v.x;
	float y = v.y;
	float z = v.z;
	res.x = x*m.m[0][0] + y*m.m[0][1] + z*m.m[0][2];
	res.y = x*m.m[1][0] + y*m.m[1][1] + z*m.m[1][2];
	res.z = x*m.m[2][0] + y*m.m[2][1] + z*m.m[2][2];
	return res;
}

cxVec wmtx_calc_pnt(const xt_wmtx& m, const cxVec& v) {
	cxVec res;
	float x = v.x;
	float y = v.y;
	float z = v.z;
	res.x = x*m.m[0][0] + y*m.m[0][1] + z*m.m[0][2] + m.m[0][3];
	res.y = x*m.m[1][0] + y*m.m[1][1] + z*m.m[1][2] + m.m[1][3];
	res.z = x*m.m[2][0] + y*m.m[2][1] + z*m.m[2][2] + m.m[2][3];
	return res;
}

} // nxMtx

cxQuat cxMtx::to_quat() const {
	cxQuat q;
	float s;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 1.0f;
	float trace = m[0][0] + m[1][1] + m[2][2];
	if (trace > 0.0f) {
		s = ::sqrtf(trace + 1.0f);
		w = s * 0.5f;
		s = 0.5f / s;
		x = (m[1][2] - m[2][1]) * s;
		y = (m[2][0] - m[0][2]) * s;
		z = (m[0][1] - m[1][0]) * s;
	} else {
		if (m[1][1] > m[0][0]) {
			if (m[2][2] > m[1][1]) {
				s = m[2][2] - m[1][1] - m[0][0];
				s = ::sqrtf(s + 1.0f);
				z = s * 0.5f;
				if (s != 0.0f) {
					s = 0.5f / s;
				}
				w = (m[0][1] - m[1][0]) * s;
				x = (m[2][0] + m[0][2]) * s;
				y = (m[2][1] + m[1][2]) * s;
			} else {
				s = m[1][1] - m[2][2] - m[0][0];
				s = ::sqrtf(s + 1.0f);
				y = s * 0.5f;
				if (s != 0.0f) {
					s = 0.5f / s;
				}
				w = (m[2][0] - m[0][2]) * s;
				z = (m[1][2] + m[2][1]) * s;
				x = (m[1][0] + m[0][1]) * s;
			}
		} else if (m[2][2] > m[0][0]) {
			s = m[2][2] - m[1][1] - m[0][0];
			s = ::sqrtf(s + 1.0f);
			z = s * 0.5f;
			if (s != 0.0f) {
				s = 0.5f / s;
			}
			w = (m[0][1] - m[1][0]) * s;
			x = (m[2][0] + m[0][2]) * s;
			y = (m[2][1] + m[1][2]) * s;
		} else {
			s = m[0][0] - m[1][1] - m[2][2];
			s = ::sqrtf(s + 1.0f);
			x = s * 0.5f;
			if (s != 0.0f) {
				s = 0.5f / s;
			}
			w = (m[1][2] - m[2][1]) * s;
			y = (m[0][1] + m[1][0]) * s;
			z = (m[0][2] + m[2][0]) * s;
		}
	}
	q.set(x, y, z, w);
	return q;
}

void cxMtx::transpose() {
	float t;
	t = m[0][1]; m[0][1] = m[1][0]; m[1][0] = t;
	t = m[0][2]; m[0][2] = m[2][0]; m[2][0] = t;
	t = m[0][3]; m[0][3] = m[3][0]; m[3][0] = t;
	t = m[1][2]; m[1][2] = m[2][1]; m[2][1] = t;
	t = m[1][3]; m[1][3] = m[3][1]; m[3][1] = t;
	t = m[2][3]; m[2][3] = m[3][2]; m[3][2] = t;
}

void cxMtx::transpose(const cxMtx& mtx) {
	float t;
	m[0][0] = mtx.m[0][0];
	m[1][1] = mtx.m[1][1];
	m[2][2] = mtx.m[2][2];
	m[3][3] = mtx.m[3][3];
	t = mtx.m[0][1]; m[0][1] = mtx.m[1][0]; m[1][0] = t;
	t = mtx.m[0][2]; m[0][2] = mtx.m[2][0]; m[2][0] = t;
	t = mtx.m[0][3]; m[0][3] = mtx.m[3][0]; m[3][0] = t;
	t = mtx.m[1][2]; m[1][2] = mtx.m[2][1]; m[2][1] = t;
	t = mtx.m[1][3]; m[1][3] = mtx.m[3][1]; m[3][1] = t;
	t = mtx.m[2][3]; m[2][3] = mtx.m[3][2]; m[3][2] = t;
}

void cxMtx::transpose_sr() {
	float t;
	t = m[0][1]; m[0][1] = m[1][0]; m[1][0] = t;
	t = m[0][2]; m[0][2] = m[2][0]; m[2][0] = t;
	t = m[1][2]; m[1][2] = m[2][1]; m[2][1] = t;
}

void cxMtx::transpose_sr(const cxMtx& mtx) {
	::memcpy(this, mtx, sizeof(cxMtx));
	transpose_sr();
}

void cxMtx::invert() {
#if XD_USE_LA
	int idx[4 * 3];
	bool ok = nxLA::inv_gj((float*)this, 4, idx);
	if (!ok) {
		::memset(*this, 0, sizeof(cxMtx));
	}
#else
	invert_fast();
#endif
}

void cxMtx::invert(const cxMtx& mtx) {
	::memcpy(this, mtx, sizeof(cxMtx));
	invert();
}

void cxMtx::invert_fast() {
	float det;
	float a0, a1, a2, a3, a4, a5;
	float b0, b1, b2, b3, b4, b5;

	a0 = m[0][0]*m[1][1] - m[0][1]*m[1][0];
	a1 = m[0][0]*m[1][2] - m[0][2]*m[1][0];
	a2 = m[0][0]*m[1][3] - m[0][3]*m[1][0];
	a3 = m[0][1]*m[1][2] - m[0][2]*m[1][1];
	a4 = m[0][1]*m[1][3] - m[0][3]*m[1][1];
	a5 = m[0][2]*m[1][3] - m[0][3]*m[1][2];

	b0 = m[2][0]*m[3][1] - m[2][1]*m[3][0];
	b1 = m[2][0]*m[3][2] - m[2][2]*m[3][0];
	b2 = m[2][0]*m[3][3] - m[2][3]*m[3][0];
	b3 = m[2][1]*m[3][2] - m[2][2]*m[3][1];
	b4 = m[2][1]*m[3][3] - m[2][3]*m[3][1];
	b5 = m[2][2]*m[3][3] - m[2][3]*m[3][2];

	det = a0*b5 - a1*b4 + a2*b3 + a3*b2 - a4*b1 + a5*b0;

	if (det == 0.0f) {
		::memset(*this, 0, sizeof(cxMtx));
	} else {
		cxMtx im;

		im.m[0][0] =  m[1][1]*b5 - m[1][2]*b4 + m[1][3]*b3;
		im.m[1][0] = -m[1][0]*b5 + m[1][2]*b2 - m[1][3]*b1;
		im.m[2][0] =  m[1][0]*b4 - m[1][1]*b2 + m[1][3]*b0;
		im.m[3][0] = -m[1][0]*b3 + m[1][1]*b1 - m[1][2]*b0;

		im.m[0][1] = -m[0][1]*b5 + m[0][2]*b4 - m[0][3]*b3;
		im.m[1][1] =  m[0][0]*b5 - m[0][2]*b2 + m[0][3]*b1;
		im.m[2][1] = -m[0][0]*b4 + m[0][1]*b2 - m[0][3]*b0;
		im.m[3][1] =  m[0][0]*b3 - m[0][1]*b1 + m[0][2]*b0;

		im.m[0][2] =  m[3][1]*a5 - m[3][2]*a4 + m[3][3]*a3;
		im.m[1][2] = -m[3][0]*a5 + m[3][2]*a2 - m[3][3]*a1;
		im.m[2][2] =  m[3][0]*a4 - m[3][1]*a2 + m[3][3]*a0;
		im.m[3][2] = -m[3][0]*a3 + m[3][1]*a1 - m[3][2]*a0;

		im.m[0][3] = -m[2][1]*a5 + m[2][2]*a4 - m[2][3]*a3;
		im.m[1][3] =  m[2][0]*a5 - m[2][2]*a2 + m[2][3]*a1;
		im.m[2][3] = -m[2][0]*a4 + m[2][1]*a2 - m[2][3]*a0;
		im.m[3][3] =  m[2][0]*a3 - m[2][1]*a1 + m[2][2]*a0;

		float idet = 1.0f / det;
		float* pDst = &m[0][0];
		float* pSrc = &im.m[0][0];
		for (int i = 0; i < 4 * 4; ++i) {
			pDst[i] = pSrc[i] * idet;
		}
	}
}

void cxMtx::invert_lu() {
	float lu[4 * 4];
	nxLA::mtx_cpy(lu, (float*)this, 4, 4);
	float wk[4];
	int idx[4];
	bool ok = nxLA::lu_decomp(lu, 4, wk, idx);
	if (!ok) {
		::memset(*this, 0, sizeof(cxMtx));
		return;
	}
	for (int i = 0; i < 4; ++i) {
		nxLA::lu_solve(m[i], lu, nxCalc::s_identityMtx[i], idx, 4);
	}
	transpose();
}

void cxMtx::invert_lu_hi() {
	double lu[4 * 4];
	nxLA::mtx_cpy(lu, (float*)this, 4, 4);
	double wk[4];
	int idx[4];
	bool ok = nxLA::lu_decomp(lu, 4, wk, idx);
	if (!ok) {
		::memset(*this, 0, sizeof(cxMtx));
		return;
	}
	double inv[4][4];
	double b[4][4];
	nxLA::mtx_cpy(&b[0][0], &nxCalc::s_identityMtx[0][0], 4, 4);
	for (int i = 0; i < 4; ++i) {
		nxLA::lu_solve(inv[i], lu, b[i], idx, 4);
	}
	nxLA::mtx_cpy((float*)this, &inv[0][0], 4, 4);
	transpose();
}

void cxMtx::mul(const cxMtx& mtx) {
	mul(*this, mtx);
}

void cxMtx::mul(const cxMtx& m1, const cxMtx& m2) {
#if XD_USE_LA
	float res[4 * 4];
	nxLA::mul_mm(res, (const float*)m1, (const float*)m2, 4, 4, 4);
	from_mem(res);
#else
	cxMtx m0;
	m0.m[0][0] = m1.m[0][0]*m2.m[0][0] + m1.m[0][1]*m2.m[1][0] + m1.m[0][2]*m2.m[2][0] + m1.m[0][3]*m2.m[3][0];
	m0.m[0][1] = m1.m[0][0]*m2.m[0][1] + m1.m[0][1]*m2.m[1][1] + m1.m[0][2]*m2.m[2][1] + m1.m[0][3]*m2.m[3][1];
	m0.m[0][2] = m1.m[0][0]*m2.m[0][2] + m1.m[0][1]*m2.m[1][2] + m1.m[0][2]*m2.m[2][2] + m1.m[0][3]*m2.m[3][2];
	m0.m[0][3] = m1.m[0][0]*m2.m[0][3] + m1.m[0][1]*m2.m[1][3] + m1.m[0][2]*m2.m[2][3] + m1.m[0][3]*m2.m[3][3];

	m0.m[1][0] = m1.m[1][0]*m2.m[0][0] + m1.m[1][1]*m2.m[1][0] + m1.m[1][2]*m2.m[2][0] + m1.m[1][3]*m2.m[3][0];
	m0.m[1][1] = m1.m[1][0]*m2.m[0][1] + m1.m[1][1]*m2.m[1][1] + m1.m[1][2]*m2.m[2][1] + m1.m[1][3]*m2.m[3][1];
	m0.m[1][2] = m1.m[1][0]*m2.m[0][2] + m1.m[1][1]*m2.m[1][2] + m1.m[1][2]*m2.m[2][2] + m1.m[1][3]*m2.m[3][2];
	m0.m[1][3] = m1.m[1][0]*m2.m[0][3] + m1.m[1][1]*m2.m[1][3] + m1.m[1][2]*m2.m[2][3] + m1.m[1][3]*m2.m[3][3];

	m0.m[2][0] = m1.m[2][0]*m2.m[0][0] + m1.m[2][1]*m2.m[1][0] + m1.m[2][2]*m2.m[2][0] + m1.m[2][3]*m2.m[3][0];
	m0.m[2][1] = m1.m[2][0]*m2.m[0][1] + m1.m[2][1]*m2.m[1][1] + m1.m[2][2]*m2.m[2][1] + m1.m[2][3]*m2.m[3][1];
	m0.m[2][2] = m1.m[2][0]*m2.m[0][2] + m1.m[2][1]*m2.m[1][2] + m1.m[2][2]*m2.m[2][2] + m1.m[2][3]*m2.m[3][2];
	m0.m[2][3] = m1.m[2][0]*m2.m[0][3] + m1.m[2][1]*m2.m[1][3] + m1.m[2][2]*m2.m[2][3] + m1.m[2][3]*m2.m[3][3];

	m0.m[3][0] = m1.m[3][0]*m2.m[0][0] + m1.m[3][1]*m2.m[1][0] + m1.m[3][2]*m2.m[2][0] + m1.m[3][3]*m2.m[3][0];
	m0.m[3][1] = m1.m[3][0]*m2.m[0][1] + m1.m[3][1]*m2.m[1][1] + m1.m[3][2]*m2.m[2][1] + m1.m[3][3]*m2.m[3][1];
	m0.m[3][2] = m1.m[3][0]*m2.m[0][2] + m1.m[3][1]*m2.m[1][2] + m1.m[3][2]*m2.m[2][2] + m1.m[3][3]*m2.m[3][2];
	m0.m[3][3] = m1.m[3][0]*m2.m[0][3] + m1.m[3][1]*m2.m[1][3] + m1.m[3][2]*m2.m[2][3] + m1.m[3][3]*m2.m[3][3];

	*this = m0;
#endif
}

// Ref: http://graphics.pixar.com/library/OrthonormalB/paper.pdf
void cxMtx::from_upvec(const cxVec& n) {
	float s = n.z < 0 ? -1.0f : 1.0f;
	float a = -1.0f / (s - n.z);
	float b = n.x * n.y * a;
	cxVec nx(1.0f + s*n.x*n.x*a, s*b, s*n.x);
	cxVec nz(b, s + n.y*n.y*a, n.y);
	set_rot_frame(nx, n, nz);
}

void cxMtx::orient_zy(const cxVec& axisZ, const cxVec& axisY, bool normalizeInput) {
	cxVec az = normalizeInput ? axisZ.get_normalized() : axisZ;
	cxVec ay = normalizeInput ? axisY.get_normalized() : axisY;
	cxVec ax = nxVec::cross(ay, az).get_normalized();
	ay = nxVec::cross(az, ax).get_normalized();
	set_rot_frame(ax, ay, az);
}

void cxMtx::orient_zx(const cxVec& axisZ, const cxVec& axisX, bool normalizeInput) {
	cxVec az = normalizeInput ? axisZ.get_normalized() : axisZ;
	cxVec ax = normalizeInput ? axisX.get_normalized() : axisX;
	cxVec ay = nxVec::cross(az, ax).get_normalized();
	ax = nxVec::cross(ay, az).get_normalized();
	set_rot_frame(ax, ay, az);
}

void cxMtx::set_rot_frame(const cxVec& axisX, const cxVec& axisY, const cxVec& axisZ) {
	m[0][0] = axisX.x;
	m[0][1] = axisX.y;
	m[0][2] = axisX.z;
	m[0][3] = 0.0f;

	m[1][0] = axisY.x;
	m[1][1] = axisY.y;
	m[1][2] = axisY.z;
	m[1][3] = 0.0f;

	m[2][0] = axisZ.x;
	m[2][1] = axisZ.y;
	m[2][2] = axisZ.z;
	m[2][3] = 0.0f;

	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = 0.0f;
	m[3][3] = 1.0f;
}

void cxMtx::set_rot_n(const cxVec& axis, float ang) {
	cxVec axis2 = axis * axis;
	float s = ::sinf(ang);
	float c = ::cosf(ang);
	float t = 1.0f - c;
	float x = axis.x;
	float y = axis.y;
	float z = axis.z;
	float xy = x*y;
	float xz = x*z;
	float yz = y*z;
	m[0][0] = t*axis2.x + c;
	m[0][1] = t*xy + s*z;
	m[0][2] = t*xz - s*y;
	m[0][3] = 0.0f;

	m[1][0] = t*xy - s*z;
	m[1][1] = t*axis2.y + c;
	m[1][2] = t*yz + s*x;
	m[1][3] = 0.0f;

	m[2][0] = t*xz + s*y;
	m[2][1] = t*yz - s*x;
	m[2][2] = t*axis2.z + c;
	m[2][3] = 0.0f;

	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = 0.0f;
	m[3][3] = 1.0f;
}

void cxMtx::set_rot(const cxVec& axis, float ang) {
	set_rot_n(axis.get_normalized(), ang);
}

void cxMtx::set_rot_x(float rx) {
	set_rot_n(nxVec::get_axis(exAxis::PLUS_X), rx);
}

void cxMtx::set_rot_y(float ry) {
	set_rot_n(nxVec::get_axis(exAxis::PLUS_Y), ry);
}

void cxMtx::set_rot_z(float rz) {
	set_rot_n(nxVec::get_axis(exAxis::PLUS_Z), rz);
}

void cxMtx::set_rot(float rx, float ry, float rz, exRotOrd ord) {
	static struct { uint8_t m0, m1, m2; } tbl[] = {
		/* XYZ */ {0, 1, 2},
		/* XZY */ {0, 2, 1},
		/* YXZ */ {1, 0, 2},
		/* YZX */ {1, 2, 0},
		/* ZXY */ {2, 0, 1},
		/* ZYX */ {2, 1, 0}
	};
	uint32_t idx = (uint32_t)ord;
	if (idx >= XD_ARY_LEN(tbl)) {
		identity();
	} else {
		cxMtx rm[3];
		rm[0].set_rot_x(rx);
		rm[1].set_rot_y(ry);
		rm[2].set_rot_z(rz);
		int m0 = tbl[idx].m0;
		int m1 = tbl[idx].m1;
		int m2 = tbl[idx].m2;
		*this = rm[m0];
		mul(rm[m1]);
		mul(rm[m2]);
	}
}

void cxMtx::set_rot_degrees(const cxVec& r, exRotOrd ord) {
	cxVec rr = r * XD_DEG2RAD(1.0f);
	set_rot(rr.x, rr.y, rr.z, ord);
}

bool cxMtx::is_valid_rot(float tol) const {
	cxVec r0 = get_row_vec(0);
	cxVec r1 = get_row_vec(1);
	cxVec r2 = get_row_vec(2);
	float v = ::fabsf(nxVec::scalar_triple(r0, r1, r2));
	if (::fabsf(v - 1.0f) > tol) return false;

	v = r0.dot(r0);
	if (::fabsf(v - 1.0f) > tol) return false;
	v = r1.dot(r1);
	if (::fabsf(v - 1.0f) > tol) return false;
	v = r2.dot(r2);
	if (::fabsf(v - 1.0f) > tol) return false;

	v = r0.dot(r1);
	if (::fabsf(v) > tol) return false;
	v = r1.dot(r2);
	if (::fabsf(v) > tol) return false;
	v = r0.dot(r2);
	if (::fabsf(v) > tol) return false;

	return true;
}

static inline float limit_pi(float rad) {
	rad = ::fmodf(rad, XD_PI*2);
	if (::fabsf(rad) > XD_PI) {
		if (rad < 0.0f) {
			rad = XD_PI*2 + rad;
		} else {
			rad = rad - XD_PI*2;
		}
	}
	return rad;
}

static struct { uint8_t i0, i1, i2, s; } s_getRotTbl[] = {
	/* XYZ */ { 0, 1, 2, 1 },
	/* XZY */ { 0, 2, 1, 0 },
	/* YXZ */ { 1, 0, 2, 0 },
	/* YZX */ { 1, 2, 0, 1 },
	/* ZXY */ { 2, 0, 1, 1 },
	/* ZYX */ { 2, 1, 0, 0 }
};

cxVec cxMtx::get_rot(exRotOrd ord) const {
	cxVec rv(0.0f);
	cxQuat q = to_quat();
	const float eps = 1.0e-6f;
	int axisMask = 0;
	for (int i = 0; i < 4; ++i) {
		if (::fabsf(q[i]) < eps) {
			axisMask |= 1 << i;
		}
	}
	bool singleAxis = false;
	float qw = nxCalc::clamp(q.w, -1.0f, 1.0f);
	switch (axisMask) {
		case 6: /* 0110 -> X */
			rv.x = ::acosf(qw) * 2.0f;
			if (q.x < 0.0f) rv.x = -rv.x;
			rv.x = limit_pi(rv.x);
			singleAxis = true;
			break;
		case 5: /* 0101 -> Y */
			rv.y = ::acosf(qw) * 2.0f;
			if (q.y < 0.0f) rv.y = -rv.y;
			rv.y = limit_pi(rv.y);
			singleAxis = true;
			break;
		case 3: /* 0011 -> Z */
			rv.z = ::acosf(qw) * 2.0f;
			if (q.z < 0.0f) rv.z = -rv.z;
			rv.z = limit_pi(rv.z);
			singleAxis = true;
			break;
		case 7: /* 0111 -> identity */
			singleAxis = true;
			break;
	}
	if (singleAxis) {
		return rv;
	}
	if ((uint32_t)ord >= XD_ARY_LEN(s_getRotTbl)) {
		ord = exRotOrd::XYZ;
	}
	int i0 = s_getRotTbl[(int)ord].i0;
	int i1 = s_getRotTbl[(int)ord].i1;
	int i2 = s_getRotTbl[(int)ord].i2;
	float sgn = s_getRotTbl[(int)ord].s ? 1.0f : -1.0f;
	cxVec rm[3];
	rm[0].set(m[i0][i0], m[i0][i1], m[i0][i2]);
	rm[1].set(m[i1][i0], m[i1][i1], m[i1][i2]);
	rm[2].set(m[i2][i0], m[i2][i1], m[i2][i2]);
	float r[3] = { 0, 0, 0 };
	r[i0] = ::atan2f(rm[1][2], rm[2][2]);
	r[i1] = ::atan2f(-rm[0][2], ::sqrtf(nxCalc::sq(rm[0][0]) + nxCalc::sq(rm[0][1])));
	// Ref: Mike Day, "Extracting Euler Angles from a Rotation Matrix"
	float s = ::sinf(r[i0]);
	float c = ::cosf(r[i0]);
	r[i2] = ::atan2f(s*rm[2][0] - c*rm[1][0], c*rm[1][1] - s*rm[2][1]);
	for (int i = 0; i < 3; ++i) {
		r[i] *= sgn;
	}
	for (int i = 0; i < 3; ++i) {
		r[i] = limit_pi(r[i]);
	}
	rv.from_mem(r);
	return rv;
}

void cxMtx::mk_scl(float sx, float sy, float sz) {
	identity();
	m[0][0] = sx;
	m[1][1] = sy;
	m[2][2] = sz;
}

cxVec cxMtx::get_scl(cxMtx* pMtxRT, exTransformOrd xord) const {
	cxVec scl(0.0f);
	cxVec iscl;
	switch (xord) {
		case exTransformOrd::SRT:
		case exTransformOrd::STR:
		case exTransformOrd::TSR:
			scl.x = get_row_vec(0).mag();
			scl.y = get_row_vec(1).mag();
			scl.z = get_row_vec(2).mag();
			break;
		case exTransformOrd::RST:
		case exTransformOrd::RTS:
		case exTransformOrd::TRS:
			scl.x = cxVec(m[0][0], m[1][0], m[2][0]).mag();
			scl.y = cxVec(m[0][1], m[1][1], m[2][1]).mag();
			scl.z = cxVec(m[0][2], m[1][2], m[2][2]).mag();
			break;
	}
	if (pMtxRT) {
		iscl = scl.inv_val();
		switch (xord) {
			case exTransformOrd::SRT:
			case exTransformOrd::STR:
				pMtxRT->mk_scl(iscl);
				pMtxRT->mul(*this);
				break;
			case exTransformOrd::RST:
				*pMtxRT = get_sr();
				pMtxRT->mul(nxMtx::mk_scl(iscl));
				pMtxRT->set_translation(get_translation());
				break;
			case exTransformOrd::RTS:
			case exTransformOrd::TRS:
				*pMtxRT = *this;
				pMtxRT->mul(nxMtx::mk_scl(iscl));
				break;
			case exTransformOrd::TSR:
				pMtxRT->mk_scl(iscl);
				pMtxRT->mul(get_sr());
				pMtxRT->set_translation((get_inverted() * *pMtxRT).get_translation().neg_val());
				break;
		}
	}
	return scl;
}

void cxMtx::calc_xform(const cxMtx& mtxT, const cxMtx& mtxR, const cxMtx& mtxS, exTransformOrd ord) {
	const uint8_t S = 0;
	const uint8_t R = 1;
	const uint8_t T = 2;
	static struct { uint8_t m0, m1, m2; } tbl[] = {
		/* SRT */ { S, R, T },
		/* STR */ { S, T, R },
		/* RST */ { R, S, T },
		/* RTS */ { R, T, S },
		/* TSR */ { T, S, R },
		/* TRS */ { T, R, S }
	};
	const cxMtx* lst[3];
	lst[S] = &mtxS;
	lst[R] = &mtxR;
	lst[T] = &mtxT;

	if ((uint32_t)ord >= XD_ARY_LEN(tbl)) {
		ord = exTransformOrd::SRT;
	}
	int m0 = tbl[(int)ord].m0;
	int m1 = tbl[(int)ord].m1;
	int m2 = tbl[(int)ord].m2;
	*this = *lst[m0];
	mul(*lst[m1]);
	mul(*lst[m2]);
}

void cxMtx::mk_view(const cxVec& pos, const cxVec& tgt, const cxVec& upvec) {
	cxVec up = upvec;
	cxVec dir = (tgt - pos).get_normalized();
	cxVec side = nxVec::cross(up, dir).get_normalized();
	up = nxVec::cross(side, dir);
	set_rot_frame(side.neg_val(), up.neg_val(), dir.neg_val());
	transpose_sr();
	cxVec org = calc_vec(pos.neg_val());
	set_translation(org);
}

void cxMtx::mk_proj(float fovy, float aspect, float znear, float zfar) {
	float h = fovy*0.5f;
	float s = ::sinf(h);
	float c = ::cosf(h);
	float cot = c / s;
	float q = zfar / (zfar - znear);
	::memset(this, 0, sizeof(cxMtx));
	m[2][3] = -1.0f;
	m[0][0] = cot / aspect;
	m[1][1] = cot;
	m[2][2] = -q;
	m[3][2] = -q * znear;
}

cxVec cxMtx::calc_vec(const cxVec& v) const {
#if XD_USE_LA
	float res[4];
	nxLA::mul_vm(res, (float*)&v, (float*)m, 3, 4);
	return nxVec::from_mem(res);
#else
	float x = v.x;
	float y = v.y;
	float z = v.z;
	float tx = x*m[0][0] + y*m[1][0] + z*m[2][0];
	float ty = x*m[0][1] + y*m[1][1] + z*m[2][1];
	float tz = x*m[0][2] + y*m[1][2] + z*m[2][2];
	return cxVec(tx, ty, tz);
#endif
}

cxVec cxMtx::calc_pnt(const cxVec& v) const {
#if XD_USE_LA
	float vec[4] = { v.x, v.y, v.z, 1.0f };
	float res[4];
	nxLA::mul_vm(res, vec, (float*)m, 4, 4);
	return nxVec::from_mem(res);
#else
	float x = v.x;
	float y = v.y;
	float z = v.z;
	float tx = x*m[0][0] + y*m[1][0] + z*m[2][0] + m[3][0];
	float ty = x*m[0][1] + y*m[1][1] + z*m[2][1] + m[3][1];
	float tz = x*m[0][2] + y*m[1][2] + z*m[2][2] + m[3][2];
	return cxVec(tx, ty, tz);
#endif
}

xt_float4 cxMtx::apply(const xt_float4& qv) const {
	float x = qv.x;
	float y = qv.y;
	float z = qv.z;
	float w = qv.w;
	xt_float4 res;
	res.x = x*m[0][0] + y*m[1][0] + z*m[2][0] + w*m[3][0];
	res.y = x*m[0][1] + y*m[1][1] + z*m[2][1] + w*m[3][1];
	res.z = x*m[0][2] + y*m[1][2] + z*m[2][2] + w*m[3][2];
	res.w = x*m[0][3] + y*m[1][3] + z*m[2][3] + w*m[3][3];
	return res;
}


void cxQuat::from_mtx(const cxMtx& mtx) {
	*this = mtx.to_quat();
}

cxMtx cxQuat::to_mtx() const {
	cxMtx m;
	m.from_quat(*this);
	return m;
}

float cxQuat::get_axis_ang(cxVec* pAxis) const {
	float ang = ::acosf(w) * 2.0f;
	if (pAxis) {
		float s = nxCalc::rcp0(::sqrtf(1.0f - w*w));
		pAxis->set(x, y, z);
		pAxis->scl(s);
		pAxis->normalize();
	}
	return ang;
}

cxVec cxQuat::get_log_vec() const {
	float cosh = w;
	float hang = ::acosf(cosh);
	cxVec axis(x, y, z);
	axis.normalize();
	axis.scl(hang);
	return axis;
}

void cxQuat::from_log_vec(const cxVec& lvec, bool nrmFlg) {
	float hang = lvec.mag();
	float cosh = ::cosf(hang);
	cxVec axis = lvec * nxCalc::sinc(hang);
	x = axis.x;
	y = axis.y;
	z = axis.z;
	w = cosh;
	if (nrmFlg) {
		normalize();
	}
}

void cxQuat::from_vecs(const cxVec& vfrom, const cxVec& vto) {
	cxVec axis = nxVec::cross(vfrom, vto);
	float sqm = axis.mag2();
	if (sqm == 0.0f) {
		identity();
	} else {
		float d = vfrom.dot(vto);
		if (d == 1.0f) {
			identity();
		} else {
			d = nxCalc::clamp(d, -1.0f, 1.0f);
			float c = ::sqrtf((1.0f + d) * 0.5f);
			float s = ::sqrtf((1.0f - d) * 0.5f);
			axis.scl(nxCalc::rcp0(::sqrtf(sqm)) * s);
			x = axis.x;
			y = axis.y;
			z = axis.z;
			w = c;
			normalize();
		}
	}
}

void cxQuat::set_rot(const cxVec& axis, float ang) {
	float hang = ang * 0.5f;
	float s = ::sinf(hang);
	float c = ::cosf(hang);
	cxVec v = axis.get_normalized() * s;
	set(v.x, v.y, v.z, c);
}

void cxQuat::set_rot_x(float rx) {
	float h = rx * 0.5f;
	float s = ::sinf(h);
	float c = ::cosf(h);
	set(s, 0.0f, 0.0f, c);
}

void cxQuat::set_rot_y(float ry) {
	float h = ry * 0.5f;
	float s = ::sinf(h);
	float c = ::cosf(h);
	set(0.0f, s, 0.0f, c);
}

void cxQuat::set_rot_z(float rz) {
	float h = rz * 0.5f;
	float s = ::sinf(h);
	float c = ::cosf(h);
	set(0.0f, 0.0f, s, c);
}

void cxQuat::set_rot(float rx, float ry, float rz, exRotOrd ord) {
	static struct { uint8_t q2, q1, q0; } tbl[] = {
		/* XYZ */ {0, 1, 2},
		/* XZY */ {0, 2, 1},
		/* YXZ */ {1, 0, 2},
		/* YZX */ {1, 2, 0},
		/* ZXY */ {2, 0, 1},
		/* ZYX */ {2, 1, 0}
	};
	uint32_t idx = (uint32_t)ord;
	if (idx >= XD_ARY_LEN(tbl)) {
		identity();
	} else {
		cxQuat rq[3];
		rq[0].set_rot_x(rx);
		rq[1].set_rot_y(ry);
		rq[2].set_rot_z(rz);
		int q0 = tbl[idx].q0;
		int q1 = tbl[idx].q1;
		int q2 = tbl[idx].q2;
		*this = rq[q0];
		mul(rq[q1]);
		mul(rq[q2]);
	}
}

void cxQuat::set_rot_degrees(float dx, float dy, float dz, exRotOrd ord) {
	set_rot_degrees(cxVec(dx, dy, dz), ord);
}

void cxQuat::set_rot_degrees(const cxVec& r, exRotOrd ord) {
	cxVec rr = r * XD_DEG2RAD(1.0f);
	set_rot(rr.x, rr.y, rr.z, ord);
}

cxVec cxQuat::get_rot(exRotOrd ord) const {
	cxVec rv(0.0f);
	int axisMask = 0;
	const float eps = 1.0e-6f;
	if (::fabsf(x) < eps) axisMask |= 1;
	if (::fabsf(y) < eps) axisMask |= 2;
	if (::fabsf(z) < eps) axisMask |= 4;
	if (::fabsf(w) < eps) axisMask |= 8;
	bool singleAxis = false;
	float qw = nxCalc::clamp(w, -1.0f, 1.0f);
	switch (axisMask) {
		case 6: /* 0110 -> X */
			rv.x = ::acosf(qw) * 2.0f;
			if (x < 0.0f) rv.x = -rv.x;
			rv.x = limit_pi(rv.x);
			singleAxis = true;
			break;
		case 5: /* 0101 -> Y */
			rv.y = ::acosf(qw) * 2.0f;
			if (y < 0.0f) rv.y = -rv.y;
			rv.y = limit_pi(rv.y);
			singleAxis = true;
			break;
		case 3: /* 0011 -> Z */
			rv.z = ::acosf(qw) * 2.0f;
			if (z < 0.0f) rv.z = -rv.z;
			rv.z = limit_pi(rv.z);
			singleAxis = true;
			break;
		case 7: /* 0111 -> identity */
			singleAxis = true;
			break;
	}
	if (singleAxis) {
		return rv;
	}
	if ((uint32_t)ord >= XD_ARY_LEN(s_getRotTbl)) {
		ord = exRotOrd::XYZ;
	}
	int i0 = s_getRotTbl[(int)ord].i0;
	int i1 = s_getRotTbl[(int)ord].i1;
	int i2 = s_getRotTbl[(int)ord].i2;
	float sgn = s_getRotTbl[(int)ord].s ? 1.0f : -1.0f;
	cxVec m[3];
	m[0] = get_axis_x();
	m[1] = get_axis_y();
	m[2] = get_axis_z();
	cxVec rm[3];
	rm[0].set(m[i0][i0], m[i0][i1], m[i0][i2]);
	rm[1].set(m[i1][i0], m[i1][i1], m[i1][i2]);
	rm[2].set(m[i2][i0], m[i2][i1], m[i2][i2]);
	float r[3] = { 0, 0, 0 };
	r[i0] = ::atan2f(rm[1][2], rm[2][2]);
	r[i1] = ::atan2f(-rm[0][2], ::sqrtf(nxCalc::sq(rm[0][0]) + nxCalc::sq(rm[0][1])));
	float s = ::sinf(r[i0]);
	float c = ::cosf(r[i0]);
	r[i2] = ::atan2f(s*rm[2][0] - c*rm[1][0], c*rm[1][1] - s*rm[2][1]);
	for (int i = 0; i < 3; ++i) {
		r[i] *= sgn;
	}
	for (int i = 0; i < 3; ++i) {
		r[i] = limit_pi(r[i]);
	}
	rv.from_mem(r);
	return rv;
}

void cxQuat::slerp(const cxQuat& q1, const cxQuat& q2, float t) {
	xt_float4 v1 = q1;
	xt_float4 v2 = q2;
	if (q1.dot(q2) < 0.0f) {
		v2.scl(-1.0f);
	}
	float u = 0.0f;
	float v = 0.0f;
	for (int i = 0; i < 4; ++i) {
		u += nxCalc::sq(v1[i] - v2[i]);
		v += nxCalc::sq(v1[i] + v2[i]);
	}
	float ang = 2.0f * ::atan2f(::sqrtf(u), ::sqrtf(v));
	float s = 1.0f - t;
	float d = 1.0f / nxCalc::sinc(ang);
	s = nxCalc::sinc(ang*s) * d * s;
	t = nxCalc::sinc(ang*t) * d * t;
	for (int i = 0; i < 4; ++i) {
		(*this)[i] = v1[i]*s + v2[i]*t;
	}
	normalize();
}

cxVec cxQuat::apply(const cxVec& v) const {
	cxVec qv = cxVec(x, y, z);
	float d = qv.dot(v);
	float ww = w*w;
	return ((qv*d + v*ww) - nxVec::cross(v, qv)*w)*2.0f - v;
}

// http://www.geometrictools.com/Documentation/ConstrainedQuaternions.pdf

static inline cxQuat closest_quat_by_axis(const cxQuat& qsrc, int axis) {
	cxQuat qres;
	qres.identity();
	float e = qsrc.get_at(axis);
	float w = qsrc.w;
	float sqmag = nxCalc::sq(e) + nxCalc::sq(w);
	if (sqmag > 0.0f) {
		float s = nxCalc::rcp0(::sqrtf(sqmag));
		qres.set_at(axis, e*s);
		qres.w = w*s;
	}
	return qres;
}

cxQuat cxQuat::get_closest_x() const { return closest_quat_by_axis(*this, 0); }

cxQuat cxQuat::get_closest_y() const { return closest_quat_by_axis(*this, 1); }

cxQuat cxQuat::get_closest_z() const { return closest_quat_by_axis(*this, 2); }

cxQuat cxQuat::get_closest_xy() const {
	cxQuat q;
	float det = ::fabsf(-x*y - z*w);
	float s;
	if (det < 0.5f) {
		float d = ::sqrtf(::fabsf(1.0f - 4.0f*nxCalc::sq(det)));
		float a = x*w - y*z;
		float b = nxCalc::sq(w) - nxCalc::sq(x) + nxCalc::sq(y) - nxCalc::sq(z);
		float s0, c0;
		if (b >= 0.0f) {
			s0 = a;
			c0 = (d + b)*0.5f;
		} else {
			s0 = (d - b)*0.5f;
			c0 = a;
		}
		s = nxCalc::rcp0(nxCalc::hypot(s0, c0));
		s0 *= s;
		c0 *= s;

		float s1 = y*c0 - z*s0;
		float c1 = w*c0 + x*s0;
		s = nxCalc::rcp0(nxCalc::hypot(s1, c1));
		s1 *= s;
		c1 *= s;

		q.set(s0*c1, c0*s1, -s0*s1, c0*c1);
	} else {
		s = nxCalc::rcp0(::sqrtf(det));
		q.set(x*s, 0.0f, 0.0f, w*s);
	}
	return q;
}

cxQuat cxQuat::get_closest_yx() const {
	cxQuat q = cxQuat(x, y, -z, w).get_closest_xy();
	q.z = -q.z;
	return q;
}

cxQuat cxQuat::get_closest_xz() const {
	cxQuat q = cxQuat(x, z, y, w).get_closest_yx();
	float t = q.y;
	q.y = q.z;
	q.z = t;
	return q;
}

cxQuat cxQuat::get_closest_zx() const {
	cxQuat q = cxQuat(x, z, y, w).get_closest_xy();
	float t = q.y;
	q.y = q.z;
	q.z = t;
	return q;
}

cxQuat cxQuat::get_closest_yz() const {
	cxQuat q = cxQuat(y, z, x, w).get_closest_xy();
	return cxQuat(q.z, q.x, q.y, q.w);
}

cxQuat cxQuat::get_closest_zy() const {
	cxQuat q = cxQuat(y, z, x, w).get_closest_yx();
	return cxQuat(q.z, q.x, q.y, q.w);
}


namespace nxQuat {

cxQuat log(const cxQuat& q) {
	float m = q.mag();
	float w = ::logf(m);
	cxVec v = q.get_imag_part();
	float s = v.mag();
	s = s < 1.0e-5f ? 1.0f : ::atan2f(s, q.get_real_part()) / s;
	v.scl(s);
	return cxQuat(v.x, v.y, v.z, w);
}

cxQuat exp(const cxQuat& q) {
	cxVec v = q.get_imag_part();
	float e = ::expf(q.get_real_part());
	float h = v.mag();
	float c = ::cosf(h);
	float s = ::sinf(h);
	v.normalize();
	v.scl(s);
	return cxQuat(v.x, v.y, v.z, c).get_scaled(e);
}

cxQuat pow(const cxQuat& q, float x) {
	if (q.mag() < 1.0e-5f) return nxQuat::zero();
	return exp(log(q).get_scaled(x));
}

float arc_dist(const cxQuat& a, const cxQuat& b) {
	double ax = a.x;
	double ay = a.y;
	double az = a.z;
	double aw = a.w;
	double bx = b.x;
	double by = b.y;
	double bz = b.z;
	double bw = b.w;
	double d = ::fabs(ax*bx + ay*by + az*bz + aw*bw);
	if (d > 1.0) d = 1.0;
	d = ::acos(d) / (XD_PI / 2.0);
	return float(d);
}

} // nxQuat


void cxDualQuat::mul(const cxDualQuat& dq) {
	cxQuat r0 = mReal;
	cxQuat d0 = mDual;
	cxQuat r1 = dq.mReal;
	cxQuat d1 = dq.mDual;
	mReal = (r1 * r0).get_normalized();
	mDual = d1*r0 + r1*d0;
}

cxVec cxDualQuat::calc_pnt(const cxVec& pos) const {
	cxQuat t(pos.x, pos.y, pos.z, 0.0f);
	t.scl(0.5f);
	cxQuat d = mDual;
	d.add(mReal * t);
	d.scl(2.0f);
	d.mul(mReal.get_conjugate());
	return d.get_vector_part();
}

void cxDualQuat::interpolate(const cxDualQuat& dqA, const cxDualQuat& dqB, float t) {
	cxDualQuat b = dqB;
	float ab = dqA.mReal.dot(dqB.mReal);
	if (ab < 0.0f) {
		b.mReal.negate();
		b.mDual.negate();
		ab = -ab;
	}

	if (1.0f - ab < 1.0e-6f) {
		mReal.lerp(dqA.mReal, dqB.mReal, t);
		mDual.lerp(dqA.mDual, dqB.mDual, t);
		normalize();
		return;
	}

	cxDualQuat d;
	d.mReal = dqA.mReal.get_conjugate();
	d.mDual = dqA.mDual.get_conjugate();
	d.mul(b);

	cxVec vr = d.mReal.get_vector_part();
	cxVec vd = d.mDual.get_vector_part();
	float ir = nxCalc::rcp0(vr.mag());
	float ang = 2.0f * ::acosf(d.mReal.w);
	float tns = -2.0f * d.mDual.w * ir;
	cxVec dir = vr * ir;
	cxVec mom = (vd - dir*tns*d.mReal.w*0.5f) * ir;

	ang *= t;
	tns *= t;
	float ha = ang * 0.5f;
	float sh = ::sinf(ha);
	float ch = ::cosf(ha);

	cxDualQuat r;
	r.mReal.from_parts(dir * sh, ch);
	r.mDual.from_parts(mom*sh + dir*ch*tns*0.5f, -tns*0.5f*sh);

	*this = dqA;
	mul(r);
}


namespace nxGeom {

bool seg_seg_overlap_2d(float s0x0, float s0y0, float s0x1, float s0y1, float s1x0, float s1y0, float s1x1, float s1y1) {
	bool res = false;
	float a1 = signed_tri_area_2d(s0x0, s0y0, s0x1, s0y1, s1x1, s1y1);
	float a2 = signed_tri_area_2d(s0x0, s0y0, s0x1, s0y1, s1x0, s1y0);
	if (a1*a2 < 0.0f) {
		float a3 = signed_tri_area_2d(s1x0, s1y0, s1x1, s1y1, s0x0, s0y0);
		float a4 = a3 + a2 - a1;
		if (a3*a4 < 0.0f) {
			res = true;
		}
	}
	return res;
}

bool seg_plane_intersect(const cxVec& p0, const cxVec& p1, const cxPlane& pln, float* pT) {
	cxVec d = p1 - p0;
	cxVec n = pln.get_normal();
	float dn = d.dot(n);
	float t = (pln.get_D() - n.dot(p0)) / dn;
	bool res = t >= 0.0f && t <= 1.0f;
	if (res && pT) {
		*pT = t;
	}
	return res;
}

bool seg_quad_intersect_cw(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& v3, cxVec* pHitPos, cxVec* pHitNrm) {
	return seg_quad_intersect_ccw(p0, p1, v3, v2, v1, v0, pHitPos, pHitNrm);
}

bool seg_quad_intersect_cw_n(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& v3, const cxVec& nrm, cxVec* pHitPos) {
	return seg_quad_intersect_ccw_n(p0, p1, v3, v2, v1, v0, nrm, pHitPos);
}

bool seg_quad_intersect_ccw(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& v3, cxVec* pHitPos, cxVec* pHitNrm) {
	cxVec vec[4];
	cxVec edge[4];
	edge[0] = v1 - v0;
	cxVec n = nxVec::cross(edge[0], v2 - v0).get_normalized();
	vec[0] = p0 - v0;
	float d0 = vec[0].dot(n);
	float d1 = (p1 - v0).dot(n);
	if (d0*d1 > 0.0f || (d0 == 0.0f && d1 == 0.0f)) return false;
	edge[1] = v2 - v1;
	edge[2] = v3 - v2;
	edge[3] = v0 - v3;
	vec[1] = p0 - v1;
	vec[2] = p0 - v2;
	vec[3] = p0 - v3;
	cxVec dir = p1 - p0;
#if 1
	if (nxVec::scalar_triple(edge[0], dir, vec[0]) < 0.0f) return false;
	if (nxVec::scalar_triple(edge[1], dir, vec[1]) < 0.0f) return false;
	if (nxVec::scalar_triple(edge[2], dir, vec[2]) < 0.0f) return false;
	if (nxVec::scalar_triple(edge[3], dir, vec[3]) < 0.0f) return false;
#else
	float tp[4];
	for (int i = 0; i < 4; ++i) {
		tp[i] = nxVec::scalar_triple(edge[i], dir, vec[i]);
	}
	for (int i = 0; i < 4; ++i) {
		if (tp[i] < 0.0f) return false;
	}
#endif
	float d = dir.dot(n);
	float t;
	if (d == 0.0f || d0 == 0.0f) {
		t = 0.0f;
	} else {
		t = -d0 / d;
	}
	if (t > 1.0f || t < 0.0f) return false;
	if (pHitPos) {
		*pHitPos = p0 + dir*t;
	}
	if (pHitNrm) {
		*pHitNrm = n;
	}
	return true;
}

bool seg_quad_intersect_ccw_n(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& v3, const cxVec& nrm, cxVec* pHitPos) {
	cxVec vec[4];
	cxVec edge[4];
	vec[0] = p0 - v0;
	float d0 = vec[0].dot(nrm);
	float d1 = (p1 - v0).dot(nrm);
	if (d0*d1 > 0.0f || (d0 == 0.0f && d1 == 0.0f)) return false;
	edge[0] = v1 - v0;
	edge[1] = v2 - v1;
	edge[2] = v3 - v2;
	edge[3] = v0 - v3;
	vec[1] = p0 - v1;
	vec[2] = p0 - v2;
	vec[3] = p0 - v3;
	cxVec dir = p1 - p0;
	if (nxVec::scalar_triple(edge[0], dir, vec[0]) < 0.0f) return false;
	if (nxVec::scalar_triple(edge[1], dir, vec[1]) < 0.0f) return false;
	if (nxVec::scalar_triple(edge[2], dir, vec[2]) < 0.0f) return false;
	if (nxVec::scalar_triple(edge[3], dir, vec[3]) < 0.0f) return false;
	float d = dir.dot(nrm);
	float t;
	if (d == 0.0f || d0 == 0.0f) {
		t = 0.0f;
	} else {
		t = -d0 / d;
	}
	if (t > 1.0f || t < 0.0f) return false;
	if (pHitPos) {
		*pHitPos = p0 + dir*t;
	}
	return true;
}

bool seg_polyhedron_intersect(const cxVec& p0, const cxVec& p1, const cxPlane* pPln, int plnNum, float* pFirst, float* pLast) {
	float first = 0.0f;
	float last = 1.0f;
	bool res = false;
	cxVec d = p1 - p0;
	for (int i = 0; i < plnNum; ++i) {
		cxVec nrm = pPln[i].get_normal();
		float dnm = d.dot(nrm);
		float dist = pPln[i].get_D() - nrm.dot(p0);
		if (dnm == 0.0f) {
			if (dist < 0.0f) goto _exit;
		} else {
			float t = dist / dnm;
			if (dnm < 0.0f) {
				if (t > first) first = t;
			} else {
				if (t < last) last = t;
			}
			if (first > last) goto _exit;
		}
	}
	res = true;
_exit:
	if (pFirst) {
		*pFirst = first;
	}
	if (pLast) {
		*pLast = last;
	}
	return res;
}

bool seg_tri_intersect_cw_n(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& nrm, cxVec* pHitPos) {
	return seg_quad_intersect_cw_n(p0, p1, v0, v1, v2, v0, nrm, pHitPos);
}

bool seg_tri_intersect_cw(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, cxVec* pHitPos, cxVec* pHitNrm) {
	return seg_quad_intersect_cw(p0, p1, v0, v1, v2, v0, pHitPos, pHitNrm);
}


bool seg_tri_intersect_ccw_n(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& nrm, cxVec* pHitPos) {
	return seg_quad_intersect_ccw_n(p0, p1, v0, v1, v2, v0, nrm, pHitPos);
}

bool seg_tri_intersect_ccw(const cxVec& p0, const cxVec& p1, const cxVec& v0, const cxVec& v1, const cxVec& v2, cxVec* pHitPos, cxVec* pHitNrm) {
	return seg_quad_intersect_ccw(p0, p1, v0, v1, v2, v0, pHitPos, pHitNrm);
}

cxVec barycentric(const cxVec& pos, const cxVec& v0, const cxVec& v1, const cxVec& v2) {
	cxVec dv0 = pos - v0;
	cxVec dv1 = v1 - v0;
	cxVec dv2 = v2 - v0;
	float d01 = dv0.dot(dv1);
	float d02 = dv0.dot(dv2);
	float d12 = dv1.dot(dv2);
	float d11 = dv1.dot(dv1);
	float d22 = dv2.dot(dv2);
	float d = d11*d22 - nxCalc::sq(d12);
	float ood = 1.0f / d;
	float u = (d01*d22 - d02*d12) * ood;
	float v = (d02*d11 - d01*d12) * ood;
	float w = (1.0f - u - v);
	return cxVec(u, v, w);
}

float quad_dist2(const cxVec& pos, const cxVec vtx[4]) {
	int i;
	cxVec v[4];
	cxVec edge[4];
	for (i = 0; i < 4; ++i) {
		v[i] = pos - vtx[i];
	}
	for (i = 0; i < 4; ++i) {
		edge[i] = vtx[i < 3 ? i + 1 : 0] - vtx[i];
	}
	cxVec n = nxVec::cross(edge[0], vtx[2] - vtx[0]).get_normalized();
	float d[4];
	for (i = 0; i < 4; ++i) {
		d[i] = nxVec::scalar_triple(edge[i], v[i], n);
	}
	int mask = 0;
	for (i = 0; i < 4; ++i) {
		if (d[i] >= 0) mask |= 1 << i;
	}
	if (mask == 0xF) {
		return nxCalc::sq(v[0].dot(n));
	}
	if (::fabsf(d[3]) < 1e-6f) {
		edge[3] = edge[2];
	}
	for (i = 0; i < 4; ++i) {
		d[i] = edge[i].mag2();
	}
	float len[4];
	for (i = 0; i < 4; ++i) {
		len[i] = ::sqrtf(d[i]);
	}
	for (i = 0; i < 4; ++i) {
		edge[i].scl(nxCalc::rcp0(len[i]));
	}
	for (i = 0; i < 4; ++i) {
		d[i] = v[i].dot(edge[i]);
	}
	for (i = 0; i < 4; ++i) {
		d[i] = nxCalc::clamp(d[i], 0.0f, len[i]);
	}
	for (i = 0; i < 4; ++i) {
		edge[i].scl(d[i]);
	}
	for (i = 0; i < 4; ++i) {
		v[i] = vtx[i] + edge[i];
	}
	for (i = 0; i < 4; ++i) {
		d[i] = nxVec::dist2(pos, v[i]);
	}
	return nxCalc::min(d[0], d[1], d[2], d[3]);
}

int quad_convex_ck(const cxVec& v0, const cxVec& v1, const cxVec& v2, const cxVec& v3) {
	int res = 0;
	cxVec e0 = v1 - v0;
	cxVec e1 = v2 - v1;
	cxVec e2 = v3 - v2;
	cxVec e3 = v0 - v3;
	cxVec n01 = nxVec::cross(e0, e1);
	cxVec n23 = nxVec::cross(e2, e3);
	if (n01.dot(n23) > 0.0f) res |= 1;
	cxVec n12 = nxVec::cross(e1, e2);
	cxVec n30 = nxVec::cross(e3, e0);
	if (n12.dot(n30) > 0.0f) res |= 2;
	return res;
}

void update_nrm_newell(cxVec* pNrm, cxVec* pVtxI, cxVec* pVtxJ) {
	cxVec dv = *pVtxI - *pVtxJ;
	cxVec sv = *pVtxI + *pVtxJ;
	*pNrm += cxVec(dv[1], dv[2], dv[0]) * cxVec(sv[2], sv[0], sv[1]);
}

cxVec poly_normal_cw(cxVec* pVtx, int vtxNum) {
	cxVec nrm;
	nrm.zero();
	for (int i = 0; i < vtxNum; ++i) {
		int j = i - 1;
		if (j < 0) j = vtxNum - 1;
		update_nrm_newell(&nrm, &pVtx[i], &pVtx[j]);
	}
	nrm.normalize();
	return nrm;
}

cxVec poly_normal_ccw(cxVec* pVtx, int vtxNum) {
	cxVec nrm;
	nrm.zero();
	for (int i = 0; i < vtxNum; ++i) {
		int j = i + 1;
		if (j >= vtxNum) j = 0;
		update_nrm_newell(&nrm, &pVtx[i], &pVtx[j]);
	}
	nrm.normalize();
	return nrm;
}

float seg_seg_dist2(const cxVec& s0p0, const cxVec& s0p1, const cxVec& s1p0, const cxVec& s1p1, cxVec* pBridgeP0, cxVec* pBridgeP1) {
	static float eps = 1e-6f;
	float t0 = 0.0f;
	float t1 = 0.0f;
	cxVec dir0 = s0p1 - s0p0;
	cxVec dir1 = s1p1 - s1p0;
	cxVec vec = s0p0 - s1p0;
	float len0 = dir0.mag2();
	float len1 = dir1.mag2();
	float vd1 = vec.dot(dir1);
	if (len0 <= eps) {
		if (len1 > eps) {
			t1 = nxCalc::saturate(vd1 / len1);
		}
	} else {
		float vd0 = vec.dot(dir0);
		if (len1 <= eps) {
			t0 = nxCalc::saturate(-vd0 / len0);
		} else {
			float dd = dir0.dot(dir1);
			float dn = len0*len1 - nxCalc::sq(dd);
			if (dn != 0.0f) {
				t0 = nxCalc::saturate((dd*vd1 - vd0*len1) / dn);
			}
			t1 = (dd*t0 + vd1) / len1;
			if (t1 < 0.0f) {
				t0 = nxCalc::saturate(-vd0 / len0);
				t1 = 0.0f;
			} else if (t1 > 1.0f) {
				t0 = nxCalc::saturate((dd - vd0) / len0);
				t1 = 1.0f;
			}
		}
	}
	cxVec bp0 = s0p0 + dir0*t0;
	cxVec bp1 = s1p0 + dir1*t1;
	if (pBridgeP0) {
		*pBridgeP0 = bp0;
	}
	if (pBridgeP1) {
		*pBridgeP1 = bp1;
	}
	return (bp1 - bp0).mag2();
}

bool cap_aabb_overlap(const cxVec& cp0, const cxVec& cp1, float cr, const cxVec& bmin, const cxVec& bmax) {
	cxVec brad = (bmax - bmin) * 0.5f;
	cxVec bcen = (bmin + bmax) * 0.5f;
	cxVec ccen = (cp0 + cp1) * 0.5f;
	cxVec h = (cp1 - cp0) * 0.5f;
	cxVec v = ccen - bcen;
	for (int i = 0; i < 3; ++i) {
		float d0 = ::fabsf(v[i]);
		float d1 = ::fabsf(h[i]) + brad[i] + cr;
		if (d0 > d1) return false;
	}
	cxVec tpos;
	line_pnt_closest(cp0, cp1, bcen, &tpos);
	cxVec a = (tpos - bcen).get_normalized();
	if (::fabsf(v.dot(a)) > ::fabsf(brad.dot(a)) + cr) return false;
	return true;
}

} // nxGeom


void cxPlane::calc(const cxVec& pos, const cxVec& nrm) {
	mABC = nrm;
	mD = pos.dot(nrm);
}


bool cxSphere::overlaps(const cxAABB& box) const {
	return nxGeom::sph_aabb_overlap(get_center(), get_radius(), box.get_min_pos(), box.get_max_pos());
}

bool cxSphere::overlaps(const cxCapsule& cap, cxVec* pCapAxisPos) const {
	return nxGeom::sph_cap_overlap(get_center(), get_radius(), cap.get_pos0(), cap.get_pos1(), cap.get_radius(), pCapAxisPos);
}


void cxAABB::transform(const cxAABB& box, const cxMtx& mtx) {
	cxVec omin = box.mMin;
	cxVec omax = box.mMax;
	cxVec nmin = mtx.get_translation();
	cxVec nmax = nmin;
	cxVec va, ve, vf;

	va = mtx.get_row_vec(0);
	ve = va * omin.x;
	vf = va * omax.x;
	nmin += nxVec::min(ve, vf);
	nmax += nxVec::max(ve, vf);

	va = mtx.get_row_vec(1);
	ve = va * omin.y;
	vf = va * omax.y;
	nmin += nxVec::min(ve, vf);
	nmax += nxVec::max(ve, vf);

	va = mtx.get_row_vec(2);
	ve = va * omin.z;
	vf = va * omax.z;
	nmin += nxVec::min(ve, vf);
	nmax += nxVec::max(ve, vf);

	mMin = nmin;
	mMax = nmax;
}

void cxAABB::from_sph(const cxSphere& sph) {
	cxVec c = sph.get_center();
	cxVec r = sph.get_radius();
	mMin = c - r;
	mMax = c + r;
}

cxVec cxAABB::closest_pnt(const cxVec& pos, bool onFace) const {
	cxVec res = nxVec::min(nxVec::max(pos, mMin), mMax);
	if (onFace && contains(res)) {
		cxVec relMin = res - mMin;
		cxVec relMax = mMax - res;
		cxVec dmin = nxVec::min(relMin, relMax);
		int elemIdx = (dmin.x < dmin.y && dmin.x < dmin.z) ? 0 : dmin.y < dmin.z ? 1 : 2;
		res.set_at(elemIdx, (relMin[elemIdx] < relMax[elemIdx] ? mMin : mMax)[elemIdx]);
	}
	return res;
}

bool cxAABB::seg_ck(const cxVec& p0, const cxVec& p1) const {
	cxVec dir = p1 - p0;
	float len = dir.mag();
	if (len < FLT_EPSILON) {
		return contains(p0);
	}
	float tmin = 0.0f;
	float tmax = len;

	if (dir.x != 0) {
		float dx = len / dir.x;
		float x1 = (mMin.x - p0.x) * dx;
		float x2 = (mMax.x - p0.x) * dx;
		float xmin = nxCalc::min(x1, x2);
		float xmax = nxCalc::max(x1, x2);
		tmin = nxCalc::max(tmin, xmin);
		tmax = nxCalc::min(tmax, xmax);
		if (tmin > tmax) {
			return false;
		}
	} else {
		if (p0.x < mMin.x || p0.x > mMax.x) return false;
	}

	if (dir.y != 0) {
		float dy = len / dir.y;
		float y1 = (mMin.y - p0.y) * dy;
		float y2 = (mMax.y - p0.y) * dy;
		float ymin = nxCalc::min(y1, y2);
		float ymax = nxCalc::max(y1, y2);
		tmin = nxCalc::max(tmin, ymin);
		tmax = nxCalc::min(tmax, ymax);
		if (tmin > tmax) {
			return false;
		}
	} else {
		if (p0.y < mMin.y || p0.y > mMax.y) return false;
	}

	if (dir.z != 0) {
		float dz = len / dir.z;
		float z1 = (mMin.z - p0.z) * dz;
		float z2 = (mMax.z - p0.z) * dz;
		float zmin = nxCalc::min(z1, z2);
		float zmax = nxCalc::max(z1, z2);
		tmin = nxCalc::max(tmin, zmin);
		tmax = nxCalc::min(tmax, zmax);
		if (tmin > tmax) {
			return false;
		}
	} else {
		if (p0.z < mMin.z || p0.z > mMax.z) return false;
	}

	if (tmax > len) return false;
	return true;
}


bool cxCapsule::overlaps(const cxAABB& box) const {
	return nxGeom::cap_aabb_overlap(mPos0, mPos1, mRadius, box.get_min_pos(), box.get_max_pos());
}

bool cxCapsule::overlaps(const cxCapsule& cap) const {
	return nxGeom::cap_cap_overlap(mPos0, mPos1, mRadius, cap.mPos0, cap.mPos1, cap.mRadius);
}


void cxFrustum::calc_normals() {
	mNrm[0] = nxGeom::tri_normal_cw(mPnt[0], mPnt[1], mPnt[3]); /* near */
	mNrm[1] = nxGeom::tri_normal_cw(mPnt[0], mPnt[3], mPnt[4]); /* left */
	mNrm[2] = nxGeom::tri_normal_cw(mPnt[0], mPnt[4], mPnt[5]); /* top */
	mNrm[3] = nxGeom::tri_normal_cw(mPnt[6], mPnt[2], mPnt[1]); /* right */
	mNrm[4] = nxGeom::tri_normal_cw(mPnt[6], mPnt[7], mPnt[3]); /* bottom */
	mNrm[5] = nxGeom::tri_normal_cw(mPnt[6], mPnt[5], mPnt[4]); /* far */
}

void cxFrustum::calc_planes() {
	for (int i = 0; i < 6; ++i) {
		static int vtxNo[] = { 0, 0, 0, 6, 6, 6 };
		mPlane[i].calc(mPnt[vtxNo[i]], mNrm[i]);
	}
}

void cxFrustum::init(const cxMtx& mtx, float fovy, float aspect, float znear, float zfar) {
	int i;
	float x, y, z;
	float t = ::tanf(fovy * 0.5f);
	z = znear;
	y = t * z;
	x = y * aspect;
	mPnt[0].set(-x, y, -z);
	mPnt[1].set(x, y, -z);
	mPnt[2].set(x, -y, -z);
	mPnt[3].set(-x, -y, -z);
	z = zfar;
	y = t * z;
	x = y * aspect;
	mPnt[4].set(-x, y, -z);
	mPnt[5].set(x, y, -z);
	mPnt[6].set(x, -y, -z);
	mPnt[7].set(-x, -y, -z);
	calc_normals();
	for (i = 0; i < 8; ++i) {
		mPnt[i] = mtx.calc_pnt(mPnt[i]);
	}
	for (i = 0; i < 6; ++i) {
		mNrm[i] = mtx.calc_vec(mNrm[i]);
	}
	calc_planes();
}

cxVec cxFrustum::get_center() const {
	cxVec c = mPnt[0];
	for (int i = 1; i < 8; ++i) {
		c += mPnt[i];
	}
	c /= 8.0f;
	return c;
}

bool cxFrustum::cull(const cxSphere& sph) const {
	cxVec c = sph.get_center();
	float r = sph.get_radius();
	cxVec v = c - mPnt[0];
	for (int i = 0; i < 6; ++i) {
		if (i == 3) v = c - mPnt[6];
		float d = v.dot(mNrm[i]);
		if (r < d) return true;
	}
	return false;
}

bool cxFrustum::overlaps(const cxSphere& sph) const {
	float sd = FLT_MAX;
	bool flg = false;
	cxVec c = sph.get_center();
	for (int i = 0; i < 6; ++i) {
		if (mPlane[i].signed_dist(c) > 0.0f) {
			static uint16_t vtxTbl[6] = {
				0x0123,
				0x0374,
				0x1045,
				0x5621,
				0x6732,
				0x4765
			};
			uint32_t vidx = vtxTbl[i];
			cxVec vtx[4];
			for (int j = 0; j < 4; ++j) {
				vtx[j] = mPnt[vidx & 0xF];
				vidx >>= 4;
			}
			float dist = nxGeom::quad_dist2(c, vtx);
			sd = nxCalc::min(sd, dist);
			flg = true;
		}
	}
	if (!flg) return true;
	if (sd <= nxCalc::sq(sph.get_radius())) return true;
	return false;
}

bool cxFrustum::cull(const cxAABB& box) const {
	cxVec c = box.get_center();
	cxVec r = box.get_max_pos() - c;
	cxVec v = c - mPnt[0];
	for (int i = 0; i < 6; ++i) {
		cxVec n = mNrm[i];
		if (i == 3) v = c - mPnt[6];
		if (r.dot(n.abs_val()) < v.dot(n)) return true;
	}
	return false;
}

bool cxFrustum::overlaps(const cxAABB& box) const {
	static struct {
		uint8_t i0, i1;
	} edgeTbl[] = {
		{ 0, 1 },{ 1, 2 },{ 3, 4 },{ 3, 0 },
		{ 4, 5 },{ 5, 6 },{ 6, 7 },{ 7, 4 },
		{ 0, 4 },{ 1, 5 },{ 2, 6 },{ 3, 7 }
	};
	for (int i = 0; i < 12; ++i) {
		cxVec p0 = mPnt[edgeTbl[i].i0];
		cxVec p1 = mPnt[edgeTbl[i].i1];
		if (box.seg_ck(p0, p1)) return true;
	}
	const int imin = 0;
	const int imax = 1;
	cxVec bb[2];
	bb[imin] = box.get_min_pos();
	bb[imax] = box.get_max_pos();
	static struct {
		uint8_t i0x, i0y, i0z, i1x, i1y, i1z;
	} boxEdgeTbl[] = {
		{ imin, imin, imin, imax, imin, imin },
		{ imax, imin, imin, imax, imin, imax },
		{ imax, imin, imax, imin, imin, imax },
		{ imin, imin, imax, imin, imin, imin },

		{ imin, imax, imin, imax, imax, imin },
		{ imax, imax, imin, imax, imax, imax },
		{ imax, imax, imax, imin, imax, imax },
		{ imin, imax, imax, imin, imax, imin },

		{ imin, imin, imin, imin, imax, imin },
		{ imax, imin, imin, imax, imax, imin },
		{ imax, imin, imax, imax, imax, imax },
		{ imin, imin, imax, imin, imax, imax }
	};
	for (int i = 0; i < 12; ++i) {
		float p0x = bb[boxEdgeTbl[i].i0x].x;
		float p0y = bb[boxEdgeTbl[i].i0y].y;
		float p0z = bb[boxEdgeTbl[i].i0z].z;
		float p1x = bb[boxEdgeTbl[i].i1x].x;
		float p1y = bb[boxEdgeTbl[i].i1y].y;
		float p1z = bb[boxEdgeTbl[i].i1z].z;
		cxVec p0(p0x, p0y, p0z);
		cxVec p1(p1x, p1y, p1z);
		if (nxGeom::seg_polyhedron_intersect(p0, p1, mPlane, 6)) return true;
	}
	return false;
}


namespace nxColor {

void init_XYZ_transform(cxMtx* pRGB2XYZ, cxMtx* pXYZ2RGB, cxVec* pPrims, cxVec* pWhite) {
	cxVec rx, ry, rz;
	if (pPrims) {
		rx = pPrims[0];
		ry = pPrims[1];
		rz = pPrims[2];
	} else {
		/* 709 primaries */
		rx.set(0.640f, 0.330f, 0.030f);
		ry.set(0.300f, 0.600f, 0.100f);
		rz.set(0.150f, 0.060f, 0.790f);
	}

	cxVec w;
	if (pWhite) {
		w = *pWhite;
	} else {
		/* D65 1931 */
		w.set(0.3127f, 0.3290f, 0.3582f);
	}
	w.scl(nxCalc::rcp0(w.y));

	cxMtx cm;
	cm.set_rot_frame(rx, ry, rz);

	cxMtx im = cm.get_inverted();
	cxVec j = im.calc_vec(w);
	cxMtx tm;
	tm.mk_scl(j);
	tm.mul(cm);
	if (pRGB2XYZ) {
		*pRGB2XYZ = tm;
	}
	if (pXYZ2RGB) {
		*pXYZ2RGB = tm.get_inverted();
	}
}

void init_XYZ_transform_xy_w(cxMtx* pRGB2XYZ, cxMtx* pXYZ2RGB, float wx, float wy) {
	cxVec w(wx, wy, 1.0f - (wx + wy));
	init_XYZ_transform(pRGB2XYZ, pXYZ2RGB, nullptr, &w);
}

void init_XYZ_transform_xy(cxMtx* pRGB2XYZ, cxMtx* pXYZ2RGB, float rx, float ry, float gx, float gy, float bx, float by, float wx, float wy) {
	cxVec prims[3];
	prims[0].set(rx, ry, 1.0f - (rx + ry));
	prims[1].set(gx, gy, 1.0f - (gx + gy));
	prims[2].set(bx, by, 1.0f - (bx + by));
	cxVec white(wx, wy, 1.0f - (wx + wy));
	init_XYZ_transform(pRGB2XYZ, pXYZ2RGB, prims, &white);
}

cxVec XYZ_to_Lab(const cxVec& xyz, cxMtx* pRGB2XYZ) {
	cxVec white = cxColor(1.0f).XYZ(pRGB2XYZ);
	cxVec l = xyz * nxVec::rcp0(white);
	for (int i = 0; i < 3; ++i) {
		float e = l.get_at(i);
		if (e > 0.008856f) {
			e = nxCalc::cb_root(e);
		} else {
			e = e*7.787f + (16.0f / 116);
		}
		l.set_at(i, e);
	}
	float lx = l.x;
	float ly = l.y;
	float lz = l.z;
	return cxVec(116.0f*ly - 16.0f, 500.0f*(lx - ly), 200.0f*(ly - lz));
}

cxVec Lab_to_XYZ(const cxVec& lab, cxMtx* pRGB2XYZ) {
	cxVec white = cxColor(1.0f).XYZ(pRGB2XYZ);
	float L = lab.x;
	float a = lab.y;
	float b = lab.z;
	float ly = (L + 16.0f) / 116.0f;
	cxVec t(a/500.0f + ly, ly, -b/200.0f + ly);
	for (int i = 0; i < 3; ++i) {
		float e = t.get_at(i);
		if (e > 0.206893f) {
			e = nxCalc::cb(e);
		} else {
			e = (e - (16.0f/116)) / 7.787f;
		}
		t.set_at(i, e);
	}
	t.mul(white);
	return t;
}

cxVec Lab_to_Lch(const cxVec& lab) {
	float L = lab.x;
	float a = lab.y;
	float b = lab.z;
	float c = nxCalc::hypot(a, b);
	float h = ::atan2f(b, a);
	return cxVec(L, c, h);
}

cxVec Lch_to_Lab(const cxVec& lch) {
	float L = lch.x;
	float c = lch.y;
	float h = lch.z;
	float hs = ::sinf(h);
	float hc = ::cosf(h);
	return cxVec(L, c*hc, c*hs);
}

float Lch_perceived_lightness(const cxVec& lch) {
	float L = lch.x;
	float c = lch.y;
	float h = lch.z;
	float fh = ::fabsf(::sinf((h - (XD_PI*0.5f)) * 0.5f))*0.116f + 0.085f;
	float fL = 2.5f - (2.5f/100)*L; // 1 at 60, 0 at 100 (white)
	return L + fh*fL*c;
}


// Single-Lobe CMF fits from Wyman et al. "Simple Analytic Approximations to the CIE XYZ Color Matching Functions"

float approx_CMF_x31(float w) {
	return 1.065f*::expf(-0.5f * nxCalc::sq((w - 595.8f) / 33.33f)) + 0.366f*::expf(-0.5f * nxCalc::sq((w - 446.8f) / 19.44f));
}

float approx_CMF_y31(float w) {
	return 1.014f*::expf(-0.5f * nxCalc::sq((::logf(w) - 6.32130766f) / 0.075f));
}

float approx_CMF_z31(float w) {
	return 1.839f*::expf(-0.5f * nxCalc::sq((::logf(w) - 6.1088028f) / 0.051f));
}

float approx_CMF_x64(float w) {
	return 0.398f*::expf(-1250.0f * nxCalc::sq(::logf((w + 570.1f) / 1014.0f))) + 1.132f*::expf(-234.0f * nxCalc::sq(::logf((1338.0f - w) / 743.5f)));
}

float approx_CMF_y64(float w) {
	return 1.011f*::expf(-0.5f * nxCalc::sq((w - 556.1f) / 46.14f));
}

float approx_CMF_z64(float w) {
	return 2.06f*::expf(-32.0f * nxCalc::sq(::logf((w - 265.8f) / 180.4f)));
}

} // nxColor

cxVec cxColor::YCgCo() const {
	cxVec rgb(r, g, b);
	return cxVec(
		rgb.dot(cxVec(0.25f, 0.5f, 0.25f)),
		rgb.dot(cxVec(-0.25f, 0.5f, -0.25f)),
		rgb.dot(cxVec(0.5f, 0.0f, -0.5f))
	);
}

void cxColor::from_YCgCo(const cxVec& ygo) {
	float Y = ygo[0];
	float G = ygo[1];
	float O = ygo[2];
	float t = Y - G;
	r = t + O;
	g = Y + G;
	b = t - O;
}

cxVec cxColor::TMI() const {
	float T = b - r;
	float M = (r - g*2.0f + b) * 0.5f;
	float I = (r + g + b) / 3.0f;
	return cxVec(T, M, I);
}

void cxColor::from_TMI(const cxVec& tmi) {
	float T = tmi[0];
	float M = tmi[1];
	float I = tmi[2];
	r = I - T*0.5f + M/3.0f;
	g = I - M*2.0f/3.0f;
	b = I + T*0.5f + M/3.0f;
}

static inline cxMtx* mtx_RGB2XYZ(cxMtx* pRGB2XYZ) {
	static float tm709[] = {
		0.412453f, 0.212671f, 0.019334f, 0.0f,
		0.357580f, 0.715160f, 0.119193f, 0.0f,
		0.180423f, 0.072169f, 0.950227f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	return pRGB2XYZ ? pRGB2XYZ : (cxMtx*)tm709;
}

static inline cxMtx* mtx_XYZ2RGB(cxMtx* pXYZ2RGB) {
	static float im709[] = {
		 3.240479f, -0.969256f,  0.055648f, 0.0f,
		-1.537150f,  1.875992f, -0.204043f, 0.0f,
		-0.498535f,  0.041556f,  1.057311f, 0.0f,
		 0.0f, 0.0f, 0.0f, 1.0f
	};
	return pXYZ2RGB ? pXYZ2RGB : (cxMtx*)im709;
}

cxVec cxColor::XYZ(cxMtx* pRGB2XYZ) const {
	return mtx_RGB2XYZ(pRGB2XYZ)->calc_vec(cxVec(r, g, b));
}

void cxColor::from_XYZ(const cxVec& xyz, cxMtx* pXYZ2RGB) {
	set(mtx_XYZ2RGB(pXYZ2RGB)->calc_vec(xyz));
}

cxVec cxColor::Lab(cxMtx* pRGB2XYZ) const {
	return nxColor::XYZ_to_Lab(XYZ(pRGB2XYZ), pRGB2XYZ);
}

void cxColor::from_Lab(const cxVec& lab, cxMtx* pRGB2XYZ, cxMtx* pXYZ2RGB) {
	from_XYZ(nxColor::Lab_to_XYZ(lab, pRGB2XYZ), pXYZ2RGB);
}

cxVec cxColor::Lch(cxMtx* pRGB2XYZ) const {
	return nxColor::Lab_to_Lch(Lab(pRGB2XYZ));
}

void cxColor::from_Lch(const cxVec& lch, cxMtx* pRGB2XYZ, cxMtx* pXYZ2RGB) {
	from_Lab(nxColor::Lch_to_Lab(lch), pRGB2XYZ, pXYZ2RGB);
}

uint32_t cxColor::encode_rgbe() const {
	float m = max();
	if (m < 1.0e-32f) return 0;
	float t[3];
	int e;
	float s = (::frexpf(m, &e) * 256.0f) / m;
	for (int i = 0; i < 3; ++i) {
		t[i] = ch[i] * s;
	}
	uxVal32 res;
	for (int i = 0; i < 3; ++i) {
		res.b[i] = (uint8_t)t[i];
	}
	res.b[3] = e + 128;
	return res.u;
}

void cxColor::decode_rgbe(uint32_t rgbe) {
	if (rgbe) {
		uxVal32 v;
		v.u = rgbe;
		int e = v.b[3];
		float s = ::ldexpf(1.0f, e - (128 + 8));
		float t[3];
		for (int i = 0; i < 3; ++i) {
			t[i] = (float)v.b[i];
		}
		for (int i = 0; i < 3; ++i) {
			ch[i] = t[i] * s;
		}
		a = 1.0f;
	} else {
		zero_rgb();
	}
}

static inline uint32_t swap_enc_clr_rb(uint32_t enc) {
	uxVal32 v;
	v.u = enc;
	uint8_t t = v.b[0];
	v.b[0] = v.b[2];
	v.b[2] = t;
	return v.u;
}

uint32_t cxColor::encode_bgre() const {
	return swap_enc_clr_rb(encode_rgbe());
}

void cxColor::decode_bgre(uint32_t bgre) {
	decode_rgbe(swap_enc_clr_rb(bgre));
}

uint32_t cxColor::encode_rgbi() const {
	float e[4];
	for (int i = 0; i < 3; ++i) {
		e[i] = nxCalc::clamp(ch[i], 0.0f, 100.0f);
	}
	e[3] = 1.0f;
	for (int i = 0; i < 4; ++i) {
		e[i] = ::sqrtf(e[i]); /* gamma 2.0 encoding */
	}
	float m = nxCalc::max(e[0], e[1], e[2]);
	if (m > 1.0f) {
		float s = 1.0f / m;
		for (int i = 0; i < 4; ++i) {
			e[i] *= s;
		}
	}
	for (int i = 0; i < 4; ++i) {
		e[i] *= 255.0f;
	}
	uxVal32 res;
	for (int i = 0; i < 4; ++i) {
		res.b[i] = (uint8_t)e[i];
	}
	return res.u;
}

void cxColor::decode_rgbi(uint32_t rgbi) {
	uxVal32 v;
	v.u = rgbi;
	for (int i = 0; i < 4; ++i) {
		ch[i] = (float)v.b[i];
	}
	scl(nxCalc::rcp0(a));
	for (int i = 0; i < 4; ++i) {
		ch[i] = nxCalc::sq(ch[i]); /* gamma 2.0 decoding */
	}
}

uint32_t cxColor::encode_bgri() const {
	return swap_enc_clr_rb(encode_rgbi());
}

void cxColor::decode_bgri(uint32_t bgri) {
	decode_rgbi(swap_enc_clr_rb(bgri));
}

uint32_t cxColor::encode_rgba8() const {
	float f[4];
	for (int i = 0; i < 4; ++i) {
		f[i] = nxCalc::saturate(ch[i]);
	}
	for (int i = 0; i < 4; ++i) {
		f[i] *= 255.0f;
	}
	uxVal32 v;
	for (int i = 0; i < 4; ++i) {
		v.b[i] = (uint8_t)f[i];
	}
	return v.u;
}

void cxColor::decode_rgba8(uint32_t rgba) {
	uxVal32 v;
	v.u = rgba;
	for (int i = 0; i < 4; ++i) {
		ch[i] = (float)v.b[i];
	}
	scl(1.0f / 255.0f);
}

uint32_t cxColor::encode_bgra8() const {
	return swap_enc_clr_rb(encode_rgba8());
}

void cxColor::decode_bgra8(uint32_t bgra) {
	decode_rgba8(swap_enc_clr_rb(bgra));
}

uint16_t cxColor::encode_bgr565() const {
	uint16_t ir = (uint16_t)::roundf(nxCalc::saturate(r) * float(0x1F));
	uint16_t ig = (uint16_t)::roundf(nxCalc::saturate(g) * float(0x3F));
	uint16_t ib = (uint16_t)::roundf(nxCalc::saturate(b) * float(0x1F));
	uint16_t ic = ib;
	ic |= ig << 5;
	ic |= ir << (5 + 6);
	return ic;
}

void cxColor::decode_bgr565(uint16_t bgr) {
	b = float(bgr & 0x1F);
	g = float((bgr >> 5) & 0x3F);
	r = float((bgr >> (5+6)) & 0x1F);
	scl_rgb(1.0f / float(0x1F), 1.0f / float(0x3F), 1.0f / float(0x1F));
	a = 1.0f;
}


namespace nxSH {

void calc_weights(float* pWgt, int order, float s, float scl) {
	if (!pWgt) return;
	for (int i = 0; i < order; ++i) {
		pWgt[i] = ::expf(float(-i*i) / (2.0f*s)) * scl;
	}
}

void get_diff_weights(float* pWgt, int order, float scl) {
	for (int i = 0; i < order; ++i) {
		pWgt[i] = 0.0f;
	}
	if (order >= 1) pWgt[0] = scl;
	if (order >= 2) pWgt[1] = scl / 1.5f;
	if (order >= 3) pWgt[2] = scl / 4.0f;
}

void apply_weights(float* pDst, int order, const float* pSrc, const float* pWgt, int nelems) {
	for (int l = 0; l < order; ++l) {
		int i0 = calc_coefs_num(l);
		int i1 = calc_coefs_num(l + 1);
		float w = pWgt[l];
		for (int i = i0; i < i1; ++i) {
			int idx = i * nelems;
			for (int j = 0; j < nelems; ++j) {
				pDst[idx + j] = pSrc[idx + j] * w;
			}
		}
	}
}

double calc_K(int l, int m) {
	int am = ::abs(m);
	double v = 1.0;
	for (int k = l + am; k > l - am; --k) {
		v *= k;
	}
	return ::sqrt((2.0*l + 1.0) / (4.0*M_PI*v));
}

double calc_Pmm(int m) {
	double v = 1.0;
	for (int k = 0; k <= m; ++k) {
		v *= 1.0 - 2.0*k;
	}
	return v;
}

double calc_constA(int m) {
	return calc_Pmm(m) * calc_K(m, m) * ::sqrt(2.0);
}

double calc_constB(int m) {
	return double(2*m + 1) * calc_Pmm(m) * calc_K(m+1, m);
}

double calc_constC1(int l, int m) {
	double c = calc_K(l, m) / calc_K(l-1, m) * double(2*l-1) / double(l-m);
	if (l > 80) {
		if (isnan(c)) c = 0.0;
	}
	return c;
}

double calc_constC2(int l, int m) {
	double c = -calc_K(l, m) / calc_K(l-2, m) * double(l+m-1) / double(l-m);
	if (l > 80) {
		if (isnan(c)) c = 0.0;
	}
	return c;
}

double calc_constD1(int m) {
	return double((2*m+3)*(2*m+1)) * calc_Pmm(m) / 2.0 * calc_K(m+2, m);
}

double calc_constD2(int m) {
	return double(-(2*m + 1)) * calc_Pmm(m) / 2.0 * calc_K(m+2, m);
}

double calc_constE1(int m) {
	return double((2*m+5)*(2*m+3)*(2*m+1)) * calc_Pmm(m) / 6.0 * calc_K(m+3,m);
}

double calc_constE2(int m) {
	double pmm = calc_Pmm(m);
	return (double((2*m+5)*(2*m+1))*pmm/6.0 + double((2*m+2)*(2*m+1))*pmm/3.0) * -calc_K(m+3, m);
}

template<typename CONST_T>
void calc_consts_t(int order, CONST_T* pConsts) {
	if (order < 1 || !pConsts) return;
	int idx = 0;
	pConsts[idx++] = (CONST_T)calc_K(0, 0);
	if (order > 1) {
		// 1, 0
		pConsts[idx++] = (CONST_T)(calc_Pmm(0) * calc_K(1, 0));
	}
	if (order > 2) {
		// 2, 0
		pConsts[idx++] = (CONST_T)calc_constD1(0);
		pConsts[idx++] = (CONST_T)calc_constD2(0);
	}
	if (order > 3) {
		// 2, 0
		pConsts[idx++] = (CONST_T)calc_constE1(0);
		pConsts[idx++] = (CONST_T)calc_constE2(0);
	}
	for (int l = 4; l < order; ++l) {
		pConsts[idx++] = (CONST_T)calc_constC1(l, 0);
		pConsts[idx++] = (CONST_T)calc_constC2(l, 0);
	}
	const double scl = ::sqrt(2.0);
	for (int m = 1; m < order-1; ++m) {
		pConsts[idx++] = (CONST_T)calc_constA(m);
		if (m + 1 < order) {
			pConsts[idx++] = (CONST_T)(calc_constB(m) * scl);
		}
		if (m + 2 < order) {
			pConsts[idx++] = (CONST_T)(calc_constD1(m) * scl);
			pConsts[idx++] = (CONST_T)(calc_constD2(m) * scl);
		}
		if (m + 3 < order) {
			pConsts[idx++] = (CONST_T)(calc_constE1(m) * scl);
			pConsts[idx++] = (CONST_T)(calc_constE2(m) * scl);
		}
		for (int l = m+4; l < order; ++l) {
			pConsts[idx++] = (CONST_T)calc_constC1(l, m);
			pConsts[idx++] = (CONST_T)calc_constC2(l, m);
		}
	}
	if (order > 1) {
		pConsts[idx++] = (CONST_T)calc_constA(order - 1);
	}
}

template<typename EVAL_T, typename CONST_T, int N>
XD_FORCEINLINE
void eval_t(int order, EVAL_T* pCoefs, const EVAL_T x[], const EVAL_T y[], const EVAL_T z[], const CONST_T* pConsts) {
	EVAL_T zz[N];
	for (int i = 0; i < N; ++i) {
		zz[i] = z[i] * z[i];
	}
	EVAL_T cval = (EVAL_T)pConsts[0];
	for (int i = 0; i < N; ++i) {
		pCoefs[i] = cval;
	}
	int idx = 1;
	int idst;
	EVAL_T* pDst;
	EVAL_T cval2;
	if (order > 1) {
		idst = calc_ary_idx(1, 0);
		pDst = &pCoefs[idst * N];
		cval = (EVAL_T)pConsts[idx++];
		for (int i = 0; i < N; ++i) {
			pDst[i] = z[i] * cval;
		}
	}
	if (order > 2) {
		idst = calc_ary_idx(2, 0);
		pDst = &pCoefs[idst * N];
		cval = (EVAL_T)pConsts[idx++];
		cval2 = (EVAL_T)pConsts[idx++];
		for (int i = 0; i < N; ++i) {
			pDst[i] = zz[i]*cval;
		}
		for (int i = 0; i < N; ++i) {
			pDst[i] += cval2;
		}
	}
	if (order > 3) {
		idst = calc_ary_idx(3, 0);
		pDst = &pCoefs[idst * N];
		cval = (EVAL_T)pConsts[idx++];
		cval2 = (EVAL_T)pConsts[idx++];
		for (int i = 0; i < N; ++i) {
			pDst[i] = zz[i]*cval;
		}
		for (int i = 0; i < N; ++i) {
			pDst[i] += cval2;
		}
		for (int i = 0; i < N; ++i) {
			pDst[i] *= z[i];
		}
	}
	for (int l = 4; l < order; ++l) {
		int isrc1 = calc_ary_idx(l-1, 0);
		int isrc2 = calc_ary_idx(l-2, 0);
		EVAL_T* pSrc1 = &pCoefs[isrc1 * N];
		EVAL_T* pSrc2 = &pCoefs[isrc2 * N];
		idst = calc_ary_idx(l, 0);
		pDst = &pCoefs[idst * N];
		cval = (EVAL_T)pConsts[idx++];
		cval2 = (EVAL_T)pConsts[idx++];
		for (int i = 0; i < N; ++i) {
			pDst[i] = z[i]*cval;
		}
		for (int i = 0; i < N; ++i) {
			pDst[i] *= pSrc1[i];
		}
		for (int i = 0; i < N; ++i) {
			pDst[i] += pSrc2[i] * cval2;
		}
	}
	EVAL_T prev[3*N];
	EVAL_T* pPrev;
	EVAL_T s[2*N];
	EVAL_T sin[N];
	for (int i = 0; i < N; ++i) {
		s[i] = y[i];
	}
	EVAL_T c[2*N];
	EVAL_T cos[N];
	for (int i = 0; i < N; ++i) {
		c[i] = x[i];
	}
	int scIdx = 0;
	for (int m = 1; m < order - 1; ++m) {
		int l = m;
		idst = l*l + l - m;
		pDst = &pCoefs[idst * N];
		cval = (EVAL_T)pConsts[idx++];
		for (int i = 0; i < N; ++i) {
			sin[i] = s[scIdx*N + i];
		}
		for (int i = 0; i < N; ++i) {
			pDst[i] = sin[i] * cval;
		}
		idst = l*l + l + m;
		pDst = &pCoefs[idst * N];
		for (int i = 0; i < N; ++i) {
			cos[i] = c[scIdx*N + i];
		}
		for (int i = 0; i < N; ++i) {
			pDst[i] = cos[i] * cval;
		}

		if (m + 1 < order) {
			// (m+1, -m), (m+1, m)
			l = m + 1;
			cval = (EVAL_T)pConsts[idx++];
			pPrev = &prev[1*N];
			for (int i = 0; i < N; ++i) {
				pPrev[i] = z[i] * cval;
			}
			idst = l*l + l - m;
			pDst = &pCoefs[idst * N];
			for (int i = 0; i < N; ++i) {
				pDst[i] = pPrev[i] * sin[i];
			}
			idst = l*l + l + m;
			pDst = &pCoefs[idst * N];
			for (int i = 0; i < N; ++i) {
				pDst[i] = pPrev[i] * cos[i];
			}
		}

		if (m + 2 < order) {
			// (m+2, -m), (m+2, m)
			l = m + 2;
			cval = (EVAL_T)pConsts[idx++];
			cval2 = (EVAL_T)pConsts[idx++];
			pPrev = &prev[2*N];
			for (int i = 0; i < N; ++i) {
				pPrev[i] = zz[i]*cval + cval2;
			}
			idst = l*l + l - m;
			pDst = &pCoefs[idst * N];
			for (int i = 0; i < N; ++i) {
				pDst[i] = pPrev[i] * sin[i];
			}
			idst = l*l + l + m;
			pDst = &pCoefs[idst * N];
			for (int i = 0; i < N; ++i) {
				pDst[i] = pPrev[i] * cos[i];
			}
		}

		if (m + 3 < order) {
			// (m+3, -m), (m+3, m)
			l = m + 3;
			cval = (EVAL_T)pConsts[idx++];
			cval2 = (EVAL_T)pConsts[idx++];
			pPrev = &prev[0*N];
			for (int i = 0; i < N; ++i) {
				pPrev[i] = zz[i]*cval;
			}
			for (int i = 0; i < N; ++i) {
				pPrev[i] += cval2;
			}
			for (int i = 0; i < N; ++i) {
				pPrev[i] *= z[i];
			}
			idst = l*l + l - m;
			pDst = &pCoefs[idst * N];
			for (int i = 0; i < N; ++i) {
				pDst[i] = pPrev[i] * sin[i];
			}
			idst = l*l + l + m;
			pDst = &pCoefs[idst * N];
			for (int i = 0; i < N; ++i) {
				pDst[i] = pPrev[i] * cos[i];
			}
		}

		const unsigned prevMask = 1 | (0 << 2) | (2 << 4) | (1 << 6) | (0 << 8) | (2 << 10) | (1 << 12);
		unsigned mask = prevMask | ((prevMask >> 2) << 14);
		unsigned maskCnt = 0;
		for (l = m + 4; l < order; ++l) {
			unsigned prevIdx = mask & 3;
			pPrev = &prev[prevIdx * N];
			EVAL_T* pPrev1 = &prev[((mask >> 2) & 3) * N];
			EVAL_T* pPrev2 = &prev[((mask >> 4) & 3) * N];
			cval = (EVAL_T)pConsts[idx++];
			cval2 = (EVAL_T)pConsts[idx++];
			for (int i = 0; i < N; ++i) {
				pPrev[i] = z[i] * cval;
			}
			for (int i = 0; i < N; ++i) {
				pPrev[i] *= pPrev1[i];
			}
			for (int i = 0; i < N; ++i) {
				pPrev[i] += pPrev2[i] * cval2;
			}
			idst = l*l + l - m;
			pDst = &pCoefs[idst * N];
			for (int i = 0; i < N; ++i) {
				pDst[i] = pPrev[i] * sin[i];
			}
			idst = l*l + l + m;
			pDst = &pCoefs[idst * N];
			for (int i = 0; i < N; ++i) {
				pDst[i] = pPrev[i] * cos[i];
			}
			if (order < 11) {
				mask >>= 4;
			} else {
				++maskCnt;
				if (maskCnt < 3) {
					mask >>= 4;
				} else {
					mask = prevMask;
					maskCnt = 0;
				}
			}
		}

		EVAL_T* pSinSrc = &s[scIdx * N];
		EVAL_T* pCosSrc = &c[scIdx * N];
		EVAL_T* pSinDst = &s[(scIdx^1) * N];
		EVAL_T* pCosDst = &c[(scIdx^1) * N];
		for (int i = 0; i < N; ++i) {
			pSinDst[i] = x[i]*pSinSrc[i] + y[i]*pCosSrc[i];
		}
		for (int i = 0; i < N; ++i) {
			pCosDst[i] = x[i]*pCosSrc[i] - y[i]*pSinSrc[i];
		}
		scIdx ^= 1;
	}

	if (order > 1) {
		cval = (EVAL_T)pConsts[idx];
		idst = calc_ary_idx(order - 1, -(order - 1));
		pDst = &pCoefs[idst * N];
		for (int i = 0; i < N; ++i) {
			sin[i] = s[scIdx*N + i];
		}
		for (int i = 0; i < N; ++i) {
			pDst[i] = sin[i] * cval;
		}
		idst = calc_ary_idx(order - 1, order - 1);
		pDst = &pCoefs[idst * N];
		for (int i = 0; i < N; ++i) {
			cos[i] = c[scIdx*N + i];
		}
		for (int i = 0; i < N; ++i) {
			pDst[i] = cos[i] * cval;
		}
	}
}

void calc_consts(int order, float* pConsts) {
	calc_consts_t(order, pConsts);
}

void calc_consts(int order, double* pConsts) {
	calc_consts_t(order, pConsts);
}

void eval(int order, float* pCoefs, float x, float y, float z, const float* pConsts) {
	eval_t<float, float, 1>(order, pCoefs, &x, &y, &z, pConsts);
}

void eval_ary4(int order, float* pCoefs, float x[4], float y[4], float z[4], const float* pConsts) {
	eval_t<float, float, 4>(order, pCoefs, x, y, z, pConsts);
}

void eval_ary8(int order, float* pCoefs, float x[8], float y[8], float z[8], const float* pConsts) {
	eval_t<float, float, 8>(order, pCoefs, x, y, z, pConsts);
}

void eval_sh3(float* pCoefs, float x, float y, float z, const float* pConsts) {
	float tc[calc_consts_num(3)];
	if (!pConsts) {
		calc_consts(3, tc);
		pConsts = tc;
	}
	eval_t<float, float, 1>(3, pCoefs, &x, &y, &z, pConsts);
}

void eval_sh3_ary4(float* pCoefs, float x[4], float y[4], float z[4], const float* pConsts) {
	float tc[calc_consts_num(3)];
	if (!pConsts) {
		calc_consts(3, tc);
		pConsts = tc;
	}
	eval_t<float, float, 4>(3, pCoefs, x, y, z, pConsts);
}

void eval_sh3_ary8(float* pCoefs, float x[8], float y[8], float z[8], const float* pConsts) {
	float tc[calc_consts_num(3)];
	if (!pConsts) {
		calc_consts(3, tc);
		pConsts = tc;
	}
	eval_t<float, float, 8>(3, pCoefs, x, y, z, pConsts);
}

} // nxSH


namespace nxDataUtil {

exAnimChan anim_chan_from_str(const char* pStr) {
	exAnimChan res = exAnimChan::UNKNOWN;
	static struct {
		const char* pName;
		exAnimChan id;
	} tbl[] = {
		{ "tx", exAnimChan::TX },
		{ "ty", exAnimChan::TY },
		{ "tz", exAnimChan::TZ },
		{ "rx", exAnimChan::RX },
		{ "ry", exAnimChan::RY },
		{ "rz", exAnimChan::RZ },
		{ "sx", exAnimChan::SX },
		{ "sy", exAnimChan::SY },
		{ "sz", exAnimChan::SZ },
	};
	if (pStr) {
		for (int i = 0; i < XD_ARY_LEN(tbl); ++i) {
			if (nxCore::str_eq(pStr, tbl[i].pName)) {
				res = tbl[i].id;
				break;
			}
		}
	}
	return res;

}
const char* anim_chan_to_str(exAnimChan chan) {
	const char* pName = "<unknown>";
	switch (chan) {
		case exAnimChan::TX: pName = "tx"; break;
		case exAnimChan::TY: pName = "ty"; break;
		case exAnimChan::TZ: pName = "tz"; break;
		case exAnimChan::RX: pName = "rx"; break;
		case exAnimChan::RY: pName = "ry"; break;
		case exAnimChan::RZ: pName = "rz"; break;
		case exAnimChan::SX: pName = "sx"; break;
		case exAnimChan::SY: pName = "sy"; break;
		case exAnimChan::SZ: pName = "sz"; break;
		default: break;
	}
	return pName;
}

exRotOrd rot_ord_from_str(const char* pStr) {
	exRotOrd rord = exRotOrd::XYZ;
	static struct {
		const char* pName;
		exRotOrd ord;
	} tbl[] = {
		{ "xyz", exRotOrd::XYZ },
		{ "xzy", exRotOrd::XZY },
		{ "yxz", exRotOrd::YXZ },
		{ "yzx", exRotOrd::YZX },
		{ "zxy", exRotOrd::ZXY },
		{ "zyx", exRotOrd::ZYX }
	};
	if (pStr) {
		for (int i = 0; i < XD_ARY_LEN(tbl); ++i) {
			if (nxCore::str_eq(pStr, tbl[i].pName)) {
				rord = tbl[i].ord;
				break;
			}
		}
	}
	return rord;
}

const char* rot_ord_to_str(exRotOrd rord) {
	const char* pStr = "xyz";
	switch (rord) {
		case exRotOrd::XYZ: pStr = "xyz"; break;
		case exRotOrd::XZY: pStr = "xzy"; break;
		case exRotOrd::YXZ: pStr = "yxz"; break;
		case exRotOrd::YZX: pStr = "yzx"; break;
		case exRotOrd::ZXY: pStr = "zxy"; break;
		case exRotOrd::ZYX: pStr = "zyx"; break;
		default: break;
	}
	return pStr;
}

exTransformOrd xform_ord_from_str(const char* pStr) {
	exTransformOrd xord = exTransformOrd::SRT;
	static struct {
		const char* pName;
		exTransformOrd ord;
	} tbl[] = {
		{ "srt", exTransformOrd::SRT },
		{ "str", exTransformOrd::STR },
		{ "rst", exTransformOrd::RST },
		{ "rts", exTransformOrd::RTS },
		{ "tsr", exTransformOrd::TSR },
		{ "trs", exTransformOrd::TRS }
	};
	if (pStr) {
		for (int i = 0; i < XD_ARY_LEN(tbl); ++i) {
			if (nxCore::str_eq(pStr, tbl[i].pName)) {
				xord = tbl[i].ord;
				break;
			}
		}
	}
	return xord;
}

const char* xform_ord_to_str(exTransformOrd xord) {
	const char* pStr = "srt";
	switch (xord) {
		case exTransformOrd::SRT: pStr = "srt"; break;
		case exTransformOrd::STR: pStr = "str"; break;
		case exTransformOrd::RST: pStr = "rst"; break;
		case exTransformOrd::RTS: pStr = "rts"; break;
		case exTransformOrd::TSR: pStr = "tsr"; break;
		case exTransformOrd::TRS: pStr = "trs"; break;
		default: break;
	}
	return pStr;
}

} // nxDataUtil

namespace nxData {

XD_NOINLINE sxData* load(const char* pPath) {
	size_t size = 0;
	sxData* pData = reinterpret_cast<sxData*>(nxCore::bin_load_impl(pPath, &size, true, true, XD_DAT_MEM_TAG));
	if (pData) {
		if (pData->mFileSize == size) {
			pData->mFilePathLen = (uint32_t)::strlen(pPath);
		} else {
			unload(pData);
			pData = nullptr;
		}
	}
	return pData;
}

XD_NOINLINE void unload(sxData* pData) {
	nxCore::bin_unload(pData);
}

static int pk_find_dict_idx(const uint32_t* pCnts, int n, uint32_t cnt) {
	const uint32_t* p = pCnts;
	uint32_t c = (uint32_t)n;
	while (c > 1) {
		uint32_t mid = c / 2;
		const uint32_t* pm = &p[mid];
		uint32_t ck = *pm;
		p = (cnt > ck) ? p : pm;
		c -= mid;
	}
	return (int)(p - pCnts) + ((p != pCnts) & (*p < cnt));
}

static uint8_t pk_get_bit_cnt(uint8_t x) {
	for (uint8_t i = 8; --i > 0;) {
		if (x & (1 << i)) return i + 1;
	}
	return 1;
}

static uint32_t pk_bit_cnt_to_bytes(uint32_t n) {
	return (n >> 3) + ((n & 7) != 0);
}

static uint32_t pk_mk_dict(uint8_t* pDict, uint8_t* pXlat, const uint8_t* pSrc, uint32_t srcSize) {
	uint32_t cnt[0x100];
	::memset(cnt, 0, sizeof(cnt));
	for (uint32_t i = 0; i < srcSize; ++i) {
		++cnt[pSrc[i]];
	}
	uint8_t idx[0x100];
	uint32_t idict = 0;
	for (uint32_t i = 0; i < 0x100; ++i) {
		if (cnt[i]) {
			idx[idict] = i;
			++idict;
		}
	}
	::memset(pDict, 0, 0x100);
	uint32_t dictSize = idict;
	uint32_t dictCnt[0x100];
	for (uint32_t i = 0; i < dictSize; ++i) {
		uint8_t id = idx[i];
		uint32_t c = cnt[id];
		if (i == 0) {
			pDict[i] = id;
			dictCnt[i] = c;
		} else {
			int idx = pk_find_dict_idx(dictCnt, i, c);
			if (idx == i - 1) {
				if (c > dictCnt[idx]) {
					pDict[i] = pDict[i - 1];
					dictCnt[i] = dictCnt[i - 1];
					pDict[i - 1] = id;
					dictCnt[i - 1] = c;
				} else {
					pDict[i] = id;
					dictCnt[i] = c;
				}
			} else {
				if (c > dictCnt[idx]) --idx;
				for (int j = i; --j >= idx + 1;) {
					pDict[j + 1] = pDict[j];
					dictCnt[j + 1] = dictCnt[j];
				}
				pDict[idx + 1] = id;
				dictCnt[idx + 1] = c;
			}
		}
	}
	::memset(pXlat, 0, 0x100);
	for (uint32_t i = 0; i < dictSize; ++i) {
		pXlat[pDict[i]] = i;
	}
	return dictSize;
}

static uint32_t pk_encode(uint8_t* pBitCnt, uint8_t* pBitCode, const uint8_t* pXlat, const uint8_t* pSrc, uint32_t srcSize) {
	uint32_t bitDst = 0;
	for (uint32_t i = 0; i < srcSize; ++i) {
		uint8_t code = pXlat[pSrc[i]];
		uint8_t nbits = pk_get_bit_cnt(code);
		uint8_t bitCode = nbits - 1;
		uint32_t cntIdx = i * 3;
		uint32_t byteIdx = cntIdx >> 3;
		uint32_t bitIdx = cntIdx & 7;
		pBitCnt[byteIdx] |= bitCode << bitIdx;
		pBitCnt[byteIdx + 1] |= bitCode >> (8 - bitIdx);

		uint8_t dstBits = nbits;
		if (nbits > 1) {
			code -= 1 << (nbits - 1);
			--dstBits;
		}

		byteIdx = bitDst >> 3;
		bitIdx = bitDst & 7;
		pBitCode[byteIdx] |= code << bitIdx;
		pBitCode[byteIdx + 1] |= code >> (8 - bitIdx);
		bitDst += dstBits;
	}
	return bitDst;
}

struct sxPkdWork {
	uint8_t mDict[0x100];
	uint8_t mXlat[0x100];
	uint32_t mSrcSize;
	uint32_t mDictSize;
	uint32_t mBitCntBytes;
	uint8_t* mpBitCnt;
	uint8_t* mpBitCode;
	uint32_t mBitCodeBits;
	uint32_t mBitCodeBytes;

	sxPkdWork() : mpBitCnt(nullptr), mpBitCode(nullptr) {}
	~sxPkdWork() { reset(); }

	void reset() {
		if (mpBitCnt) {
			nxCore::mem_free(mpBitCnt);
			mpBitCnt = nullptr;
		}
		if (mpBitCode) {
			nxCore::mem_free(mpBitCode);
			mpBitCode = nullptr;
		}
	}

	bool is_valid() const { return (mpBitCnt && mpBitCode); }

	bool encode(const uint8_t* pSrc, uint32_t srcSize) {
		bool res = false;
		mSrcSize = srcSize;
		mDictSize = pk_mk_dict(mDict, mXlat, pSrc, srcSize);
		mBitCntBytes = pk_bit_cnt_to_bytes(srcSize * 3);
		mpBitCnt = (uint8_t*)nxCore::mem_alloc(mBitCntBytes);
		if (mpBitCnt) {
			::memset(mpBitCnt, 0, mBitCntBytes);
			mpBitCode = (uint8_t*)nxCore::mem_alloc(srcSize);
			if (mpBitCode) {
				::memset(mpBitCode, 0, srcSize);
				mBitCodeBits = pk_encode(mpBitCnt, mpBitCode, mXlat, pSrc, srcSize);
				mBitCodeBytes = pk_bit_cnt_to_bytes(mBitCodeBits);
				res = true;
			} else {
				nxCore::mem_free(mpBitCnt);
				mpBitCnt = nullptr;
			}
		}
		return res;
	}

	uint32_t get_pkd_size() const {
		return is_valid() ? sizeof(sxPackedData) + mDictSize + mBitCntBytes + mBitCodeBytes : 0;
	}
};

static sxPackedData* pkd0(const sxPkdWork& wk) {
	sxPackedData* pPkd = nullptr;
	if (wk.is_valid()) {
		uint32_t size = wk.get_pkd_size();
		pPkd = (sxPackedData*)nxCore::mem_alloc(size, sxPackedData::SIG);
		if (pPkd) {
			pPkd->mSig = sxPackedData::SIG;
			pPkd->mAttr = 0 | ((wk.mDictSize - 1) << 8);
			pPkd->mPackSize = size;
			pPkd->mRawSize = wk.mSrcSize;
			uint8_t* pDict = (uint8_t*)(pPkd + 1);
			uint8_t* pCnt = pDict + wk.mDictSize;
			uint8_t* pCode = pCnt + wk.mBitCntBytes;
			::memcpy(pDict, wk.mDict, wk.mDictSize);
			::memcpy(pCnt, wk.mpBitCnt, wk.mBitCntBytes);
			::memcpy(pCode, wk.mpBitCode, wk.mBitCodeBytes);
		}
	}
	return pPkd;
}

static sxPackedData* pkd1(const sxPkdWork& wk, const sxPkdWork& wk2) {
	sxPackedData* pPkd = nullptr;
	if (wk.is_valid() && wk2.is_valid()) {
		uint32_t size = sizeof(sxPackedData) + 4 + wk.mDictSize + wk2.mDictSize + wk2.mBitCntBytes + wk2.mBitCodeBytes + wk.mBitCodeBytes;
		pPkd = (sxPackedData*)nxCore::mem_alloc(size, sxPackedData::SIG);
		if (pPkd) {
			pPkd->mSig = sxPackedData::SIG;
			pPkd->mAttr = 1 | ((wk.mDictSize - 1) << 8) | ((wk2.mDictSize - 1) << 16);
			pPkd->mPackSize = size;
			pPkd->mRawSize = wk.mSrcSize;
			uint32_t* pCntSize = (uint32_t*)(pPkd + 1);
			uint8_t* pDict = (uint8_t*)(pPkd + 1) + 4;
			uint8_t* pDict2 = pDict + wk.mDictSize;
			uint8_t* pCnt2 = pDict2 + wk2.mDictSize;
			uint8_t* pCode2 = pCnt2 + wk2.mBitCntBytes;
			uint8_t* pCode = pCode2 + wk2.mBitCodeBytes;
			*pCntSize = wk2.mBitCodeBytes;
			::memcpy(pDict, wk.mDict, wk.mDictSize);
			::memcpy(pDict2, wk2.mDict, wk2.mDictSize);
			::memcpy(pCnt2, wk2.mpBitCnt, wk2.mBitCntBytes);
			::memcpy(pCode2, wk2.mpBitCode, wk2.mBitCodeBytes);
			::memcpy(pCode, wk.mpBitCode, wk.mBitCodeBytes);
		}
	}
	return pPkd;
}

sxPackedData* pack(const uint8_t* pSrc, uint32_t srcSize, uint32_t mode) {
	sxPackedData* pPkd = nullptr;
	if (pSrc && srcSize > 0x10 && mode < 2) {
		sxPkdWork wk;
		wk.encode(pSrc, srcSize);
		uint32_t pkdSize = wk.get_pkd_size();
		if (pkdSize && pkdSize < srcSize) {
			if (mode == 1) {
				sxPkdWork wk2;
				wk2.encode(wk.mpBitCnt, wk.mBitCntBytes);
				uint32_t pkdSize2 = wk2.is_valid() ? wk2.get_pkd_size() + 4 + wk.mDictSize + wk.mBitCodeBytes : 0;
				if (pkdSize2 && pkdSize2 < pkdSize) {
					pPkd = pkd1(wk, wk2);
				} else {
					wk2.reset();
					pPkd = pkd0(wk);
				}
			} else {
				pPkd = pkd0(wk);
			}
		}
	}
	return pPkd;
}

static void pk_decode(uint8_t* pDst, uint32_t size, uint8_t* pDict, uint8_t* pBitCnt, uint8_t* pBitCodes) {
	uint32_t codeBitIdx = 0;
	for (uint32_t i = 0; i < size; ++i) {
		uint32_t cntBitIdx = i * 3;
		uint32_t byteIdx = cntBitIdx >> 3;
		uint32_t bitIdx = cntBitIdx & 7;
		uint8_t nbits = pBitCnt[byteIdx] >> bitIdx;
		nbits |= pBitCnt[byteIdx + 1] << (8 - bitIdx);
		nbits &= 7;
		nbits += 1;

		uint8_t codeBits = nbits;
		if (nbits > 1) {
			--codeBits;
		}
		byteIdx = codeBitIdx >> 3;
		bitIdx = codeBitIdx & 7;
		uint8_t code = pBitCodes[byteIdx] >> bitIdx;
		code |= pBitCodes[byteIdx + 1] << (8 - bitIdx);
		code &= (uint8_t)(((uint32_t)1 << codeBits) - 1);
		codeBitIdx += codeBits;
		if (nbits > 1) {
			code += 1 << (nbits - 1);
		}
		pDst[i] = pDict[code];
	}
}

uint8_t* unpack(sxPackedData* pPkd, uint32_t memTag, uint8_t* pDstMem, uint32_t dstMemSize) {
	uint8_t* pDst = nullptr;
	if (pPkd && pPkd->mSig == sxPackedData::SIG) {
		if (pDstMem && dstMemSize >= pPkd->mRawSize) {
			pDst = pDstMem;
		} else {
			pDst = (uint8_t*)nxCore::mem_alloc(pPkd->mRawSize, memTag);
		}
		if (pDst) {
			int mode = pPkd->get_mode();
			if (mode == 0) {
				uint32_t dictSize = ((pPkd->mAttr >> 8) & 0xFF) + 1;
				uint8_t* pDict = (uint8_t*)(pPkd + 1);
				uint8_t* pBitCnt = pDict + dictSize;
				uint8_t* pBitCodes = pBitCnt + pk_bit_cnt_to_bytes(pPkd->mRawSize * 3);
				pk_decode(pDst, pPkd->mRawSize, pDict, pBitCnt, pBitCodes);
			} else if (mode == 1) {
				uint32_t dictSize = ((pPkd->mAttr >> 8) & 0xFF) + 1;
				uint32_t dictSize2 = ((pPkd->mAttr >> 16) & 0xFF) + 1;
				uint32_t* pCode2Size = (uint32_t*)(pPkd + 1);
				uint8_t* pDict = (uint8_t*)(pCode2Size + 1);
				uint8_t* pDict2 = pDict + dictSize;
				uint8_t* pCnt2 = pDict2 + dictSize2;
				uint32_t cntSize = pk_bit_cnt_to_bytes(pPkd->mRawSize * 3);
				uint8_t* pCode2 = pCnt2 + pk_bit_cnt_to_bytes(cntSize * 3);
				uint8_t* pCode = pCode2 + *pCode2Size;
				uint8_t* pCnt = (uint8_t*)nxCore::mem_alloc(cntSize, XD_TMP_MEM_TAG);
				if (pCnt) {
					pk_decode(pCnt, cntSize, pDict2, pCnt2, pCode2);
					pk_decode(pDst, pPkd->mRawSize, pDict, pCnt, pCode);
					nxCore::mem_free(pCnt);
				} else {
					if (pDst != pDstMem) {
						nxCore::mem_free(pDst);
					}
					pDst = nullptr;
				}
			}
		}
	}
	return pDst;
}

} // nxData


static int find_str_hash_idx(const uint16_t* pHash, int n, uint16_t h) {
	const uint16_t* p = pHash;
	uint32_t cnt = (uint32_t)n;
	while (cnt > 1) {
		uint32_t mid = cnt / 2;
		const uint16_t* pm = &p[mid];
		uint16_t ck = *pm;
		p = (h < ck) ? p : pm;
		cnt -= mid;
	}
	return (int)(p - pHash) + ((p != pHash) & (*p > h));
}

bool sxStrList::is_sorted() const {
	uint8_t flags = ((uint8_t*)this + mSize)[-1];
	return (flags != 0) && ((flags & 1) != 0);
}

XD_NOINLINE int sxStrList::find_str(const char* pStr) const {
	if (!pStr) return -1;
	uint16_t h = nxCore::str_hash16(pStr);
	uint16_t* pHash = get_hash_top();
	if (is_sorted()) {
		int idx = find_str_hash_idx(pHash, mNum, h);
		if (h == pHash[idx]) {
			int nck = 1;
			for (int i = idx; --i >= 0;) {
				if (pHash[i] != h) break;
				--idx;
				++nck;
			}
			for (int i = 0; i < nck; ++i) {
				int strIdx = idx + i;
				if (nxCore::str_eq(get_str(strIdx), pStr)) {
					return strIdx;
				}
			}
		}
	} else {
		for (uint32_t i = 0; i < mNum; ++i) {
			if (h == pHash[i]) {
				if (nxCore::str_eq(get_str(i), pStr)) {
					return i;
				}
			}
		}
	}
	return -1;
}

XD_NOINLINE int sxStrList::find_str_any(const char** ppStrs, int n) const {
	if (!ppStrs || n < 1) return -1;
	int org;
	uint16_t hblk[8];
	int bsize = XD_ARY_LEN(hblk);
	int nblk = n / bsize;
	uint16_t* pHash = get_hash_top();
	for (int iblk = 0; iblk < nblk; ++iblk) {
		org = iblk * bsize;
		for (int j = 0; j < bsize; ++j) {
			hblk[j] = nxCore::str_hash16(ppStrs[org + j]);
		}
		for (uint32_t i = 0; i < mNum; ++i) {
			uint16_t htst = pHash[i];
			const char* pStr = get_str(i);
			for (int j = 0; j < bsize; ++j) {
				if (htst == hblk[j]) {
					if (nxCore::str_eq(pStr, ppStrs[org + j])) {
						return i;
					}
				}
			}
		}
	}
	org = nblk * bsize;
	for (int j = org; j < n; ++j) {
		hblk[j - org] = nxCore::str_hash16(ppStrs[j]);
	}
	for (uint32_t i = 0; i < mNum; ++i) {
		uint16_t htst = pHash[i];
		const char* pStr = get_str(i);
		for (int j = org; j < n; ++j) {
			if (htst == hblk[j - org]) {
				if (nxCore::str_eq(pStr, ppStrs[j])) {
					return i;
				}
			}
		}
	}
	return -1;
}


int sxVecList::get_elems(float* pDst, int idx, int num) const {
	int n = 0;
	if (ck_idx(idx)) {
		const float* pVal = get_ptr(idx);
		const float* pLim = reinterpret_cast<const float*>(XD_INCR_PTR(this, mSize));
		for (int i = 0; i < num; ++i) {
			if (pVal >= pLim) break;
			if (pDst) {
				pDst[i] = *pVal;
			}
			++pVal;
			++n;
		}
	}
	return n;
}

xt_float2 sxVecList::get_f2(int idx) {
	xt_float2 v;
	v.fill(0.0f);
	get_elems(v, idx, 2);
	return v;
}

xt_float3 sxVecList::get_f3(int idx) {
	xt_float3 v;
	v.fill(0.0f);
	get_elems(v, idx, 3);
	return v;
}

xt_float4 sxVecList::get_f4(int idx) {
	xt_float4 v;
	v.fill(0.0f);
	get_elems(v, idx, 4);
	return v;
}


int sxValuesData::Group::find_val_idx(const char* pName) const {
	int idx = -1;
	if (pName && is_valid()) {
		sxStrList* pStrLst = mpVals->get_str_list();
		if (pStrLst) {
			int nameId = pStrLst->find_str(pName);
			if (nameId >= 0) {
				const GrpInfo* pInfo = get_info();
				const ValInfo* pVal = pInfo->mVals;
				int nval = pInfo->mValNum;
				for (int i = 0; i < nval; ++i) {
					if (pVal[i].mNameId == nameId) {
						idx = i;
						break;
					}
				}
			}
		}
	}
	return idx;
}

int sxValuesData::Group::find_val_idx_any(const char** ppNames, int n) const {
	int idx = -1;
	if (ppNames && n > 0 && is_valid()) {
		sxStrList* pStrLst = mpVals->get_str_list();
		if (pStrLst) {
			for (int i = 0; i < n; ++i) {
				const char* pName = ppNames[i];
				int tst = find_val_idx(pName);
				if (ck_val_idx(tst)) {
					idx = tst;
					break;
				}
			}
		}
	}
	return idx;
}

int sxValuesData::Group::get_val_i(int idx) const {
	int res = 0;
	const ValInfo* pVal = get_val_info(idx);
	if (pVal) {
		float ftmp[1];
		eValType typ = pVal->get_type();
		switch (typ) {
			case eValType::INT:
				res = pVal->mValId.i;
				break;
			case eValType::FLOAT:
				res = (int)pVal->mValId.f;
				break;
			case eValType::VEC2:
			case eValType::VEC3:
			case eValType::VEC4:
				mpVals->get_vec_list()->get_elems(ftmp, pVal->mValId.i, 1);
				res = (int)ftmp[0];
				break;
		}
	}
	return res;
}

float sxValuesData::Group::get_val_f(int idx) const {
	float res = 0.0f;
	const ValInfo* pVal = get_val_info(idx);
	if (pVal) {
		eValType typ = pVal->get_type();
		switch (typ) {
			case eValType::INT:
				res = (float)pVal->mValId.i;
				break;
			case eValType::FLOAT:
				res = pVal->mValId.f;
				break;
			case eValType::VEC2:
			case eValType::VEC3:
			case eValType::VEC4:
				mpVals->get_vec_list()->get_elems(&res, pVal->mValId.i, 1);
				break;
		}
	}
	return res;
}

xt_float2 sxValuesData::Group::get_val_f2(int idx) const {
	xt_float2 res;
	res.fill(0.0f);
	const ValInfo* pVal = get_val_info(idx);
	if (pVal) {
		eValType typ = pVal->get_type();
		switch (typ) {
			case eValType::INT:
				res.fill((float)pVal->mValId.i);
				break;
			case eValType::FLOAT:
				res.fill(pVal->mValId.f);
				break;
			case eValType::VEC2:
			case eValType::VEC3:
			case eValType::VEC4:
				res = mpVals->get_vec_list()->get_f2(pVal->mValId.i);
				break;
		}
	}
	return res;
}

xt_float3 sxValuesData::Group::get_val_f3(int idx) const {
	xt_float3 res;
	res.fill(0.0f);
	const ValInfo* pVal = get_val_info(idx);
	if (pVal) {
		eValType typ = pVal->get_type();
		switch (typ) {
			case eValType::INT:
				res.fill((float)pVal->mValId.i);
				break;
			case eValType::FLOAT:
				res.fill(pVal->mValId.f);
				break;
			case eValType::VEC2:
			case eValType::VEC3:
			case eValType::VEC4:
				res = mpVals->get_vec_list()->get_f3(pVal->mValId.i);
				break;
		}
	}
	return res;
}

xt_float4 sxValuesData::Group::get_val_f4(int idx) const {
	xt_float4 res;
	res.fill(0.0f);
	const ValInfo* pVal = get_val_info(idx);
	if (pVal) {
		eValType typ = pVal->get_type();
		switch (typ) {
			case eValType::INT:
				res.fill((float)pVal->mValId.i);
				break;
			case eValType::FLOAT:
				res.fill(pVal->mValId.f);
				break;
			case eValType::VEC2:
			case eValType::VEC3:
			case eValType::VEC4:
				res = mpVals->get_vec_list()->get_f4(pVal->mValId.i);
				break;
		}
	}
	return res;
}

const char* sxValuesData::Group::get_val_s(int idx) const {
	const char* pStr = nullptr;
	const ValInfo* pVal = get_val_info(idx);
	if (pVal) {
		sxStrList* pStrLst = mpVals->get_str_list();
		if (pStrLst) {
			if (pVal->get_type() == eValType::STRING) {
				pStr = pStrLst->get_str(pVal->mValId.i);
			}
		}
	}
	return pStr;
}

float sxValuesData::Group::get_float(const char* pName, const float defVal) const {
	float f = defVal;
	int idx = find_val_idx(pName);
	if (idx >= 0) {
		f = get_val_f(idx);
	}
	return f;
}

float sxValuesData::Group::get_float_any(const char** ppNames, int n, const float defVal) const {
	float f = defVal;
	int idx = find_val_idx_any(ppNames, n);
	if (idx >= 0) {
		f = get_val_f(idx);
	}
	return f;
}

int sxValuesData::Group::get_int(const char* pName, const int defVal) const {
	int i = defVal;
	int idx = find_val_idx(pName);
	if (idx >= 0) {
		i = get_val_i(idx);
	}
	return i;
}

int sxValuesData::Group::get_int_any(const char** ppNames, int n, const int defVal) const {
	int i = defVal;
	int idx = find_val_idx_any(ppNames, n);
	if (idx >= 0) {
		i = get_val_i(idx);
	}
	return i;
}

cxVec sxValuesData::Group::get_vec(const char* pName, const cxVec& defVal) const {
	cxVec v = defVal;
	int idx = find_val_idx(pName);
	if (idx >= 0) {
		xt_float3 f3 = get_val_f3(idx);
		v.set(f3.x, f3.y, f3.z);
	}
	return v;
}

cxVec sxValuesData::Group::get_vec_any(const char** ppNames, int n, const cxVec& defVal) const {
	cxVec v = defVal;
	int idx = find_val_idx_any(ppNames, n);
	if (idx >= 0) {
		xt_float3 f3 = get_val_f3(idx);
		v.set(f3.x, f3.y, f3.z);
	}
	return v;
}

cxColor sxValuesData::Group::get_rgb(const char* pName, const cxColor& defVal) const {
	cxColor c = defVal;
	int idx = find_val_idx(pName);
	if (idx >= 0) {
		xt_float3 f3 = get_val_f3(idx);
		c.set(f3.x, f3.y, f3.z);
	}
	return c;
}

cxColor sxValuesData::Group::get_rgb_any(const char** ppNames, int n, const cxColor& defVal) const {
	cxColor c = defVal;
	int idx = find_val_idx_any(ppNames, n);
	if (idx >= 0) {
		xt_float3 f3 = get_val_f3(idx);
		c.set(f3.x, f3.y, f3.z);
	}
	return c;
}

const char* sxValuesData::Group::get_str(const char* pName, const char* pDefVal) const {
	const char* pStr = pDefVal;
	int idx = find_val_idx(pName);
	if (idx >= 0) {
		pStr = get_val_s(idx);
	}
	return pStr;
}

const char* sxValuesData::Group::get_str_any(const char** ppNames, int n, const char* pDefVal) const {
	const char* pStr = pDefVal;
	int idx = find_val_idx_any(ppNames, n);
	if (idx >= 0) {
		pStr = get_val_s(idx);
	}
	return pStr;
}


int sxValuesData::find_grp_idx(const char* pName, const char* pPath, int startIdx) const {
	int idx = -1;
	sxStrList* pStrLst = get_str_list();
	if (ck_grp_idx(startIdx) && pStrLst) {
		int ngrp = get_grp_num();
		int nameId = pName ? pStrLst->find_str(pName) : -1;
		if (nameId >= 0) {
			if (pPath) {
				int pathId = pStrLst->find_str(pPath);
				if (pathId >= 0) {
					for (int i = startIdx; i < ngrp; ++i) {
						GrpInfo* pGrp = get_grp_info(i);
						if (nameId == pGrp->mNameId && pathId == pGrp->mPathId) {
							idx = i;
							break;
						}
					}
				}
			} else {
				for (int i = startIdx; i < ngrp; ++i) {
					GrpInfo* pGrp = get_grp_info(i);
					if (nameId == pGrp->mNameId) {
						idx = i;
						break;
					}
				}
			}
		} else if (pPath) {
			int pathId = pStrLst->find_str(pPath);
			if (pathId >= 0) {
				int ngrp = get_grp_num();
				for (int i = startIdx; i < ngrp; ++i) {
					GrpInfo* pGrp = get_grp_info(i);
					if (pathId == pGrp->mPathId) {
						idx = i;
						break;
					}
				}
			}
		}
	}
	return idx;
}

sxValuesData::Group sxValuesData::get_grp(int idx) const {
	Group grp;
	if (ck_grp_idx(idx)) {
		grp.mpVals = this;
		grp.mGrpId = idx;
	} else {
		grp.mpVals = nullptr;
		grp.mGrpId = -1;
	}
	return grp;
}

/*static*/ const char* sxValuesData::get_val_type_str(eValType typ) {
	const char* pStr = "UNKNOWN";
	switch (typ) {
		case eValType::FLOAT:
			pStr = "FLOAT";
			break;
		case eValType::VEC2:
			pStr = "VEC2";
			break;
		case eValType::VEC3:
			pStr = "VEC3";
			break;
		case eValType::VEC4:
			pStr = "VEC4";
			break;
		case eValType::INT:
			pStr = "INT";
			break;
		case eValType::STRING:
			pStr = "STRING";
			break;
		default:
			break;
	}
	return pStr;
}


sxRigData::Node* sxRigData::get_node_ptr(int idx) const {
	Node* pNode = nullptr;
	Node* pTop = get_node_top();
	if (pTop) {
		if (ck_node_idx(idx)) {
			pNode = pTop + idx;
		}
	}
	return pNode;
}

const char* sxRigData::get_node_name(int idx) const {
	const char* pName = nullptr;
	if (ck_node_idx(idx)) {
		sxStrList* pStrLst = get_str_list();
		if (pStrLst) {
			pName = pStrLst->get_str(get_node_ptr(idx)->mNameId);
		}
	}
	return pName;
}

const char* sxRigData::get_node_path(int idx) const {
	const char* pPath = nullptr;
	if (ck_node_idx(idx)) {
		sxStrList* pStrLst = get_str_list();
		if (pStrLst) {
			pPath = pStrLst->get_str(get_node_ptr(idx)->mPathId);
		}
	}
	return pPath;
}

const char* sxRigData::get_node_type(int idx) const {
	const char* pType = nullptr;
	if (ck_node_idx(idx)) {
		sxStrList* pStrLst = get_str_list();
		if (pStrLst) {
			pType = pStrLst->get_str(get_node_ptr(idx)->mTypeId);
		}
	}
	return pType;
}

int sxRigData::find_node(const char* pName, const char* pPath) const {
	int idx = -1;
	sxStrList* pStrLst = get_str_list();
	if (pStrLst) {
		int n = mNodeNum;
		int nameId = pStrLst->find_str(pName);
		if (nameId >= 0) {
			if (pPath) {
				int pathId = pStrLst->find_str(pPath);
				if (pathId >= 0) {
					for (int i = 0; i < n; ++i) {
						Node* pNode = get_node_ptr(i);
						if (pNode->mNameId == nameId && pNode->mPathId == pathId) {
							idx = i;
							break;
						}
					}
				}
			} else {
				for (int i = 0; i < n; ++i) {
					Node* pNode = get_node_ptr(i);
					if (pNode->mNameId == nameId) {
						idx = i;
						break;
					}
				}
			}
		}
	}
	return idx;
}

cxMtx sxRigData::get_wmtx(int idx) const {
	cxMtx mtx;
	cxMtx* pMtx = get_wmtx_ptr(idx);
	if (pMtx) {
		mtx = *pMtx;
	} else {
		mtx.identity();
	}
	return mtx;
}

cxMtx* sxRigData::get_wmtx_ptr(int idx) const {
	cxMtx* pMtx = nullptr;
	if (ck_node_idx(idx) && mOffsWMtx) {
		cxMtx* pTop = reinterpret_cast<cxMtx*>(XD_INCR_PTR(this, mOffsWMtx));
		pMtx = pTop + idx;
	}
	return pMtx;
}

cxMtx sxRigData::get_imtx(int idx) const {
	cxMtx mtx;
	cxMtx* pMtx = get_imtx_ptr(idx);
	if (pMtx) {
		mtx = *pMtx;
	} else {
		pMtx = get_wmtx_ptr(idx);
		if (pMtx) {
			mtx = pMtx->get_inverted();
		} else {
			mtx.identity();
		}
	}
	return mtx;
}

cxMtx* sxRigData::get_imtx_ptr(int idx) const {
	cxMtx* pMtx = nullptr;
	if (ck_node_idx(idx) && mOffsIMtx) {
		cxMtx* pTop = reinterpret_cast<cxMtx*>(XD_INCR_PTR(this, mOffsIMtx));
		pMtx = pTop + idx;
	}
	return pMtx;
}

cxMtx sxRigData::get_lmtx(int idx) const {
	cxMtx mtx;
	cxMtx* pMtx = get_lmtx_ptr(idx);
	if (pMtx) {
		mtx = *pMtx;
	} else {
		mtx = calc_lmtx(idx);
	}
	return mtx;
}

cxMtx* sxRigData::get_lmtx_ptr(int idx) const {
	cxMtx* pMtx = nullptr;
	if (ck_node_idx(idx) && mOffsLMtx) {
		cxMtx* pTop = reinterpret_cast<cxMtx*>(XD_INCR_PTR(this, mOffsLMtx));
		pMtx = pTop + idx;
	}
	return pMtx;
}

cxMtx sxRigData::calc_lmtx(int idx) const {
	Node* pNode = get_node_ptr(idx);
	if (pNode->mParentIdx < 0) {
		return get_wmtx(idx);
	}
	cxMtx m = get_wmtx(idx);
	m.mul(get_imtx(pNode->mParentIdx));
	return m;
}

cxVec sxRigData::calc_parent_offs(int idx) const {
	Node* pNode = get_node_ptr(idx);
	if (pNode->mParentIdx < 0) {
		return get_wmtx_ptr(idx)->get_translation();
	}
	return get_wmtx_ptr(idx)->get_translation() - get_wmtx_ptr(pNode->mParentIdx)->get_translation();
}

cxVec sxRigData::get_lpos(int idx) const {
	cxVec pos(0.0f);
	if (ck_node_idx(idx) && mOffsLPos) {
		cxVec* pTop = reinterpret_cast<cxVec*>(XD_INCR_PTR(this, mOffsLPos));
		pos = pTop[idx];
	}
	return pos;
}

cxVec sxRigData::get_lscl(int idx) const {
	cxVec scl(1.0f);
	if (ck_node_idx(idx) && mOffsLScl) {
		cxVec* pTop = reinterpret_cast<cxVec*>(XD_INCR_PTR(this, mOffsLScl));
		scl = pTop[idx];
	}
	return scl;
}

cxVec sxRigData::get_lrot(int idx, bool inRadians) const {
	cxVec rot(0.0f);
	if (ck_node_idx(idx) && mOffsLRot) {
		cxVec* pTop = reinterpret_cast<cxVec*>(XD_INCR_PTR(this, mOffsLRot));
		rot = pTop[idx];
		if (inRadians) {
			rot.scl(XD_DEG2RAD(1.0f));
		}
	}
	return rot;
}

cxQuat sxRigData::calc_lquat(int idx) const {
	cxQuat q;
	cxVec r = get_lrot(idx, true);
	q.set_rot(r.x, r.y, r.z, get_rot_order(idx));
	return q;
}

cxMtx sxRigData::calc_wmtx(int idx, const cxMtx* pMtxLocal, cxMtx* pParentWMtx) const {
	cxMtx mtx;
	mtx.identity();
	cxMtx parentMtx;
	parentMtx.identity();
	if (pMtxLocal && ck_node_idx(idx)) {
		Node* pNode = get_node_ptr(idx);
		mtx = pMtxLocal[pNode->mSelfIdx];
		pNode = get_node_ptr(pNode->mParentIdx);
		while (pNode && !pNode->is_hrc_top()) {
			parentMtx.mul(pMtxLocal[pNode->mSelfIdx]);
			pNode = get_node_ptr(pNode->mParentIdx);
		}
		if (pNode) {
			parentMtx.mul(pMtxLocal[pNode->mSelfIdx]);
		}
		mtx.mul(parentMtx);
	}
	if (pParentWMtx) {
		*pParentWMtx = parentMtx;
	}
	return mtx;
}

void sxRigData::LimbChain::set(LimbInfo* pInfo) {
	if (pInfo) {
		mTopCtrl = pInfo->mTopCtrl;
		mEndCtrl = pInfo->mEndCtrl;
		mExtCtrl = pInfo->mExtCtrl;
		mTop = pInfo->mTop;
		mRot = pInfo->mRot;
		mEnd = pInfo->mEnd;
		mExt = pInfo->mExt;
		mAxis = (exAxis)pInfo->mAxis;
		mUp = (exAxis)pInfo->mUp;
		mExtCompensate = pInfo->get_ext_comp_flg();
	} else {
		mTopCtrl = -1;
		mEndCtrl = -1;
		mExtCtrl = -1;
		mTop = -1;
		mRot = -1;
		mEnd = -1;
		mExt = -1;
		mAxis = exAxis::MINUS_Y;
		mUp = exAxis::PLUS_Z;
		mExtCompensate = false;
	}
}

static xt_float2 ik_cos_law(float a, float b, float c) {
	xt_float2 ang;
	if (c < a + b) {
		float aa = nxCalc::sq(a);
		float bb = nxCalc::sq(b);
		float cc = nxCalc::sq(c);
		float c0 = (aa - bb + cc) / (2.0f*a*c);
		float c1 = (aa + bb - cc) / (2.0f*a*b);
		c0 = nxCalc::clamp(c0, -1.0f, 1.0f);
		c1 = nxCalc::clamp(c1, -1.0f, 1.0f);
		ang[0] = -::acosf(c0);
		ang[1] = XD_PI - ::acosf(c1);
	} else {
		ang.fill(0.0f);
	}
	return ang;
}

struct sxLimbIKWork {
	cxMtx mRootW;
	cxMtx mParentW;
	cxMtx mTopW;
	cxMtx mRotW;
	cxMtx mEndW;
	cxMtx mExtW;
	cxMtx mTopL;
	cxMtx mRotL;
	cxMtx mEndL;
	cxMtx mExtL;
	cxVec mRotOffs;
	const sxRigData* mpRig;
	const sxRigData::LimbChain* mpChain;
	float mDistTopRot;
	float mDistRotEnd;
	float mDistTopEnd;
	exAxis mAxis;
	exAxis mUp;

	void calc_world();
	void calc_local(bool fixPos = true);
};

void sxLimbIKWork::calc_world() {
	xt_float2 ang = ik_cos_law(mDistTopRot, mDistRotEnd, mDistTopEnd);
	cxVec axis = nxVec::get_axis(mAxis);
	cxVec up = nxVec::get_axis(mUp);
	cxMtx mtxZY = nxMtx::orient_zy(axis, up, false);
	cxVec side = mtxZY.calc_vec(nxVec::get_axis(exAxis::PLUS_X));
	cxVec axisX = mTopW.calc_vec(side);
	cxVec axisZ = (mEndW.get_translation() - mTopW.get_translation()).get_normalized();
	cxMtx mtxZX = nxMtx::orient_zx(axisZ, axisX, false);
	cxMtx mtxIK = mtxZY.get_transposed() * mtxZX;
	cxMtx rotMtx = nxMtx::from_axis_angle(side, ang[0]);
	cxVec topPos = mTopW.get_translation();
	mTopW = rotMtx * mtxIK;
	mTopW.set_translation(topPos);
	cxVec rotPos = mTopW.calc_pnt(mRotOffs);
	rotMtx = nxMtx::from_axis_angle(side, ang[1]);
	mRotW = rotMtx * mTopW;
	mRotW.set_translation(rotPos);
}

void sxLimbIKWork::calc_local(bool fixPos) {
	mTopL = mTopW * mParentW.get_inverted();
	mRotL = mRotW * mTopW.get_inverted();
	mEndL = mEndW * mRotW.get_inverted();
	if (fixPos && mpChain) {
		mTopL.set_translation(mpRig->get_lpos(mpChain->mTop));
		mRotL.set_translation(mpRig->get_lpos(mpChain->mRot));
		mEndL.set_translation(mpRig->get_lpos(mpChain->mEnd));
	}
}

void sxRigData::calc_limb_local(LimbChain::Solution* pSolution, const LimbChain& chain, cxMtx* pMtx, LimbChain::AdjustFunc* pAdjFunc) const {
	if (!pMtx) return;
	if (!pSolution) return;
	if (!ck_node_idx(chain.mTopCtrl)) return;
	if (!ck_node_idx(chain.mEndCtrl)) return;
	if (!ck_node_idx(chain.mTop)) return;
	if (!ck_node_idx(chain.mRot)) return;
	if (!ck_node_idx(chain.mEnd)) return;
	int parentIdx = get_parent_idx(chain.mTopCtrl);
	if (!ck_node_idx(parentIdx)) return;
	bool isExt = ck_node_idx(chain.mExtCtrl);
	int rootIdx = rootIdx = isExt ? get_parent_idx(chain.mExtCtrl) : get_parent_idx(chain.mEndCtrl);
	if (!ck_node_idx(rootIdx)) return;

	sxLimbIKWork ik;
	ik.mpRig = this;
	ik.mpChain = &chain;
	ik.mTopW = calc_wmtx(chain.mTopCtrl, pMtx, &ik.mParentW);
	ik.mRootW = calc_wmtx(rootIdx, pMtx);
	if (isExt) {
		ik.mExtW = pMtx[chain.mExtCtrl] * ik.mRootW;
		ik.mEndW = pMtx[chain.mExtCtrl] * pMtx[chain.mEndCtrl].get_sr() * ik.mRootW;
	} else {
		ik.mExtW.identity();
		ik.mEndW = pMtx[chain.mEndCtrl] * ik.mRootW;
	}

	cxVec effPos = isExt ? ik.mExtW.get_translation() : ik.mEndW.get_translation();
	if (pAdjFunc) {
		effPos = (*pAdjFunc)(*this, chain, effPos);
	}

	cxVec endPos;
	if (isExt) {
		ik.mExtW.set_translation(effPos);
		cxVec extOffs = ck_node_idx(chain.mExt) ? get_lpos(chain.mExt).neg_val() : get_lpos(chain.mExtCtrl);
		extOffs = pMtx[chain.mExtCtrl].calc_vec(extOffs);
		endPos = (pMtx[chain.mEndCtrl] * ik.mRootW).calc_vec(extOffs) + ik.mExtW.get_translation();
	} else {
		endPos = effPos;
	}
	ik.mEndW.set_translation(endPos);

	ik.mRotOffs = get_lpos(chain.mRot);
	ik.mDistTopRot = calc_parent_dist(chain.mRot);
	ik.mDistRotEnd = calc_parent_dist(chain.mEnd);
	ik.mDistTopEnd = nxVec::dist(endPos, ik.mTopW.get_translation());
	ik.mAxis = chain.mAxis;
	ik.mUp = chain.mUp;
	ik.calc_world();
	ik.calc_local();

	if (isExt) {
		if (chain.mExtCompensate) {
			ik.mExtL = ik.mExtW * ik.mEndW.get_inverted();
		} else {
			ik.mExtL = pMtx[chain.mExtCtrl] * ik.mRootW * ik.mEndW.get_inverted();
		}
		ik.mExtL.set_translation(get_lpos(chain.mExt));
	} else {
		ik.mExtL.identity();
	}

	pSolution->mTop = ik.mTopL;
	pSolution->mRot = ik.mRotL;
	pSolution->mEnd = ik.mEndL;
	pSolution->mExt = ik.mExtL;
}

void sxRigData::copy_limb_solution(cxMtx* pDstMtx, const LimbChain& chain, const LimbChain::Solution& solution) {
	if (!pDstMtx) return;
	if (ck_node_idx(chain.mTop)) {
		pDstMtx[chain.mTop] = solution.mTop;
	}
	if (ck_node_idx(chain.mRot)) {
		pDstMtx[chain.mRot] = solution.mRot;
	}
	if (ck_node_idx(chain.mEnd)) {
		pDstMtx[chain.mEnd] = solution.mEnd;
	}
	if (ck_node_idx(chain.mExt)) {
		pDstMtx[chain.mExt] = solution.mExt;
	}
}

bool sxRigData::has_info_list() const {
	bool res = false;
	ptrdiff_t offs = (uint8_t*)&mOffsInfo - (uint8_t*)this;
	if (mHeadSize > (uint32_t)offs) {
		if (mOffsInfo != 0) {
			Info* pInfo = reinterpret_cast<Info*>(XD_INCR_PTR(this, mOffsInfo));
			if (pInfo->get_kind() == eInfoKind::LIST) {
				res = true;
			}
		}
	}
	return res;
}

sxRigData::Info* sxRigData::find_info(eInfoKind kind) const {
	Info* pInfo = nullptr;
	if (has_info_list()) {
		Info* pList = reinterpret_cast<Info*>(XD_INCR_PTR(this, mOffsInfo));
		Info* pEntry = reinterpret_cast<Info*>(XD_INCR_PTR(this, pList->mOffs));
		int n = pList->mNum;
		for (int i = 0; i < n; ++i) {
			if (pEntry->get_kind() == kind) {
				pInfo = pEntry;
				break;
			}
			++pEntry;
		}
	}
	return pInfo;
}

sxRigData::LimbInfo* sxRigData::find_limb(sxRigData::eLimbType type, int idx) const {
	sxRigData::LimbInfo* pLimb = nullptr;
	Info* pInfo = find_info(eInfoKind::LIMBS);
	if (pInfo && pInfo->mOffs) {
		int n = pInfo->mNum;
		LimbInfo* pCk = (LimbInfo*)XD_INCR_PTR(this, pInfo->mOffs);
		for (int i = 0; i < n; ++i) {
			if (pCk[i].get_type() == type && pCk[i].get_idx() == idx) {
				pLimb = &pCk[i];
				break;
			}
		}
	}
	return pLimb;
}

int sxRigData::get_expr_num() const {
	int n = 0;
	Info* pInfo = find_info(eInfoKind::EXPRS);
	if (pInfo) {
		n = pInfo->mNum;
	}
	return n;
}

int sxRigData::get_expr_stack_size() const {
	int stksize = 0;
	Info* pInfo = find_info(eInfoKind::EXPRS);
	if (pInfo) {
		stksize = (int)(pInfo->mParam & 0xFF);
		if (0 == stksize) {
			stksize = 16;
		}
	}
	return stksize;
}

sxRigData::ExprInfo* sxRigData::get_expr_info(int idx) const {
	ExprInfo* pExprInfo = nullptr;
	Info* pInfo = find_info(eInfoKind::EXPRS);
	if (pInfo && pInfo->mOffs) {
		if ((uint32_t)idx < pInfo->mNum) {
			uint32_t* pOffs = reinterpret_cast<uint32_t*>(XD_INCR_PTR(this, pInfo->mOffs));
			uint32_t offs = pOffs[idx];
			if (offs) {
				pExprInfo = reinterpret_cast<sxRigData::ExprInfo*>(XD_INCR_PTR(this, offs));
			}
		}
	}
	return pExprInfo;
}

sxCompiledExpression* sxRigData::get_expr_code(int idx) const {
	sxCompiledExpression* pCode = nullptr;
	ExprInfo* pExprInfo = get_expr_info(idx);
	if (pExprInfo) {
		pCode = pExprInfo->get_code();
	}
	return pCode;
}


int sxGeometryData::get_vtx_idx_size() const {
	int size = 0;
	if (mPntNum <= (1 << 8)) {
		size = 1;
	} else if (mPntNum <= (1 << 16)) {
		size = 2;
	} else {
		size = 3;
	}
	return size;
}

sxGeometryData::Polygon sxGeometryData::get_pol(int idx) const {
	Polygon pol;
	if (ck_pol_idx(idx)) {
		pol.mPolId = idx;
		pol.mpGeom = this;
	} else {
		pol.mPolId = -1;
		pol.mpGeom = nullptr;
	}
	return pol;
}

int32_t* sxGeometryData::get_skin_node_name_ids() const {
	int32_t* pIds = nullptr;
	cxSphere* pSphTop = get_skin_sph_top();
	if (pSphTop) {
		pIds = reinterpret_cast<int32_t*>(&pSphTop[mSkinNodeNum]);
	}
	return pIds;
}

const char* sxGeometryData::get_skin_node_name(int idx) const {
	const char* pName = nullptr;
	if (ck_skin_idx(idx)) {
		sxStrList* pStrLst = get_str_list();
		cxSphere* pSphTop = get_skin_sph_top();
		if (pStrLst && pSphTop) {
			int32_t* pNameIds = reinterpret_cast<int32_t*>(&pSphTop[mSkinNodeNum]);
			pName = pStrLst->get_str(pNameIds[idx]);
		}
	}
	return pName;
}

int sxGeometryData::find_skin_node(const char* pName) const {
	int idx = -1;
	sxStrList* pStrLst = get_str_list();
	if (has_skin_nodes() && pStrLst) {
		int nameId = pStrLst->find_str(pName);
		if (nameId >= 0) {
			cxSphere* pSphTop = get_skin_sph_top();
			int32_t* pNameIds = reinterpret_cast<int32_t*>(&pSphTop[mSkinNodeNum]);
			int n = get_skin_nodes_num();
			for (int i = 0; i < n; ++i) {
				if (pNameIds[i] == nameId) {
					idx = i;
					break;
				}
			}
		}
	}
	return idx;
}

uint32_t sxGeometryData::get_attr_info_offs(eAttrClass cls) const {
	uint32_t offs = 0;
	switch (cls) {
		case eAttrClass::GLOBAL:
			offs = mGlbAttrOffs;
			break;
		case eAttrClass::POINT:
			offs = mPntAttrOffs;
			break;
		case eAttrClass::POLYGON:
			offs = mPolAttrOffs;
			break;
	}
	return offs;
}

uint32_t sxGeometryData::get_attr_info_num(eAttrClass cls) const {
	uint32_t num = 0;
	switch (cls) {
		case eAttrClass::GLOBAL:
			num = mGlbAttrNum;
			break;
		case eAttrClass::POINT:
			num = mPntAttrNum;
			break;
		case eAttrClass::POLYGON:
			num = mPolAttrNum;
			break;
	}
	return num;
}

uint32_t sxGeometryData::get_attr_item_num(eAttrClass cls) const {
	uint32_t num = 0;
	switch (cls) {
		case eAttrClass::GLOBAL:
			num = 1;
			break;
		case eAttrClass::POINT:
			num = mPntNum;
			break;
		case eAttrClass::POLYGON:
			num = mPolNum;
			break;
	}
	return num;
}

int sxGeometryData::find_attr(const char* pName, eAttrClass cls) const {
	int attrId = -1;
	uint32_t offs = get_attr_info_offs(cls);
	uint32_t num = get_attr_info_num(cls);
	if (offs && num) {
		sxStrList* pStrLst = get_str_list();
		if (pStrLst) {
			int nameId = pStrLst->find_str(pName);
			if (nameId >= 0) {
				AttrInfo* pAttr = reinterpret_cast<AttrInfo*>(XD_INCR_PTR(this, offs));
				for (int i = 0; i < (int)num; ++i) {
					if (pAttr[i].mNameId == nameId) {
						attrId = i;
						break;
					}
				}
			}
		}
	}
	return attrId;
}

sxGeometryData::AttrInfo* sxGeometryData::get_attr_info(int attrIdx, eAttrClass cls) const {
	AttrInfo* pAttr = nullptr;
	uint32_t offs = get_attr_info_offs(cls);
	uint32_t num = get_attr_info_num(cls);
	if (offs && num && (uint32_t)attrIdx < num) {
		pAttr = &reinterpret_cast<AttrInfo*>(XD_INCR_PTR(this, offs))[attrIdx];
	}
	return pAttr;
}

const char* sxGeometryData::get_attr_val_s(int attrIdx, eAttrClass cls, int itemIdx) const {
	const char* pStr = nullptr;
	uint32_t itemNum = get_attr_item_num(cls);
	if ((uint32_t)itemIdx < itemNum) {
		AttrInfo* pInfo = get_attr_info(attrIdx, cls);
		if (pInfo && pInfo->is_string()) {
			uint32_t* pTop = reinterpret_cast<uint32_t*>(XD_INCR_PTR(this, pInfo->mDataOffs));
			int strId = pTop[itemIdx*pInfo->mElemNum];
			pStr = get_str(strId);
		}
	}
	return pStr;
}

float* sxGeometryData::get_attr_data_f(int attrIdx, eAttrClass cls, int itemIdx, int minElem) const {
	float* pData = nullptr;
	uint32_t itemNum = get_attr_item_num(cls);
	if ((uint32_t)itemIdx < itemNum) {
		AttrInfo* pInfo = get_attr_info(attrIdx, cls);
		if (pInfo && pInfo->is_float() && pInfo->mElemNum >= minElem) {
			float* pTop = reinterpret_cast<float*>(XD_INCR_PTR(this, pInfo->mDataOffs));
			pData = &pTop[itemIdx*pInfo->mElemNum];
		}
	}
	return pData;
}

float sxGeometryData::get_pnt_attr_val_f(int attrIdx, int pntIdx) const {
	float res = 0.0f;
	float* pData = get_attr_data_f(attrIdx, eAttrClass::POINT, pntIdx);
	if (pData) {
		res = *pData;
	}
	return res;
}

xt_float3 sxGeometryData::get_pnt_attr_val_f3(int attrIdx, int pntIdx) const {
	xt_float3 res;
	float* pData = get_attr_data_f(attrIdx, eAttrClass::POINT, pntIdx, 3);
	if (pData) {
		res.set(pData[0], pData[1], pData[2]);
	} else {
		res.fill(0.0f);
	}
	return res;
}

cxVec sxGeometryData::get_pnt_normal(int pntIdx) const {
	cxVec nrm(0.0f, 1.0f, 0.0f);
	int attrIdx = find_pnt_attr("N");
	if (attrIdx >= 0) {
		float* pData = get_attr_data_f(attrIdx, eAttrClass::POINT, pntIdx, 3);
		if (pData) {
			nrm.from_mem(pData);
		}
	}
	return nrm;
}

cxVec sxGeometryData::get_pnt_tangent(int pntIdx) const {
	cxVec tng(1.0f, 0.0f, 0.0f);
	int attrIdx = find_pnt_attr("tangentu");
	if (attrIdx >= 0) {
		float* pData = get_attr_data_f(attrIdx, eAttrClass::POINT, pntIdx, 3);
		if (pData) {
			tng.from_mem(pData);
		}
	}
	return tng;
}

cxVec sxGeometryData::get_pnt_bitangent(int pntIdx) const {
	cxVec btg(0.0f, 0.0f, 1.0f);
	int attrIdx = find_pnt_attr("tangentv");
	if (attrIdx >= 0) {
		float* pData = get_attr_data_f(attrIdx, eAttrClass::POINT, pntIdx, 3);
		if (pData) {
			btg.from_mem(pData);
		} else {
			btg = calc_pnt_bitangent(pntIdx);
		}
	} else {
		btg = calc_pnt_bitangent(pntIdx);
	}
	return btg;
}

cxVec sxGeometryData::calc_pnt_bitangent(int pntIdx) const {
	cxVec nrm = get_pnt_normal(pntIdx);
	cxVec tng = get_pnt_tangent(pntIdx);
	cxVec btg = nxVec::cross(tng, nrm);
	btg.normalize();
	return btg;
}

cxColor sxGeometryData::get_pnt_color(int pntIdx, bool useAlpha) const {
	cxColor clr(1.0f, 1.0f, 1.0f);
	int attrIdx = find_pnt_attr("Cd");
	if (attrIdx >= 0) {
		float* pData = get_attr_data_f(attrIdx, eAttrClass::POINT, pntIdx, 3);
		if (pData) {
			clr.set(pData[0], pData[1], pData[2]);
		}
	}
	if (useAlpha) {
		attrIdx = find_pnt_attr("Alpha");
		if (attrIdx >= 0) {
			float* pData = get_attr_data_f(attrIdx, eAttrClass::POINT, pntIdx, 1);
			if (pData) {
				clr.a = *pData;
			}
		}
	}
	return clr;
}

xt_texcoord sxGeometryData::get_pnt_texcoord(int pntIdx) const {
	xt_texcoord tex;
	tex.set(0.0f, 0.0f);
	int attrIdx = find_pnt_attr("uv");
	if (attrIdx >= 0) {
		float* pData = get_attr_data_f(attrIdx, eAttrClass::POINT, pntIdx, 2);
		if (pData) {
			tex.set(pData[0], pData[1]);
		}
	}
	return tex;
}

xt_texcoord sxGeometryData::get_pnt_texcoord2(int pntIdx) const {
	xt_texcoord tex;
	tex.set(0.0f, 0.0f);
	int attrIdx = find_pnt_attr("uv2");
	if (attrIdx < 0) {
		attrIdx = find_pnt_attr("uv");
	}
	if (attrIdx >= 0) {
		float* pData = get_attr_data_f(attrIdx, eAttrClass::POINT, pntIdx, 2);
		if (pData) {
			tex.set(pData[0], pData[1]);
		}
	}
	return tex;
}

int sxGeometryData::get_pnt_wgt_num(int pntIdx) const {
	int n = 0;
	if (has_skin() && ck_pnt_idx(pntIdx)) {
		uint32_t* pSkinTbl = reinterpret_cast<uint32_t*>(XD_INCR_PTR(this, mSkinOffs));
		uint8_t* pWgtNum = reinterpret_cast<uint8_t*>(&pSkinTbl[mPntNum]);
		n = pWgtNum[pntIdx];
	}
	return n;
}

int sxGeometryData::get_pnt_skin_jnt(int pntIdx, int wgtIdx) const {
	int idx = -1;
	if (has_skin() && ck_pnt_idx(pntIdx)) {
		uint32_t* pSkinTbl = reinterpret_cast<uint32_t*>(XD_INCR_PTR(this, mSkinOffs));
		uint8_t* pWgtNum = reinterpret_cast<uint8_t*>(&pSkinTbl[mPntNum]);
		int nwgt = pWgtNum[pntIdx];
		if ((uint32_t)wgtIdx < (uint32_t)nwgt) {
			float* pWgt = reinterpret_cast<float*>(XD_INCR_PTR(this, pSkinTbl[pntIdx]));
			uint8_t* pIdx = reinterpret_cast<uint8_t*>(&pWgt[nwgt]);
			int nnodes = get_skin_nodes_num();
			if (nnodes <= (1 << 8)) {
				idx = pIdx[wgtIdx];
			} else {
				idx = pIdx[wgtIdx*2];
				idx |= pIdx[wgtIdx*2 + 1] << 8;
			}
		}
	}
	return idx;
}

float sxGeometryData::get_pnt_skin_wgt(int pntIdx, int wgtIdx) const {
	float w = 0.0f;
	if (has_skin() && ck_pnt_idx(pntIdx)) {
		uint32_t* pSkinTbl = reinterpret_cast<uint32_t*>(XD_INCR_PTR(this, mSkinOffs));
		uint8_t* pWgtNum = reinterpret_cast<uint8_t*>(&pSkinTbl[mPntNum]);
		int nwgt = pWgtNum[pntIdx];
		if ((uint32_t)wgtIdx < (uint32_t)nwgt) {
			float* pWgt = reinterpret_cast<float*>(XD_INCR_PTR(this, pSkinTbl[pntIdx]));
			w = pWgt[wgtIdx];
		}
	}
	return w;
}

int sxGeometryData::find_mtl_grp_idx(const char* pName, const char* pPath) const {
	int idx = -1;
	sxStrList* pStrLst = get_str_list();
	int nmtl = (int)mMtlNum;
	if (nmtl && mMtlOffs && pStrLst) {
		int nameId = pStrLst->find_str(pName);
		if (nameId >= 0) {
			uint32_t* pOffs = reinterpret_cast<uint32_t*>(XD_INCR_PTR(this, mMtlOffs));
			if (pPath) {
				int pathId = pStrLst->find_str(pPath);
				if (pathId >= 0) {
					for (int i = 0; i < nmtl; ++i) {
						if (pOffs[i]) {
							GrpInfo* pInfo = reinterpret_cast<GrpInfo*>(XD_INCR_PTR(this, pOffs[i]));
							if (pInfo->mNameId == nameId && pInfo->mPathId == pathId) {
								idx = i;
								break;
							}
						}
					}
				}
			} else {
				for (int i = 0; i < nmtl; ++i) {
					if (pOffs[i]) {
						GrpInfo* pInfo = reinterpret_cast<GrpInfo*>(XD_INCR_PTR(this, pOffs[i]));
						if (pInfo->mNameId == nameId) {
							idx = i;
							break;
						}
					}
				}
			}
		}
	}
	return idx;
}

sxGeometryData::GrpInfo* sxGeometryData::get_mtl_info(int idx) const {
	GrpInfo* pInfo = nullptr;
	if (ck_mtl_idx(idx) && mMtlOffs) {
		uint32_t offs = reinterpret_cast<uint32_t*>(XD_INCR_PTR(this, mMtlOffs))[idx];
		if (offs) {
			pInfo = reinterpret_cast<GrpInfo*>(XD_INCR_PTR(this, offs));
		}
	}
	return pInfo;
}

sxGeometryData::Group sxGeometryData::get_mtl_grp(int idx) const {
	Group grp;
	GrpInfo* pInfo = get_mtl_info(idx);
	if (pInfo) {
		grp.mpGeom = this;
		grp.mpInfo = pInfo;
	} else {
		grp.mpGeom = nullptr;
		grp.mpInfo = nullptr;
	}
	return grp;
}

const char* sxGeometryData::get_mtl_name(int idx) const {
	const char* pName = nullptr;
	GrpInfo* pInfo = get_mtl_info(idx);
	if (pInfo) {
		pName = get_str(pInfo->mNameId);
	}
	return pName;
}

const char* sxGeometryData::get_mtl_path(int idx) const {
	const char* pPath = nullptr;
	GrpInfo* pInfo = get_mtl_info(idx);
	if (pInfo) {
		pPath = get_str(pInfo->mPathId);
	}
	return pPath;
}

sxGeometryData::GrpInfo* sxGeometryData::get_pnt_grp_info(int idx) const {
	GrpInfo* pInfo = nullptr;
	if (ck_pnt_grp_idx(idx) && mPntGrpOffs) {
		uint32_t offs = reinterpret_cast<uint32_t*>(XD_INCR_PTR(this, mPntGrpOffs))[idx];
		if (offs) {
			pInfo = reinterpret_cast<GrpInfo*>(XD_INCR_PTR(this, offs));
		}
	}
	return pInfo;
}

sxGeometryData::Group sxGeometryData::get_pnt_grp(int idx) const {
	Group grp;
	GrpInfo* pInfo = get_pnt_grp_info(idx);
	if (pInfo) {
		grp.mpGeom = this;
		grp.mpInfo = pInfo;
	} else {
		grp.mpGeom = nullptr;
		grp.mpInfo = nullptr;
	}
	return grp;
}

sxGeometryData::GrpInfo* sxGeometryData::get_pol_grp_info(int idx) const {
	GrpInfo* pInfo = nullptr;
	if (ck_pol_grp_idx(idx) && mPolGrpOffs) {
		uint32_t offs = reinterpret_cast<uint32_t*>(XD_INCR_PTR(this, mPolGrpOffs))[idx];
		if (offs) {
			pInfo = reinterpret_cast<GrpInfo*>(XD_INCR_PTR(this, offs));
		}
	}
	return pInfo;
}

sxGeometryData::Group sxGeometryData::get_pol_grp(int idx) const {
	Group grp;
	GrpInfo* pInfo = get_pol_grp_info(idx);
	if (pInfo) {
		grp.mpGeom = this;
		grp.mpInfo = pInfo;
	} else {
		grp.mpGeom = nullptr;
		grp.mpInfo = nullptr;
	}
	return grp;
}

void sxGeometryData::hit_query_nobvh(const cxLineSeg& seg, HitFunc& fun) const {
	cxVec hitPos;
	cxVec hitNrm;
	int npol = get_pol_num();
	for (int i = 0; i < npol; ++i) {
		Polygon pol = get_pol(i);
		bool hitFlg = pol.intersect(seg, &hitPos, &hitNrm);
		if (hitFlg) {
			float hitDist = nxVec::dist(seg.get_pos0(), hitPos);
			bool contFlg = fun(pol, hitPos, hitNrm, hitDist);
			if (!contFlg) {
				break;
			}
		}
	}
}

struct sxBVHWork {
	const sxGeometryData* mpGeo;
	cxAABB mQryBBox;
	cxLineSeg mQrySeg;
	sxGeometryData::HitFunc* mpHitFunc;
	sxGeometryData::RangeFunc* mpRangeFunc;
	bool mStopFlg;

	sxBVHWork() : mpGeo(nullptr), mpHitFunc(nullptr), mpRangeFunc(nullptr) {}
};

static void BVH_hit_sub(sxBVHWork& wk, int nodeId) {
	if (wk.mStopFlg) return;
	sxGeometryData::BVH::Node* pNode = wk.mpGeo->get_BVH_node(nodeId);
	if (pNode->mBBox.overlaps(wk.mQryBBox) && pNode->mBBox.seg_ck(wk.mQrySeg)) {
		if (pNode->is_leaf()) {
			cxVec hitPos;
			cxVec hitNrm;
			sxGeometryData::Polygon pol = wk.mpGeo->get_pol(pNode->get_pol_id());
			bool hitFlg = pol.intersect(wk.mQrySeg, &hitPos, &hitNrm);
			if (hitFlg) {
				float hitDist = nxVec::dist(wk.mQrySeg.get_pos0(), hitPos);
				bool contFlg = (*wk.mpHitFunc)(pol, hitPos, hitNrm, hitDist);
				if (!contFlg) {
					wk.mStopFlg = true;
				}
			}
		} else {
			BVH_hit_sub(wk, pNode->mLeft);
			BVH_hit_sub(wk, pNode->mRight);
		}
	}
}

void sxGeometryData::hit_query(const cxLineSeg& seg, HitFunc& fun) const {
	BVH* pBVH = get_BVH();
	if (pBVH) {
		sxBVHWork wk;
		wk.mpGeo = this;
		wk.mQrySeg = seg;
		wk.mQryBBox.from_seg(seg);
		wk.mpHitFunc = &fun;
		wk.mStopFlg = false;
		BVH_hit_sub(wk, 0);
	} else {
		hit_query_nobvh(seg, fun);
	}
}

void sxGeometryData::range_query_nobvh(const cxAABB& box, RangeFunc& fun) const {
	int npol = get_pol_num();
	for (int i = 0; i < npol; ++i) {
		Polygon pol = get_pol(i);
		bool rangeFlg = pol.calc_bbox().overlaps(box);
		if (rangeFlg) {
			bool contFlg = fun(pol);
			if (!contFlg) {
				break;
			}
		}
	}
}

static void BVH_range_sub(sxBVHWork& wk, int nodeId) {
	if (wk.mStopFlg) return;
	sxGeometryData::BVH::Node* pNode = wk.mpGeo->get_BVH_node(nodeId);
	if (wk.mQryBBox.overlaps(pNode->mBBox)) {
		if (pNode->is_leaf()) {
			sxGeometryData::Polygon pol = wk.mpGeo->get_pol(pNode->get_pol_id());
			bool contFlg = (*wk.mpRangeFunc)(pol);
			if (!contFlg) {
				wk.mStopFlg = true;
			}
		} else {
			BVH_range_sub(wk, pNode->mLeft);
			BVH_range_sub(wk, pNode->mRight);
		}
	}
}

void sxGeometryData::range_query(const cxAABB& box, RangeFunc& fun) const {
	BVH* pBVH = get_BVH();
	if (pBVH) {
		sxBVHWork wk;
		wk.mpGeo = this;
		wk.mQryBBox = box;
		wk.mpRangeFunc = &fun;
		wk.mStopFlg = false;
		BVH_range_sub(wk, 0);
	} else {
		range_query_nobvh(box, fun);
	}
}

cxAABB sxGeometryData::calc_world_bbox(cxMtx* pMtxW, int* pIdxMap) const {
	cxAABB bbox = mBBox;
	if (pMtxW) {
		if (has_skin()) {
			cxSphere* pSph = get_skin_sph_top();
			if (pSph) {
				int n = mSkinNodeNum;
				for (int i = 0; i < n; ++i) {
					int idx = i;
					if (pIdxMap) {
						idx = pIdxMap[i];
					}
					cxSphere sph = pSph[i];
					cxVec spos = pMtxW[idx].calc_pnt(sph.get_center());
					cxVec rvec(sph.get_radius());
					cxAABB sbb(spos - rvec, spos + rvec);
					if (i == 0) {
						bbox = sbb;
					} else {
						bbox.merge(sbb);
					}
				}
			}
		} else {
			bbox.transform(*pMtxW);
		}
	}
	return bbox;
}

void sxGeometryData::calc_tangents(cxVec* pTng, bool flip, const char* pAttrName) const {
	if (!pTng) return;
	int npnt = get_pnt_num();
	::memset(pTng, 0, npnt * sizeof(cxVec));
	if (!is_all_tris()) return;
	int nrmAttrIdx = find_pnt_attr("N");
	if (nrmAttrIdx < 0) return;
	int texAttrIdx = find_pnt_attr(pAttrName ? pAttrName : "uv");
	if (texAttrIdx < 0) return;
	int ntri = get_pol_num();
	cxVec triPts[3];
	xt_texcoord triUVs[3];
	for (int i = 0; i < ntri; ++i) {
		Polygon tri = get_pol(i);
		for (int j = 0; j < 3; ++j) {
			int pid = tri.get_vtx_pnt_id(j);
			triPts[j] = get_pnt(pid);
			float* pUVData = get_attr_data_f(texAttrIdx, eAttrClass::POINT, pid, 2);
			if (pUVData) {
				triUVs[j].set(pUVData[0], pUVData[1]);
			} else {
				triUVs[j].set(0.0f, 0.0f);
			}
			cxVec dp1 = triPts[1] - triPts[0];
			cxVec dp2 = triPts[2] - triPts[0];
			xt_texcoord dt1;
			dt1.set(triUVs[1].u - triUVs[0].u, triUVs[1].v - triUVs[0].v);
			xt_texcoord dt2;
			dt2.set(triUVs[2].u - triUVs[0].u, triUVs[2].v - triUVs[0].v);
			float d = nxCalc::rcp0(dt1.u*dt2.v - dt1.v*dt2.u);
			cxVec tu = (dp1*dt2.v - dp2*dt1.v) * d;
			pTng[pid] = tu;
		}
	}
	for (int i = 0; i < npnt; ++i) {
		cxVec nrm;
		float* pData = get_attr_data_f(nrmAttrIdx, eAttrClass::POINT, i, 3);
		if (pData) {
			nrm.from_mem(pData);
		} else {
			nrm.zero();
		}
		float d = nrm.dot(pTng[i]);
		nrm.scl(d);
		if (flip) {
			pTng[i] = nrm - pTng[i];
		} else {
			pTng[i].sub(nrm);
		}
		pTng[i].normalize();
	}
}

cxVec* sxGeometryData::calc_tangents(bool flip, const char* pAttrName) const {
	int npnt = get_pnt_num();
	cxVec* pTng = (cxVec*)nxCore::mem_alloc(npnt * sizeof(cxVec), XD_FOURCC('T', 'N', 'G', 'U'));
	calc_tangents(pTng, flip, pAttrName);
	return pTng;
}

void* sxGeometryData::alloc_triangulation_wk() const {
	void* pWk = nullptr;
	int n = mMaxVtxPerPol;
	if (n > 4) {
		pWk = nxCore::mem_alloc(n * sizeof(int) * 2, XD_FOURCC('T', 'R', 'I', 'W'));
	}
	return pWk;
}

uint8_t* sxGeometryData::Polygon::get_vtx_lst() const {
	uint8_t* pLst = nullptr;
	if (is_valid()) {
		if (mpGeom->is_same_pol_size()) {
			uint32_t offs = mpGeom->mPolOffs;
			if (!mpGeom->is_same_pol_mtl()) {
				offs += mpGeom->get_pol_num() * get_mtl_id_size();
			}
			int vlstSize = mpGeom->get_vtx_idx_size() * mpGeom->mMaxVtxPerPol;
			offs += vlstSize*mPolId;
			pLst = reinterpret_cast<uint8_t*>(XD_INCR_PTR(mpGeom, offs));
		} else {
			uint32_t offs = reinterpret_cast<uint32_t*>(XD_INCR_PTR(mpGeom, mpGeom->mPolOffs))[mPolId];
			pLst = reinterpret_cast<uint8_t*>(XD_INCR_PTR(mpGeom, offs));
		}
	}
	return pLst;
}

int sxGeometryData::Polygon::get_vtx_pnt_id(int vtxIdx) const {
	int pntIdx = -1;
	if (ck_vtx_idx(vtxIdx)) {
		uint8_t* pIdx = get_vtx_lst();
		int idxSize = mpGeom->get_vtx_idx_size();
		pIdx += idxSize * vtxIdx;
		switch (idxSize) {
			case 1:
				pntIdx = *pIdx;
				break;
			case 2:
				pntIdx = pIdx[0] | (pIdx[1] << 8);
				break;
			case 3:
				pntIdx = pIdx[0] | (pIdx[1] << 8) | (pIdx[2] << 16);
				break;
		}
	}
	return pntIdx;
}

int sxGeometryData::Polygon::get_vtx_num() const {
	int nvtx = 0;
	if (is_valid()) {
		if (mpGeom->is_same_pol_size()) {
			nvtx = mpGeom->mMaxVtxPerPol;
		} else {
			int npol = mpGeom->get_pol_num();
			uint8_t* pNum = reinterpret_cast<uint8_t*>(XD_INCR_PTR(mpGeom, mpGeom->mPolOffs));
			pNum += npol * 4;
			if (!mpGeom->is_same_pol_mtl()) {
				pNum += npol * get_mtl_id_size();
			}
			if (mpGeom->mMaxVtxPerPol < (1 << 8)) {
				nvtx = pNum[mPolId];
			} else {
				pNum += mPolId * 2;
				nvtx = pNum[0] | (pNum[1] << 8);
			}
		}
	}
	return nvtx;
}

int sxGeometryData::Polygon::get_mtl_id() const {
	int mtlId = -1;
	if (mpGeom->get_mtl_num() > 0 && is_valid()) {
		if (mpGeom->is_same_pol_mtl()) {
			mtlId = 0;
		} else {
			uint8_t* pId = reinterpret_cast<uint8_t*>(XD_INCR_PTR(mpGeom, mpGeom->mPolOffs));
			if (!mpGeom->is_same_pol_size()) {
				pId += mpGeom->get_pol_num() * 4;
			}
			switch (get_mtl_id_size()) {
				case 1:
					mtlId = reinterpret_cast<int8_t*>(pId)[mPolId];
					break;
				case 2:
				default:
					mtlId = reinterpret_cast<int16_t*>(pId)[mPolId];
					break;
			}
		}
	}
	return mtlId;
}

cxVec sxGeometryData::Polygon::calc_centroid() const {
	int nvtx = get_vtx_num();
	cxVec c(0.0f);
	for (int i = 0; i < nvtx; ++i) {
		c += get_vtx_pos(i);
	}
	c.scl(nxCalc::rcp0((float)nvtx));
	return c;
}

cxAABB sxGeometryData::Polygon::calc_bbox() const {
	cxAABB bbox;
	bbox.init();
	int nvtx = get_vtx_num();
	for (int i = 0; i < nvtx; ++i) {
		bbox.add_pnt(get_vtx_pos(i));
	}
	return bbox;
}

cxVec sxGeometryData::Polygon::calc_normal_cw() const {
	cxVec nrm;
	nrm.zero();
	cxVec* pPnt = mpGeom->get_pnt_top();
	int nvtx = get_vtx_num();
	for (int i = 0; i < nvtx; ++i) {
		int j = i - 1;
		if (j < 0) j = nvtx - 1;
		int pntI = get_vtx_pnt_id(i);
		int pntJ = get_vtx_pnt_id(j);
		nxGeom::update_nrm_newell(&nrm, &pPnt[pntI], &pPnt[pntJ]);
	}
	nrm.normalize();
	return nrm;
}

cxVec sxGeometryData::Polygon::calc_normal_ccw() const {
	cxVec nrm;
	nrm.zero();
	cxVec* pPnt = mpGeom->get_pnt_top();
	int nvtx = get_vtx_num();
	for (int i = 0; i < nvtx; ++i) {
		int j = i + 1;
		if (j >= nvtx) j = 0;
		int pntI = get_vtx_pnt_id(i);
		int pntJ = get_vtx_pnt_id(j);
		nxGeom::update_nrm_newell(&nrm, &pPnt[pntI], &pPnt[pntJ]);
	}
	nrm.normalize();
	return nrm;
}

bool sxGeometryData::Polygon::is_planar(float eps) const {
	int nvtx = get_vtx_num();
	if (nvtx < 4) return true;
	cxVec c = calc_centroid();
	cxVec n = calc_normal_cw();
	cxPlane pln;
	pln.calc(c, n);
	for (int i = 0; i < nvtx; ++i) {
		if (pln.dist(get_vtx_pos(i)) > eps) return false;
	}
	return true;
}

bool sxGeometryData::Polygon::intersect(const cxLineSeg& seg, cxVec* pHitPos, cxVec* pHitNrm) const {
	int i;
	bool res = false;
	int nvtx = get_vtx_num();
	bool planarFlg = false;
	int cvxMsk = 0;
	int hitTriIdx = 0;
	cxVec qvtx[4];
	cxVec sp0 = seg.get_pos0();
	cxVec sp1 = seg.get_pos1();
	switch (nvtx) {
		case 3:
			res = nxGeom::seg_tri_intersect_cw(sp0, sp1, get_vtx_pos(0), get_vtx_pos(1), get_vtx_pos(2), pHitPos, pHitNrm);
			break;
		case 4:
			for (i = 0; i < 4; ++i) {
				qvtx[i] = get_vtx_pos(i);
			}
			if (mpGeom->all_quads_planar_convex()) {
				planarFlg = true;
				cvxMsk = 3;
			} else {
				planarFlg = is_planar();
				cvxMsk = nxGeom::quad_convex_ck(qvtx[0], qvtx[1], qvtx[2], qvtx[3]);
			}
			if (planarFlg && cvxMsk == 3) {
				res = nxGeom::seg_quad_intersect_cw(sp0, sp1, qvtx[0], qvtx[1], qvtx[2], qvtx[3], pHitPos, pHitNrm);
			}
			break;
	}
	return res;
}

bool sxGeometryData::Polygon::contains_xz(const cxVec& pos) const {
	if (!is_valid()) return false;
	int cnt = 0;
	float rx0 = pos.x;
	float rz0 = pos.z;
	float rx1 = mpGeom->mBBox.get_max_pos().x + 1.0e-5f;
	float rz1 = rz0;
	int nvtx = get_vtx_num();
	for (int i = 0; i < nvtx; ++i) {
		int j = i + 1;
		if (j >= nvtx) j = 0;
		cxVec p0 = get_vtx_pos(i);
		cxVec p1 = get_vtx_pos(j);
		float ex0 = p0.x;
		float ez0 = p0.z;
		float ex1 = p1.x;
		float ez1 = p1.z;
		if (nxGeom::seg_seg_overlap_2d(rx0, rz0, rx1, rz1, ex0, ez0, ex1, ez1)) {
			++cnt;
		}
	}
	return !!(cnt & 1);
}

int sxGeometryData::Polygon::triangulate(int* pTris, void* pWk) const {
	if (!is_valid()) return 0;
	if (!pTris) return 0;
	int nvtx = get_vtx_num();
	if (nvtx < 3) return 0;
	if (nvtx == 3) {
		for (int i = 0; i < 3; ++i) {
			pTris[i] = get_vtx_pnt_id(i);
		}
		return 1;
	}
	struct Lnk {
		int prev;
		int next;
	} lnk[4];
	Lnk* pLnk = lnk;
	if (nvtx > XD_ARY_LEN(lnk)) {
		if (pWk) {
			pLnk = (Lnk*)pWk;
		} else {
			pLnk = (Lnk*)nxCore::mem_alloc(nvtx * sizeof(Lnk));
		}
	}
	for (int i = 0; i < nvtx; ++i) {
		pLnk[i].prev = i > 0 ? i - 1 : nvtx - 1;
		pLnk[i].next = i < nvtx - 1 ? i + 1 : 0;
	}
	cxVec nrm = calc_normal_cw();
	int vcnt = nvtx;
	int tptr = 0;
	int idx = 0;
	while (vcnt > 3) {
		int prev = pLnk[idx].prev;
		int next = pLnk[idx].next;
		cxVec v0 = get_vtx_pos(prev);
		cxVec v1 = get_vtx_pos(idx);
		cxVec v2 = get_vtx_pos(next);
		cxVec tnrm = nxGeom::tri_normal_cw(v0, v1, v2);
		bool flg = tnrm.dot(nrm) >= 0.0f;
		if (flg) {
			next = pLnk[next].next;
			do {
				cxVec v = get_vtx_pos(next);
				if (nxGeom::pnt_in_tri(v, v0, v1, v2)) {
					flg = false;
					break;
				}
				next = pLnk[next].next;
			} while (next != prev);
		}
		if (flg) {
			pTris[tptr++] = pLnk[idx].prev;
			pTris[tptr++] = idx;
			pTris[tptr++] = pLnk[idx].next;
			pLnk[pLnk[idx].prev].next = pLnk[idx].next;
			pLnk[pLnk[idx].next].prev = pLnk[idx].prev;
			--vcnt;
			idx = pLnk[idx].prev;
		} else {
			idx = pLnk[idx].next;
		}
	}
	pTris[tptr++] = pLnk[idx].prev;
	pTris[tptr++] = idx;
	pTris[tptr++] = pLnk[idx].next;
	if (pLnk != lnk && !pWk) {
		nxCore::mem_free(pLnk);
		pLnk = nullptr;
	}
	return nvtx - 2;
}

int sxGeometryData::Polygon::tri_count() const {
	if (!is_valid()) return 0;
	int nvtx = get_vtx_num();
	if (nvtx < 3) return 0;
	return nvtx - 2;
}


void* sxGeometryData::Group::get_idx_top() const {
	void* pIdxTop = is_valid() ? mpInfo + 1 : nullptr;
	if (pIdxTop) {
		int nskn = mpInfo->mSkinNodeNum;
		if (nskn) {
			if (mpGeom->has_skin_spheres()) {
				pIdxTop = XD_INCR_PTR(pIdxTop, sizeof(cxSphere)*nskn); /* skip spheres */
			}
			pIdxTop = XD_INCR_PTR(pIdxTop, sizeof(uint16_t)*nskn); /* skip node list */
		}
	}
	return pIdxTop;
}

int sxGeometryData::Group::get_idx_elem_size() const {
	int idxSpan = mpInfo->mMaxIdx - mpInfo->mMinIdx;
	int res = 0;
	if (idxSpan < (1 << 8)) {
		res = 1;
	} else if (idxSpan < (1 << 16)) {
		res = 2;
	} else {
		res = 3;
	}
	return res;
}

int sxGeometryData::Group::get_rel_idx(int at) const {
	int idx = -1;
	if (ck_idx(at)) {
		void* pIdxTop = get_idx_top();
		int elemSize = get_idx_elem_size();
		if (elemSize == 1) {
			idx = reinterpret_cast<uint8_t*>(pIdxTop)[at];
		} else if (elemSize == 2) {
			idx = reinterpret_cast<uint16_t*>(pIdxTop)[at];
		} else {
			uint8_t* pIdx = reinterpret_cast<uint8_t*>(XD_INCR_PTR(pIdxTop, at * 3));
			idx = pIdx[0] | (pIdx[1] << 8) | (pIdx[2] << 16);
		}
	}
	return idx;
}

int sxGeometryData::Group::get_idx(int at) const {
	int idx = -1;
	if (ck_idx(at)) {
		idx = get_rel_idx(at) + mpInfo->mMinIdx;
	}
	return idx;
}

bool sxGeometryData::Group::contains(int idx) const {
	bool res = false;
	if (is_valid() && idx >= mpInfo->mMinIdx && idx <= mpInfo->mMaxIdx) {
		int bit = idx - mpInfo->mMinIdx;
		void* pIdxTop = get_idx_top();
		int idxElemSize = get_idx_elem_size();
		uint8_t* pBits = reinterpret_cast<uint8_t*>(pIdxTop) + mpInfo->mIdxNum*idxElemSize;
		res = !!(pBits[bit >> 3] & (1 << (bit & 7)));
	}
	return res;
}

cxSphere* sxGeometryData::Group::get_skin_spheres() const {
	cxSphere* pSph = nullptr;
	if (is_valid()) {
		int nskn = mpInfo->mSkinNodeNum;
		if (nskn) {
			if (mpGeom->has_skin_spheres()) {
				pSph = (cxSphere*)(mpInfo + 1);
			}
		}
	}
	return pSph;
}

uint16_t* sxGeometryData::Group::get_skin_ids() const {
	uint16_t* pLst = nullptr;
	if (is_valid()) {
		int nskn = mpInfo->mSkinNodeNum;
		if (nskn) {
			if (mpGeom->has_skin_spheres()) {
				pLst = (uint16_t*)XD_INCR_PTR(mpInfo + 1, sizeof(cxSphere)*nskn);
			} else {
				pLst = (uint16_t*)(mpInfo + 1);
			}
		}
	}
	return pLst;
}

void sxGeometryData::DisplayList::create(const sxGeometryData& geo, const char* pBatGrpPrefix, bool triangulate) {
	int nmtl = geo.get_mtl_num();
	int nbat = nmtl > 0 ? nmtl : 1;
	int batGrpCount = 0;
	int* pBatGrpIdx = nullptr;
	int* pPolTriLst = nullptr;
	void* pPolTriWk = nullptr;
	if (geo.mMaxVtxPerPol > 3) {
		pPolTriLst = (int*)nxCore::mem_alloc((geo.mMaxVtxPerPol - 2) * 3 * sizeof(int), XD_TMP_MEM_TAG);
		pPolTriWk = geo.alloc_triangulation_wk();
	}
	if (pBatGrpPrefix) {
		for (int i = 0; i < geo.get_pol_grp_num(); ++i) {
			Group polGrp = geo.get_pol_grp(i);
			if (polGrp.is_valid()) {
				const char* pPolGrpName = polGrp.get_name();
				if (nxCore::str_starts_with(pPolGrpName, pBatGrpPrefix)) {
					++batGrpCount;
				}
			}
		}
		if (batGrpCount > 0) {
			nbat = batGrpCount;
			pBatGrpIdx = reinterpret_cast<int*>(nxCore::mem_alloc(batGrpCount * sizeof(int), XD_TMP_MEM_TAG));
			batGrpCount = 0;
			if (pBatGrpIdx) {
				for (int i = 0; i < geo.get_pol_grp_num(); ++i) {
					Group polGrp = geo.get_pol_grp(i);
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
	}
	int ntri = 0;
	int nidx16 = 0;
	int nidx32 = 0;
	for (int i = 0; i < nbat; ++i) {
		int npol = 0;
		if (batGrpCount > 0) {
			int batGrpIdx = pBatGrpIdx[i];
			Group batGrp = geo.get_pol_grp(batGrpIdx);
			npol = batGrp.get_idx_num();
		} else {
			if (nmtl > 0) {
				Group mtlGrp = geo.get_mtl_grp(i);
				npol = mtlGrp.get_idx_num();
			} else {
				npol = geo.get_pol_num();
			}
		}
		int32_t minIdx = 0;
		int32_t maxIdx = 0;
		int batIdxCnt = 0;
		for (int j = 0; j < npol; ++j) {
			int polIdx = 0;
			if (batGrpCount > 0) {
				int batGrpIdx = pBatGrpIdx[i];
				Group batGrp = geo.get_pol_grp(batGrpIdx);
				polIdx = batGrp.get_idx(j);
			} else {
				if (nmtl > 0) {
					polIdx = geo.get_mtl_grp(i).get_idx(j);
				} else {
					polIdx = j;
				}
			}
			Polygon pol = geo.get_pol(polIdx);
			if (pol.is_tri() || (triangulate && pol.tri_count() > 0)) {
				for (int k = 0; k < pol.get_vtx_num(); ++k) {
					int32_t vtxId = pol.get_vtx_pnt_id(k);
					if (batIdxCnt > 0) {
						minIdx = nxCalc::min(minIdx, vtxId);
						maxIdx = nxCalc::max(maxIdx, vtxId);
					} else {
						minIdx = vtxId;
						maxIdx = vtxId;
					}
				}
				batIdxCnt += pol.tri_count() * 3;
				ntri += pol.tri_count();
			}
		}
		if ((maxIdx - minIdx) < (1 << 16)) {
			nidx16 += batIdxCnt;
		} else {
			nidx32 += batIdxCnt;
		}
	}
	size_t size = XD_ALIGN(sizeof(Data), 0x10);
	uint32_t offsBat = (uint32_t)size;
	size += XD_ALIGN(nbat * sizeof(Batch), 0x10);
	uint32_t offsIdx32 = nidx32 > 0 ? (uint32_t)size : 0;
	size += XD_ALIGN(nidx32 * sizeof(uint32_t), 0x10);
	uint32_t offsIdx16 = nidx16 > 0 ? (uint32_t)size : 0;
	size += XD_ALIGN(nidx16 * sizeof(uint16_t), 0x10);
	Data* pData = (Data*)nxCore::mem_alloc(size, XD_FOURCC('G', 'D', 'L', 'D'));
	if (!pData) {
		return;
	}
	::memset(pData, 0, size);
	pData->mOffsBat = offsBat;
	pData->mOffsIdx32 = offsIdx32;
	pData->mOffsIdx16 = offsIdx16;
	pData->mIdx16Num = nidx16;
	pData->mIdx32Num = nidx32;
	pData->mMtlNum = nmtl;
	pData->mBatNum = nbat;
	pData->mTriNum = ntri;
	mpGeo = &geo;
	mpData = pData;
	nidx16 = 0;
	nidx32 = 0;
	for (int i = 0; i < nbat; ++i) {
		Batch* pBat = get_bat(i);
		int npol = 0;
		if (pBat) {
			if (batGrpCount > 0) {
				int batGrpIdx = pBatGrpIdx[i];
				Group batGrp = geo.get_pol_grp(batGrpIdx);
				pBat->mMtlId = geo.get_pol(batGrp.get_idx(0)).get_mtl_id();
				if (geo.has_skin()) {
					pBat->mMaxWgtNum = batGrp.get_max_wgt_num();
				}
				npol = batGrp.get_idx_num();
			} else {
				pBat->mGrpId = -1;
				if (nmtl > 0) {
					Group mtlGrp = geo.get_mtl_grp(i);
					pBat->mMtlId = i;
					if (geo.has_skin()) {
						pBat->mMaxWgtNum = mtlGrp.get_max_wgt_num();
					}
					npol = mtlGrp.get_idx_num();
				} else {
					pBat->mMtlId = -1;
					if (geo.has_skin()) {
						pBat->mMaxWgtNum = geo.mMaxSkinWgtNum;
					}
					npol = geo.get_pol_num();
				}
			}
			int batIdxCnt = 0;
			pBat->mTriNum = 0;
			for (int j = 0; j < npol; ++j) {
				int polIdx = 0;
				if (batGrpCount > 0) {
					int batGrpIdx = pBatGrpIdx[i];
					Group batGrp = geo.get_pol_grp(batGrpIdx);
					polIdx = batGrp.get_idx(j);
				} else {
					if (nmtl > 0) {
						polIdx = geo.get_mtl_grp(i).get_idx(j);
					} else {
						polIdx = j;
					}
				}
				Polygon pol = geo.get_pol(polIdx);
				if (pol.is_tri() || (triangulate && pol.tri_count() > 0)) {
					for (int k = 0; k < pol.get_vtx_num(); ++k) {
						int32_t vtxId = pol.get_vtx_pnt_id(k);
						if (batIdxCnt > 0) {
							pBat->mMinIdx = nxCalc::min(pBat->mMinIdx, vtxId);
							pBat->mMaxIdx = nxCalc::max(pBat->mMaxIdx, vtxId);
						} else {
							pBat->mMinIdx = vtxId;
							pBat->mMaxIdx = vtxId;
						}
					}
					batIdxCnt += pol.tri_count() * 3;
					pBat->mTriNum += pol.tri_count();
				}
			}
			if (pBat->is_idx16()) {
				pBat->mIdxOrg = nidx16;
				nidx16 += batIdxCnt;
			} else {
				pBat->mIdxOrg = nidx32;
				nidx32 += batIdxCnt;
			}
		}
	}
	nidx16 = 0;
	nidx32 = 0;
	for (int i = 0; i < nbat; ++i) {
		Batch* pBat = get_bat(i);
		int npol = 0;
		if (pBat) {
			if (batGrpCount > 0) {
				int batGrpIdx = pBatGrpIdx[i];
				Group batGrp = geo.get_pol_grp(batGrpIdx);
				npol = batGrp.get_idx_num();
			} else {
				pBat->mGrpId = -1;
				if (nmtl > 0) {
					Group mtlGrp = geo.get_mtl_grp(i);
					npol = mtlGrp.get_idx_num();
				} else {
					npol = geo.get_pol_num();
				}
			}
			for (int j = 0; j < npol; ++j) {
				int polIdx = 0;
				if (batGrpCount > 0) {
					int batGrpIdx = pBatGrpIdx[i];
					Group batGrp = geo.get_pol_grp(batGrpIdx);
					polIdx = batGrp.get_idx(j);
				} else {
					if (nmtl > 0) {
						polIdx = geo.get_mtl_grp(i).get_idx(j);
					} else {
						polIdx = j;
					}
				}
				Polygon pol = geo.get_pol(polIdx);
				if (pol.is_tri() || (triangulate && pol.tri_count() > 0)) {
					if (pBat->is_idx16()) {
						uint16_t* pIdx16 = pData->get_idx16();
						if (pIdx16) {
							if (pol.is_tri()) {
								for (int k = 0; k < 3; ++k) {
									pIdx16[nidx16 + k] = (uint16_t)(pol.get_vtx_pnt_id(k) - pBat->mMinIdx);
								}
								nidx16 += 3;
							} else {
								int ntris = pol.triangulate(pPolTriLst, pPolTriWk);
								for (int t = 0; t < ntris; ++t) {
									for (int k = 0; k < 3; ++k) {
										pIdx16[nidx16 + k] = (uint16_t)(pol.get_vtx_pnt_id(pPolTriLst[t*3 + k]) - pBat->mMinIdx);
									}
									nidx16 += 3;
								}
							}
						}
					} else {
						uint32_t* pIdx32 = pData->get_idx32();
						if (pIdx32) {
							if (pol.is_tri()) {
								for (int k = 0; k < 3; ++k) {
									pIdx32[nidx32 + k] = (uint32_t)(pol.get_vtx_pnt_id(k) - pBat->mMinIdx);
								}
								nidx32 += 3;
							} else {
								int ntris = pol.triangulate(pPolTriLst, pPolTriWk);
								for (int t = 0; t < ntris; ++t) {
									for (int k = 0; k < 3; ++k) {
										pIdx32[nidx32 + k] = (uint32_t)(pol.get_vtx_pnt_id(pPolTriLst[t*3 + k]) - pBat->mMinIdx);
									}
									nidx32 += 3;
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBatGrpIdx) {
		nxCore::mem_free(pBatGrpIdx);
		pBatGrpIdx = nullptr;
	}
	if (pPolTriLst) {
		nxCore::mem_free(pPolTriLst);
		pPolTriLst = nullptr;
	}
	if (pPolTriWk) {
		nxCore::mem_free(pPolTriWk);
		pPolTriWk = nullptr;
	}
}

void sxGeometryData::DisplayList::destroy() {
	if (mpData) {
		nxCore::mem_free(mpData);
	}
	mpData = nullptr;
	mpGeo = nullptr;
}

const char* sxGeometryData::DisplayList::get_bat_mtl_name(int batIdx) const {
	const char* pMtlName = nullptr;
	if (is_valid()) {
		Batch* pBat = get_bat(batIdx);
		if (pBat) {
			pMtlName = mpGeo->get_mtl_name(pBat->mMtlId);
		}
	}
	return pMtlName;
}


void sxDDSHead::init(uint32_t w, uint32_t h) {
	::memset(this, 0, sizeof(sxDDSHead));
	mMagic = XD_FOURCC('D', 'D', 'S', ' ');
	mSize = sizeof(sxDDSHead) - 4;
	mHeight = h;
	mWidth = w;
}

void sxDDSHead::init_dds128(uint32_t w, uint32_t h) {
	init(w, h);
	mFlags = 0x081007;
	mPitchLin = w * h * (sizeof(float) * 4);
	mFormat.mSize = 0x20;
	mFormat.mFlags = 4;
	mFormat.mFourCC = 0x74;
	mCaps1 = 0x1000;
}

void sxDDSHead::init_dds64(uint32_t w, uint32_t h) {
	init(w, h);
	mFlags = 0x081007;
	mPitchLin = w * h * (sizeof(xt_half) * 4);
	mFormat.mSize = 0x20;
	mFormat.mFlags = 4;
	mFormat.mFourCC = 0x71;
	mCaps1 = 0x1000;
}

void sxDDSHead::init_dds32(uint32_t w, uint32_t h, bool encRGB) {
	init(w, h);
	mFlags = 0x081007;
	mPitchLin = w * h * (sizeof(uint8_t) * 4);
	mFormat.mSize = 0x20;
	mFormat.mFlags = 0x41;
	mFormat.mBitCount = 0x20;
	if (encRGB) {
		mFormat.mMaskR = 0xFF << 0;
		mFormat.mMaskG = 0xFF << 8;
		mFormat.mMaskB = 0xFF << 16;
		mFormat.mMaskA = 0xFF << 24;
	} else {
		mFormat.mMaskR = 0xFF << 16;
		mFormat.mMaskG = 0xFF << 8;
		mFormat.mMaskB = 0xFF << 0;
		mFormat.mMaskA = 0xFF << 24;
	}
	mCaps1 = 0x1000;
}


namespace nxTexture {

sxDDSHead* alloc_dds128(uint32_t w, uint32_t h, uint32_t* pSize) {
	sxDDSHead* pDDS = nullptr;
	uint32_t npix = w * h;
	if ((int)npix > 0) {
		uint32_t dataSize = npix * sizeof(cxColor);
		uint32_t size = sizeof(sxDDSHead) + dataSize;
		pDDS = reinterpret_cast<sxDDSHead*>(nxCore::mem_alloc(size, XD_FOURCC('X', 'D', 'D', 'S')));
		if (pDDS) {
			pDDS->init_dds128(w, h);
			if (pSize) {
				*pSize = size;
			}
		}
	}
	return pDDS;
}

void save_dds128(const char* pPath, const cxColor* pClr, uint32_t w, uint32_t h) {
	uint32_t npix = w*h;
	if ((int)npix > 0) {
		FILE* pOut = nxSys::fopen_w_bin(pPath);
		if (pOut) {
			sxDDSHead dds = {};
			dds.init_dds128(w, h);
			::fwrite(&dds, sizeof(dds), 1, pOut);
			::fwrite(pClr, sizeof(cxColor), npix, pOut);
			::fclose(pOut);
		}
	}
}

void save_dds64(const char* pPath, const cxColor* pClr, uint32_t w, uint32_t h) {
	uint32_t npix = w*h;
	if ((int)npix > 0) {
		FILE* pOut = nxSys::fopen_w_bin(pPath);
		if (pOut) {
			sxDDSHead dds = {};
			dds.init_dds64(w, h);
			::fwrite(&dds, sizeof(dds), 1, pOut);
			xt_half4 h;
			for (uint32_t i = 0; i < npix; ++i) {
				cxColor c = pClr[i];
				h.set(c.r, c.g, c.b, c.a);
				::fwrite(&h, sizeof(h), 1, pOut);
			}
			::fclose(pOut);
		}
	}
}

void save_dds32_rgbe(const char* pPath, const cxColor* pClr, uint32_t w, uint32_t h) {
	uint32_t npix = w*h;
	if ((int)npix > 0) {
		FILE* pOut = nxSys::fopen_w_bin(pPath);
		if (pOut) {
			sxDDSHead dds = {};
			dds.init_dds32(w, h);
			dds.set_encoding(sxDDSHead::ENC_RGBE);
			::fwrite(&dds, sizeof(dds), 1, pOut);
			for (uint32_t i = 0; i < npix; ++i) {
				uint32_t e = pClr[i].encode_rgbe();
				::fwrite(&e, sizeof(e), 1, pOut);
			}
			::fclose(pOut);
		}
	}
}

void save_dds32_bgre(const char* pPath, const cxColor* pClr, uint32_t w, uint32_t h) {
	uint32_t npix = w*h;
	if ((int)npix > 0) {
		FILE* pOut = nxSys::fopen_w_bin(pPath);
		if (pOut) {
			sxDDSHead dds = {};
			dds.init_dds32(w, h, false);
			dds.set_encoding(sxDDSHead::ENC_BGRE);
			::fwrite(&dds, sizeof(dds), 1, pOut);
			for (uint32_t i = 0; i < npix; ++i) {
				uint32_t e = pClr[i].encode_bgre();
				::fwrite(&e, sizeof(e), 1, pOut);
			}
			::fclose(pOut);
		}
	}
}

void save_dds32_rgbi(const char* pPath, const cxColor* pClr, uint32_t w, uint32_t h) {
	uint32_t npix = w*h;
	if ((int)npix > 0) {
		FILE* pOut = nxSys::fopen_w_bin(pPath);
		if (pOut) {
			sxDDSHead dds = {};
			dds.init_dds32(w, h);
			dds.set_encoding(sxDDSHead::ENC_RGBI);
			::fwrite(&dds, sizeof(dds), 1, pOut);
			for (uint32_t i = 0; i < npix; ++i) {
				uint32_t e = pClr[i].encode_rgbi();
				::fwrite(&e, sizeof(e), 1, pOut);
			}
			::fclose(pOut);
		}
	}
}

void save_dds32_bgri(const char* pPath, const cxColor* pClr, uint32_t w, uint32_t h) {
	uint32_t npix = w*h;
	if ((int)npix > 0) {
		FILE* pOut = nxSys::fopen_w_bin(pPath);
		if (pOut) {
			sxDDSHead dds = {};
			dds.init_dds32(w, h, false);
			dds.set_encoding(sxDDSHead::ENC_BGRI);
			::fwrite(&dds, sizeof(dds), 1, pOut);
			for (uint32_t i = 0; i < npix; ++i) {
				uint32_t e = pClr[i].encode_bgri();
				::fwrite(&e, sizeof(e), 1, pOut);
			}
			::fclose(pOut);
		}
	}
}

void save_dds32_rgba8(const char* pPath, const cxColor* pClr, uint32_t w, uint32_t h, float gamma) {
	uint32_t npix = w*h;
	if ((int)npix > 0) {
		FILE* pOut = nxSys::fopen_w_bin(pPath);
		if (pOut) {
			sxDDSHead dds = {};
			dds.init_dds32(w, h);
			if (gamma > 0.0f) {
				dds.set_gamma(gamma);
			} else {
				dds.set_gamma(1.0f);
			}
			::fwrite(&dds, sizeof(dds), 1, pOut);
			if (gamma > 0.0f && gamma != 1.0f) {
				float invg = 1.0f / gamma;
				for (uint32_t i = 0; i < npix; ++i) {
					cxColor cc = pClr[i];
					for (int j = 0; j < 3; ++j) {
						if (cc.ch[j] > 0.0f) {
							cc.ch[j] = ::powf(cc.ch[j], invg);
						} else {
							cc.ch[j] = 0.0f;
						}
					}
					uint32_t e = cc.encode_rgba8();
					::fwrite(&e, sizeof(e), 1, pOut);
				}
			} else {
				for (uint32_t i = 0; i < npix; ++i) {
					uint32_t e = pClr[i].encode_rgba8();
					::fwrite(&e, sizeof(e), 1, pOut);
				}
			}
			::fclose(pOut);
		}
	}
}

void save_dds32_bgra8(const char* pPath, const cxColor* pClr, uint32_t w, uint32_t h, float gamma) {
	uint32_t npix = w*h;
	if ((int)npix > 0) {
		FILE* pOut = nxSys::fopen_w_bin(pPath);
		if (pOut) {
			sxDDSHead dds = {};
			dds.init_dds32(w, h, false);
			if (gamma > 0.0f) {
				dds.set_gamma(gamma);
			} else {
				dds.set_gamma(1.0f);
			}
			::fwrite(&dds, sizeof(dds), 1, pOut);
			if (gamma > 0.0f && gamma != 1.0f) {
				float invg = 1.0f / gamma;
				for (uint32_t i = 0; i < npix; ++i) {
					cxColor cc = pClr[i];
					for (int j = 0; j < 3; ++j) {
						if (cc.ch[j] > 0.0f) {
							cc.ch[j] = ::powf(cc.ch[j], invg);
						} else {
							cc.ch[j] = 0.0f;
						}
					}
					uint32_t e = cc.encode_bgra8();
					::fwrite(&e, sizeof(e), 1, pOut);
				}
			} else {
				for (uint32_t i = 0; i < npix; ++i) {
					uint32_t e = pClr[i].encode_bgra8();
					::fwrite(&e, sizeof(e), 1, pOut);
				}
			}
			::fclose(pOut);
		}
	}
}

cxColor* decode_dds(sxDDSHead* pDDS, uint32_t* pWidth, uint32_t* pHeight, float gamma) {
	if (!pDDS) return nullptr;
	int w = pDDS->mWidth;
	int h = pDDS->mHeight;
	int n = w * h;
	size_t size = n * sizeof(cxColor);
	if (pWidth) {
		*pWidth = w;
	}
	if (pHeight) {
		*pHeight = h;
	}
	cxColor* pClr = (cxColor*)nxCore::mem_alloc(size, XD_FOURCC('D', 'D', 'S', 'C'));
	if (pClr) {
		if (pDDS->is_dds128()) {
			::memcpy(pClr, pDDS + 1, size);
		} else if (pDDS->is_dds64()) {
			xt_half4* pHalf = (xt_half4*)(pDDS + 1);
			xt_float4* pFloat = (xt_float4*)pClr;
			for (int i = 0; i < n; ++i) {
				pFloat[i] = pHalf[i].get();
			}
		} else if (pDDS->is_dds32()) {
			if (pDDS->get_encoding() == sxDDSHead::ENC_RGBE) {
				uint32_t* pRGBE = (uint32_t*)(pDDS + 1);
				for (int i = 0; i < n; ++i) {
					pClr[i].decode_rgbe(pRGBE[i]);
				}
			} else if (pDDS->get_encoding() == sxDDSHead::ENC_BGRE) {
				uint32_t* pBGRE = (uint32_t*)(pDDS + 1);
				for (int i = 0; i < n; ++i) {
					pClr[i].decode_bgre(pBGRE[i]);
				}
			} else if (pDDS->get_encoding() == sxDDSHead::ENC_RGBI) {
				uint32_t* pRGBI = (uint32_t*)(pDDS + 1);
				for (int i = 0; i < n; ++i) {
					pClr[i].decode_rgbi(pRGBI[i]);
				}
			} else if (pDDS->get_encoding() == sxDDSHead::ENC_BGRI) {
				uint32_t* pBGRI = (uint32_t*)(pDDS + 1);
				for (int i = 0; i < n; ++i) {
					pClr[i].decode_bgri(pBGRI[i]);
				}
			} else {
				float wg = pDDS->get_gamma();
				if (wg <= 0.0f) wg = gamma;
				uint32_t maskR = pDDS->mFormat.mMaskR;
				uint32_t maskG = pDDS->mFormat.mMaskG;
				uint32_t maskB = pDDS->mFormat.mMaskB;
				uint32_t maskA = pDDS->mFormat.mMaskA;
				uint32_t shiftR = nxCore::ctz32(maskR);
				uint32_t shiftG = nxCore::ctz32(maskG);
				uint32_t shiftB = nxCore::ctz32(maskB);
				uint32_t shiftA = nxCore::ctz32(maskA);
				uint32_t* pEnc32 = (uint32_t*)(pDDS + 1);
				for (int i = 0; i < n; ++i) {
					uint32_t enc = pEnc32[i];
					uint32_t ir = (enc & maskR) >> shiftR;
					uint32_t ig = (enc & maskG) >> shiftG;
					uint32_t ib = (enc & maskB) >> shiftB;
					uint32_t ia = (enc & maskA) >> shiftA;
					pClr[i].r = (float)(int)ir;
					pClr[i].g = (float)(int)ig;
					pClr[i].b = (float)(int)ib;
					pClr[i].a = (float)(int)ia;
				}
				float scl[4];
				scl[0] = nxCalc::rcp0((float)(int)(maskR >> shiftR));
				scl[1] = nxCalc::rcp0((float)(int)(maskG >> shiftG));
				scl[2] = nxCalc::rcp0((float)(int)(maskB >> shiftB));
				scl[3] = nxCalc::rcp0((float)(int)(maskA >> shiftA));
				for (int i = 0; i < n; ++i) {
					float* pDst = pClr[i].ch;
					for (int j = 0; j < 4; ++j) {
						pDst[j] *= scl[j];
					}
				}
				if (wg > 0.0f) {
					for (int i = 0; i < n; ++i) {
						float* pDst = pClr[i].ch;
						for (int j = 0; j < 4; ++j) {
							pDst[j] = ::powf(pDst[j], wg);
						}
					}
				}
			}
		} else {
			nxSys::dbgmsg("unsupported DDS format\n");
			for (int i = 0; i < n; ++i) {
				pClr[i].zero_rgb();
			}
		}
	}
	return pClr;
}

void save_sgi(const char* pPath, const cxColor* pClr, uint32_t w, uint32_t h, float gamma) {
	if (!pPath || !pClr || !(w*h)) return;
	FILE* pOut = nxSys::fopen_w_bin(pPath);
	if (!pOut) return;
	size_t size = w * h * 4;
	if (size < 0x200) size = 0x200;
	void* pMem = nxCore::mem_alloc(size, XD_TMP_MEM_TAG);
	if (pMem) {
		::memset(pMem, 0, size);
		uint8_t* pHead = (uint8_t*)pMem;
		pHead[0] = 1;
		pHead[1] = 0xDA;
		pHead[3] = 1;
		pHead[5] = 3;
		pHead[0xB] = 4;
		pHead[0x13] = 0xFF;
		pHead[6] = (uint8_t)((w >> 8) & 0xFF);
		pHead[7] = (uint8_t)(w & 0xFF);
		pHead[8] = (uint8_t)((h >> 8) & 0xFF);
		pHead[9] = (uint8_t)(h & 0xFF);
		::fwrite(pHead, 0x200, 1, pOut);
		uint8_t* pDst = (uint8_t*)pMem;
		bool gflg = gamma > 0.0f && gamma != 1.0f;
		float invg = gflg ? 1.0f / gamma : 1.0f;
		int offs = int(w*h);
		int idst = 0;
		for (int y = int(h); --y >= 0;) {
			for (int x = 0; x < int(w); ++x) {
				uint32_t idx = y*w + x;
				cxColor c = pClr[idx];
				if (gflg) {
					for (int i = 0; i < 3; ++i) {
						if (c.ch[i] > 0.0f) {
							c.ch[i] = ::powf(c.ch[i], invg);
						} else {
							c.ch[i] = 0.0f;
						}
					}
				}
				uint32_t ic = c.encode_rgba8();
				pDst[idst] = uint8_t(ic & 0xFF);
				pDst[idst + offs] = uint8_t((ic >> 8) & 0xFF);
				pDst[idst + offs*2] = uint8_t((ic >> 16) & 0xFF);
				pDst[idst + offs*3] = uint8_t((ic >> 24) & 0xFF);
				++idst;
			}
		}
		::fwrite(pMem, w*h*4, 1, pOut);
		nxCore::mem_free(pMem);
	}
	::fclose(pOut);
}

/* see PBRT book for details */
void calc_resample_wgts(int oldRes, int newRes, xt_float4* pWgt, int16_t* pOrg) {
	float rt = float(oldRes) / float(newRes);
	float fw = 2.0f;
	for (int i = 0; i < newRes; ++i) {
		float c = (float(i) + 0.5f) * rt;
		float org = ::floorf((c - fw) + 0.5f);
		pOrg[i] = (int16_t)org;
		float* pW = pWgt[i];
		float s = 0.0f;
		for (int j = 0; j < 4; ++j) {
			float pos = org + float(j) + 0.5f;
			float x = ::fabsf((pos - c) / fw);
			float w;
			if (x < 1.0e-5f) {
				w = 1.0f;
			} else if (x > 1.0f) {
				w = 1.0f;
			} else {
				x *= XD_PI;
				w = nxCalc::sinc(x*2.0f) * nxCalc::sinc(x);
			}
			pW[j] = w;
			s += w;
		}
		s = nxCalc::rcp0(s);
		pWgt[i].scl(s);
	}
}

/* VC: must be noinline to work around AVX/x64 optimizer bug */
static XD_NOINLINE cxColor* clr_dup(cxColor* pDst, const cxColor c, int num) {
	for (int i = 0; i < num; ++i) {
		*pDst++ = c;
	}
	return pDst;
}

cxColor* upscale(const cxColor* pSrc, int srcW, int srcH, int xscl, int yscl, bool filt, cxColor* pDstBuff, void* pWrkMem, bool cneg) {
	if (!pSrc) return nullptr;
	int w = srcW * xscl;
	int h = srcH * yscl;
	if (w <= 0 || h <= 0) return nullptr;
	cxColor* pDst = pDstBuff;
	if (!pDstBuff) {
		pDst = reinterpret_cast<cxColor*>(nxCore::mem_alloc(w * h * sizeof(cxColor), XD_FOURCC('n', 'T', 'e', 'x')));
	}
	if (!pDst) return nullptr;
	if (filt) {
		int wgtNum = nxCalc::max(w, h);
		xt_float4* pWgt;
		cxColor* pTmp;
		int16_t* pOrg;
		if (pWrkMem) {
			pWgt = reinterpret_cast<xt_float4*>(pWrkMem);
			pTmp = reinterpret_cast<cxColor*>(XD_INCR_PTR(pWgt, wgtNum * sizeof(xt_float4)));
			pOrg = reinterpret_cast<int16_t*>(XD_INCR_PTR(pTmp, wgtNum * sizeof(cxColor)));
		} else {
			pWgt = reinterpret_cast<xt_float4*>(nxCore::mem_alloc(wgtNum * sizeof(xt_float4), XD_TMP_MEM_TAG));
			pTmp = reinterpret_cast<cxColor*>(nxCore::mem_alloc(wgtNum * sizeof(cxColor), XD_TMP_MEM_TAG));
			pOrg = reinterpret_cast<int16_t*>(nxCore::mem_alloc(wgtNum * sizeof(int16_t), XD_TMP_MEM_TAG));
		}
		nxTexture::calc_resample_wgts(srcW, w, pWgt, pOrg);
		for (int y = 0; y < srcH; ++y) {
			for (int x = 0; x < w; ++x) {
				cxColor clr;
				clr.zero();
				xt_float4 wgt = pWgt[x];
				for (int i = 0; i < 4; ++i) {
					int x0 = nxCalc::clamp(pOrg[x] + i, 0, srcW - 1);
					cxColor csrc = pSrc[y*srcW + x0];
					csrc.scl(wgt[i]);
					clr.add(csrc);
				}
				pDst[y*w + x] = clr;
			}
		}
		nxTexture::calc_resample_wgts(srcH, h, pWgt, pOrg);
		for (int x = 0; x < w; ++x) {
			for (int y = 0; y < h; ++y) {
				cxColor clr;
				clr.zero();
				xt_float4 wgt = pWgt[y];
				for (int i = 0; i < 4; ++i) {
					int y0 = nxCalc::clamp(pOrg[y] + i, 0, srcH - 1);
					cxColor csrc = pDst[y0*w + x];
					csrc.scl(wgt[i]);
					clr.add(csrc);
				}
				pTmp[y] = clr;
			}
			for (int y = 0; y < h; ++y) {
				pDst[y*w + x] = pTmp[y];
			}
		}
		if (!pWrkMem) {
			nxCore::mem_free(pOrg);
			nxCore::mem_free(pTmp);
			nxCore::mem_free(pWgt);
		}
	} else {
		const cxColor* pClr = pSrc;
		cxColor* pDstX = pDst;
		for (int y = 0; y < srcH; ++y) {
			for (int x = 0; x < srcW; ++x) {
				cxColor c = *pClr++;
				if (0) {
					// VC: AVX/x64 optimizer bug for constant xscl
					for (int i = 0; i < xscl; ++i) {
						*pDstX++ = c;
					}
				} else {
					pDstX = clr_dup(pDstX, c, xscl);
				}
			}
			cxColor* pDstY = pDstX;
			for (int i = 1; i < yscl; ++i) {
				for (int j = 0; j < w; ++j) {
					*pDstY++ = pDstX[j - w];
				}
			}
			pDstX = pDstY;
		}
	}
	if (cneg) {
		for (int i = 0; i < w * h; ++i) {
			pDst[i].clip_neg();
		}
	}
	return pDst;
}

size_t calc_upscale_work_size(int srcW, int srcH, int xscl, int yscl) {
	int w = srcW * xscl;
	int h = srcH * yscl;
	if (w <= 0 || h <= 0) return 0;
	int wgtNum = nxCalc::max(w, h);
	size_t size = wgtNum * (sizeof(xt_float4) + sizeof(cxColor) + sizeof(int16_t));
	return XD_ALIGN(size, 0x10);
}

} // nxTexture


void sxTextureData::Plane::expand(float* pDst, int pixelStride) const {
	PlaneInfo* pInfo = mpTex->get_plane_info(mPlaneId);
	uint8_t* pBits = reinterpret_cast<uint8_t*>(XD_INCR_PTR(mpTex, pInfo->mDataOffs));
	const int tblSize = 1 << 8;
	int32_t tbl[tblSize];
	::memset(tbl, 0, sizeof(tbl));
	int bitCnt = pInfo->mBitCount;
	uxVal32 uval;
	int bitPtr = 0;
	int pred = 0;
	int hash = 0;
	while (bitCnt) {
		int bitLen = nxCore::fetch_bits32(pBits, bitPtr, 5);
		bitPtr += 5;
		int bxor = 0;
		if (bitLen) {
			bxor = nxCore::fetch_bits32(pBits, bitPtr, bitLen);
			bitPtr += bitLen;
		}
		bitCnt -= 5 + bitLen;
		int ival = bxor ^ pred;
		tbl[hash] = ival;
		hash = (ival >> 21) & (tblSize - 1);
		pred = tbl[hash];
		uval.i = ival << pInfo->mTrailingZeroes;
		float fval = uval.f;
		fval += pInfo->mValOffs;
		*pDst = fval;
		pDst += pixelStride;
	}
}

void sxTextureData::Plane::get_data(float* pDst, int pixelStride) const {
	int w = mpTex->get_width();
	int h = mpTex->get_height();
	int npix = w * h;
	PlaneInfo* pInfo = mpTex->get_plane_info(mPlaneId);
	if (pInfo) {
		void* pDataTop = XD_INCR_PTR(mpTex, pInfo->mDataOffs);
		if (pInfo->is_const()) {
			float cval = *reinterpret_cast<float*>(pDataTop);
			for (int i = 0; i < npix; ++i) {
				*pDst = cval;
				pDst += pixelStride;
			}
		} else if (pInfo->is_compressed()) {
			expand(pDst, pixelStride);
		} else {
			float* pSrc = reinterpret_cast<float*>(pDataTop);
			for (int i = 0; i < npix; ++i) {
				*pDst = *pSrc;
				pDst += pixelStride;
				++pSrc;
			}
		}
	}
}

float* sxTextureData::Plane::get_data() const {
	int w = mpTex->get_width();
	int h = mpTex->get_height();
	int npix = w * h;
	int memsize = npix * sizeof(float);
	float* pDst = reinterpret_cast<float*>(nxCore::mem_alloc(memsize, XD_FOURCC('X', 'T', 'X', 'D')));
	get_data(pDst);
	return pDst;
}

int sxTextureData::calc_mip_num() const {
	int n = 1;
	uint32_t w = get_width();
	uint32_t h = get_height();
	while (w > 1 && h > 1) {
		if (w > 1) w >>= 1;
		if (h > 1) h >>= 1;
		++n;
	}
	return n;
}

int sxTextureData::find_plane_idx(const char* pName) const {
	int idx = -1;
	sxStrList* pStrLst = get_str_list();
	int npln = mPlaneNum;
	if (npln && pStrLst) {
		int nameId = pStrLst->find_str(pName);
		if (nameId >= 0) {
			for (int i = 0; i < npln; ++i) {
				PlaneInfo* pInfo = get_plane_info(i);
				if (pInfo && pInfo->mNameId == nameId) {
					idx = i;
					break;
				}
			}
		}
	}
	return idx;
}

sxTextureData::Plane sxTextureData::find_plane(const char* pName) const {
	Plane plane;
	int idx = find_plane_idx(pName);
	if (idx < 0) {
		plane.mpTex = nullptr;
		plane.mPlaneId = -1;
	} else {
		plane.mpTex = this;
		plane.mPlaneId = idx;
	}
	return plane;
}

sxTextureData::Plane sxTextureData::get_plane(int idx) const {
	Plane plane;
	if (ck_plane_idx(idx)) {
		plane.mpTex = this;
		plane.mPlaneId = idx;
	} else {
		plane.mpTex = nullptr;
		plane.mPlaneId = -1;
	}
	return plane;
}

bool sxTextureData::is_plane_hdr(int idx) const {
	bool res = false;
	PlaneInfo* pInfo = get_plane_info(idx);
	if (pInfo) {
		res = pInfo->mMinVal < 0.0f || pInfo->mMaxVal > 1.0f;
	}
	return res;
}

bool sxTextureData::is_hdr() const {
	int n = mPlaneNum;
	for (int i = 0; i < n; ++i) {
		if (is_plane_hdr(i)) return true;
	}
	return false;
}

void sxTextureData::get_rgba(float* pDst) const {
	if (!pDst) return;
	int w = get_width();
	int h = get_height();
	int npix = w * h;
	int memsize = npix * sizeof(cxColor);
	::memset(pDst, 0, memsize);
	static const char* plnName[] = { "r", "g", "b", "a" };
	for (int i = 0; i < 4; ++i) {
		Plane pln = find_plane(plnName[i]);
		if (pln.is_valid()) {
			pln.get_data(pDst + i, 4);
		}
	}
}

cxColor* sxTextureData::get_rgba() const {
	int w = get_width();
	int h = get_height();
	int npix = w * h;
	int memsize = npix * sizeof(cxColor);
	float* pDst = reinterpret_cast<float*>(nxCore::mem_alloc(memsize, XD_FOURCC('X', 'T', 'X', 'C')));
	get_rgba(pDst);
	return reinterpret_cast<cxColor*>(pDst);
}

sxTextureData::DDS sxTextureData::get_dds() const {
	DDS dds;
	int w = get_width();
	int h = get_height();
	dds.mpHead = nxTexture::alloc_dds128(w, h, &dds.mSize);
	if (dds.mpHead) {
		float* pDst = reinterpret_cast<float*>(dds.mpHead + 1);
		get_rgba(pDst);
	}
	return dds;
}

void sxTextureData::DDS::free() {
	if (mpHead) {
		nxCore::mem_free(mpHead);
	}
	mpHead = nullptr;
	mSize = 0;
}

void sxTextureData::DDS::save(const char* pOutPath) const {
	if (is_valid()) {
		nxCore::bin_save(pOutPath, mpHead, mSize);
	}
}

sxTextureData::Pyramid* sxTextureData::get_pyramid() const {
	sxTextureData::Pyramid* pPmd = nullptr;
	int w0 = get_width();
	int h0 = get_height();
	int w = w0;
	int h = h0;
	bool flgW = nxCore::is_pow2(w);
	bool flgH = nxCore::is_pow2(h);
	int baseW = flgW ? w : (1 << (1 + (int)::log2f(float(w))));
	int baseH = flgH ? h : (1 << (1 + (int)::log2f(float(h))));
	w = baseW;
	h = baseH;
	int nlvl = 1;
	int npix = 1;
	while (w > 1 || h > 1) {
		npix += w*h;
		if (w > 1) w >>= 1;
		if (h > 1) h >>= 1;
		++nlvl;
	}
	size_t headSize = sizeof(Pyramid) + (nlvl - 1)*sizeof(uint32_t);
	uint32_t topOffs = (uint32_t)XD_ALIGN(headSize, 0x10);
	size_t memSize = topOffs + npix*sizeof(cxColor);
	pPmd = reinterpret_cast<sxTextureData::Pyramid*>(nxCore::mem_alloc(memSize, XD_FOURCC('X', 'T', 'P', 'D')));
	pPmd->mBaseWidth = baseW;
	pPmd->mBaseHeight = baseH;
	pPmd->mLvlNum = nlvl;
	w = baseW;
	h = baseH;
	uint32_t offs = topOffs;
	for (int i = 0; i < nlvl; ++i) {
		pPmd->mLvlOffs[i] = offs;
		offs += w*h*sizeof(cxColor);
		if (w > 1) w >>= 1;
		if (h > 1) h >>= 1;
	}
	int wgtNum = nxCalc::max(baseW, baseH);
	cxColor* pBase = get_rgba();
	cxColor* pLvl = pPmd->get_lvl(0);
	if (flgW && flgH) {
		::memcpy(pLvl, pBase, baseW*baseH*sizeof(cxColor));
	} else {
		xt_float4* pWgt = reinterpret_cast<xt_float4*>(nxCore::mem_alloc(wgtNum*sizeof(xt_float4), XD_TMP_MEM_TAG));
		int16_t* pOrg = reinterpret_cast<int16_t*>(nxCore::mem_alloc(wgtNum*sizeof(int16_t), XD_TMP_MEM_TAG));
		cxColor* pTmp = reinterpret_cast<cxColor*>(nxCore::mem_alloc(wgtNum*sizeof(cxColor), XD_TMP_MEM_TAG));
		nxTexture::calc_resample_wgts(w0, baseW, pWgt, pOrg);
		for (int y = 0; y < h0; ++y) {
			for (int x = 0; x < baseW; ++x) {
				cxColor clr;
				clr.zero();
				xt_float4 wgt = pWgt[x];
				for (int i = 0; i < 4; ++i) {
					int x0 = nxCalc::clamp(pOrg[x] + i, 0, w0-1);
					cxColor csrc = pBase[y*w0 + x0];
					csrc.scl(wgt[i]);
					clr.add(csrc);
				}
				pLvl[y*baseW + x] = clr;
			}
		}
		nxTexture::calc_resample_wgts(h0, baseH, pWgt, pOrg);
		for (int x = 0; x < baseW; ++x) {
			for (int y = 0; y < baseH; ++y) {
				cxColor clr;
				clr.zero();
				xt_float4 wgt = pWgt[y];
				for (int i = 0; i < 4; ++i) {
					int y0 = nxCalc::clamp(pOrg[y] + i, 0, h0-1);
					cxColor csrc = pLvl[y0*baseW + x];
					csrc.scl(wgt[i]);
					clr.add(csrc);
				}
				pTmp[y] = clr;
			}
			for (int y = 0; y < baseH; ++y) {
				pLvl[y*baseW + x] = pTmp[y];
			}
		}
		nxCore::mem_free(pTmp);
		nxCore::mem_free(pOrg);
		nxCore::mem_free(pWgt);
	}
	nxCore::mem_free(pBase);
	w = baseW/2;
	h = baseH/2;
	for (int i = 1; i < nlvl; ++i) {
		cxColor* pLvlSrc = pPmd->get_lvl(i-1);
		cxColor* pLvlDst = pPmd->get_lvl(i);
		int wsrc = w*2;
		int hsrc = h*2;
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				int sx0 = nxCalc::min(x*2, wsrc - 1);
				int sy0 = nxCalc::min(y*2, hsrc - 1);
				int sx1 = nxCalc::min(sx0 + 1, wsrc - 1);
				int sy1 = nxCalc::min(sy0 + 1, hsrc - 1);
				cxColor clr = pLvlSrc[sy0*wsrc + sx0];
				clr.add(pLvlSrc[sy0*wsrc + sx1]);
				clr.add(pLvlSrc[sy1*wsrc + sx0]);
				clr.add(pLvlSrc[sy1*wsrc + sx1]);
				clr.scl(0.25f);
				pLvlDst[y*w + x] = clr;
			}
		}
		if (w > 1) w >>= 1;
		if (h > 1) h >>= 1;
	}
	return pPmd;
}

void sxTextureData::Pyramid::get_lvl_dims(int idx, int* pWidth, int* pHeight) const {
	int w = 0;
	int h = 0;
	if (ck_lvl_idx(idx)) {
		w = mBaseWidth;
		h = mBaseHeight;
		for (int i = 0; i < idx; ++i) {
			if (w > 1) w >>= 1;
			if (h > 1) h >>= 1;
		}
	}
	if (pWidth) {
		*pWidth = w;
	}
	if (pHeight) {
		*pHeight = h;
	}
}

sxTextureData::DDS sxTextureData::Pyramid::get_lvl_dds(int idx) const {
	DDS dds;
	dds.mpHead = nullptr;
	dds.mSize = 0;
	if (ck_lvl_idx(idx)) {
		int w, h;
		get_lvl_dims(idx, &w, &h);
		dds.mpHead = nxTexture::alloc_dds128(w, h, &dds.mSize);
		cxColor* pLvl = get_lvl(idx);
		if (pLvl) {
			cxColor* pDst = reinterpret_cast<cxColor*>(dds.mpHead + 1);
			::memcpy(pDst, pLvl, w*h*sizeof(cxColor));
		}
	}
	return dds;
}



float sxKeyframesData::RigLink::Node::get_pos_chan(int idx) {
	float val = 0.0f;
	if ((uint32_t)idx < 3) {
		Val* pVal = get_pos_val();
		if (pVal && pVal->fcvId[idx] >= 0) {
			val = pVal->f3[idx];
		}
	}
	return val;
}

float sxKeyframesData::RigLink::Node::get_rot_chan(int idx) {
	float val = 0.0f;
	if ((uint32_t)idx < 3) {
		Val* pVal = get_rot_val();
		if (pVal && pVal->fcvId[idx] >= 0) {
			val = pVal->f3[idx];
		}
	}
	return val;
}

float sxKeyframesData::RigLink::Node::get_scl_chan(int idx) {
	float val = 1.0f;
	if ((uint32_t)idx < 3) {
		Val* pVal = get_scl_val();
		if (pVal && pVal->fcvId[idx] >= 0) {
			val = pVal->f3[idx];
		}
	}
	return val;
}

float sxKeyframesData::RigLink::Node::get_anim_chan(exAnimChan chan) {
	float val = 0.0f;
	switch (chan) {
		case exAnimChan::TX: val = get_pos_chan(0); break;
		case exAnimChan::TY: val = get_pos_chan(1); break;
		case exAnimChan::TZ: val = get_pos_chan(2); break;
		case exAnimChan::RX: val = get_rot_chan(0); break;
		case exAnimChan::RY: val = get_rot_chan(1); break;
		case exAnimChan::RZ: val = get_rot_chan(2); break;
		case exAnimChan::SX: val = get_scl_chan(0); break;
		case exAnimChan::SY: val = get_scl_chan(1); break;
		case exAnimChan::SZ: val = get_scl_chan(2); break;
	}
	return val;
}

bool sxKeyframesData::RigLink::Node::ck_pos_chan(int idx) {
	bool res = false;
	if ((uint32_t)idx < 3) {
		Val* pVal = get_pos_val();
		res = (pVal && pVal->fcvId[idx] >= 0);
	}
	return res;
}

bool sxKeyframesData::RigLink::Node::ck_rot_chan(int idx) {
	bool res = false;
	if ((uint32_t)idx < 3) {
		Val* pVal = get_rot_val();
		res = (pVal && pVal->fcvId[idx] >= 0);
	}
	return res;
}

bool sxKeyframesData::RigLink::Node::ck_scl_chan(int idx) {
	bool res = false;
	if ((uint32_t)idx < 3) {
		Val* pVal = get_scl_val();
		res = (pVal && pVal->fcvId[idx] >= 0);
	}
	return res;
}

bool sxKeyframesData::RigLink::Node::ck_anim_chan(exAnimChan chan) {
	bool res = false;
	switch (chan) {
		case exAnimChan::TX: res = ck_pos_chan(0); break;
		case exAnimChan::TY: res = ck_pos_chan(1); break;
		case exAnimChan::TZ: res = ck_pos_chan(2); break;
		case exAnimChan::RX: res = ck_rot_chan(0); break;
		case exAnimChan::RY: res = ck_rot_chan(1); break;
		case exAnimChan::RZ: res = ck_rot_chan(2); break;
		case exAnimChan::SX: res = ck_scl_chan(0); break;
		case exAnimChan::SY: res = ck_scl_chan(1); break;
		case exAnimChan::SZ: res = ck_scl_chan(2); break;
	}
	return res;
}

bool sxKeyframesData::has_node_info() const {
	bool res = false;
	ptrdiff_t offs = (uint8_t*)&mNodeInfoNum - (uint8_t*)this;
	if (mHeadSize > (uint32_t)offs) {
		res = mNodeInfoNum != 0 && mNodeInfoOffs != 0;
	}
	return res;
}

int sxKeyframesData::find_node_info_idx(const char* pName, const char* pPath, int startIdx) const {
	int idx = -1;
	if (has_node_info()) {
		sxStrList* pStrLst = get_str_list();
		if (pStrLst && ck_node_info_idx(startIdx)) {
			NodeInfo* pInfo = reinterpret_cast<NodeInfo*>(XD_INCR_PTR(this, mNodeInfoOffs));
			int ninfo = get_node_info_num();
			int nameId = pName ? pStrLst->find_str(pName) : -1;
			if (nameId >= 0) {
				if (pPath) {
					int pathId = pStrLst->find_str(pPath);
					if (pathId >= 0) {
						for (int i = startIdx; i < ninfo; ++i) {
							if (pInfo[i].mNameId == nameId && pInfo[i].mPathId == pathId) {
								idx = i;
								break;
							}
						}
					}
				} else {
					for (int i = startIdx; i < ninfo; ++i) {
						if (pInfo[i].mNameId == nameId) {
							idx = i;
							break;
						}
					}
				}
			} else if (pPath) {
				int pathId = pStrLst->find_str(pPath);
				if (pathId >= 0) {
					for (int i = startIdx; i < ninfo; ++i) {
						if (pInfo[i].mPathId == pathId) {
							idx = i;
							break;
						}
					}
				}
			}
		}
	}
	return idx;
}

template<int IDX_SIZE> int fno_get(uint8_t* pTop, int at) {
	uint8_t* pFno = pTop + at*IDX_SIZE;
	int fno = -1;
	switch (IDX_SIZE) {
		case 1:
			fno = *pFno;
			break;
		case 2:
			fno = pFno[0] | (pFno[1] << 8);
			break;
		case 3:
			fno = pFno[0] | (pFno[1] << 8) | (pFno[2] << 16);
			break;
	}
	return fno;
}

template<int IDX_SIZE> int find_fno_idx_lin(uint8_t* pTop, int num, int fno) {
	for (int i = 1; i < num; ++i) {
		int ck = fno_get<IDX_SIZE>(pTop, i);
		if (fno < ck) {
			return i - 1;
		}
	}
	return num - 1;
}

template<int IDX_SIZE> int find_fno_idx_bin(uint8_t* pTop, int num, int fno) {
	uint32_t istart = 0;
	uint32_t iend = num;
	do {
		uint32_t imid = (istart + iend) / 2;
		int ck = fno_get<IDX_SIZE>(pTop, imid);
		if (fno < ck) {
			iend = imid;
		} else {
			istart = imid;
		}
	} while (iend - istart >= 2);
	return istart;
}

template<int IDX_SIZE> int find_fno_idx(uint8_t* pTop, int num, int fno) {
	if (num > 10) {
		return find_fno_idx_bin<IDX_SIZE>(pTop, num, fno);
	}
	return find_fno_idx_lin<IDX_SIZE>(pTop, num, fno);
}

int sxKeyframesData::FCurve::find_key_idx(int fno) const {
	int idx = -1;
	if (is_valid() && mpKfr->ck_fno(fno)) {
		FCurveInfo* pInfo = get_info();
		if (!pInfo->is_const()) {
			if (pInfo->mFnoOffs) {
				uint8_t* pFno = reinterpret_cast<uint8_t*>(XD_INCR_PTR(mpKfr, pInfo->mFnoOffs));
				int nfno = pInfo->mKeyNum;
				int maxFno = mpKfr->get_max_fno();
				if (maxFno < (1 << 8)) {
					idx = find_fno_idx<1>(pFno, nfno, fno);
				} else if (maxFno < (1 << 16)) {
					idx = find_fno_idx<2>(pFno, nfno, fno);
				} else {
					idx = find_fno_idx<3>(pFno, nfno, fno);
				}
			} else {
				idx = fno;
			}
		}
	}
	return idx;
}

int sxKeyframesData::FCurve::get_fno(int idx) const {
	int fno = 0;
	FCurveInfo* pInfo = get_info();
	if (pInfo) {
		if (!pInfo->is_const() && (uint32_t)idx < (uint32_t)get_key_num()) {
			if (pInfo->has_fno_lst()) {
				uint8_t* pFno = reinterpret_cast<uint8_t*>(XD_INCR_PTR(mpKfr, pInfo->mFnoOffs));
				int maxFno = mpKfr->get_max_fno();
				if (maxFno < (1 << 8)) {
					fno = fno_get<1>(pFno, idx);
				} else if (maxFno < (1 << 16)) {
					fno = fno_get<2>(pFno, idx);
				} else {
					fno = fno_get<3>(pFno, idx);
				}
			} else {
				fno = idx;
			}
		}
	}
	return fno;
}

float sxKeyframesData::FCurve::eval(float frm, bool extrapolate) const {
	float val = 0.0f;
	int fno = (int)frm;
	if (is_valid()) {
		FCurveInfo* pInfo = get_info();
		if (pInfo->is_const()) {
			val = pInfo->mMinVal;
		} else {
			bool loopFlg = false;
			if (pInfo->has_fno_lst()) {
				float lastFno = (float)get_fno(pInfo->mKeyNum - 1);
				if (frm > lastFno && frm < lastFno + 1.0f) {
					loopFlg = true;
				}
			}
			if (loopFlg) {
				int lastIdx = pInfo->mKeyNum - 1;
				eFunc func = pInfo->get_common_func();
				if (pInfo->mFuncOffs) {
					func = (eFunc)reinterpret_cast<uint8_t*>(XD_INCR_PTR(mpKfr, pInfo->mFuncOffs))[lastIdx];
				}
				float* pVals = reinterpret_cast<float*>(XD_INCR_PTR(mpKfr, pInfo->mValOffs));
				if (func == eFunc::CONSTANT) {
					val = pVals[lastIdx];
				} else {
					float t = frm - (float)fno;
					if (extrapolate) {
						val = nxCalc::lerp(pVals[lastIdx], pVals[lastIdx] + pVals[0], t);
					} else {
						val = nxCalc::lerp(pVals[lastIdx], pVals[0], t);
					}
				}
			} else if (mpKfr->ck_fno(fno)) {
				float* pVals = reinterpret_cast<float*>(XD_INCR_PTR(mpKfr, pInfo->mValOffs));
				int i0 = find_key_idx(fno);
				int i1 = i0 + 1;
				float f0 = (float)get_fno(i0);
				float f1 = (float)get_fno(i1);
				float ffrc = frm - (float)fno;
				if (frm != f0) {
					eFunc func = pInfo->get_common_func();
					if (pInfo->mFuncOffs) {
						func = (eFunc)reinterpret_cast<uint8_t*>(XD_INCR_PTR(mpKfr, pInfo->mFuncOffs))[i0];
					}
					if (func == eFunc::CONSTANT) {
						val = pVals[i0];
					} else {
						float t = (frm - f0) / (f1 - f0);
						float v0 = pVals[i0];
						float v1 = pVals[i1];
						if (func == eFunc::LINEAR) {
							val = nxCalc::lerp(v0, v1, t);
						} else if (func == eFunc::CUBIC) {
							float* pSlopeL = pInfo->mLSlopeOffs ? reinterpret_cast<float*>(XD_INCR_PTR(mpKfr, pInfo->mLSlopeOffs)) : nullptr;
							float* pSlopeR = pInfo->mRSlopeOffs ? reinterpret_cast<float*>(XD_INCR_PTR(mpKfr, pInfo->mRSlopeOffs)) : nullptr;
							float outgoing = 0.0f;
							float incoming = 0.0f;
							if (pSlopeR) {
								outgoing = pSlopeR[i0];
							}
							if (pSlopeL) {
								incoming = pSlopeL[i1];
							}
							float fscl = nxCalc::rcp0(mpKfr->mFPS);
							float seg = (f1 - f0) * fscl;
							outgoing *= seg;
							incoming *= seg;
							val = nxCalc::hermite(v0, outgoing, v1, incoming, t);
						}
					}
				} else {
					val = pVals[i0];
				}
			}
		}
	}
	return val;
}

const char* sxKeyframesData::get_node_name(int idx) const {
	const char* pName = nullptr;
	NodeInfo* pInfo = get_node_info_ptr(idx);
	if (pInfo) {
		pName = get_str(pInfo->mNameId);
	}
	return pName;
}

int sxKeyframesData::find_fcv_idx(const char* pNodeName, const char* pChanName, const char* pNodePath) const {
	int idx = -1;
	sxStrList* pStrLst = get_str_list();
	if (pStrLst) {
		int chanId = pStrLst->find_str(pChanName);
		int nameId = chanId > 0 ? pStrLst->find_str(pNodeName) : -1;
		if (chanId >= 0 && nameId >= 0) {
			int i;
			FCurveInfo* pInfo;
			int nfcv = get_fcv_num();
			if (pNodePath) {
				int pathId = pStrLst->find_str(pNodePath);
				if (pathId >= 0) {
					for (i = 0; i < nfcv; ++i) {
						pInfo = get_fcv_info(i);
						if (pInfo && pInfo->mChanNameId == chanId && pInfo->mNodeNameId == nameId && pInfo->mNodePathId == pathId) {
							idx = i;
							break;
						}
					}
				}
			} else {
				for (i = 0; i < nfcv; ++i) {
					pInfo = get_fcv_info(i);
					if (pInfo && pInfo->mChanNameId == chanId && pInfo->mNodeNameId == nameId) {
						idx = i;
						break;
					}
				}
			}
		}
	}
	return idx;
}

sxKeyframesData::FCurve sxKeyframesData::get_fcv(int idx) const {
	FCurve fcv;
	if (ck_fcv_idx(idx)) {
		fcv.mpKfr = this;
		fcv.mFcvId = idx;
	} else {
		fcv.mpKfr = nullptr;
		fcv.mFcvId = -1;
	}
	return fcv;
}

sxKeyframesData::RigLink* sxKeyframesData::make_rig_link(const sxRigData& rig) const {
	RigLink* pLink = nullptr;
	if (has_node_info()) {
		static const char* posChans[] = { "tx", "ty", "tz" };
		static const char* rotChans[] = { "rx", "ry", "rz" };
		static const char* sclChans[] = { "sx", "sy", "sz" };
		int n = get_node_info_num();
		int nodeNum = 0;
		int posNum = 0;
		int rotNum = 0;
		int sclNum = 0;
		for (int i = 0; i < n; ++i) {
			NodeInfo* pNodeInfo = get_node_info_ptr(i);
			if (pNodeInfo) {
				const char* pNodeName = get_str(pNodeInfo->mNameId);
				int rigNodeId = rig.find_node(pNodeName);
				if (rigNodeId >= 0) {
					++nodeNum;
					for (int j = 0; j < 3; ++j) {
						if (find_fcv_idx(pNodeName, posChans[j]) >= 0) {
							++posNum;
							break;
						}
					}
					for (int j = 0; j < 3; ++j) {
						if (find_fcv_idx(pNodeName, rotChans[j]) >= 0) {
							++rotNum;
							break;
						}
					}
					for (int j = 0; j < 3; ++j) {
						if (find_fcv_idx(pNodeName, sclChans[j]) >= 0) {
							++sclNum;
							break;
						}
					}
				}
			}
		}
		if (nodeNum > 0) {
			int valNum = posNum + rotNum + sclNum;
			size_t memsize = sizeof(RigLink) + (nodeNum - 1)*sizeof(RigLink::Node);
			size_t valTop = memsize;
			memsize += valNum*sizeof(RigLink::Val);
			size_t mapTop = memsize;
			memsize += rig.get_nodes_num()*sizeof(int16_t);
			pLink = reinterpret_cast<RigLink*>(nxCore::mem_alloc(memsize, XD_FOURCC('R','L','N','K')));
			if (pLink) {
				::memset(pLink, 0, memsize);
				pLink->mNodeNum = nodeNum;
				pLink->mRigNodeNum = rig.get_nodes_num();
				pLink->mRigMapOffs = (uint32_t)mapTop;
				int16_t* pRigMap = pLink->get_rig_map();
				if (pRigMap) {
					for (int i = 0; i < pLink->mRigNodeNum; ++i) {
						pRigMap[i] = -1;
					}
				}
				RigLink::Val* pVal = (RigLink::Val*)XD_INCR_PTR(pLink, valTop);
				RigLink::Node* pLinkNode = pLink->mNodes;
				int inode = 0;
				for (int i = 0; i < n; ++i) {
					NodeInfo* pNodeInfo = get_node_info_ptr(i);
					if (pNodeInfo) {
						const char* pNodeName = get_str(pNodeInfo->mNameId);
						int rigNodeId = rig.find_node(pNodeName);
						if (rigNodeId >= 0) {
							int32_t posFcvId[3];
							bool posFlg = false;
							for (int j = 0; j < 3; ++j) {
								posFcvId[j] = find_fcv_idx(pNodeName, posChans[j]);
								posFlg |= posFcvId[j] >= 0;
							}
							int32_t rotFcvId[3];
							bool rotFlg = false;
							for (int j = 0; j < 3; ++j) {
								rotFcvId[j] = find_fcv_idx(pNodeName, rotChans[j]);
								rotFlg |= rotFcvId[j] >= 0;
							}
							int32_t sclFcvId[3];
							bool sclFlg = false;
							for (int j = 0; j < 3; ++j) {
								sclFcvId[j] = find_fcv_idx(pNodeName, sclChans[j]);
								sclFlg |= sclFcvId[j] >= 0;
							}
							if (posFlg || rotFlg || sclFlg) {
								pLinkNode->mKfrNodeId = i;
								pLinkNode->mRigNodeId = rigNodeId;
								pLinkNode->mRotOrd = pNodeInfo->get_rot_order();
								pLinkNode->mXformOrd = pNodeInfo->get_xform_order();
								pLinkNode->mUseSlerp = false;
								if (posFlg) {
									pVal->set_vec(rig.get_lpos(rigNodeId));
									for (int j = 0; j < 3; ++j) {
										pVal->fcvId[j] = posFcvId[j];
									}
									pLinkNode->mPosValOffs = (uint32_t)((uint8_t*)pVal - (uint8_t*)pLinkNode);
									++pVal;
								}
								if (rotFlg) {
									pVal->set_vec(rig.get_lrot(rigNodeId));
									for (int j = 0; j < 3; ++j) {
										pVal->fcvId[j] = rotFcvId[j];
									}
									pLinkNode->mRotValOffs = (uint32_t)((uint8_t*)pVal - (uint8_t*)pLinkNode);
									++pVal;
								}
								if (sclFlg) {
									pVal->set_vec(rig.get_lscl(rigNodeId));
									for (int j = 0; j < 3; ++j) {
										pVal->fcvId[j] = sclFcvId[j];
									}
									pLinkNode->mSclValOffs = (uint32_t)((uint8_t*)pVal - (uint8_t*)pLinkNode);
									++pVal;
								}
								pRigMap[rigNodeId] = inode;
								++inode;
								++pLinkNode;
							}
						}
					}
				}
			}
		}
	}
	return pLink;
}

void sxKeyframesData::eval_rig_link_node(RigLink* pLink, int nodeIdx, float frm, const sxRigData* pRig, cxMtx* pRigLocalMtx) const {
	if (!pLink) return;
	if (!pLink->ck_node_idx(nodeIdx)) return;
	const int POS_MASK = 1 << 0;
	const int ROT_MASK = 1 << 1;
	const int SCL_MASK = 1 << 2;
	int xformMask = 0;
	RigLink::Node* pNode = &pLink->mNodes[nodeIdx];
	RigLink::Val* pPosVal = pNode->get_pos_val();
	if (pPosVal) {
		for (int j = 0; j < 3; ++j) {
			int fcvId = pPosVal->fcvId[j];
			if (ck_fcv_idx(fcvId)) {
				xformMask |= POS_MASK;
				FCurve fcv = get_fcv(fcvId);
				if (fcv.is_valid()) {
					pPosVal->f3[j] = fcv.eval(frm);
				}
			}
		}
	}
	RigLink::Val* pRotVal = pNode->get_rot_val();
	if (pRotVal) {
		int ifrm = (int)frm;
		if (pNode->mUseSlerp && frm != (float)ifrm) {
			cxVec rot0(0.0f);
			cxVec rot1(0.0f);
			for (int j = 0; j < 3; ++j) {
				int fcvId = pRotVal->fcvId[j];
				if (ck_fcv_idx(fcvId)) {
					xformMask |= ROT_MASK;
					FCurve fcv = get_fcv(fcvId);
					if (fcv.is_valid()) {
						rot0.set_at(j, fcv.eval((float)ifrm));
						rot1.set_at(j, fcv.eval((float)(ifrm + 1)));
					}
				}
			}
			cxQuat q0;
			q0.set_rot_degrees(rot0, pNode->mRotOrd);
			cxQuat q1;
			q1.set_rot_degrees(rot1, pNode->mRotOrd);
			cxQuat qr = nxQuat::slerp(q0, q1, frm - (float)ifrm);
			cxVec rv = qr.get_rot_degrees(pNode->mRotOrd);
			pRotVal->set_vec(rv);
		} else {
			for (int j = 0; j < 3; ++j) {
				int fcvId = pRotVal->fcvId[j];
				if (ck_fcv_idx(fcvId)) {
					xformMask |= ROT_MASK;
					FCurve fcv = get_fcv(fcvId);
					if (fcv.is_valid()) {
						pRotVal->f3[j] = fcv.eval(frm);
					}
				}
			}
		}
	}
	RigLink::Val* pSclVal = pNode->get_scl_val();
	if (pSclVal) {
		for (int j = 0; j < 3; ++j) {
			int fcvId = pSclVal->fcvId[j];
			if (ck_fcv_idx(fcvId)) {
				xformMask |= SCL_MASK;
				FCurve fcv = get_fcv(fcvId);
				if (fcv.is_valid()) {
					pSclVal->f3[j] = fcv.eval(frm);
				}
			}
		}
	}
	if (xformMask && pRig && pRigLocalMtx) {
		cxMtx sm;
		cxMtx rm;
		cxMtx tm;
		exRotOrd rord = pNode->mRotOrd;
		exTransformOrd xord = pNode->mXformOrd;
		if (xformMask & SCL_MASK) {
			sm.mk_scl(pSclVal->get_vec());
		} else {
			sm.mk_scl(pRig->get_lscl(pNode->mRigNodeId));
		}
		if (xformMask & ROT_MASK) {
			rm.set_rot_degrees(pRotVal->get_vec(), rord);
		} else {
			rm.set_rot_degrees(pRig->get_lrot(pNode->mRigNodeId));
		}
		if (xformMask & POS_MASK) {
			tm.mk_translation(pPosVal->get_vec());
		} else {
			tm.mk_translation(pRig->get_lpos(pNode->mRigNodeId));
		}
		pRigLocalMtx[pNode->mRigNodeId].calc_xform(tm, rm, sm, xord);
	}
}

void sxKeyframesData::eval_rig_link(RigLink* pLink, float frm, const sxRigData* pRig, cxMtx* pRigLocalMtx) const {
	if (!pLink) return;
	int n = pLink->mNodeNum;
	for (int i = 0; i < n; ++i) {
		eval_rig_link_node(pLink, i, frm, pRig, pRigLocalMtx);
	}
}

void sxKeyframesData::dump_clip(FILE* pOut) const {
	if (!pOut) return;
	int nfcv = get_fcv_num();
	int nfrm = get_frame_count();
	::fprintf(pOut, "{\n");
	::fprintf(pOut, "	rate = %d\n", (int)get_frame_rate());
	::fprintf(pOut, "	start = -1\n");
	::fprintf(pOut, "	tracklength = %d\n", nfrm);
	::fprintf(pOut, "	tracks = %d\n", nfcv);
	for (int i = 0; i < nfcv; ++i) {
		::fprintf(pOut, "   {\n");
		FCurve fcv = get_fcv(i);
		const char* pNodePath = fcv.get_node_path();
		const char* pNodeName = fcv.get_node_name();
		const char* pChanName = fcv.get_chan_name();
		::fprintf(pOut, "      name = %s/%s:%s\n", pNodePath, pNodeName, pChanName);
		::fprintf(pOut, "      data =");
		for (int j = 0; j < nfrm; ++j) {
			float frm = (float)j;
			float val = fcv.eval(frm);
			::fprintf(pOut, " %f", val);
		}
		::fprintf(pOut, "\n");
		::fprintf(pOut, "   }\n");
	}
	::fprintf(pOut, "}\n");
}

void sxKeyframesData::dump_clip(const char* pOutPath) const {
	char outPath[XD_MAX_PATH];
	if (!pOutPath) {
		const char* pSrcPath = get_file_path();
		if (!pSrcPath) {
			return;
		}
#if defined(_MSC_VER)
		::sprintf_s(outPath, sizeof(outPath), "%s.clip", pSrcPath);
#else
		::sprintf(outPath, "%s.clip", pSrcPath);
#endif
		pOutPath = outPath;
	}
	FILE* pOut = nxSys::fopen_w_txt(pOutPath);
	if (!pOut) {
		return;
	}
	dump_clip(pOut);
	::fclose(pOut);
}


enum class exExprFunc {
	_abs, _acos, _asin, _atan, _atan2, _ceil,
	_ch, _clamp, _cos, _deg, _detail, _distance, _exp,
	_fit, _fit01, _fit10, _fit11,
	_floor, _frac, _if, _int, _length,
	_log, _log10, _max, _min, _pow, _rad,
	_rint, _round, _sign, _sin, _sqrt, _tan
};


void sxCompiledExpression::Stack::alloc(int n) {
	free();
	if (n < 1) return;
	size_t memsize = n * sizeof(Val) + Tags::calc_mem_size(n);
	mpMem = nxCore::mem_alloc(memsize, XD_FOURCC('X', 'S', 'T', 'K'));
	if (mpMem) {
		::memset(mpMem, 0, memsize);
		mpVals = reinterpret_cast<Val*>(mpMem);
		mTags.init(mpVals + n, n);
	}
}

void sxCompiledExpression::Stack::free() {
	if (mpMem) {
		nxCore::mem_free(mpMem);
	}
	mpMem = nullptr;
	mpVals = nullptr;
	mPtr = 0;
	mTags.reset();
}

XD_NOINLINE void sxCompiledExpression::Stack::push_num(float num) {
	if (is_full()) {
		return;
	}
	mpVals[mPtr].num = num;
	mTags.set_num(mPtr);
	++mPtr;
}

XD_NOINLINE float sxCompiledExpression::Stack::pop_num() {
	if (is_empty()) {
		return 0.0f;
	}
	--mPtr;
	if (mTags.is_num(mPtr)) {
		return mpVals[mPtr].num;
	}
	return 0.0f;
}

XD_NOINLINE void sxCompiledExpression::Stack::push_str(int ptr) {
	if (is_full()) {
		return;
	}
	mpVals[mPtr].sptr = ptr;
	mTags.set_str(mPtr);
	++mPtr;
}

XD_NOINLINE int sxCompiledExpression::Stack::pop_str() {
	if (is_empty()) {
		return -1;
	}
	--mPtr;
	if (mTags.is_str(mPtr)) {
		return mpVals[mPtr].sptr;
	}
	return -1;
}

void sxCompiledExpression::get_str(int idx, String* pStr) const {
	if (!pStr) return;
	const StrInfo* pInfo = get_str_info(idx);
	if (pInfo) {
		pStr->mpChars = reinterpret_cast<const char*>(XD_INCR_PTR(&get_str_info_top()[mStrsNum], pInfo->mOffs));
		pStr->mHash = pInfo->mHash;
		pStr->mLen = pInfo->mLen;
	} else {
		pStr->mpChars = nullptr;
		pStr->mHash = 0;
		pStr->mLen = 0;
	}
}

sxCompiledExpression::String sxCompiledExpression::get_str(int idx) const {
	String str;
	get_str(idx, &str);
	return str;
}

static inline float expr_acos(float val) {
	return XD_RAD2DEG(::acosf(nxCalc::clamp(val, -1.0f, 1.0f)));
}

static inline float expr_asin(float val) {
	return XD_RAD2DEG(::asinf(nxCalc::clamp(val, -1.0f, 1.0f)));
}

static inline float expr_atan(float val) {
	return XD_RAD2DEG(::atanf(val));
}

static inline float expr_atan2(float y, float x) {
	return XD_RAD2DEG(::atan2f(y, x));
}

static inline float expr_dist(sxCompiledExpression::Stack* pStk) {
	cxVec p1;
	p1.z = pStk->pop_num();
	p1.y = pStk->pop_num();
	p1.x = pStk->pop_num();
	cxVec p0;
	p0.z = pStk->pop_num();
	p0.y = pStk->pop_num();
	p0.x = pStk->pop_num();
	return nxVec::dist(p0, p1);
}

static inline float expr_fit(sxCompiledExpression::Stack* pStk) {
	float newmax = pStk->pop_num();
	float newmin = pStk->pop_num();
	float oldmax = pStk->pop_num();
	float oldmin = pStk->pop_num();
	float num = pStk->pop_num();
	return nxCalc::fit(num, oldmin, oldmax, newmin, newmax);
}

static inline float expr_fit01(sxCompiledExpression::Stack* pStk) {
	float newmax = pStk->pop_num();
	float newmin = pStk->pop_num();
	float num = nxCalc::saturate(pStk->pop_num());
	return nxCalc::fit(num, 0.0f, 1.0f, newmin, newmax);
}

static inline float expr_fit10(sxCompiledExpression::Stack* pStk) {
	float newmax = pStk->pop_num();
	float newmin = pStk->pop_num();
	float num = nxCalc::saturate(pStk->pop_num());
	return nxCalc::fit(num, 1.0f, 0.0f, newmin, newmax);
}

static inline float expr_fit11(sxCompiledExpression::Stack* pStk) {
	float newmax = pStk->pop_num();
	float newmin = pStk->pop_num();
	float num = nxCalc::clamp(pStk->pop_num(), -1.0f, 1.0f);
	return nxCalc::fit(num, -1.0f, 1.0f, newmin, newmax);
}

static inline float expr_len(sxCompiledExpression::Stack* pStk) {
	cxVec v;
	v.z = pStk->pop_num();
	v.y = pStk->pop_num();
	v.x = pStk->pop_num();
	return v.mag();
}

void sxCompiledExpression::exec(ExecIfc& ifc) const {
	Stack* pStk = ifc.get_stack();
	if (!pStk) {
		return;
	}

	float valA;
	float valB;
	float valC;
	float cmpRes;
	int funcId;
	String str1;
	String str2;
	int n = mCodeNum;
	const Code* pCode = get_code_top();
	for (int i = 0; i < n; ++i) {
		eOp op = pCode->get_op();
		if (op == eOp::END) break;
		switch (op) {
		case eOp::NUM:
			pStk->push_num(get_val(pCode->mInfo));
			break;
		case eOp::STR:
			pStk->push_str(pCode->mInfo);
			break;
		case eOp::VAR:
			get_str(pStk->pop_str(), &str1);
			pStk->push_num(ifc.var(str1));
			break;
		case eOp::CMP:
			valB = pStk->pop_num();
			valA = pStk->pop_num();
			cmpRes = 0.0f;
			switch ((eCmp)pCode->mInfo) {
			case eCmp::EQ:
				cmpRes = valA == valB ? 1.0f : 0.0f;
				break;
			case eCmp::NE:
				cmpRes = valA != valB ? 1.0f : 0.0f;
				break;
			case eCmp::LT:
				cmpRes = valA < valB ? 1.0f : 0.0f;
				break;
			case eCmp::LE:
				cmpRes = valA <= valB ? 1.0f : 0.0f;
				break;
			case eCmp::GT:
				cmpRes = valA > valB ? 1.0f : 0.0f;
				break;
			case eCmp::GE:
				cmpRes = valA >= valB ? 1.0f : 0.0f;
				break;
			}
			pStk->push_num(cmpRes);
			break;
		case eOp::ADD:
			valB = pStk->pop_num();
			valA = pStk->pop_num();
			pStk->push_num(valA + valB);
			break;
		case eOp::SUB:
			valB = pStk->pop_num();
			valA = pStk->pop_num();
			pStk->push_num(valA - valB);
			break;
		case eOp::MUL:
			valB = pStk->pop_num();
			valA = pStk->pop_num();
			pStk->push_num(valA * valB);
			break;
		case eOp::DIV:
			valB = pStk->pop_num();
			valA = pStk->pop_num();
			pStk->push_num(nxCalc::div0(valA, valB));
			break;
		case eOp::MOD:
			valB = pStk->pop_num();
			valA = pStk->pop_num();
			pStk->push_num(valB != 0.0f ? ::fmodf(valA, valB) : 0.0f);
			break;
		case eOp::NEG:
			valA = pStk->pop_num();
			pStk->push_num(-valA);
			break;
		case eOp::FUN:
			funcId = pCode->mInfo;
			switch ((exExprFunc)funcId) {
			case exExprFunc::_abs:
				valA = pStk->pop_num();
				pStk->push_num(::fabsf(valA));
				break;
			case exExprFunc::_acos:
				valA = pStk->pop_num();
				pStk->push_num(expr_acos(valA));
				break;
			case exExprFunc::_asin:
				valA = pStk->pop_num();
				pStk->push_num(expr_asin(valA));
				break;
			case exExprFunc::_atan:
				valA = pStk->pop_num();
				pStk->push_num(expr_atan(valA));
				break;
			case exExprFunc::_atan2:
				valB = pStk->pop_num();
				valA = pStk->pop_num();
				pStk->push_num(expr_atan2(valA, valB));
				break;
			case exExprFunc::_ceil:
				valA = pStk->pop_num();
				pStk->push_num(::ceilf(valA));
				break;
			case exExprFunc::_ch:
				get_str(pStk->pop_str(), &str1);
				pStk->push_num(ifc.ch(str1));
				break;
			case exExprFunc::_clamp:
				valC = pStk->pop_num();
				valB = pStk->pop_num();
				valA = pStk->pop_num();
				pStk->push_num(nxCalc::clamp(valA, valB, valC));
				break;
			case exExprFunc::_cos:
				valA = pStk->pop_num();
				pStk->push_num(::cosf(XD_DEG2RAD(valA)));
				break;
			case exExprFunc::_deg:
				valA = pStk->pop_num();
				pStk->push_num(XD_RAD2DEG(valA));
				break;
			case exExprFunc::_detail:
				valA = pStk->pop_num();
				get_str(pStk->pop_str(), &str2);
				get_str(pStk->pop_str(), &str1);
				pStk->push_num(ifc.detail(str1, str2, (int)valA));
				break;
			case exExprFunc::_distance:
				pStk->push_num(expr_dist(pStk));
				break;
			case exExprFunc::_exp:
				valA = pStk->pop_num();
				pStk->push_num(::expf(valA));
				break;
			case exExprFunc::_fit:
				pStk->push_num(expr_fit(pStk));
				break;
			case exExprFunc::_fit01:
				pStk->push_num(expr_fit01(pStk));
				break;
			case exExprFunc::_fit10:
				pStk->push_num(expr_fit10(pStk));
				break;
			case exExprFunc::_fit11:
				pStk->push_num(expr_fit11(pStk));
				break;
			case exExprFunc::_floor:
				valA = pStk->pop_num();
				pStk->push_num(::floorf(valA));
				break;
			case exExprFunc::_frac:
				valA = pStk->pop_num();
				pStk->push_num(valA - ::floorf(valA)); // Houdini-compatible: frac(-2.7) = 0.3
				break;
			case exExprFunc::_if:
				valC = pStk->pop_num();
				valB = pStk->pop_num();
				valA = pStk->pop_num();
				pStk->push_num(valA != 0.0f ? valB : valC);
				break;
			case exExprFunc::_int:
				valA = pStk->pop_num();
				pStk->push_num(::truncf(valA));
				break;
			case exExprFunc::_length:
				pStk->push_num(expr_len(pStk));
				break;
			case exExprFunc::_log:
				valA = pStk->pop_num();
				pStk->push_num(::logf(valA));
				break;
			case exExprFunc::_log10:
				valA = pStk->pop_num();
				pStk->push_num(::log10f(valA));
				break;
			case exExprFunc::_max:
				valB = pStk->pop_num();
				valA = pStk->pop_num();
				pStk->push_num(nxCalc::max(valA, valB));
				break;
			case exExprFunc::_min:
				valB = pStk->pop_num();
				valA = pStk->pop_num();
				pStk->push_num(nxCalc::min(valA, valB));
				break;
			case exExprFunc::_pow:
				valB = pStk->pop_num();
				valA = pStk->pop_num();
				pStk->push_num(::powf(valA, valB));
				break;
			case exExprFunc::_rad:
				valA = pStk->pop_num();
				pStk->push_num(XD_DEG2RAD(valA));
				break;
			case exExprFunc::_rint:
			case exExprFunc::_round:
				valA = pStk->pop_num();
				pStk->push_num(::roundf(valA));
				break;
			case exExprFunc::_sign:
				valA = pStk->pop_num();
				pStk->push_num(valA == 0.0f ? 0 : valA < 0.0f ? -1.0f : 1.0f);
				break;
			case exExprFunc::_sin:
				valA = pStk->pop_num();
				pStk->push_num(::sinf(XD_DEG2RAD(valA)));
				break;
			case exExprFunc::_sqrt:
				valA = pStk->pop_num();
				pStk->push_num(::sqrtf(valA));
				break;
			case exExprFunc::_tan:
				valA = pStk->pop_num();
				pStk->push_num(::tanf(XD_DEG2RAD(valA)));
				break;
			default:
				break;
			}
			break;
		case eOp::XOR:
			valB = pStk->pop_num();
			valA = pStk->pop_num();
			pStk->push_num((float)((int)valA ^ (int)valB));
			break;
		case eOp::AND:
			valB = pStk->pop_num();
			valA = pStk->pop_num();
			pStk->push_num((float)((int)valA & (int)valB));
			break;
		case eOp::OR:
			valB = pStk->pop_num();
			valA = pStk->pop_num();
			pStk->push_num((float)((int)valA | (int)valB));
			break;
		default:
			break;
		}

		++pCode;
	}
	float res = pStk->pop_num();
	ifc.set_result(res);
}

static const char* s_exprOpNames[] = {
	"NOP", "END", "NUM", "STR", "VAR", "CMP", "ADD", "SUB", "MUL", "DIV", "MOD", "NEG", "FUN", "XOR", "AND", "OR"
};

static const char* s_exprCmpNames[] = {
	"EQ", "NE", "LT", "LE", "GT", "GE"
};

static const char* s_exprFuncNames[] = {
	"abs", "acos", "asin", "atan", "atan2", "ceil",
	"ch", "clamp", "cos", "deg", "detail", "distance",
	"exp", "fit", "fit01", "fit10", "fit11",
	"floor", "frac", "if", "int", "length",
	"log", "log10", "max", "min", "pow", "rad",
	"rint", "round", "sign", "sin", "sqrt", "tan"
};

void sxCompiledExpression::disasm(FILE* pFile) const {
	int n = mCodeNum;
	const Code* pCode = get_code_top();
	char abuf[128];
	for (int i = 0; i < n; ++i) {
		int addr = i;
		eOp op = pCode->get_op();
		const char* pOpName = "<bad>";
		if ((uint32_t)op < (uint32_t)XD_ARY_LEN(s_exprOpNames)) {
			pOpName = s_exprOpNames[(int)op];
		}
		const char* pArg = "";
		if (op == eOp::CMP) {
			eCmp cmp = (eCmp)pCode->mInfo;
			pArg = "<bad>";
			if ((uint32_t)cmp < (uint32_t)XD_ARY_LEN(s_exprCmpNames)) {
				pArg = s_exprCmpNames[(int)cmp];
			}
		} else if (op == eOp::NUM) {
			float num = get_val(pCode->mInfo);
#if defined(_MSC_VER)
			::sprintf_s(abuf, sizeof(abuf), "%f", num);
#else
			::sprintf(abuf, "%f", num);
#endif
			pArg = abuf;
		} else if (op == eOp::STR) {
			String str = get_str(pCode->mInfo);
			if (str.is_valid()) {
#if defined(_MSC_VER)
				::sprintf_s(abuf, sizeof(abuf), "%s", str.mpChars);
#else
				::sprintf(abuf, "%s", str.mpChars);
#endif
				pArg = abuf;
			}
		} else if (op == eOp::FUN) {
			int func = pCode->mInfo;
			pArg = "<bad>";
			if ((uint32_t)func < (uint32_t)XD_ARY_LEN(s_exprFuncNames)) {
				pArg = s_exprFuncNames[func];
			}
		}
		::fprintf(pFile, "0x%02X: %s %s\n", addr, pOpName, pArg);

		++pCode;
	}
}

int sxExprLibData::find_expr_idx(const char* pNodeName, const char* pChanName, const char* pNodePath, int startIdx) const {
	int idx = -1;
	sxStrList* pStrLst = get_str_list();
	if (ck_expr_idx(startIdx) && pStrLst) {
		int nameId = pStrLst->find_str(pNodeName);
		int chanId = pStrLst->find_str(pChanName);
		int pathId = pStrLst->find_str(pNodePath);
		bool doneFlg = (pNodeName && (nameId < 0)) || (pChanName && (chanId < 0)) || (pNodePath && pathId < 0);
		if (!doneFlg) {
			int n = get_expr_num();
			for (int i = startIdx; i < n; ++i) {
				const ExprInfo* pInfo = get_info(i);
				bool nameFlg = pNodeName ? pInfo->mNodeNameId == nameId : true;
				bool chanFlg = pChanName ? pInfo->mChanNameId == chanId : true;
				bool pathFlg = pNodePath ? pInfo->mNodePathId == pathId : true;
				if (nameFlg && chanFlg && pathFlg) {
					idx = i;
					break;
				}
			}
		}
	}
	return idx;
}

sxExprLibData::Entry sxExprLibData::get_entry(int idx) const {
	Entry ent;
	if (ck_expr_idx(idx)) {
		ent.mpLib = this;
		ent.mExprId = idx;
	} else {
		ent.mpLib = nullptr;
		ent.mExprId = -1;
	}
	return ent;
}

int sxExprLibData::count_rig_entries(const sxRigData& rig) const {
	int n = 0;
	sxStrList* pStrLst = get_str_list();
	for (int i = 0; i < rig.get_nodes_num(); ++i) {
		const char* pNodeName = rig.get_node_name(i);
		const char* pNodePath = rig.get_node_path(i);
		int nameId = pStrLst->find_str(pNodeName);
		int pathId = pStrLst->find_str(pNodePath);
		if (nameId >= 0) {
			for (int j = 0; j < get_expr_num(); ++j) {
				const ExprInfo* pInfo = get_info(j);
				bool nameFlg = pInfo->mNodeNameId == nameId;
				bool pathFlg = pathId < 0 ? true : pInfo->mNodePathId == pathId;
				if (nameFlg && pathFlg) {
					++n;
				}
			}
		}
	}
	return n;
}

int sxFileCatalogue::find_name_idx(const char* pName) const {
	int idx = -1;
	if (pName) {
		sxStrList* pStrLst = get_str_list();
		int nameId = pStrLst->find_str(pName);
		if (nameId >= 0) {
			const FileInfo* pInfo = get_info(0);
			for (uint32_t i = 0; i < mFilesNum; ++i) {
				if (pInfo->mNameId == nameId) {
					idx = i;
					break;
				}
				++pInfo;
			}
		}
	}
	return idx;
}


#define XD_STRSTORE_ALLOC_SIZE size_t(4096)
#define XD_STRSTORE_TAG XD_FOURCC('S','T','O','R')

cxStrStore* cxStrStore::create() {
	const size_t defSize = XD_STRSTORE_ALLOC_SIZE;
	size_t size = sizeof(cxStrStore) + defSize;
	cxStrStore* pStore = reinterpret_cast<cxStrStore*>(nxCore::mem_alloc(size, XD_STRSTORE_TAG));
	if (pStore) {
		pStore->mSize = defSize;
		pStore->mpNext = nullptr;
		pStore->mPtr = 0;
	}
	return pStore;
}

void cxStrStore::destroy(cxStrStore* pStore) {
	cxStrStore* p = pStore;
	while (p) {
		cxStrStore* pNext = p->mpNext;
		nxCore::mem_free(p);
		p = pNext;
	}
}

char* cxStrStore::add(const char* pStr) {
	char* pRes = nullptr;
	if (pStr) {
		cxStrStore* pStore = this;
		size_t len = ::strlen(pStr) + 1;
		while (!pRes) {
			size_t free = pStore->mSize - pStore->mPtr;
			if (free >= len) {
				pRes = reinterpret_cast<char*>(pStore + 1) + pStore->mPtr;
				::memcpy(pRes, pStr, len);
				pStore->mPtr += len;
			} else {
				cxStrStore* pNext = pStore->mpNext;
				if (!pNext) {
					size_t size = sizeof(cxStrStore) + nxCalc::max(XD_STRSTORE_ALLOC_SIZE, len);
					pNext = reinterpret_cast<cxStrStore*>(nxCore::mem_alloc(size, XD_STRSTORE_TAG));
					if (pNext) {
						pNext->mSize = size;
						pNext->mpNext = nullptr;
						pNext->mPtr = 0;
					} else {
						break;
					}
				}
				pStore = pNext;
			}
		}
	}
	return pRes;
}


void cxCmdLine::ctor(int argc, char* argv[]) {
	mpStore = nullptr;
	mpOptMap = nullptr;
	if (argc < 1 || !argv) return;
	mpStore = cxStrStore::create();
	if (!mpStore) return;
	mpProgPath = mpStore->add(argv[0]);
	if (argc < 2) return;
	mpOptMap = MapT::create();
	mpArgLst = ListT::create();
	for (int i = 1; i < argc; ++i) {
		char* pArg = argv[i];
		if (nxCore::str_starts_with(pArg, "-")) {
			int argLen = int(::strlen(pArg));
			char* pVal = nullptr;
			for (int j = argLen; --j >= 1;) {
				if (pArg[j] == ':') {
					pVal = &pArg[j + 1];
					break;
				}
			}
			if (pVal && pVal != pArg + argLen) {
				char* pOptStr = mpStore->add(pArg + 1);
				char* pValStr = &pOptStr[pVal - (pArg + 1)];
				pValStr[-1] = 0;
				if (mpOptMap) {
					mpOptMap->put(pOptStr, pValStr);
				}
			}
		} else {
			char* pArgStr = mpStore->add(pArg);
			if (pArgStr && mpArgLst) {
				char** pp = mpArgLst->new_item();
				if (pp) {
					*pp = pArgStr;
				}
			}
		}
	}
}

XD_NOINLINE void cxCmdLine::dtor() {
	ListT::destroy(mpArgLst);
	mpArgLst = nullptr;
	MapT::destroy(mpOptMap);
	mpOptMap = nullptr;
	cxStrStore::destroy(mpStore);
	mpStore = nullptr;
}

XD_NOINLINE const char* cxCmdLine::get_arg(int i) const {
	const char* pArg = nullptr;
	if (mpArgLst) {
		char** ppArg = mpArgLst->get_item(i);
		if (ppArg) {
			pArg = *ppArg;
		}
	}
	return pArg;
}

XD_NOINLINE const char* cxCmdLine::get_opt(const char* pName) const {
	char* pVal = nullptr;
	if (mpOptMap) {
		mpOptMap->get(pName, &pVal);
	}
	return pVal;
}
