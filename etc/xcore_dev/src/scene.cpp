#include "crosscore.hpp"
#include "draw.hpp"
#include "scene.hpp"

static Draw::Ifc* s_pDraw = nullptr;

static cxResourceManager* s_pRsrcMgr = nullptr;

static cxBrigade* s_pBgd = nullptr;
static sxJobQueue* s_pJobQue = nullptr;
static cxHeap* s_pGlobalHeap = nullptr;
static cxHeap** s_ppLocalHeaps = nullptr;
static int s_numLocalHeaps = 0;

static int* s_pBgdJobCnts = nullptr;
static int s_numBgdJobCnts = 0;

static sxLock* s_pGlbRNGLock = nullptr;
static sxRNG s_glbRNG;

static sxLock* s_pGlbMemLock = nullptr;

typedef cxStrMap<ScnObj*> ObjMap;
typedef cxPlexList<ScnObj> ObjList;

static ObjList* s_pObjList = nullptr;
static ObjMap* s_pObjMap = nullptr;

static bool s_scnInitFlg = false;

static Draw::Context s_drwCtx;
static Draw::Context s_drwCtxStk[4];
static int s_drwCtxSP = 0;

static int s_viewRot = 0;
static bool s_useBump = true;
static bool s_useSpec = true;

static float s_speed = 1.0f;

static bool s_viewUpdateFlg = true;

static bool s_shadowUniform = true;
static bool s_shadowUpdateFlg = true;
static float s_smapViewSize = 30.0f;
static float s_smapViewDist = 50.0f;
static float s_smapMargin = 30.0f;
static bool s_useShadowCastCull = true;

static float s_refScrW = -1.0f;
static float s_refScrH = -1.0f;
static xt_float3 s_sprGamma;

static uint64_t s_frameCnt = 0;

static uint32_t s_sleepMillis = 0;

static bool s_printMemInfo = false;
static bool s_printBatteryInfo = false;
static int s_thermalZones[2];

void ScnCfg::set_defaults() {
	pAppPath = "";
#if defined(XD_SYS_ANDROID)
	pDataDir = "data";
	shadowMapSize = 1024;
#else
	pDataDir = "../data";
	shadowMapSize = 2048;
#endif
	numWorkers = 4;
	localHeapSize = 0;
	useSpec = true;
	useBump = true;
}


static Draw::MdlParam* get_obj_mdl_params(ScnObj& obj) {
	return obj.mpMdlWk ? (Draw::MdlParam*)obj.mpMdlWk->mpParamMem : nullptr;
}

static void obj_exec_job(const sxJobContext* pCtx) {
	if (!pCtx) return;
	sxJob* pJob = pCtx->mpJob;
	if (!pJob) return;
	ScnObj* pObj = (ScnObj*)pJob->mpData;
	if (!pObj) return;
	if (pObj->mExecFunc) {
		pObj->mExecFunc(pObj, pCtx);
	} else {
		pObj->move(nullptr, 0.0f);
	}
}

static void obj_visibility_job(const sxJobContext* pCtx) {
	if (!pCtx) return;
	sxJob* pJob = pCtx->mpJob;
	if (!pJob) return;
	ScnObj* pObj = (ScnObj*)pJob->mpData;
	if (!pObj) return;
	pObj->update_visibility();
}

static void obj_copy_prev_job(const sxJobContext* pCtx) {
	if (!pCtx) return;
	sxJob* pJob = pCtx->mpJob;
	if (!pJob) return;
	ScnObj* pObj = (ScnObj*)pJob->mpData;
	if (pObj) {
		if (pObj->mpMdlWk) {
			pObj->mpMdlWk->copy_prev_world_bbox();
		}
		if (pObj->mpMotWk) {
			pObj->mpMotWk->copy_prev_world();
		} else {
			if (pObj->mpMdlWk) {
				pObj->mpMdlWk->copy_prev_world_xform();
			}
		}
	}
}

static void obj_ctor(ScnObj* pObj) {
}

static void obj_dtor(ScnObj* pObj) {
	if (!pObj) return;
	if (pObj->mDelFunc) {
		pObj->mDelFunc(pObj);
	}
	nxCore::mem_free((void*)pObj->mpName);
	pObj->mpName = nullptr;
	cxMotionWork::destroy(pObj->mpMotWk);
	pObj->mpMotWk = nullptr;
	cxModelWork::destroy(pObj->mpMdlWk);
	pObj->mpMdlWk = nullptr;
	nxCore::mem_free(pObj->mpBatJobs);
	pObj->mpBatJobs = nullptr;
}

