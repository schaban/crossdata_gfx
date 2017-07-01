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
	uint32_t mTag;
	uint32_t mOffs;
};

static sxMemInfo* s_pMemHead = nullptr;
static sxMemInfo* s_pMemTail = nullptr;
static uint32_t s_allocCount = 0;
static uint64_t s_allocBytes = 0;
static uint64_t s_allocPeakBytes = 0;

void* mem_alloc(size_t size, uint32_t tag) {
	void* p = nullptr;
	if (size > 0) {
		size_t asize = XD_ALIGN(size + sizeof(sxMemInfo) + 0x10, 0x10);
		void* p0 = nxSys::malloc(asize);
		if (p0) {
			p = (void*)XD_ALIGN((intptr_t)((uint8_t*)p0 + sizeof(sxMemInfo)), 0x10);
			sxMemInfo* pInfo = &((sxMemInfo*)p)[-1];
			pInfo->mSize = size;
			pInfo->mTag = tag;
			pInfo->mOffs = (uint32_t)((uint8_t*)p - (uint8_t*)p0);
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

void mem_free(void* pMem) {
	if (!pMem) return;
	sxMemInfo* pInfo = &((sxMemInfo*)pMem)[-1];
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

void mem_dbg() {
	dbg_msg("%d allocs\n", s_allocCount);
	sxMemInfo* pInfo = s_pMemHead;
	char tag[5];
	tag[4] = 0;
	while (pInfo) {
		void* pMem = pInfo + 1;
		::memcpy(tag, &pInfo->mTag, 4);
		dbg_msg("%s @ %p, size=0x%X\n", tag, pMem, pInfo->mSize);
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

void* bin_load(const char* pPath, size_t* pSize, bool appendPath) {
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
		pData = mem_alloc(memsize, XD_DAT_MEM_TAG);
		if (pData) {
			size = nxSys::fread(fh, pData, fsize);
		}
		nxSys::fclose(fh);
		if (appendPath) {
			nxSys::x_strcpy(&((char*)pData)[fsize], pathLen + 1, pPath);
		}
	}
	if (pSize) {
		*pSize = size;
	}
	return pData;
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
	float rel = (val - oldMin) / (oldMax - oldMin);
	return lerp(newMin, newMax, rel);
}

float calc_fovy(float focal, float aperture, float aspect) {
	float zoom = ((2.0f * focal) / aperture) * aspect;
	float fovy = 2.0f * ::atan2f(1.0f, zoom);
	return fovy;
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

// http://www.insomniacgames.com/mike-day-vector-length-and-normalization-difficulties/
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

// http://lgdv.cs.fau.de/publications/publication/Pub.2010.tech.IMMD.IMMD9.onfloa/
// http://jcgt.org/published/0003/02/01/
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
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				m[i][j] = im.m[i][j] * idet;
			}
		}
	}
}

void cxMtx::invert(const cxMtx& mtx) {
	::memcpy(this, mtx, sizeof(cxMtx));
	invert();
}

void cxMtx::mul(const cxMtx& mtx) {
	mul(*this, mtx);
}

void cxMtx::mul(const cxMtx& m1, const cxMtx& m2) {
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
	// http://www.insomniacgames.com/mike-day-extracting-euler-angles-from-a-rotation-matrix/
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
	float x = v.x;
	float y = v.y;
	float z = v.z;
	float tx = x*m[0][0] + y*m[1][0] + z*m[2][0];
	float ty = x*m[0][1] + y*m[1][1] + z*m[2][1];
	float tz = x*m[0][2] + y*m[1][2] + z*m[2][2];
	return cxVec(tx, ty, tz);
}

cxVec cxMtx::calc_pnt(const cxVec& v) const {
	float x = v.x;
	float y = v.y;
	float z = v.z;
	float tx = x*m[0][0] + y*m[1][0] + z*m[2][0] + m[3][0];
	float ty = x*m[0][1] + y*m[1][1] + z*m[2][1] + m[3][1];
	float tz = x*m[0][2] + y*m[1][2] + z*m[2][2] + m[3][2];
	return cxVec(tx, ty, tz);
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


namespace nxSH {

void calc_weights(float* pWgt, int order, float s, float scl) {
	if (!pWgt) return;
	for (int i = 0; i < order; ++i) {
		pWgt[i] = ::expf(float(-i*i) / (2.0f*s)) * scl;
	}
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
	sxData* pData = reinterpret_cast<sxData*>(nxCore::bin_load(pPath, &size, true));
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

} // nxData


XD_NOINLINE int sxStrList::find_str(const char* pStr) const {
	if (!pStr) return -1;
	uint16_t h = nxCore::str_hash16(pStr);
	uint16_t* pHash = get_hash_top();
	for (uint32_t i = 0; i < mNum; ++i) {
		if (h == pHash[i]) {
			if (nxCore::str_eq(get_str(i), pStr)) {
				return i;
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

int sxValuesData::Group::get_int(const char* pName, const int defVal) const {
	int i = defVal;
	int idx = find_val_idx(pName);
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

cxColor sxValuesData::Group::get_rgb(const char* pName, const cxColor& defVal) const {
	cxColor c = defVal;
	int idx = find_val_idx(pName);
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
		parentMtx.mul(pMtxLocal[pNode->mSelfIdx]);
		mtx.mul(parentMtx);
	}
	if (pParentWMtx) {
		*pParentWMtx = parentMtx;
	}
	return mtx;
}

void sxRigData::calc_ik_chain(IKChain& chain) {
	if (!chain.mpMtxL) return;
	if (!ck_node_idx(chain.mTopCtrl)) return;
	if (!ck_node_idx(chain.mEndCtrl)) return;
	if (!ck_node_idx(chain.mTop)) return;
	if (!ck_node_idx(chain.mRot)) return;
	if (!ck_node_idx(chain.mEnd)) return;
	int parentIdx = get_parent_idx(chain.mTopCtrl);
	if (!ck_node_idx(parentIdx)) return;
	bool isExt = ck_node_idx(chain.mExtCtrl);
	int rootIdx = isExt ? get_parent_idx(chain.mExtCtrl) : get_parent_idx(chain.mEndCtrl);
	if (!ck_node_idx(rootIdx)) return;

	cxMtx parentW;
	cxMtx topW;
	if (chain.mpMtxW) {
		topW = chain.mpMtxW[chain.mTopCtrl];
		parentW = chain.mpMtxW[parentIdx];
	} else {
		topW = calc_wmtx(chain.mTopCtrl, chain.mpMtxL, &parentW);
	}

	cxMtx rootW;
	if (chain.mpMtxW) {
		rootW = chain.mpMtxW[rootIdx];
	} else {
		rootW = calc_wmtx(rootIdx, chain.mpMtxL);
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
				idx = pIdx[wgtIdx * 2];
				idx |= pIdx[wgtIdx * 2 + 1] << 8;
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


namespace nxTexture {

sxDDSHead* alloc_dds128(int w, int h, uint32_t* pSize) {
	uint32_t npix = w * h;
	uint32_t dataSize = npix * sizeof(cxColor);
	uint32_t size = sizeof(sxDDSHead) + dataSize;
	sxDDSHead* pDDS = reinterpret_cast<sxDDSHead*>(nxCore::mem_alloc(size, XD_FOURCC('X', 'D', 'D', 'S')));
	if (pDDS) {
		::memset(pDDS, 0, sizeof(sxDDSHead));
		pDDS->mMagic = XD_FOURCC('D', 'D', 'S', ' ');
		pDDS->mSize = sizeof(sxDDSHead) - 4;
		pDDS->mFlags = 0x081007;
		pDDS->mHeight = h;
		pDDS->mWidth = w;
		pDDS->mPitchLin = dataSize;
		pDDS->mFormat.mSize = 0x20;
		pDDS->mFormat.mFlags = 4;
		pDDS->mFormat.mFourCC = 0x74;
		pDDS->mCaps1 = 0x1000;
		if (pSize) {
			*pSize = size;
		}
	}
	return pDDS;
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

void sxTextureData::DDS::release() {
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

/* see PBRT book for details */
static void calc_resample_wgts(int oldRes, int newRes, xt_float4* pWgt, int16_t* pOrg) {
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
		calc_resample_wgts(w0, baseW, pWgt, pOrg);
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
		calc_resample_wgts(h0, baseH, pWgt, pOrg);
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
								pRigMap[rigNodeId] = i;
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

void sxKeyframesData::eval_rig_link(RigLink* pLink, float frm, const sxRigData* pRig, cxMtx* pRigLocalMtx) const {
	if (!pLink) return;
	const int POS_MASK = 1<<0;
	const int ROT_MASK = 1<<1;
	const int SCL_MASK = 1<<2;
	int n = pLink->mNodeNum;
	for (int i = 0; i < n; ++i) {
		int xformMask = 0;
		RigLink::Node* pNode = &pLink->mNodes[i];
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
							rot1.set_at(j, fcv.eval((float)(ifrm+1)));
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

