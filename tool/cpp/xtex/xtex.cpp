#ifndef _CRT_SECURE_NO_DEPRECATE
#	define _CRT_SECURE_NO_DEPRECATE
#endif
#pragma warning(disable: 4996)

#include <UT/UT_VectorTypes.h>
#include <SYS/SYS_Types.h>
#include <UT/UT_Version.h>
#include <PRM/PRM_Include.h>
#include <OP/OP_Director.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Vector3.h>
#include <TIL/TIL_Plane.h>
#include <TIL/TIL_Sequence.h>
#include <TIL/TIL_Region.h>
#include <TIL/TIL_Raster.h>
#include <COP2/COP2_Node.h>
#include <COP2/COP2_Resolver.h>
#include <CMD/CMD_Args.h>
#include <CMD/CMD_Manager.h>

#include <intrin.h>
#include <windows.h>
#include <tchar.h>

using namespace std;

#define _FOURCC(c1, c2, c3, c4) ((((uint8)(c4))<<24)|(((uint8)(c3))<<16)|(((uint8)(c2))<<8)|((uint8)(c1)))


static void bin_save(const char* fname, void* pData, int len) {
	FILE* f;
	::fopen_s(&f, fname, "w+b");
	if (f) {
		::fwrite(pData, 1, len, f);
		::fclose(f);
	}
}

static inline uint32 align32(uint32 x, uint32 y) {
	return ((x + (y - 1)) & (~(y - 1)));
}

class cBin {
private:
	FILE* mpFile;

public:
	cBin() : mpFile(0) {
	}
	~cBin() {
		close();
	}

	void open(const char* fpath) {
		::fopen_s(&mpFile, fpath, "w+b");
	}

	void close() {
		if (mpFile) {
			::fclose(mpFile);
			mpFile = 0;
		}
	}

	void write(const void* pData, int size) {
		if (mpFile) {
			::fwrite(pData, 1, size, mpFile);
		}
	}

	void write_f32(float val) {
		write(&val, 4);
	}

	void write_u32(uint32 val) {
		write(&val, 4);
	}

	void write_u16(uint16 val) {
		write(&val, 2);
	}

	void write_u8(uint8 val) {
		write(&val, 1);
	}

	uint32 get_pos() const {
		if (mpFile) {
			return (uint32)::ftell(mpFile);
		}
		return 0;
	}

	void seek(uint32 pos) {
		if (mpFile) {
			::fseek(mpFile, pos, SEEK_SET);
		}
	}

	void patch_u32(uint32 pos, uint32 val) {
		uint32 nowPos = get_pos();
		seek(pos);
		write_u32(val);
		seek(nowPos);
	}

	void patch_u16(uint32 pos, uint16 val) {
		uint32 nowPos = get_pos();
		seek(pos);
		write_u16(val);
		seek(nowPos);
	}

	void patch_u8(uint32 pos, uint8 val) {
		uint32 nowPos = get_pos();
		seek(pos);
		write_u8(val);
		seek(nowPos);
	}

	void patch_cur(uint32 pos) {
		patch_u32(pos, get_pos());
	}

	void align(int x = 0x10) {
		uint32 pos0 = get_pos();
		uint32 pos1 = align32(pos0, x);
		uint32 n = pos1 - pos0;
		for (uint32 i = 0; i < n; ++i) {
			write_u8(0xFF);
		}
	}

	void write_str(const char* pStr) {
		if (!pStr) return;
		int len = ::strlen(pStr);
		write(pStr, len+1);
	}
};

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

inline uint32_t bit_scan_fwd(uint32_t mask) {
	unsigned long idx;
	::_BitScanForward(&idx, mask);
	return (uint32_t)idx;
}

inline uint32_t bit_scan_rev(uint32_t mask) {
	unsigned long idx;
	::_BitScanReverse(&idx, mask);
	return (uint32_t)idx;
}

inline uint32_t clz32(uint32_t x) {
	if (x) return 31 - bit_scan_rev(x);
	return 32;
}

inline uint32_t ctz32(uint32_t x) {
	if (x) return bit_scan_fwd(x);
	return 32;
}

inline uint32_t bit_len32(uint32_t x) {
	return 32 - clz32(x);
}

uint32_t f32_get_bits(float x) {
	union {
		float f;
		uint32_t u;
	} v;
	v.f = x;
	return v.u;
}