namespace Scene {

void create_global_locks() {
	s_pGlbRNGLock = nxSys::lock_create();
	s_pGlbMemLock = nxSys::lock_create();
}

void init(const ScnCfg& cfg) {
	if (s_scnInitFlg) return;

	s_printMemInfo = nxApp::get_bool_opt("meminfo");
	s_printBatteryInfo = nxApp::get_bool_opt("battery");
	for (int i = 0; i < XD_ARY_LEN(s_thermalZones); ++i) {
		char tbuf[32];
		XD_SPRINTF(XD_SPRINTF_BUF(tbuf, sizeof(tbuf)), "thermal%d", i);
		s_thermalZones[i] = nxApp::get_int_opt(tbuf, -1);
	}

	s_pDraw = Draw::get_ifc_impl();

	s_pRsrcMgr = cxResourceManager::create(cfg.pAppPath, cfg.pDataDir);
	if (!s_pRsrcMgr) return;

	if (cfg.numWorkers > 0) {
		s_pBgd = cxBrigade::create(cfg.numWorkers);
		create_global_locks();
	} else {
#ifdef XD_USE_OMP
		create_global_locks();
#else
		s_pGlbRNGLock = nullptr;
		s_pGlbMemLock = nullptr;
#endif
	}
	if (s_pBgd) {
		s_numBgdJobCnts = (/*cpy_prev*/1 + SCN_NUM_EXEC_PRIO + /*vis*/1) * s_pBgd->get_workers_num();
		size_t jcntsMemSize = s_numBgdJobCnts*sizeof(int);
		s_pBgdJobCnts = (int*)nxCore::mem_alloc(jcntsMemSize, "Scn:job_cnts");
		if (s_pBgdJobCnts) {
			::memset(s_pBgdJobCnts, 0, jcntsMemSize);
		}
		if (nxApp::get_int_opt("static_sched", 0)) {
			nxCore::dbg_msg("using static scene scheduler\n");
			s_pBgd->set_static_scheduling();
		}
	}

	glb_rng_reset();

	s_pObjList = ObjList::create();
	if (s_pObjList) {
		s_pObjList->set_item_handlers(obj_ctor, obj_dtor);
	}

	s_pObjMap = ObjMap::create();

	s_drwCtx.reset();
	s_drwCtxSP = 0;

	s_viewRot = nxApp::get_int_opt("viewrot", 0);
	s_useBump = cfg.useBump;
	s_useSpec = cfg.useSpec;

	s_speed = nxApp::get_float_opt("speed", 1.0f);

	if (s_pDraw) {
		s_pDraw->init(cfg.shadowMapSize, s_pRsrcMgr);
	}

	set_spr_gamma(2.2f);

	nxCore::dbg_msg("draw ifc: %s\n", get_draw_ifc_name());

	s_frameCnt = 0;

	s_sleepMillis = nxApp::get_int_opt("sleep", 0);

	s_scnInitFlg = true;
}

void reset() {
	if (!s_scnInitFlg) return;

	if (s_pBgd) {
		cxBrigade::destroy(s_pBgd);
		s_pBgd = nullptr;
	}
	if (s_pBgdJobCnts) {
		nxCore::mem_free(s_pBgdJobCnts);
		s_pBgdJobCnts = nullptr;
	}
	s_numBgdJobCnts = 0;
	if (s_pJobQue) {
		nxTask::queue_destroy(s_pJobQue);
		s_pJobQue = nullptr;
	}
	if (s_pGlbRNGLock) {
		nxSys::lock_destroy(s_pGlbRNGLock);
		s_pGlbRNGLock = nullptr;
	}
	if (s_pGlbMemLock) {
		nxSys::lock_destroy(s_pGlbMemLock);
		s_pGlbMemLock = nullptr;
	}

	free_local_heaps();
	free_global_heap();

	release_all_gfx();
	if (s_pDraw) {
		s_pDraw->reset();
	}
	del_all_objs();
	unload_all_pkgs();

	ObjList::destroy(s_pObjList);
	s_pObjList = nullptr;
	ObjMap::destroy(s_pObjMap);
	s_pObjMap = nullptr;
	cxResourceManager::destroy(s_pRsrcMgr);
	s_pRsrcMgr = nullptr;

	s_scnInitFlg = false;
}

const char* get_draw_ifc_name() {
	return s_pDraw ? s_pDraw->info.pName : "<none>";
}

cxResourceManager* get_rsrc_mgr() {
	return s_pRsrcMgr;
}

const char* get_data_path() {
	return s_pRsrcMgr ? s_pRsrcMgr->get_data_path() : nullptr;
}

Pkg* load_pkg(const char* pName) {
	return s_pRsrcMgr ? s_pRsrcMgr->load_pkg(pName) : nullptr;
}

Pkg* find_pkg(const char* pName) {
	return s_pRsrcMgr ? s_pRsrcMgr->find_pkg(pName) : nullptr;
}

Pkg* find_pkg_for_data(sxData* pData) {
	return s_pRsrcMgr ? s_pRsrcMgr->find_pkg_for_data(pData) : nullptr;
}

sxModelData* find_model_in_pkg(Pkg* pPkg, const char* pMdlName) {
	return s_pRsrcMgr ? s_pRsrcMgr->find_model_in_pkg(pPkg, pMdlName) : nullptr;
}

sxModelData* get_pkg_default_model(Pkg* pPkg) {
	return s_pRsrcMgr ? s_pRsrcMgr->get_pkg_default_model(pPkg) : nullptr;
}

sxTextureData* find_texture_in_pkg(Pkg* pPkg, const char* pTexName) {
	return s_pRsrcMgr ? s_pRsrcMgr->find_texture_in_pkg(pPkg, pTexName) : nullptr;
}

sxTextureData* find_texture_for_model(sxModelData* pMdl, const char* pTexName) {
	return s_pRsrcMgr ? s_pRsrcMgr->find_texture_for_model(pMdl, pTexName) : nullptr;
}

sxMotionData* find_motion_in_pkg(Pkg* pPkg, const char* pMotName) {
	return s_pRsrcMgr ? s_pRsrcMgr->find_motion_in_pkg(pPkg, pMotName) : nullptr;
}

sxMotionData* find_motion_for_model(sxModelData* pMdl, const char* pMotName) {
	return s_pRsrcMgr ? s_pRsrcMgr->find_motion_for_model(pMdl, pMotName) : nullptr;
}

sxCollisionData* find_collision_in_pkg(Pkg* pPkg, const char* pColName) {
	return s_pRsrcMgr ? s_pRsrcMgr->find_collision_in_pkg(pPkg, pColName) : nullptr;
}

int get_num_mdls_in_pkg(Pkg* pPkg) {
	return s_pRsrcMgr ? s_pRsrcMgr->get_num_models_in_pkg(pPkg) : 0;
}

void prepare_pkg_gfx(Pkg* pPkg) {
	if (s_pRsrcMgr) {
		s_pRsrcMgr->prepare_pkg_gfx(pPkg);
	}
}

void release_pkg_gfx(Pkg* pPkg) {
	if (s_pRsrcMgr) {
		s_pRsrcMgr->release_pkg_gfx(pPkg);
	}
}

void prepare_all_gfx() {
	if (s_pRsrcMgr) {
		s_pRsrcMgr->prepare_all_gfx();
	}
}

void release_all_gfx() {
	if (s_pRsrcMgr) {
		s_pRsrcMgr->release_all_gfx();
	}
}

void unload_pkg(Pkg* pPkg) {
	if (s_pRsrcMgr) {
		s_pRsrcMgr->unload_pkg(pPkg);
	}
}

void unload_all_pkgs() {
	if (s_pRsrcMgr) {
		s_pRsrcMgr->unload_all();
	}
}


void frame_begin(const cxColor& clearColor) {
	if (s_pDraw) {
		s_pDraw->begin(clearColor);
	}
	purge_local_heaps();
	purge_global_heap();

	if (s_sleepMillis > 0) {
		nxSys::sleep_millis(s_sleepMillis);
	}
}

void frame_end() {
	if (s_pDraw) {
		s_pDraw->end();
	}
	++s_frameCnt;
}

uint64_t get_frame_count() {
	return s_frameCnt;
}

void push_ctx() {
	if (s_drwCtxSP < XD_ARY_LEN(s_drwCtxStk)) {
		s_drwCtxStk[s_drwCtxSP] = s_drwCtx;
		++s_drwCtxSP;
		s_viewUpdateFlg = true;
		s_shadowUpdateFlg = true;
	}
}

void pop_ctx() {
	if (s_drwCtxSP > 0) {
		--s_drwCtxSP;
		s_drwCtx = s_drwCtxStk[s_drwCtxSP];
		s_viewUpdateFlg = true;
		s_shadowUpdateFlg = true;
	}
}


int get_num_workers() {
	return s_pBgd ? s_pBgd->get_workers_num() : 0;
}

int get_num_active_workers() {
	return s_pBgd ? s_pBgd->get_active_workers_num() : 0;
}

int get_visibility_job_lvl() {
	return 1 + SCN_NUM_EXEC_PRIO;
}

int get_wrk_jobs_done_cnt(const int lvl, const int wrkId) {
	int cnt = 0;
	if (lvl >= 0 && s_pBgd && s_pBgd->ck_worker_id(wrkId) && s_pBgdJobCnts) {
		int nwrk = s_pBgd->get_workers_num();
		int idx = lvl*nwrk + wrkId;
		if (idx < s_numBgdJobCnts) {
			cnt = s_pBgdJobCnts[idx];
		}
	}
	return cnt;
}

int get_lvl_jobs_done_cnt(const int lvl) {
	int cnt = 0;
	if (lvl >= 0 && s_pBgd && s_pBgdJobCnts) {
		int nwrk = s_pBgd->get_workers_num();
		int idx = lvl*nwrk;
		if (idx < s_numBgdJobCnts) {
			for (int i = 0; i < nwrk; ++i) {
				cnt += s_pBgdJobCnts[idx + i];
			}
		}
	}
	return cnt;
}


void alloc_local_heaps(const size_t localHeapSize) {
	free_local_heaps();
	if (localHeapSize) {
		s_numLocalHeaps = s_pBgd ? s_pBgd->get_workers_num() : 1;
		s_ppLocalHeaps = (cxHeap**)nxCore::mem_alloc(s_numLocalHeaps * sizeof(cxHeap*));
		if (s_ppLocalHeaps) {
			for (int i = 0; i < s_numLocalHeaps; ++i) {
				s_ppLocalHeaps[i] = cxHeap::create(localHeapSize, "ScnLocalHeap");
			}
		}
	}
}

void free_local_heaps() {
	if (s_ppLocalHeaps) {
		for (int i = 0; i < s_numLocalHeaps; ++i) {
			cxHeap::destroy(s_ppLocalHeaps[i]);
			s_ppLocalHeaps[i] = nullptr;
		}
		nxCore::mem_free(s_ppLocalHeaps);
		s_ppLocalHeaps = nullptr;
		s_numLocalHeaps = 0;
	}
}

void purge_local_heaps() {
	if (s_ppLocalHeaps) {
		for (int i = 0; i < s_numLocalHeaps; ++i) {
			cxHeap* pHeap = s_ppLocalHeaps[i];
			if (pHeap) {
				pHeap->purge();
			}
		}
	}
}

cxHeap* get_local_heap(const int id) {
	cxHeap* pHeap = nullptr;
	if (s_ppLocalHeaps) {
		if (s_numLocalHeaps == 1) {
			pHeap = s_ppLocalHeaps[0];
		} else {
			if (s_pBgd) {
				if (s_pBgd->ck_worker_id(id)) {
					pHeap = s_ppLocalHeaps[id];
				} else {
					pHeap = s_ppLocalHeaps[0];
				}
			}
		}
	}
	return pHeap;
}

cxHeap* get_job_local_heap(const sxJobContext* pJobCtx) {
	return pJobCtx ? get_local_heap(pJobCtx->mWrkId) : get_local_heap(0);
}


void alloc_global_heap(const size_t globalHeapSize) {
	free_global_heap();
	s_pGlobalHeap = cxHeap::create(globalHeapSize, "ScnGlobalHeap");;
}

void free_global_heap() {
	if (s_pGlobalHeap) {
		cxHeap::destroy(s_pGlobalHeap);
		s_pGlobalHeap = nullptr;
	}
}

void purge_global_heap() {
	if (s_pGlobalHeap) {
		s_pGlobalHeap->purge();
	}
}

cxHeap* get_global_heap() {
	return s_pGlobalHeap;
}

static void glb_mem_lock_acq() {
	if (s_pGlbMemLock) {
		nxSys::lock_acquire(s_pGlbMemLock);
	}
}

static void glb_mem_lock_rel() {
	if (s_pGlbMemLock) {
		nxSys::lock_release(s_pGlbMemLock);
	}
}

static void* glb_mem_alloc_impl(const size_t size, const uint32_t tag) {
	void* pMem = nullptr;
	if (s_pGlobalHeap) {
		pMem = s_pGlobalHeap->alloc(size, tag);
	} else {
		char tagStr[5];
		::memcpy(tagStr, &tag, 4);
		tagStr[4] = 0;
		pMem = nxCore::mem_alloc(size, tagStr);
	}
	return pMem;
}

static void glb_mem_free_impl(void* pMem) {
	if (s_pGlobalHeap) {
		s_pGlobalHeap->free(pMem);
	} else {
		nxCore::mem_free(pMem);
	}
}

void* glb_mem_alloc(const size_t size, const uint32_t tag) {
	glb_mem_lock_acq();
	void* pMem = glb_mem_alloc_impl(size, tag);
	glb_mem_lock_rel();
	return pMem;
}

void glb_mem_free(void* pMem) {
	glb_mem_lock_acq();
	glb_mem_free_impl(pMem);
	glb_mem_lock_rel();
}

void mem_info() {
	if (s_printMemInfo) {
		nxCore::mem_dbg();
		uint64_t alloced = nxCore::mem_allocated_bytes();
		uint64_t peak = nxCore::mem_peak_bytes();
		const uint64_t lim = 0x7FFFFFFF;
		if (alloced < lim && peak < lim) {
			nxCore::dbg_msg("allocated: %d bytes\n", (uint32_t)(alloced & lim));
			nxCore::dbg_msg("peak: %d bytes\n", (uint32_t)(peak & lim));
		} else {
			nxCore::dbg_msg("allocated: too much\n");
		}
	}
}


void thermal_info() {
#ifdef XD_SYS_LINUX
	char buf[512];
	static const char* pPath = "/sys/class/thermal/thermal_zone";
	for (int i = 0; i < XD_ARY_LEN(s_thermalZones); ++i) {
		int id = s_thermalZones[i];
		if (id >= 0) {
			XD_SPRINTF(XD_SPRINTF_BUF(buf, sizeof(buf)),
				"echo \": \" | cat \"%s%d/type\" - \"%s%d/temp\" | tr -d '\\n' && echo",
				pPath, id, pPath, id);
			int res = system(buf);
			if (res != 0) {
				nxCore::dbg_msg("!system\n");
			}
		}
	}
#endif
}

void battery_info() {
#ifdef XD_SYS_LINUX
	if (!s_printBatteryInfo) return;
	char buf[512];
	static const char* pPath = "/sys/class/power_supply/battery";
	XD_SPRINTF(XD_SPRINTF_BUF(buf, sizeof(buf)),
		"echo \": \" | cat \"%s/status\" - \"%s/capacity\" | tr -d '\\n' && echo", pPath, pPath);
	int res = system(buf);
	if (res != 0) {
		nxCore::dbg_msg("!system\n");
	}
#endif
}


void glb_rng_reset() {
	glb_rng_seed(1);
}

void glb_rng_seed(const uint64_t seed) {
	nxCore::rng_seed(&s_glbRNG, seed);
}

uint64_t glb_rng_next() {
	if (s_pGlbRNGLock) {
		nxSys::lock_acquire(s_pGlbRNGLock);
	}
	uint64_t r = nxCore::rng_next(&s_glbRNG);
	if (s_pGlbRNGLock) {
		nxSys::lock_release(s_pGlbRNGLock);
	}
	return r;
}


static void update_view() {
	if (s_viewUpdateFlg) {
		if (s_pDraw) {
			s_drwCtx.view.update(s_pDraw->get_screen_width(), s_pDraw->get_screen_height());
		}
	}
	s_viewUpdateFlg = false;
}

void set_view(const cxVec& pos, const cxVec& tgt, const cxVec& up) {
	s_drwCtx.view.set_view(pos, tgt, up);
	s_viewUpdateFlg = true;
	s_shadowUpdateFlg = true;
}

void set_view_range(const float znear, const float zfar) {
	s_drwCtx.view.znear = znear;
	s_drwCtx.view.zfar = zfar;
	s_viewUpdateFlg = true;
	s_shadowUpdateFlg = true;
}

void set_deg_FOVY(const float fovy) {
	s_drwCtx.view.degFOVY = fovy;
	s_viewUpdateFlg = true;
	s_shadowUpdateFlg = true;
}

const cxFrustum* get_view_frustum_ptr() {
	update_view();
	return &s_drwCtx.view.frustum;
}

cxMtx get_view_mtx() {
	update_view();
	return s_drwCtx.view.viewMtx;
}

cxMtx get_view_proj_mtx() {
	update_view();
	return s_drwCtx.view.viewProjMtx;
}

cxMtx get_proj_mtx() {
	update_view();
	return s_drwCtx.view.projMtx;
}

cxMtx get_inv_view_mtx() {
	update_view();
	return s_drwCtx.view.invViewMtx;
}

cxMtx get_inv_proj_mtx() {
	update_view();
	return s_drwCtx.view.invProjMtx;
}

cxMtx get_inv_view_proj_mtx() {
	update_view();
	return s_drwCtx.view.invViewProjMtx;
}

cxMtx get_view_mode_mtx() {
	return s_drwCtx.view.get_rot_mode_mtx();
}

cxVec get_view_pos() {
	return s_drwCtx.view.pos;
}

cxVec get_view_tgt() {
	return s_drwCtx.view.tgt;
}

cxVec get_view_up() {
	return s_drwCtx.view.up;
}

cxVec get_view_dir() {
	return s_drwCtx.view.get_dir();
}

float get_view_near() {
	return s_drwCtx.view.znear;
}

float get_view_far() {
	return s_drwCtx.view.zfar;
}

bool is_rot_view() {
	return s_drwCtx.view.rotMode != 0;
}

bool is_sphere_visible(const cxSphere& sph, const bool exact) {
	bool vis = true;
	const cxFrustum* pFst = get_view_frustum_ptr();
	if (pFst) {
		vis = !pFst->cull(sph);
		if (vis && exact) {
			vis = pFst->overlaps(sph);
		}
	}
	return vis;
}

bool is_shadow_uniform() {
	return s_shadowUniform;
}

void set_shadow_uniform(const bool flg) {
	s_shadowUniform = flg;
	s_shadowUpdateFlg = true;
}

void set_shadow_density(const float dens) {
	s_drwCtx.shadow.dens = dens;
}

void set_shadow_density_bias(const float bias) {
	s_drwCtx.shadow.densBias = bias;
}

void set_shadow_fade(const float start, const float end) {
	s_drwCtx.shadow.fadeStart = start;
	s_drwCtx.shadow.fadeEnd = end;
}

void set_shadow_proj_params(const float size, const float margin, const float dist) {
	s_smapViewSize = size;
	s_smapViewDist = dist;
	s_smapMargin = margin;
	s_shadowUpdateFlg = true;
}

void set_shadow_dir(const cxVec& sdir) {
	s_drwCtx.shadow.set_dir(sdir);
	s_shadowUpdateFlg = true;
}

void set_shadow_dir_degrees(const float dx, const float dy) {
	s_drwCtx.shadow.set_dir_degrees(dx, dy);
	s_shadowUpdateFlg = true;
}

cxVec get_shadow_dir() {
	return s_drwCtx.shadow.dir;
}

static void update_shadow_uni() {
	if (!s_shadowUpdateFlg) {
		return;
	}
	Draw::Context* pCtx = &s_drwCtx;
	cxMtx view;
	cxVec vpos = pCtx->view.pos;
	cxVec vtgt = pCtx->view.tgt;
	cxVec vvec = vpos - vtgt;
	float ext = s_smapMargin;
	cxVec dir = pCtx->shadow.dir.get_normalized();
	cxVec tgt = vpos - vvec;
	cxVec pos = tgt - dir*ext;
	cxVec up(0.0f, 1.0f, 0.0f);
	view.mk_view(pos, tgt, up);
	float size = s_smapViewSize;
	float dist = s_smapViewDist;
	cxMtx proj = nxMtx::identity();
	proj.m[0][0] = nxCalc::rcp0(size);
	proj.m[1][1] = proj.m[0][0];
	proj.m[2][2] = nxCalc::rcp0(1.0f - (ext + dist));
	proj.m[3][2] = proj.m[2][2];
	pCtx->shadow.viewProjMtx = view * proj;
}

static void update_shadow_persp() {
	if (!s_shadowUpdateFlg) {
		return;
	}
	Draw::Context* pCtx = &s_drwCtx;
	cxMtx mtxProj = Scene::get_proj_mtx();
	cxMtx mtxInvView = Scene::get_inv_view_mtx();
	cxMtx mtxInvProj = Scene::get_inv_proj_mtx();
	cxMtx mtxViewProj = Scene::get_view_proj_mtx();
	cxMtx mtxInvViewProj = Scene::get_inv_view_proj_mtx();
	cxVec dir = pCtx->shadow.dir.get_normalized();

	if (is_rot_view()) {
		cxMtx mm = get_view_mode_mtx().get_inverted();
		mtxProj.rev_mul(mm);
		mtxInvProj.rev_mul(mm);
	}

	float vdist = s_smapViewDist;
	vdist *= mtxProj.m[1][1];
	float cosVL = mtxInvView.get_row_vec(2).dot(dir);
	float reduce = ::fabsf(cosVL);
	if (reduce > 0.7f) {
		vdist *= 0.5f;
	}
	reduce *= 0.8f;
	xt_float4 projPos;
	projPos.set(0.0f, 0.0f, 0.0f, 1.0f);
	projPos = mtxInvProj.apply(projPos);
	float nearDist = -nxCalc::div0(projPos.z, projPos.w);
	float dnear = nearDist;
	float dfar = dnear + (vdist - nearDist) * (1.0f - reduce);
	cxVec vdir = mtxInvView.get_row_vec(2).neg_val().get_normalized();
	cxVec pos = mtxInvView.get_translation() + vdir*dnear;
	projPos.set(pos.x, pos.y, pos.z, 1.0f);
	projPos = mtxViewProj.apply(projPos);
	float znear = nxCalc::max(0.0f, nxCalc::div0(projPos.z, projPos.w));
	pos = mtxInvView.get_translation() + vdir*dfar;
	projPos.set(pos.x, pos.y, pos.z, 1.0f);
	projPos = mtxViewProj.apply(projPos);
	float zfar = nxCalc::min(nxCalc::div0(projPos.z, projPos.w), 1.0f);

	xt_float4 box[8] = {};
	box[0].set(-1.0f, -1.0f, znear, 1.0f);
	box[1].set(1.0f, -1.0f, znear, 1.0f);
	box[2].set(-1.0f, 1.0f, znear, 1.0f);
	box[3].set(1.0f, 1.0f, znear, 1.0f);

	box[4].set(-1.0f, -1.0f, zfar, 1.0f);
	box[5].set(1.0f, -1.0f, zfar, 1.0f);
	box[6].set(-1.0f, 1.0f, zfar, 1.0f);
	box[7].set(1.0f, 1.0f, zfar, 1.0f);

	int i;
	for (i = 0; i < 8; ++i) {
		box[i] = mtxInvViewProj.apply(box[i]);
		box[i].scl(nxCalc::rcp0(box[i].w));
	}

	cxVec up = nxVec::cross(vdir, dir);
	up = nxVec::cross(dir, up).get_normalized();
	pos = mtxInvView.get_translation();
	cxVec tgt = pos - dir;
	cxMtx vm;
	vm.mk_view(pos, tgt, up);

	float ymin = FLT_MAX;
	float ymax = -ymin;
	for (i = 0; i < 8; ++i) {
		float ty = box[i].x*vm.m[0][1] + box[i].y*vm.m[1][1] + box[i].z*vm.m[2][1] + vm.m[3][1];
		ymin = nxCalc::min(ymin, ty);
		ymax = nxCalc::max(ymax, ty);
	}

	float sy = nxCalc::max(1e-8f, ::sqrtf(1.0f - nxCalc::sq(-vdir.dot(dir))));
	float t = (dnear + ::sqrtf(dnear*dfar)) / sy;
	float y = (ymax - ymin) + t;

	cxMtx cm;
	cm.identity();
	cm.m[0][0] = -1;
	cm.m[1][1] = (y + t) / (y - t);
	cm.m[1][3] = 1.0f;
	cm.m[3][1] = (-2.0f*y*t) / (y - t);
	cm.m[3][3] = 0.0f;

	pos += up * (ymin - t);
	tgt = pos - dir;
	vm.mk_view(pos, tgt, up);

	cxMtx tm = vm * cm;
	cxVec vmin(FLT_MAX);
	cxVec vmax(-FLT_MAX);
	for (i = 0; i < 8; ++i) {
		xt_float4 tqv = tm.apply(box[i]);
		cxVec tv;
		tv.from_mem(tqv);
		tv.scl(nxCalc::rcp0(tqv.w));
		vmin = nxVec::min(vmin, tv);
		vmax = nxVec::max(vmax, tv);
	}

	float zmin = vmin.z;
	float ext = s_smapMargin;
	cxVec offs = dir * ext;
	for (i = 0; i < 8; ++i) {
		xt_float4 tqv;
		tqv.x = box[i].x - offs.x;
		tqv.y = box[i].y - offs.y;
		tqv.z = box[i].z - offs.z;
		tqv.w = 1.0f;
		tqv = tm.apply(tqv);
		zmin = nxCalc::min(zmin, nxCalc::div0(tqv.z, tqv.w));
	}
	vmin.z = zmin;

	cxMtx fm;
	fm.identity();
	cxVec vd = vmax - vmin;
	vd.x = nxCalc::rcp0(vd.x);
	vd.y = nxCalc::rcp0(vd.y);
	vd.z = nxCalc::rcp0(vd.z);
	cxVec vs = cxVec(2.0, 2.0f, 1.0f) * vd;
	cxVec vt = cxVec(vmin.x + vmax.x, vmin.y + vmax.y, vmin.z).neg_val() * vd;
	fm.m[0][0] = vs.x;
	fm.m[1][1] = vs.y;
	fm.m[2][2] = vs.z;
	fm.m[3][0] = vt.x;
	fm.m[3][1] = vt.y;
	fm.m[3][2] = vt.z;
	fm = cm * fm;

	pCtx->shadow.viewProjMtx = vm * fm;
}

static void update_shadow() {
	if (s_shadowUniform) {
		update_shadow_uni();
	} else {
		update_shadow_persp();
	}
	if (s_shadowUpdateFlg) {
		if (s_pDraw) {
			s_drwCtx.shadow.mtx = s_drwCtx.shadow.viewProjMtx * s_pDraw->get_shadow_bias_mtx();
		}
	}
	s_shadowUpdateFlg = false;
}

cxMtx get_shadow_view_proj_mtx() {
	update_shadow();
	return s_drwCtx.shadow.viewProjMtx;
}

void set_hemi_upper(const float r, const float g, const float b) {
	s_drwCtx.hemi.upper.set(r, g, b);
}

void set_hemi_lower(const float r, const float g, const float b) {
	s_drwCtx.hemi.lower.set(r, g, b);
}

void set_hemi_const(const float r, const float g, const float b) {
	set_hemi_upper(r, g, b);
	set_hemi_lower(r, g, b);
}

void set_hemi_const(const float val) {
	set_hemi_const(val, val, val);
}

void scl_hemi_upper(const float s) {
	s_drwCtx.hemi.upper.scl(s);
}

void scl_hemi_lower(const float s) {
	s_drwCtx.hemi.lower.scl(s);
}

void set_hemi_up(const cxVec& v) {
	s_drwCtx.hemi.set_upvec(v);
}

void set_hemi_exp(const float e) {
	s_drwCtx.hemi.exp = e;
}

void set_hemi_gain(const float g) {
	s_drwCtx.hemi.gain = g;
}

void reset_hemi() {
	s_drwCtx.hemi.reset();
}

void set_spec_dir(const cxVec& v) {
	s_drwCtx.spec.set_dir(v);
}

void set_spec_dir_to_shadow() {
	set_spec_dir(get_shadow_dir());
}

void set_spec_rgb(const float r, const float g, const float b) {
	s_drwCtx.spec.set_rgb(r, g, b);
}

void set_spec_shadowing(const float s) {
	s_drwCtx.spec.shadowing = nxCalc::saturate(s);
}

void set_fog_rgb(const float r, const float g, const float b) {
	s_drwCtx.fog.set_rgb(r, g, b);
}

void set_fog_density(const float a) {
	s_drwCtx.fog.set_density(a);
}

void set_fog_range(const float start, const float end) {
	s_drwCtx.fog.set_range(start, end);
}

void set_fog_curve(const float cp1, const float cp2) {
	s_drwCtx.fog.set_curve(cp1, cp2);
}

void set_fog_linear() {
	s_drwCtx.fog.set_linear();
}

void reset_fog() {
	s_drwCtx.fog.reset();
}

void set_gamma(const float gamma) {
	s_drwCtx.cc.set_gamma(gamma);
}

void set_gamma_rgb(const float r, const float g, const float b) {
	s_drwCtx.cc.set_gamma_rgb(r, g, b);
}

void set_exposure(const float e) {
	s_drwCtx.cc.set_exposure(e);
}

void set_exposure_rgb(const float r, const float g, const float b) {
	s_drwCtx.cc.set_exposure_rgb(r, g, b);
}

void set_linear_white(const float val) {
	s_drwCtx.cc.set_linear_white(val);
}

void set_linear_white_rgb(const float r, const float g, const float b) {
	s_drwCtx.cc.set_linear_white_rgb(r, g, b);
}

void set_linear_gain(const float val) {
	s_drwCtx.cc.set_linear_gain(val);
}

void set_linear_gain_rgb(const float r, const float g, const float b) {
	s_drwCtx.cc.set_linear_gain_rgb(r, g, b);
}

void set_linear_bias(const float val) {
	s_drwCtx.cc.set_linear_bias(val);
}

void set_linear_bias_rgb(const float r, const float g, const float b) {
	s_drwCtx.cc.set_linear_bias_rgb(r, g, b);
}

void reset_color_correction() {
	s_drwCtx.cc.reset();
}

ScnObj* add_obj(sxModelData* pMdl, const char* pName) {
	ScnObj* pObj = nullptr;
	if (pMdl && s_pObjList && s_pObjMap) {
		if (pName) {
			s_pObjMap->get(pName, &pObj);
		}
		if (!pObj) {
			pObj = s_pObjList->new_item();
			if (pObj) {
				::memset(pObj, 0, sizeof(ScnObj));
				char name[32];
				const char* pObjName = pName;
				if (!pObjName) {
					XD_SPRINTF(XD_SPRINTF_BUF(name, sizeof(name)), "$obj@%p", pObj);
					pObjName = name;
				}
				int nbat = pMdl->mBatNum;
				size_t extMemSize = XD_BIT_ARY_SIZE(uint8_t, nbat);
				size_t paramMemSize = sizeof(Draw::MdlParam);
				pObj->mpName = nxCore::str_dup(pObjName);
				pObj->mpMdlWk = cxModelWork::create(pMdl, paramMemSize, extMemSize);
				pObj->mpMotWk = cxMotionWork::create(pMdl);
				pObj->mJob.mFunc = obj_exec_job;
				pObj->mJob.mpData = pObj;
				Draw::MdlParam* pMdlParam = get_obj_mdl_params(*pObj);
				if (pMdlParam) {
					pMdlParam->reset();
				}
				pObj->mpBatJobs = (sxJob*)nxCore::mem_alloc(sizeof(sxJob) * nbat);
				if (pObj->mpBatJobs) {
					for (int i = 0; i < nbat; ++i) {
						pObj->mpBatJobs[i].mpData = pObj;
						pObj->mpBatJobs[i].mParam = i;
					}
				}
				s_pObjMap->put(pObj->mpName, pObj);
			}
		}
	}
	return pObj;
}

ScnObj* add_obj(Pkg* pPkg, const char* pName) {
	return add_obj(get_pkg_default_model(pPkg), pName);
}

ScnObj* add_obj(const char* pName) {
	ScnObj* pObj = nullptr;
	if (pName) {
		Scene::load_pkg(pName);
		Pkg* pPkg = Scene::find_pkg(pName);
		if (pPkg) {
			pObj = add_obj(pPkg, pName);
		}
	}
	return pObj;
}

ScnObj* find_obj(const char* pName) {
	ScnObj* pObj = nullptr;
	if (pName && s_pObjMap) {
		s_pObjMap->get(pName, &pObj);
	}
	return pObj;
}

void del_obj(ScnObj* pObj) {
	if (!pObj) return;
	if (!s_pObjMap) return;
	if (!s_pObjList) return;
	s_pObjMap->remove(pObj->mpName);
	s_pObjList->remove(pObj);
}

void del_all_objs() {
	if (!s_pObjList) return;
	if (s_pObjMap) {
		for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
			ScnObj* pObj = itr.item();
			s_pObjMap->remove(pObj->mpName);
		}
	}
	s_pObjList->purge();
}

