#include "crosscore.hpp"

// ~~~~~~~~~~~~~~~~~

struct MemTestType {
	int val0;
	int val1;
	MemTestType() {
		val0 = 10;
		val0 = 11;
		nxCore::dbg_msg("ctor\n");
	}
	~MemTestType() {
		nxCore::dbg_msg("dtor\n");
	}
};

void test_xmem() {
	const int n1 = 10;
	uint8_t* pBytes = (uint8_t*)nxCore::mem_alloc(n1, "Bytes");
	for (int i = 0; i < n1; ++i) {
		pBytes[i] = uint8_t(i);
	}
	const char* pTag = nxCore::mem_tag(pBytes);
	nxCore::mem_dbg();

	const int n2 = n1 + 5;
	pBytes = (uint8_t*)nxCore::mem_realloc(pBytes, n2);
	for (int i = n1; i < n2; ++i) {
		pBytes[i] = uint8_t(i);
	}
	nxCore::mem_dbg();

	int tnum = 5;
	MemTestType* pTTT = nxCore::tMem<MemTestType>::alloc(tnum, "test_type");
	nxCore::mem_dbg();
	nxCore::tMem<MemTestType>::free(pTTT, tnum);
}

// ~~~~~~~~~~~~~~~~~

void test_heap() {
	size_t hmemSize = 1024;
	cxHeap* pHeap = cxHeap::create(hmemSize, "TestHeap");
	uint8_t* p0 = (uint8_t*)pHeap->alloc(32);
	uint8_t* p1 = (uint8_t*)pHeap->alloc(16);
	pHeap->free(p0);
	uint8_t* p2 = (uint8_t*)pHeap->alloc(4);
	uint8_t* p3 = (uint8_t*)pHeap->alloc(4);
	cxHeap::destroy(pHeap);
	pHeap = nullptr;
}


// ~~~~~~~~~~~~~~~~~ data tests

static void test_xmdl() {
	const char* pPath = "../data/Lin/Lin.xmdl";
	sxData* pData = nxData::load(pPath);
	if (!pData) return;
	if (!pData->is<sxModelData>()) {
		nxData::unload(pData);
		return;
	}
	sxModelData* pMdlData = pData->as<sxModelData>();
	const char* pName = pMdlData->get_name();
	pMdlData->dump_geo("d:/temp/xmdl.geo");
	pMdlData->dump_ocapt("d:/temp/xmdl.ocapt");
	pMdlData->dump_skel("d:/temp/xmdl_skel.py");
	nxData::unload(pData);
}

static void test_xmot() {
	const char* pPath = "../data/Lin/walk.xmot";
	sxData* pData = nxData::load(pPath);
	if (!pData) return;
	if (!pData->is<sxMotionData>()) {
		nxData::unload(pData);
		return;
	}
	sxMotionData* pMotData = pData->as<sxMotionData>();
	const char* pName = pMotData->get_name();
	pMotData->dump_clip("d:/temp/xmot_dump.clip");
	nxData::unload(pData);
}

static void test_xcol() {
	const char* pPath = "../data/col_test.xcol";
	sxData* pData = nxData::load(pPath);
	if (!pData) return;
	if (!pData->is<sxCollisionData>()) {
		nxData::unload(pData);
		return;
	}
	sxCollisionData* pColData = pData->as<sxCollisionData>();
	const char* pName = pColData->get_name();
	pColData->dump_pol_geo("d:/temp/xcol_pols.geo");
	pColData->dump_tri_geo("d:/temp/xcol_tris.geo");
	nxData::unload(pData);
}

void test_data() {
	test_xmdl();
	test_xmot();
	test_xcol();
}

void test_sleep() {
	double t0 = nxSys::time_micros();
	nxSys::sleep_millis(150);
	double dt = nxSys::time_micros() - t0;
	::printf("elapsed %f millis\n", dt / 1e3);
}