struct sCompressedPlane {
	uint8_t* mpData;
	int mFormat;
	int mBitCnt;
	int mMinTZ;
	float mMinVal;
	float mMaxVal;
	float mConstVal;
	float mValOffs;

	void free() {
		if (mpData) {
			delete[] mpData;
			mpData = nullptr;
		}
		mBitCnt = 0;
	}

	bool is_const() const { return mFormat == 0; }

	int get_byte_count() const {
		int n = mBitCnt >> 3;
		if (mBitCnt & 7) ++n;
		return n;
	}

	bool data_cmp(const sCompressedPlane& pln) const {
		if (!mpData) return false;
		if (!pln.mpData) return false;
		if (is_const()) return false;
		if (mBitCnt != pln.mBitCnt) return false;
		return ::memcmp(mpData, pln.mpData, get_byte_count()) == 0;
	}

	uint32_t write_info(cBin& bin, int nameId) {
		uint32_t top = bin.get_pos();
		bin.write_u32(0); // +00 -> data
		bin.write_u16(nameId); // +04
		bin.write_u8(mMinTZ); // +06
		bin.write_u8(mFormat); // +07
		bin.write_f32(mMinVal); // +08
		bin.write_f32(mMaxVal); // +0C
		bin.write_f32(mValOffs); // +10
		bin.write_u32(mBitCnt); // +14
		bin.write_u32(0); // +18 reserved0
		bin.write_u32(0); // +1C reserved1
		return top;
	}
};

static sCompressedPlane compress(float* pSrc, int w, int h) {
	sCompressedPlane cp;
	::memset(&cp, 0, sizeof(cp));
	cp.mMinTZ = 32;
	if (!pSrc) return cp;
	int n = w*h;
	bool isConst = true;
	float val0 = pSrc[0];
	cp.mMinVal = val0;
	cp.mMaxVal = val0;
	for (int i = 1; i < n; ++i) {
		if (pSrc[i] != val0) {
			isConst = false;
		}
	}
	if (isConst) {
		cp.mConstVal = val0;
		return cp;
	}

	size_t tmpsize = n * 4;
	uint8_t* pBits = new uint8_t[tmpsize];
	::memset(pBits, 0, tmpsize);

	for (int i = 1; i < n; ++i) {
		float val = pSrc[i];
		if (val < cp.mMinVal) cp.mMinVal = val;
		if (val > cp.mMaxVal) cp.mMaxVal = val;
	}

	cp.mValOffs = cp.mMinVal < 0 ? cp.mMinVal : 0;

	for (int i = 0; i < n; ++i) {
		float fval = pSrc[i];
		fval -= cp.mValOffs;
		if (fval < 0.0f) fval = 0.0f;
		uint32_t ival = f32_get_bits(fval) & ((1U << 31) - 1);
		uint32_t tz = ctz32(ival);
		if (tz < cp.mMinTZ) cp.mMinTZ = tz;
	}

	const int tblSize = 1 << 8;
	uint32_t tbl[tblSize];
	::memset(tbl, 0, sizeof(tbl));
	uint32_t pred = 0;
	uint32_t hash = 0;
	const int nlenBits = 5;
	union BIT_BUF {
		uint64_t u64;
		uint8_t b[8];
	} bitBuf;
	for (int i = 0; i < n; ++i) {
		float fval = pSrc[i];
		fval -= cp.mValOffs;
		if (fval < 0.0f) fval = 0.0f;
		uint32_t ival = f32_get_bits(fval) & ((1U << 31) - 1);
		ival >>= cp.mMinTZ;
		uint32_t xr = ival ^ pred;
		tbl[hash] = ival;
		hash = ival >> 21;
		hash &= tblSize - 1;
		pred = tbl[hash];
		uint32_t xlen = 0;
		if (xr) {
			xlen = bit_len32(xr);
		}
		uint64_t dat = xlen;
		if (xlen) {
			dat |= ((uint64_t)xr & (((uint64_t)1 << xlen) - 1)) << nlenBits;
		}
		bitBuf.u64 = dat;
		bitBuf.u64 <<= cp.mBitCnt & 7;
		int bitPtr = cp.mBitCnt >> 3;
		for (int j = 0; j < 6; ++j) {
			pBits[bitPtr + j] |= bitBuf.b[j];
		}
		cp.mBitCnt += nlenBits + xlen;
	}

	cp.mFormat = 1;
	int nbytes = cp.get_byte_count();
	cp.mpData = new uint8_t[nbytes];
	::memcpy(cp.mpData, pBits, nbytes);

	delete[] pBits;

	return cp;
}