int add_all_pkg_objs(Pkg* pPkg, const char* pNamePrefix) {
	int nobj = 0;
	if (s_pRsrcMgr && s_pRsrcMgr->contains_pkg(pPkg) && pPkg->mMdlNum > 0) {
		for (Pkg::EntryList::Itr itr = pPkg->get_iterator(); !itr.end(); itr.next()) {
			Pkg::Entry* pEnt = itr.item();
			if (pEnt->mpData && pEnt->mpData->is<sxModelData>()) {
				sxModelData* pMdl = pEnt->mpData->as<sxModelData>();
				char name[256];
				XD_SPRINTF(XD_SPRINTF_BUF(name, sizeof(name)), "%s%s", pNamePrefix ? pNamePrefix : "", pEnt->mpName);
				add_obj(pMdl, name);
				++nobj;
			}
		}
	}
	return nobj;
}

int add_all_pkg_objs(const char* pPkgName, const char* pNamePrefix) {
	return add_all_pkg_objs(Scene::load_pkg(pPkgName), pNamePrefix);
}

void for_all_pkg_models(Pkg* pPkg, void (*func)(sxModelData*, void*), void* pFuncData) {
	if (func && s_pRsrcMgr && s_pRsrcMgr->contains_pkg(pPkg) && pPkg->mMdlNum > 0) {
		for (Pkg::EntryList::Itr itr = pPkg->get_iterator(); !itr.end(); itr.next()) {
			Pkg::Entry* pEnt = itr.item();
			if (pEnt->mpData && pEnt->mpData->is<sxModelData>()) {
				sxModelData* pMdl = pEnt->mpData->as<sxModelData>();
				func(pMdl, pFuncData);
			}
		}
	}
}

void add_obj_instances(const char* pPkgName, const ScnObj::InstInfo* pInstInfos, const int num, const char* pName) {
	if (num <= 0) return;
	if (!pInstInfos) return;
	if (!pPkgName) return;
	Pkg* pPkg = find_pkg(pPkgName);
	if (!pPkg) {
		pPkg = load_pkg(pPkgName);
	}
	if (!pPkg) return;
	char objName[128];
	for (int i = 0; i < num; ++i) {
		XD_SPRINTF(XD_SPRINTF_BUF(objName, sizeof(objName)), "%s%d", pName ? pName : pPkgName, i);
		ScnObj* pObj = Scene::add_obj(pPkg, objName);
		if (pObj) {
			const ScnObj::InstInfo* pInfo = &pInstInfos[i];
			pObj->set_world_quat_pos(pInfo->get_quat(), pInfo->get_pos());
			pObj->set_model_variation(pInfo->variation);
		}
	}
}

int get_num_objs() {
	return s_pObjList ? s_pObjList->get_count() : 0;
}

void for_each_obj(bool (*func)(ScnObj*, void*), void* pWkMem) {
	if (!s_pObjList) return;
	if (!func) return;
	for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
		ScnObj* pObj = itr.item();
		if (pObj) {
			bool cont = func(pObj, pWkMem);
			if (!cont) break;
		}
	}
}

void set_obj_exec_func(const char* pName, ScnObj::ExecFunc exec) {
	ScnObj* pObj = find_obj(pName);
	if (pObj) {
		pObj->mExecFunc = exec;
	}
}

void set_obj_del_func(const char* pName, ScnObj::DelFunc del) {
	ScnObj* pObj = find_obj(pName);
	if (pObj) {
		pObj->mDelFunc = del;
	}
}

cxVec get_obj_world_pos(const char* pName) {
	ScnObj* pObj = find_obj(pName);
	if (pObj) {
		return pObj->get_world_pos();
	}
	return cxVec(0.0f);
}

cxVec get_obj_center_pos(const char* pName) {
	ScnObj* pObj = find_obj(pName);
	if (pObj) {
		return pObj->get_center_pos();
	}
	return cxVec(0.0f);
}


void set_ref_scr_size(const float w, const float h) {
	s_refScrW = w;
	s_refScrH = h;
}

void set_spr_gamma(const float gval) {
	s_sprGamma.fill(Draw::clip_gamma(gval));
}

void set_spr_gamma_rgb(const float r, const float g, const float b) {
	s_sprGamma.set(Draw::clip_gamma(r), Draw::clip_gamma(g), Draw::clip_gamma(b));
}