class cXTEX {
private:
	COP2_Node* mpCOP;
	UT_String* mpOutPath;

public:
	cXTEX() : mpCOP(0), mpOutPath(0) {
	}

	~cXTEX() {
		if (mpOutPath) {
			delete mpOutPath;
			mpOutPath = 0;
		}
	}

	void init(CMD_Args& args) {
		mpCOP = 0;
		if (args.argc() < 2) {
			args.err() << "xtex <COP2 path>" << endl;
			return;
		}
		UT_String outPath;
		if (args.found('o')) {
			/* -o<dir_path|file_path> */
			outPath = args.argp('o');
			if (!outPath.endsWith(".xtex", false)) {
				outPath += "/";
			}
		} else {
			if (CMD_Manager::getContextExists()) {
				if (CMD_Manager::getContext()->getVariable("HIP", outPath)) {
					outPath += "/";
				}
			}
		}
		mpCOP = OPgetDirector()->findCOP2Node(args(1));
		if (!mpCOP) {
			args.err() << "Invalid COP2 path." << endl;
			return;
		}

		UT_String copPath;
		mpCOP->getFullPath(copPath);
		//args.out() << copPath << " : " << copPath.fileName() << endl;

		mpOutPath = new UT_String();
		(*mpOutPath) += outPath;
	}