void set_spr_defaults(Draw::Sprite* pSpr) {
	if (!pSpr) return;
	::memset(pSpr, 0, sizeof(Draw::Sprite));
	if (s_refScrW < 0.0f || s_refScrH < 0.0f) {
		if (s_pDraw) {
			pSpr->refWidth = float(s_pDraw->get_screen_width());
			pSpr->refHeight = float(s_pDraw->get_screen_height());
		} else {
			pSpr->refWidth = 0.0f;
			pSpr->refHeight = 0.0f;
		}
	} else {
		pSpr->refWidth = s_refScrW;
		pSpr->refHeight = s_refScrH;
	}
	pSpr->gamma = s_sprGamma;
	pSpr->color.set(1.0f);
}

void sprite(const xt_float2 pos[4], const xt_float2 tex[4], const cxColor clr, sxTextureData* pTex, cxColor* pClrs) {
	if (!s_pDraw) return;
	if (!s_pDraw->sprite) return;
	Draw::Sprite spr;
	set_spr_defaults(&spr);
	for (int i = 0; i < 4; ++i) {
		spr.pos[i] = pos[i];
		spr.tex[i] = tex[i];
	}
	spr.color = clr;
	spr.pTex = pTex;
	spr.pClrs = pClrs;
	s_pDraw->sprite(&spr);
}


float get_ground_height(sxCollisionData* pCol, const cxVec pos, const float offsTop, const float offsBtm) {
	float h = pos.y;
	if (pCol) {
		cxVec posTop = pos;
		cxVec posBtm = pos;
		posTop.y += offsTop;
		posBtm.y -= offsBtm;
		sxCollisionData::NearestHit gndHit = pCol->nearest_hit(cxLineSeg(posTop, posBtm));
		if (gndHit.count > 0) {
			h = gndHit.pos.y;
		}
	}
	return h;
}

struct WallAdjTriInfo {
	int32_t ipol;
	int32_t itri;
};

struct WallAdjWk {
	uint32_t* pStamps;
	WallAdjTriInfo* pTris;
	int triCount;
	float slopeLim;
};

static bool wall_adj_tri_func(const sxCollisionData& col, const sxCollisionData::Tri& tri, void* pWkMem) {
	if (!pWkMem) return false;
	WallAdjWk* pWk = (WallAdjWk*)pWkMem;
	bool flg = true;
	if (pWk->slopeLim > 0.0f) {
		flg = ::fabsf(tri.nrm.y) < pWk->slopeLim;
	}
	if (flg) {
		WallAdjTriInfo* pTriInfo = &pWk->pTris[pWk->triCount];
		pTriInfo->ipol = tri.ipol;
		pTriInfo->itri = tri.itri;
		++pWk->triCount;
	}
	return true;
}

static bool wall_adj_pnt_in_tri(const cxVec& pnt, const cxVec vtx[3], const cxVec nrm) {
	for (int i = 0; i < 3; ++i) {
		cxVec ev = vtx[(i + 1) % 3] - vtx[i];
		cxVec vv = pnt - vtx[i];
		cxVec t = nxVec::cross(ev, vv);
		if (t.dot(nrm) < 0.0f) return false;
	}
	return true;
}

#define SCN_GLB_MEM_CMN_LOCK 0

bool wall_adj(const sxJobContext* pJobCtx, sxCollisionData* pCol, const cxVec& newPos, const cxVec& oldPos, const float radius, cxVec* pAdjPos, const float wallSlopeLim) {
	if (!pCol) return false;
	bool res = false;
	int ntri = pCol->mTriNum;
	size_t numStampBytes = XD_BIT_ARY_SIZE(uint32_t, ntri) * sizeof(uint32_t);
	size_t numTriBytes = ntri * sizeof(WallAdjTriInfo);
	uint32_t* pStamps = nullptr;
	WallAdjTriInfo* pTris = nullptr;
	const uint32_t stampsTag = XD_FOURCC('W', 'F', 'l', 'g');
	const uint32_t trisTag = XD_FOURCC('W', 'T', 'r', 'i');
	cxHeap* pHeap = get_job_local_heap(pJobCtx);
	if (pHeap) {
#ifdef XD_USE_OMP
		glb_mem_lock_acq();
#endif
		pStamps = (uint32_t*)pHeap->alloc(numStampBytes, stampsTag);
		pTris = (WallAdjTriInfo*)pHeap->alloc(numTriBytes, trisTag);
#ifdef XD_USE_OMP
		glb_mem_lock_rel();
#endif
	} else {
#if SCN_GLB_MEM_CMN_LOCK
		glb_mem_lock_acq();
		pStamps = (uint32_t*)glb_mem_alloc_impl(numStampBytes, stampsTag);
		pTris = (WallAdjTriInfo*)glb_mem_alloc_impl(numTriBytes, trisTag);
		glb_mem_lock_rel();
#else
		pStamps = (uint32_t*)glb_mem_alloc(numStampBytes, stampsTag);
		pTris = (WallAdjTriInfo*)glb_mem_alloc(numTriBytes, trisTag);
#endif
	}
	if (pStamps && pTris) {
		WallAdjWk wk;
		::memset(pStamps, 0, numStampBytes);
		wk.pStamps = pStamps;
		wk.pTris = pTris;
		wk.triCount = 0;
		wk.slopeLim = wallSlopeLim;
		int adjCount = 0;
		float maxRange = radius + nxVec::dist(oldPos, newPos)*2.0f;
		cxAABB range;
		range.from_sph(cxSphere(newPos, maxRange));
		pCol->for_tris_in_range(wall_adj_tri_func, range, &wk);
		int n = wk.triCount;
		if (n > 0) {
			cxVec triVtx[3];
			cxAABB triBBox;
			cxVec triNrm;
			const int itrMax = 15;
			int itrCnt = -1;
			int state = 0;
			int prevState = 0;
			int adjTriIdx = -1;
			cxVec opos = oldPos;
			cxVec npos = newPos;
			cxVec apos = newPos;
			float sqDist = FLT_MAX;
			do {
				++itrCnt;
				for (int i = 0; i < n; ++i) {
					if (adjTriIdx != i) {
						WallAdjTriInfo triInfo = wk.pTris[i];
						for (int j = 0; j < 3; ++j) {
							int vtxPntIdx = pCol->get_pol_tri_pnt_idx(triInfo.ipol, triInfo.itri, j);
							triVtx[2 - j] = pCol->get_pnt(vtxPntIdx); /* CW -> CCW */
						}
						triBBox.set(triVtx[0]);
						triBBox.add_pnt(triVtx[1]);
						triBBox.add_pnt(triVtx[2]);
						bool calcFlg = true;
						if (itrCnt > 0) {
							calcFlg = !XD_BIT_ARY_CK(uint32_t, wk.pStamps, i);
						} else {
							float ymin = triBBox.get_min_pos().y;
							float ymax = triBBox.get_max_pos().y;
							float ny = newPos.y;
							if (!(ny >= ymin && ny <= ymax)) {
								XD_BIT_ARY_ST(uint32_t, wk.pStamps, i);
								calcFlg = false;
							}
						}
						if (calcFlg) {
							cxPlane triPlane;
							triNrm = nxGeom::tri_normal_ccw(triVtx[0], triVtx[1], triVtx[2]);
							triPlane.calc(triVtx[0], triNrm);
							float sdist = triPlane.signed_dist(npos);
							float adist = ::fabsf(sdist);
							if (sdist > 0.0f && adist > radius) {
								if (adist > maxRange) {
									XD_BIT_ARY_ST(uint32_t, wk.pStamps, i);
									calcFlg = false;
								}
							}
							if (calcFlg) {
								cxVec isect;
								if (adist > radius) {
									if (adist > maxRange) {
										XD_BIT_ARY_ST(uint32_t, wk.pStamps, i);
									}
									if (!triPlane.seg_intersect(npos, opos, nullptr, &isect)) {
										calcFlg = false;
									}
								} else {
									isect = npos + (sdist > 0.0f ? triNrm.neg_val() : triNrm) * adist;
								}
								if (calcFlg) {
									float dist2;
									float r = radius;
									cxVec adj;
									const float eps = 1e-6f;
									float margin = 0.005f;
									if (wall_adj_pnt_in_tri(isect, triVtx, triNrm)) {
										state = 1;
										adist = (r - adist) + margin;
										adj = triNrm;
										if (sdist <= 0.0f) {
											adj.neg();
										}
										adj.scl(adist);
									} else {
										if (state != 1) {
											cxVec vec(0.0f);
											float mov = 0.0f;
											float rr = nxCalc::sq(r);
											adist = FLT_MAX;
											bool flg = false;
											for (int j = 0; j < 3; ++j) {
												cxVec vtx = triVtx[j];
												cxVec vv = npos - vtx;
												dist2 = vv.mag2();
												if (dist2 <= rr && dist2 < adist) {
													vec = vv.get_normalized();
													mov = ::sqrtf(dist2);
													if (vec.dot(opos - vtx) < 0.0f) {
														vec.neg();
													} else {
														mov = -mov;
													}
													flg = true;
												}
												cxVec ev = triVtx[(j + 1) % 3] - vtx;
												dist2 = ev.mag2();
												if (dist2 > eps) {
													float nd = vv.dot(ev);
													if (nd >= eps && dist2 >= nd) {
														isect = vtx + vv*(nd / dist2);
														cxVec av = npos - isect;
														dist2 = av.mag2();
														if (dist2 <= rr && dist2 < adist) {
															mov = ::sqrtf(dist2);
															vec = av.get_normalized();
															if (vec.dot(opos - isect) < 0.0f) {
																vec.neg();
															} else {
																mov = -mov;
															}
															adist = dist2;
															flg = true;
														}
													}
												}
											}
											if (flg) {
												state = 2;
												adj = vec * (r + mov + margin);
											} else {
												calcFlg = false;
											}
										}
									}
									if (calcFlg) {
										adj.add(npos);
										dist2 = nxVec::dist2(adj, opos);
										if (dist2 > sqDist) {
											if (prevState == 1) {
												calcFlg = false;
											}
											if (state != 1) {
												calcFlg = false;
											}
										}
										if (calcFlg) {
											prevState = state;
											sqDist = dist2;
											apos = adj;
											if (nxGeom::seg_tri_intersect_ccw_n(opos, apos, triVtx[0], triVtx[1], triVtx[2], triNrm, &adj)) {
												apos = nxVec::lerp(opos, adj, 0.9f);
												if (nxGeom::seg_tri_intersect_ccw_n(opos, npos, triVtx[0], triVtx[1], triVtx[2], triNrm, &adj)) {
													apos = adj + (opos - adj).get_normalized()*radius;
												}
											}
											adjTriIdx = i;
										}
									}
								}
							}
						}
					}
				}
				if (state) {
					npos = apos;
					++adjCount;
				}
			} while (state && itrCnt < itrMax);
			res = adjCount > 0;
			if (res) {
				if (pAdjPos) {
					*pAdjPos = npos;
				}
			}
		}
	}
	if (pHeap) {
#ifdef XD_USE_OMP
		glb_mem_lock_acq();
#endif
		pHeap->free(pTris);
		pHeap->free(pStamps);
#ifdef XD_USE_OMP
		glb_mem_lock_rel();
#endif
	} else {
#if SCN_GLB_MEM_CMN_LOCK
		glb_mem_lock_acq();
		glb_mem_free_impl(pTris);
		glb_mem_free_impl(pStamps);
		glb_mem_lock_rel();
#else
		glb_mem_free(pTris);
		glb_mem_free(pStamps);
#endif
	}
	return res;
}