	void exec(CMD_Args& args) {
		if (!mpCOP) {
			return;
		}

		UT_String copPath;
		mpCOP->getFullPath(copPath);
		int rid;
		TIL_Raster* pRast = COP2_Resolver::getRaster(copPath, rid);
		if (pRast->getFormat() != PXL_FLOAT32) {
			//args.out() << "Warning: data format for " << copPath << " is not F32." << endl;
			TIL_CopResolver::doneWithRaster(pRast);
			TIL_CopResolver* pRsl = TIL_CopResolver::getResolver();
			pRast = pRsl->getNodeRaster(copPath, "C", "A", false, 0, PXL_FLOAT32);
			if (!pRast) {
				args.err() << "Can't convert raster to F32 for " << copPath << "." << endl;
				return;
			}
		}
		int w = pRast->getXres();
		int h = pRast->getYres();
		float* pPix = reinterpret_cast<float*>(pRast->getPixels());
		if (!pPix) {
			args.err() << "Can't get pixels for " << copPath << "." << endl;
			return;
		}

		float* pR = new float[w*h];
		float* pG = new float[w*h];
		float* pB = new float[w*h];
		float* pA = new float[w*h];

		float* pSrc = pPix + (h-1)*w*4;
		int idx = 0;
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				pR[idx] = pSrc[0];
				pG[idx] = pSrc[1];
				pB[idx] = pSrc[2];
				pA[idx] = pSrc[3];
				++idx;
				pSrc += 4;
			}
			pSrc -= w*2*4;
		}

		sCompressedPlane cpR = compress(pR, w, h);
		sCompressedPlane cpG = compress(pG, w, h);
		sCompressedPlane cpB = compress(pB, w, h);
		sCompressedPlane cpA = compress(pA, w, h);

		delete[] pR;
		delete[] pG;
		delete[] pB;
		delete[] pA;

		UT_String path = *mpOutPath;
		if (!path.endsWith(".xtex", false)) {
			path += mpCOP->getName();
			path += ".xtex";
		}
		args.out() << "Saving " << path << "." << endl;

		cBin bin;
		bin.open(path);
		bin.write_u32(_FOURCC('X','T','E','X')); // +00
		bin.write_u32(0); // +04 flags
		bin.write_u32(0); // +08 fileSize
		bin.write_u32(0x30); // +0C headSize
		bin.write_u32(0x30); // +10 offsStr
		bin.write_u16(0); // +14 nameId
		bin.write_u16(1); // +16 pathId
		bin.write_u32(0); // +18 filePathLen
		bin.write_u32(0); // +1C reserved
		// +20 XTEX head
		bin.write_u32(w); // +20
		bin.write_u32(h); // +24
		bin.write_u32(4); // +28 nplanes
		bin.write_u32(0); // +2C -> info

		const char* pTexName = copPath.fileName();
		size_t texNameLen = ::strlen(pTexName);
		size_t texPathLen = copPath.length() - texNameLen - 1;
		char pathBuff[1024];
		::memset(pathBuff, 0, sizeof(pathBuff));
		::memcpy(pathBuff, copPath.c_str(), texPathLen);
		const char* pTexPath = pathBuff;

		// +30 strList
		int strTop = 4 + 4 + 4*6 + 2*6;
		int strDataSize = strTop + texNameLen+1 + texPathLen+1 + 2*4;
		bin.write_u32(strDataSize); // +30
		bin.write_u32(6); // +34 nstr
		uint32_t strPtr = strTop;
		bin.write_u32(strPtr);
		strPtr += texNameLen + 1;
		bin.write_u32(strPtr);
		strPtr += texPathLen + 1;
		bin.write_u32(strPtr); // #2 -> "r"
		strPtr += 2;
		bin.write_u32(strPtr); // #3 -> "g"
		strPtr += 2;
		bin.write_u32(strPtr); // #4 -> "b"
		strPtr += 2;
		bin.write_u32(strPtr); // #5 -> "a"
		bin.write_u16(str_hash16(pTexName));
		bin.write_u16(str_hash16(pTexPath));
		bin.write_u16(str_hash16("r"));
		bin.write_u16(str_hash16("g"));
		bin.write_u16(str_hash16("b"));
		bin.write_u16(str_hash16("a"));
		bin.write_str(pTexName);
		bin.write_str(pTexPath);
		bin.write_str("r");
		bin.write_str("g");
		bin.write_str("b");
		bin.write_str("a");
		bin.align(0x10);

		bin.patch_cur(0x2C); // -> info
		uint32_t infoTopR = cpR.write_info(bin, 2);
		uint32_t infoTopG = cpG.write_info(bin, 3);
		uint32_t infoTopB = cpB.write_info(bin, 4);
		uint32_t infoTopA = cpA.write_info(bin, 5);

		bool useDups = true;

		uint32_t dataOffsR = 0;
		bin.align(4);
		bin.patch_cur(infoTopR);
		if (cpR.is_const()) {
			bin.write_f32(cpR.mMinVal);
		} else {
			dataOffsR = bin.get_pos();
			bin.write(cpR.mpData, cpR.get_byte_count());
		}

		uint32_t dataOffsG = 0;
		bin.align(4);
		if (cpG.is_const()) {
			bin.patch_cur(infoTopG);
			bin.write_f32(cpG.mMinVal);
		} else {
			if (useDups && dataOffsR != 0 && cpG.data_cmp(cpR)) {
				bin.patch_u32(infoTopG, dataOffsR);
			} else {
				dataOffsG = bin.get_pos();
				bin.patch_cur(infoTopG);
				bin.write(cpG.mpData, cpG.get_byte_count());
			}
		}

		uint32_t dataOffsB = 0;
		bin.align(4);
		if (cpB.is_const()) {
			bin.patch_cur(infoTopB);
			bin.write_f32(cpB.mMinVal);
		} else {
			if (useDups && dataOffsR != 0 && cpB.data_cmp(cpR)) {
				bin.patch_u32(infoTopB, dataOffsR);
			} else if (useDups && dataOffsG != 0 && cpB.data_cmp(cpG)) {
				bin.patch_u32(infoTopB, dataOffsG);
			} else {
				dataOffsB = bin.get_pos();
				bin.patch_cur(infoTopB);
				bin.write(cpB.mpData, cpB.get_byte_count());
			}
		}

		uint32_t dataOffsA = 0;
		bin.align(4);
		bin.patch_cur(infoTopA);
		if (cpA.is_const()) {
			bin.write_f32(cpA.mMinVal);
		} else {
			dataOffsA = bin.get_pos();
			bin.write(cpA.mpData, cpA.get_byte_count());
		}

		bin.patch_cur(8); // fileSize
		bin.close();

		cpR.free();
		cpG.free();
		cpB.free();
		cpA.free();
	}
};

static void cmd_xtex(CMD_Args& args) {
	cXTEX xtex;

	xtex.init(args);
	xtex.exec(args);
}

void CMDextendLibrary(CMD_Manager* pMgr) {
	pMgr->installCommand("xtex", "o:", cmd_xtex);
}