static bool sph_sph_sub(const cxSphere& movSph, const cxVec& vel, const cxSphere& staticSph, cxVec* pSepVec, float margin) {
	cxVec sepVec(0.0f);
	bool flg = movSph.overlaps(staticSph);
	if (flg) {
		float sepDist = movSph.get_radius() + staticSph.get_radius() + margin;
		cxVec sepDir = vel.neg_val();
		cxVec dv = movSph.get_center() - staticSph.get_center();
		cxVec vec = dv + sepDir*dv.mag();
		float len = vec.mag();
		if (len < 1e-5f) {
			vec = sepDir.get_normalized();
		} else {
			sepDist /= len;
		}
		sepVec = vec*sepDist - dv;
	}
	if (pSepVec) {
		*pSepVec = sepVec;
	}
	return flg;
}

bool sph_sph_adj(const cxVec& newPos, const cxVec& oldPos, float radius, const cxVec& staticPos, const float staticRadius, cxVec* pAdjPos, const float reflectFactor, const float margin) {
	cxVec sepVec;
	cxVec tstPos = newPos;
	cxVec adjPos = tstPos;
	cxVec vel = newPos - oldPos;
	cxSphere stSph(staticPos, staticRadius);
	bool flg = sph_sph_sub(cxSphere(newPos, radius), vel, stSph, &sepVec, margin);
	if (flg) {
		cxVec nv = (tstPos + sepVec - staticPos).get_normalized();
		cxVec rv = nxVec::reflect(vel, nv) * reflectFactor;
		adjPos += rv;
		if (cxSphere(adjPos, radius).overlaps(stSph)) {
			adjPos = tstPos + sepVec;
		}
	}
	if (pAdjPos) {
		*pAdjPos = adjPos;
	}
	return flg;
}

static bool sph_cap_sub(const cxSphere& movSph, const cxVec& vel, const cxCapsule& staticCap, cxVec* pSepVec, cxVec* pAxisPnt, float margin) {
	cxVec sepVec(0.0f);
	cxVec axisPnt(0.0f);
	bool flg = movSph.overlaps(staticCap, &axisPnt);
	if (flg) {
		float sepDist = movSph.get_radius() + staticCap.get_radius() + margin;
		cxVec sepDir = vel.neg_val();
		cxVec dv = movSph.get_center() - axisPnt;
		cxVec vec = dv + sepDir*dv.mag();
		float len = vec.mag();
		if (len < 1e-5f) {
			vec = sepDir.get_normalized();
		} else {
			sepDist /= len;
		}
		sepVec = vec*sepDist - dv;
	}
	if (pSepVec) {
		*pSepVec = sepVec;
	}
	if (pAxisPnt) {
		*pAxisPnt = axisPnt;
	}
	return flg;
}

bool sph_cap_adj(const cxVec& newPos, const cxVec& oldPos, float radius, const cxVec& staticPos0, const cxVec& staticPos1, const float staticRadius, cxVec* pAdjPos, const float reflectFactor, const float margin) {
	cxVec sepVec;
	cxVec axisPnt;
	cxVec tstPos = newPos;
	cxVec adjPos = tstPos;
	cxVec vel = newPos - oldPos;
	cxCapsule stCap(staticPos0, staticPos1, staticRadius);
	bool flg = sph_cap_sub(cxSphere(newPos, radius), vel, stCap, &sepVec, &axisPnt, margin);
	if (flg) {
		cxVec nv = (tstPos + sepVec - axisPnt).get_normalized();
		cxVec rv = nxVec::reflect(vel, nv) * reflectFactor;
		adjPos += rv;
		if (cxSphere(adjPos, radius).overlaps(stCap)) {
			adjPos = tstPos + sepVec;
		}
	}
	if (pAdjPos) {
		*pAdjPos = adjPos;
	}
	return flg;
}

static void save_job_cnts(const int lvl) {
	if (lvl < 0) return;
	if (!s_pBgd) return;
	if (!s_pBgdJobCnts) return;
	int nwrk = s_pBgd->get_workers_num();
	int idx = lvl * nwrk;
	if (idx >= s_numBgdJobCnts) return;
	for (int i = 0; i < nwrk; ++i) {
		s_pBgdJobCnts[idx + i] = s_pBgd->get_jobs_done_count(i);
	}
}

static void job_queue_alloc(int njob) {
	if (s_pJobQue) {
		if (njob > nxTask::queue_get_max_job_num(s_pJobQue)) {
			nxTask::queue_destroy(s_pJobQue);
			s_pJobQue = nullptr;
		}
	}
	if (!s_pJobQue) {
		s_pJobQue = nxTask::queue_create(njob);
	}
}

void copy_prev_world_data() {
	int nobj = get_num_objs();
	if (nobj < 1) return;
#if 0
	for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
		ScnObj* pObj = itr.item();
		if (pObj) {
			if (pObj->mpMdlWk) {
				pObj->mpMdlWk->copy_prev_world_bbox();
			}
			if (pObj->mpMotWk) {
				pObj->mpMotWk->copy_prev_world();
			} else {
				if (pObj->mpMdlWk) {
					pObj->mpMdlWk->copy_prev_world_xform();
				}
			}
		}
	}
#else
	int njob = nobj;
	job_queue_alloc(njob);
	if (s_pJobQue) {
		nxTask::queue_purge(s_pJobQue);
		for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
			ScnObj* pObj = itr.item();
			if (pObj) {
				pObj->mJob.mFunc = obj_copy_prev_job;
				nxTask::queue_add(s_pJobQue, &pObj->mJob);
			}
		}
		nxTask::queue_exec(s_pJobQue, s_pBgd);
		save_job_cnts(0);
	}
#endif
}

void exec() {
	int nobj = get_num_objs();
	int njob = nobj;
	if (njob < 1) return;
	copy_prev_world_data();
	job_queue_alloc(njob);
	if (s_pJobQue) {
		for (int i = 0; i < SCN_NUM_EXEC_PRIO; ++i) {
			nxTask::queue_purge(s_pJobQue);
			if (s_pObjList) {
				for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
					ScnObj* pObj = itr.item();
					if (pObj) {
						if (pObj->mPriority.exec == i) {
							pObj->mJob.mFunc = obj_exec_job;
							nxTask::queue_add(s_pJobQue, &pObj->mJob);
						}
					}
				}
			}
			nxTask::queue_exec(s_pJobQue, s_pBgd);
			save_job_cnts(1 + i);
		}
	}
}

void visibility() {
	if (!s_pObjList) return;
	int nobj = get_num_objs();
	if (nobj < 1) return;
	int njob = nobj;
	update_view();
	update_shadow();
	job_queue_alloc(njob);
	if (s_pJobQue) {
		nxTask::queue_purge(s_pJobQue);
		for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
			ScnObj* pObj = itr.item();
			if (pObj) {
				pObj->mJob.mFunc = obj_visibility_job;
				nxTask::queue_add(s_pJobQue, &pObj->mJob);
			}
		}
		nxTask::queue_exec(s_pJobQue, s_pBgd);
		save_job_cnts(get_visibility_job_lvl());
	}
}

void draw(bool discard) {
	if (!s_pObjList) return;

	for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
		ScnObj* pObj = itr.item();
		if (pObj) {
			pObj->shadow_cast();
		}
	}

	for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
		ScnObj* pObj = itr.item();
		if (pObj) {
			pObj->draw_opaq();
		}
	}

	for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
		ScnObj* pObj = itr.item();
		if (pObj) {
			pObj->draw_semi(discard);
		}
	}
}

float speed() {
	return s_speed;
}

} // Scene


static void obj_bat_draw(ScnObj* pObj, const int ibat, const Draw::Mode mode) {
	cxModelWork* pWk = pObj->mpMdlWk;
	if (!pWk) return;
	if (pWk->is_bat_mtl_hidden(ibat)) return;
	bool cullFlg = false;
	bool isShadowcast = mode == Draw::DRWMODE_SHADOW_CAST;
	if (isShadowcast) {
		if (pWk->mpExtMem) {
			uint32_t* pCastCullBits = (uint32_t*)pWk->mpExtMem;
			cullFlg = XD_BIT_ARY_CK(uint32_t, pCastCullBits, ibat);
		}
	} else {
		cullFlg = XD_BIT_ARY_CK(uint32_t, pWk->mpCullBits, ibat);
	}
	if (cullFlg) {
		return;
	}
	if (!isShadowcast) {
		if (pObj->mBatchPreDrawFunc) {
			pObj->mBatchPreDrawFunc(pObj, ibat);
		}
	}
	Scene::update_view();
	Scene::update_shadow();
	Draw::Context* pCtx = &s_drwCtx;
	if (s_pDraw) {
		pCtx->view.rotMode = s_viewRot;
		pCtx->glb.useBump = s_useBump;
		pCtx->glb.useSpec = s_useSpec;
		float sdens = pCtx->shadow.dens;
		if (!isShadowcast && pObj->mDisableShadowRecv) {
			pCtx->shadow.dens = 0.0f;
		}
		s_pDraw->batch(pWk, ibat, mode, pCtx);
		pCtx->shadow.dens = sdens;
	}
	if (!isShadowcast) {
		if (pObj->mBatchPostDrawFunc) {
			pObj->mBatchPostDrawFunc(pObj, ibat);
		}
	}
}

const char* ScnObj::get_batch_mtl_name(const int ibat) const {
	const char* pName = nullptr;
	const sxModelData* pMdl = get_model_data();
	if (pMdl) {
		const sxModelData::Material* pMtl = pMdl->get_batch_material(ibat);
		if (pMtl) {
			pName = pMdl->get_str(pMtl->mNameId);
		}
	}
	return pName;
}

void ScnObj::set_base_color_scl(const float r, const float g, const float b) {
	Draw::MdlParam* pParam = get_obj_mdl_params(*this);
	if (pParam) {
		pParam->baseColorScl.set(r, g, b);
	}
}

void ScnObj::set_base_color_scl(const float s) {
	set_base_color_scl(s, s, s);
}

void ScnObj::set_shadow_offs_bias(const float bias) {
	Draw::MdlParam* pParam = get_obj_mdl_params(*this);
	if (pParam) {
		pParam->shadowOffsBias = bias;
	}
}

void ScnObj::set_shadow_weight_bias(const float bias) {
	Draw::MdlParam* pParam = get_obj_mdl_params(*this);
	if (pParam) {
		pParam->shadowWeightBias = bias;
	}
}

void ScnObj::set_shadow_density_scl(const float scl) {
	Draw::MdlParam* pParam = get_obj_mdl_params(*this);
	if (pParam) {
		pParam->shadowDensScl = scl;
	}
}

void ScnObj::clear_int_wk() {
	int n = XD_ARY_LEN(mIntWk);
	for (int i = 0; i < n; ++i) {
		mIntWk[i] = 0;
	}
}

void ScnObj::clear_flt_wk() {
	int n = XD_ARY_LEN(mFltWk);
	for (int i = 0; i < n; ++i) {
		mFltWk[i] = 0.0f;
	}
}

void ScnObj::clear_ptr_wk() {
	int n = XD_ARY_LEN(mPtrWk);
	for (int i = 0; i < n; ++i) {
		mPtrWk[i] = nullptr;
	}
}

sxMotionData* ScnObj::find_motion(const char* pMotName) {
	sxMotionData* pMot = nullptr;
	if (pMotName && s_pRsrcMgr) {
		pMot = s_pRsrcMgr->find_motion_for_model(get_model_data(), pMotName);
	}
	return pMot;
}

int ScnObj::get_current_motion_num_frames() const {
	int n = 0;
	if (mpMotWk && mpMotWk->mpCurrentMotData) {
		n = mpMotWk->mpCurrentMotData->mFrameNum;
	}
	return n;
}

float ScnObj::get_motion_frame() const {
	float frame = 0.0f;
	if (mpMotWk) {
		frame = mpMotWk->mFrame;
	}
	return frame;
}

void ScnObj::set_motion_frame(const float frame) {
	if (mpMotWk) {
		mpMotWk->mFrame = frame;
	}
}

void ScnObj::exec_motion(const sxMotionData* pMot, const float frameAdd) {
	if (pMot && mpMotWk) {
		mpMotWk->apply_motion(pMot, frameAdd);
	}
}

void ScnObj::init_motion_blend(const int duration) {
	if (mpMotWk) {
		mpMotWk->blend_init(duration);
	}
}

void ScnObj::exec_motion_blend() {
	if (mpMotWk) {
		mpMotWk->blend_exec();
	}
}

void ScnObj::set_motion_uniform_scl(const float scl) {
	if (mpMotWk) {
		mpMotWk->mUniformScale = scl;
	}
}

void ScnObj::update_world() {
	if (mpMotWk) {
		mpMotWk->calc_world();
	}
}

void ScnObj::update_skin() {
	if (mpMdlWk) {
		mpMdlWk->set_pose(mpMotWk);
	}
}

void ScnObj::update_bounds() {
	if (mpMdlWk) {
		mpMdlWk->update_bounds();
	}
}

static bool ck_bat_shadow_cast_vis(cxModelWork* pWk, const int ibat) {
	cxAABB batBB = pWk->mpBatBBoxes[ibat];
	cxMtx castVP = Scene::get_shadow_view_proj_mtx();
	cxVec bbmin = batBB.get_min_pos();
	cxVec bbmax = batBB.get_max_pos();
	float x0, y0, x1, y1;
	if (Scene::is_shadow_uniform()) {
		xt_float4 vmin = castVP.apply(bbmin.get_pnt());
		xt_float4 vmax = castVP.apply(bbmax.get_pnt());
		float xmin = nxCalc::div0(vmin.x, vmin.w);
		float ymin = nxCalc::div0(vmin.y, vmin.w);
		float xmax = nxCalc::div0(vmax.x, vmax.w);
		float ymax = nxCalc::div0(vmax.y, vmax.w);
		x0 = nxCalc::min(xmin, xmax);
		y0 = nxCalc::min(ymin, ymax);
		x1 = nxCalc::max(xmin, xmax);
		y1 = nxCalc::max(ymin, ymax);
	} else {
		xt_float4 bbv[8];
		bbv[0].set(bbmin.x, bbmin.y, bbmin.z, 1.0f);
		bbv[1].set(bbmin.x, bbmax.y, bbmin.z, 1.0f);
		bbv[2].set(bbmax.x, bbmax.y, bbmin.z, 1.0f);
		bbv[3].set(bbmax.x, bbmin.y, bbmin.z, 1.0f);
		bbv[4].set(bbmin.x, bbmin.y, bbmax.z, 1.0f);
		bbv[5].set(bbmin.x, bbmax.y, bbmax.z, 1.0f);
		bbv[6].set(bbmax.x, bbmax.y, bbmax.z, 1.0f);
		bbv[7].set(bbmax.x, bbmin.y, bbmax.z, 1.0f);
		for (int i = 0; i < 8; ++i) {
			bbv[i] = castVP.apply(bbv[i]);
		}
		float rw[8];
		float sx[8];
		float sy[8];
		for (int i = 0; i < 8; ++i) {
			rw[i] = nxCalc::rcp0(bbv[i].w);
		}
		for (int i = 0; i < 8; ++i) {
			sx[i] = bbv[i].x * rw[i];
		}
		for (int i = 0; i < 8; ++i) {
			sy[i] = bbv[i].y * rw[i];
		}
		x0 = sx[0];
		for (int i = 1; i < 8; ++i) { x0 = nxCalc::min(x0, sx[i]); }
		y0 = sy[0];
		for (int i = 1; i < 8; ++i) { y0 = nxCalc::min(y0, sy[i]); }
		x1 = sx[0];
		for (int i = 1; i < 8; ++i) { x1 = nxCalc::max(x1, sx[i]); }
		y1 = sy[0];
		for (int i = 1; i < 8; ++i) { y1 = nxCalc::max(y1, sy[i]); }
	}
	x0 *= 0.5f;
	y0 *= 0.5f;
	x1 *= 0.5f;
	y1 *= 0.5f;
	bool visible = x0 <= 1.0f && x1 >= -1.0f && y0 <= 1.0f && y1 >= -1.0f;
	return visible;
}

void ScnObj::update_visibility() {
	if (mpMdlWk) {
		mpMdlWk->frustum_cull(Scene::get_view_frustum_ptr());

		if (mpMdlWk->mpExtMem) {
			int nbat = mpMdlWk->get_batches_num();
			size_t bitMemSize = XD_BIT_ARY_SIZE(uint8_t, nbat);
			uint32_t* pCastBits = (uint32_t*)mpMdlWk->mpExtMem;
			if (mDisableShadowCast) {
				::memset(pCastBits, 0xFF, bitMemSize);
			} else {
				::memset(pCastBits, 0, bitMemSize);
				if (s_useShadowCastCull) {
					for (int i = 0; i < nbat; ++i) {
						bool vis = ck_bat_shadow_cast_vis(mpMdlWk, i);
						if (!vis) {
							XD_BIT_ARY_ST(uint32_t, pCastBits, i);
						}
					}
				}
			}
		}
	}
}

void ScnObj::update_batch_vilibility(const int ibat) {
	if (mpMdlWk) {
		mpMdlWk->calc_batch_visibility(Scene::get_view_frustum_ptr(), ibat);
		if (mpMdlWk->mpExtMem) {
			uint32_t* pCastBits = (uint32_t*)mpMdlWk->mpExtMem;
			if (mDisableShadowCast) {
				XD_BIT_ARY_ST(uint32_t, pCastBits, ibat);
			} else {
				if (s_useShadowCastCull) {
					bool vis = ck_bat_shadow_cast_vis(mpMdlWk, ibat);
					if (!vis) {
						XD_BIT_ARY_ST(uint32_t, pCastBits, ibat);
					}
				} else {
					XD_BIT_ARY_CL(uint32_t, pCastBits, ibat);
				}
			}
		}
	}
}

void ScnObj::move(const sxMotionData* pMot, const float frameAdd) {
	exec_motion(pMot, frameAdd * s_speed);
	if (mBeforeBlendFunc) {
		mBeforeBlendFunc(this);
	}
	exec_motion_blend();
	if (mAfterBlendFunc) {
		mAfterBlendFunc(this);
	}
	update_world();
	if (mWorldFunc) {
		mWorldFunc(this);
	}
	update_skin();
	update_bounds();
}

int ScnObj::find_skel_id(const char* pName) const {
	int id = -1;
	if (mpMdlWk && mpMdlWk->mpData) {
		id = mpMdlWk->mpData->find_skel_node_id(pName);
	}
	return id;
}

cxMtx ScnObj::get_skel_local_mtx(const int iskl) const {
	xt_xmtx lm;
	if (mpMotWk) {
		lm = mpMotWk->get_node_local_xform(iskl);
	} else {
		lm.identity();
	}
	return nxMtx::mtx_from_xmtx(lm);
}

cxMtx ScnObj::get_skel_local_rest_mtx(const int iskl) const {
	xt_xmtx lm;
	if (mpMdlWk && mpMdlWk->mpData) {
		lm = mpMdlWk->mpData->get_skel_local_xform(iskl);
	} else {
		lm.identity();
	}
	return nxMtx::mtx_from_xmtx(lm);
}

cxMtx ScnObj::get_skel_prev_world_mtx(const int iskl) const {
	xt_xmtx owm;
	if (mpMotWk) {
		owm = mpMotWk->get_node_prev_world_xform(iskl);
	} else {
		owm.identity();
	}
	return nxMtx::mtx_from_xmtx(owm);
}

cxMtx ScnObj::get_skel_root_prev_world_mtx() const {
	xt_xmtx xm;
	if (mpMotWk) {
		xm = mpMotWk->get_node_prev_world_xform(mpMotWk->mRootId);
	} else {
		xm.identity();
	}
	return nxMtx::mtx_from_xmtx(xm);
}

cxMtx ScnObj::calc_skel_world_mtx(const int iskl, cxMtx* pNodeParentMtx) const {
	xt_xmtx wm;
	xt_xmtx wmp;
	if (mpMotWk) {
		wm = mpMotWk->calc_node_world_xform(iskl, pNodeParentMtx ? &wmp : nullptr);
	} else {
		wm.identity();
		wmp.identity();
	}
	if (pNodeParentMtx) {
		*pNodeParentMtx = nxMtx::mtx_from_xmtx(wmp);
	}
	return nxMtx::mtx_from_xmtx(wm);
}

cxQuat ScnObj::get_skel_local_quat(const int iskl, const bool clean) const {
	cxMtx lm = get_skel_local_mtx(iskl);
	if (clean) {
		nxMtx::clean_rotations(&lm, 1);
	}
	return lm.to_quat();
}

cxQuat ScnObj::get_skel_local_rest_quat(const int iskl, const bool clean) const {
	cxMtx lm = get_skel_local_rest_mtx(iskl);
	if (clean) {
		nxMtx::clean_rotations(&lm, 1);
	}
	return lm.to_quat();
}

cxVec ScnObj::get_skel_local_pos(const int iskl) const {
	return get_skel_local_mtx(iskl).get_translation();
}

cxVec ScnObj::get_skel_local_rest_pos(const int iskl) const {
	return get_skel_local_rest_mtx(iskl).get_translation();
}

void ScnObj::set_skel_local_mtx(const int iskl, const cxMtx& mtx) {
	if (mpMotWk) {
		mpMotWk->set_node_local_xform(iskl, nxMtx::xmtx_from_mtx(mtx));
	}
}

void ScnObj::reset_skel_local_mtx(const int iskl) {
	if (mpMotWk) {
		mpMotWk->reset_node_local_xform(iskl);
	}
}

void ScnObj::reset_skel_local_quat(const int iskl) {
	if (ck_skel_id(iskl)) {
		set_skel_local_quat(iskl, get_skel_local_rest_quat(iskl));
	}
}

void ScnObj::set_skel_local_quat(const int iskl, const cxQuat& quat) {
	if (mpMotWk) {
		mpMotWk->set_node_local_xform(iskl, nxMtx::xmtx_from_quat_pos(quat, get_skel_local_pos(iskl)));
	}
}

void ScnObj::set_skel_local_quat_pos(const int iskl, const cxQuat& quat, const cxVec& pos) {
	if (mpMotWk) {
		mpMotWk->set_node_local_xform(iskl, nxMtx::xmtx_from_quat_pos(quat, pos));
	}
}

void ScnObj::set_skel_local_ty(const int iskl, const float y) {
	if (mpMotWk) {
		mpMotWk->set_node_local_ty(iskl, y);
	}
}

void ScnObj::set_skel_root_local_tx(const float x) {
	if (mpMotWk) {
		mpMotWk->set_root_local_tx(x);
	}
}

void ScnObj::set_skel_root_local_ty(const float y) {
	if (mpMotWk) {
		mpMotWk->set_root_local_ty(y);
	}
}

void ScnObj::set_skel_root_local_tz(const float z) {
	if (mpMotWk) {
		mpMotWk->set_root_local_tz(z);
	}
}

void ScnObj::set_skel_root_local_pos(const float x, const float y, const float z) {
	set_skel_root_local_tx(x);
	set_skel_root_local_ty(y);
	set_skel_root_local_tz(z);
}

void ScnObj::set_world_quat(const cxQuat& quat) {
	set_world_quat_pos(quat, get_world_pos());
}

void ScnObj::set_world_quat_pos(const cxQuat& quat, const cxVec& pos) {
	if (mpMotWk) {
		if (mpMotWk->mRootId >= 0) {
			mpMotWk->mpXformsL[mpMotWk->mRootId] = nxMtx::xmtx_from_quat_pos(quat, pos);
		}
	} else if (mpMdlWk) {
		if (mpMdlWk->mpWorldXform) {
			*mpMdlWk->mpWorldXform = nxMtx::xmtx_from_quat_pos(quat, pos);
		}
	}
}

void ScnObj::set_world_mtx(const cxMtx& mtx) {
	if (mpMotWk) {
		if (mpMotWk->mRootId >= 0) {
			mpMotWk->mpXformsL[mpMotWk->mRootId] = nxMtx::xmtx_from_mtx(mtx);
		}
	} else if (mpMdlWk) {
		if (mpMdlWk->mpWorldXform) {
			*mpMdlWk->mpWorldXform = nxMtx::xmtx_from_mtx(mtx);
		}
	}
}

cxMtx ScnObj::get_world_mtx() const {
	cxMtx m;
	m.identity();
	if (mpMotWk) {
		if (mpMotWk->mRootId >= 0) {
			m = nxMtx::mtx_from_xmtx(mpMotWk->mpXformsL[mpMotWk->mRootId]);
		}
	} else if (mpMdlWk) {
		if (mpMdlWk->mpWorldXform) {
			m = nxMtx::mtx_from_xmtx(*mpMdlWk->mpWorldXform);
		}
	}
	return m;
}

cxVec ScnObj::get_world_pos() const {
	return get_world_mtx().get_translation();
}

float ScnObj::get_world_deg_x() const {
	return get_world_mtx().get_rot_degrees().x;
}

void ScnObj::add_world_deg_x(const float xadd) {
	cxVec r = get_world_quat().get_rot_degrees();
	r.x += xadd;
	set_world_quat(nxQuat::from_degrees(r.x, r.y, r.z));
}

float ScnObj::get_world_deg_y() const {
	return get_world_mtx().get_rot_degrees().y;
}

void ScnObj::add_world_deg_y(const float yadd) {
#if 0
	cxVec r = get_world_quat().get_rot_degrees();
	r.y += yadd;
	set_world_quat(nxQuat::from_degrees(r.x, r.y, r.z));
#else
	xform_world_quat(nxQuat::from_degrees(0.0f, yadd, 0.0f));
#endif
}

float ScnObj::get_world_deg_z() const {
	return get_world_mtx().get_rot_degrees().z;
}

void ScnObj::add_world_deg_z(const float zadd) {
	cxVec r = get_world_quat().get_rot_degrees();
	r.z += zadd;
	set_world_quat(nxQuat::from_degrees(r.x, r.y, r.z));
}

void ScnObj::xform_world_deg_xyz(const float dx, const float dy, const float dz, exRotOrd rord) {
	set_world_mtx(get_world_mtx() * nxMtx::mk_rot_degrees(dx, dy, dz, rord));
}

void ScnObj::xform_world_quat(const cxQuat& q) {
	cxQuat qold = get_world_quat();
	cxQuat qnew = q * qold;
	qnew.normalize();
	set_world_quat(qnew);
}

cxQuat ScnObj::get_world_quat() const {
	return get_world_mtx().to_quat();
}

cxAABB ScnObj::get_world_bbox() const {
	cxAABB bbox;
	if (mpMdlWk) {
		bbox = mpMdlWk->mWorldBBox;
	} else {
		bbox.set(cxVec(0.0f));
	}
	return bbox;
}

cxMtx ScnObj::get_prev_world_mtx() const {
	cxMtx m;
	if (mpMotWk) {
		m = get_skel_root_prev_world_mtx();
	} else if (mpMdlWk) {
		m = mpMdlWk->get_prev_world_xform();
	} else {
		m.identity();
	}
	return m;
}

cxVec ScnObj::get_prev_world_pos() const {
	return get_prev_world_mtx().get_translation();
}

cxVec ScnObj::get_center_pos() const {
	cxVec cpos(0.0f);
	if (mpMotWk) {
		if (mpMotWk->mCenterId >= 0) {
			xt_xmtx cwm = mpMotWk->calc_node_world_xform(mpMotWk->mCenterId);
			cpos = nxMtx::xmtx_get_pos(cwm);
		} else {
			cpos = get_world_bbox().get_center();
		}
	} else {
		cpos = get_world_bbox().get_center();
	}
	return cpos;
}

void ScnObj::draw_opaq() {
	if (mDisableDraw) return;
	cxModelWork* pWk = mpMdlWk;
	if (!pWk) return;
	sxModelData* pMdl = pWk->mpData;
	if (!pMdl) return;
	if (mPreOpaqFunc) {
		mPreOpaqFunc(this);
	}
	for (uint32_t i = 0; i < pMdl->mBatNum; ++i) {
		bool flg = false;
		const sxModelData::Material* pMtl = pMdl->get_batch_material(i);
		if (pMtl) flg = !pMtl->is_alpha();
		if (flg) {
			obj_bat_draw(this, i, Draw::DRWMODE_STD);
		}
	}
	if (mPostOpaqFunc) {
		mPostOpaqFunc(this);
	}
}

void ScnObj::draw_semi(const bool discard) {
	if (mDisableDraw) return;
	cxModelWork* pWk = mpMdlWk;
	if (!pWk) return;
	sxModelData* pMdl = pWk->mpData;
	if (!pMdl) return;
	for (int pass = 0; pass < 2; ++pass) {
		for (uint32_t i = 0; i < pMdl->mBatNum; ++i) {
			bool flg = false;
			const sxModelData::Material* pMtl = pMdl->get_batch_material(i);
			if (pMtl) flg = pMtl->is_alpha();
			if (flg) {
				flg &= bool(pMtl->mFlags.forceBlend) ^ (pass == 0);
			}
			if (flg) {
				obj_bat_draw(this, i, discard ? Draw::DRWMODE_DISCARD : Draw::DRWMODE_STD);
			}
		}
	}
}

void ScnObj::draw(const bool discard) {
	if (mDisableDraw) return;
	draw_opaq();
	draw_semi(discard);
}

void ScnObj::shadow_cast() {
	if (mDisableShadowCast) return;
	cxModelWork* pWk = mpMdlWk;
	if (!pWk) return;
	sxModelData* pMdl = pWk->mpData;
	if (!pMdl) return;
	for (uint32_t i = 0; i < pMdl->mBatNum; ++i) {
		obj_bat_draw(this, i, Draw::DRWMODE_SHADOW_CAST);
	}
}


namespace Draw {

static Ifc* s_ifcLst[16];

int32_t register_ifc_impl(Ifc* pIfc) {
	static int32_t s_idx = 0;
	int idx = s_idx;
	if (pIfc) {
		idx = nxSys::atomic_inc(&s_idx) - 1;
		int maxIfcs = (int)XD_ARY_LEN(s_ifcLst);
		if (idx < maxIfcs) {
			s_ifcLst[idx] = pIfc;
		} else {
			nxSys::atomic_dec(&s_idx);
			idx = -1;
		}
	}
	return idx;
}

Ifc* find_ifc_impl(const char* pName) {
	Ifc* pIfc = nullptr;
	if (pName) {
		int n = register_ifc_impl(nullptr);
		for (int i = 0; i < n; ++i) {
			if (nxCore::str_eq(pName, s_ifcLst[i]->info.pName)) {
				pIfc = s_ifcLst[i];
				break;
			}
		}
	}
	return pIfc;
}

Ifc* get_ifc_impl() {
	static const char* s_pDefDrawIfcName = "ogl";
	const char* pIfcName = s_pDefDrawIfcName;
	const char* pImplName = nxApp::get_opt("draw");
	if (pImplName) {
		pIfcName = pImplName;
	}
	Draw::Ifc* pIfc = Draw::find_ifc_impl(pIfcName);
	if (!pIfc) {
		pIfcName = s_pDefDrawIfcName;
		pIfc = Draw::find_ifc_impl(pIfcName);
	}
	return pIfc;
}

} // Draw
