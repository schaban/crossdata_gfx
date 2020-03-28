#include "crosscore.hpp"
#include "draw.hpp"
#include "scene.hpp"

static Draw::Ifc s_draw;

static cxResourceManager* s_pRsrcMgr = nullptr;

static cxBrigade* s_pBgd = nullptr;
static sxJobQueue* s_pJobQue = nullptr;
static cxHeap** s_ppLocalHeaps = nullptr;
static int s_numLocalHeaps = 0;
static bool s_visTaskPerBat = true;

static sxLock* s_pGlbRNGLock = nullptr;
static sxRNG s_glbRNG;

typedef cxStrMap<ScnObj*> ObjMap;
typedef cxPlexList<ScnObj> ObjList;

static ObjList* s_pObjList = nullptr;
static ObjMap* s_pObjMap = nullptr;

static bool s_scnInitFlg = false;

static Draw::Context s_drwCtx;
static Draw::Context s_drwCtxStk[4];
static int s_drwCtxSP = 0;

static bool s_viewUpdateFlg = true;

static bool s_shadowUniform = true;
static bool s_shadowUpdateFlg = true;
static float s_smapViewSize = 30.0f;
static float s_smapViewDist = 50.0f;
static float s_smapMargin = 30.0f;
static bool s_useShadowCastCull = true;

static uint64_t s_frameCnt = 0;

void ScnCfg::set_defaults() {
	pAppPath = "";
#if defined(XD_SYS_ANDROID)
	pDataDir = "data";
	shadowMapSize = 1024;
#else
	pDataDir = "../data";
	shadowMapSize = 2048;
#endif
	drawImplType = 0;
	numWorkers = 4;
	localHeapSize = 0;
	useSpec = true;
	useBump = true;
}


static Draw::MdlParam* get_obj_mdl_params(ScnObj& obj) {
	return obj.mpMdlWk ? (Draw::MdlParam*)obj.mpMdlWk->mpParamMem : nullptr;
}

static void obj_exec_func(const sxJobContext* pCtx) {
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

static void obj_visibility_job_func(const sxJobContext* pCtx) {
	if (!pCtx) return;
	sxJob* pJob = pCtx->mpJob;
	if (!pJob) return;
	ScnObj* pObj = (ScnObj*)pJob->mpData;
	if (!pObj) return;
	pObj->update_visibility();
}

static void bat_visibility_job_func(const sxJobContext* pCtx) {
	if (!pCtx) return;
	sxJob* pJob = pCtx->mpJob;
	if (!pJob) return;
	ScnObj* pObj = (ScnObj*)pJob->mpData;
	if (!pObj) return;
	int ibat = pJob->mParam;
	pObj->update_batch_vilibility(ibat);
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

void init(const ScnCfg& cfg) {
	if (s_scnInitFlg) return;

	switch (cfg.drawImplType) {
		case 0:
		default:
			s_draw = Draw::get_impl_ogl();
			break;
		case 1:
			s_draw = Draw::get_impl_ogl_x();
			break;
		case 2:
			s_draw = Draw::get_impl_vk();
			break;
	}

	s_pRsrcMgr = cxResourceManager::create(cfg.pAppPath, cfg.pDataDir);
	if (!s_pRsrcMgr) return;

	if (cfg.numWorkers > 0) {
		s_pBgd = cxBrigade::create(cfg.numWorkers);
		s_pGlbRNGLock = nxSys::lock_create();
	}

	nxCore::rng_seed(&s_glbRNG, 1);

	s_pObjList = ObjList::create();
	if (s_pObjList) {
		s_pObjList->set_item_handlers(obj_ctor, obj_dtor);
	}

	s_pObjMap = ObjMap::create();

	s_drwCtx.reset();
	s_drwCtxSP = 0;

	s_draw.init(cfg.shadowMapSize, s_pRsrcMgr);

	s_frameCnt = 0;

	s_scnInitFlg = true;
}

void reset() {
	if (!s_scnInitFlg) return;

	if (s_pBgd) {
		cxBrigade::destroy(s_pBgd);
		s_pBgd = nullptr;
	}
	if (s_pJobQue) {
		nxTask::queue_destroy(s_pJobQue);
		s_pJobQue = nullptr;
	}
	if (s_pGlbRNGLock) {
		nxSys::lock_destroy(s_pGlbRNGLock);
		s_pGlbRNGLock = nullptr;
	}

	free_local_heaps();

	release_all_gfx();
	s_draw.reset();
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
	s_draw.begin(clearColor);
	purge_local_heaps();
}

void frame_end() {
	s_draw.end();
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
		s_drwCtx.view.update(s_draw.get_screen_width(), s_draw.get_screen_height());
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
	//cxMtx mtxView = Scene::get_view_mtx();
	cxMtx mtxProj = Scene::get_proj_mtx();
	cxMtx mtxInvView = Scene::get_inv_view_mtx();
	cxMtx mtxInvProj = Scene::get_inv_proj_mtx();
	cxMtx mtxViewProj = Scene::get_view_proj_mtx();
	cxMtx mtxInvViewProj = Scene::get_inv_view_proj_mtx();

	cxVec dir = pCtx->shadow.dir.get_normalized();

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
		s_drwCtx.shadow.mtx = s_drwCtx.shadow.viewProjMtx * s_draw.get_shadow_bias_mtx();
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
				pObj->mJob.mFunc = obj_exec_func;
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

int get_num_objs() {
	return s_pObjList ? s_pObjList->get_count() : 0;
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

void exec() {
	int nobj = get_num_objs();
	int njob = nobj;
	if (njob < 1) return;
	job_queue_alloc(njob);
	if (s_pJobQue) {
		nxTask::queue_purge(s_pJobQue);
		if (s_pObjList) {
			for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
				ScnObj* pObj = itr.item();
				if (pObj) {
					pObj->mJob.mFunc = obj_exec_func;
					nxTask::queue_add(s_pJobQue, &pObj->mJob);
				}
			}
		}
		nxTask::queue_exec(s_pJobQue, s_pBgd);
	}
}

void visibility() {
	if (!s_pObjList) return;
	int nobj = get_num_objs();
	if (nobj < 1) return;
	int njob = 0;
	if (s_visTaskPerBat) {
		int nbat = 0;
		for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
			ScnObj* pObj = itr.item();
			if (pObj) {
				nbat += pObj->get_batches_num();
			}
		}
		njob = nbat;
	} else {
		njob = nobj;
	}
	update_view();
	update_shadow();
	job_queue_alloc(njob);
	if (s_pJobQue) {
		nxTask::queue_purge(s_pJobQue);
		if (s_visTaskPerBat) {
			for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
				ScnObj* pObj = itr.item();
				if (pObj) {
					int n = pObj->get_batches_num();
					for (int i = 0; i < n; ++i) {
						pObj->mpBatJobs[i].mFunc = bat_visibility_job_func;
						nxTask::queue_add(s_pJobQue, &pObj->mpBatJobs[i]);
					}
				}
			}
		} else {
			for (ObjList::Itr itr = s_pObjList->get_itr(); !itr.end(); itr.next()) {
				ScnObj* pObj = itr.item();
				if (pObj) {
					pObj->mJob.mFunc = obj_visibility_job_func;
					nxTask::queue_add(s_pJobQue, &pObj->mJob);
				}
			}
		}
		nxTask::queue_exec(s_pJobQue, s_pBgd);
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

} // Scene


static void obj_bat_draw(ScnObj* pObj, const int ibat, const Draw::Mode mode) {
	cxModelWork* pWk = pObj->mpMdlWk;
	if (!pWk) return;
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
	s_draw.batch(pWk, ibat, mode, pCtx);
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
	if (pMot) {
		exec_motion(pMot, frameAdd);
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
	}
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

float ScnObj::get_world_deg_y() const {
	return get_world_mtx().get_rot_degrees().y;
}

void ScnObj::add_world_deg_y(const float yadd) {
	cxVec r = get_world_quat().get_rot_degrees();
	r.y += yadd;
	set_world_quat(nxQuat::from_degrees(r.x, r.y, r.z));
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
	for (uint32_t i = 0; i < pMdl->mBatNum; ++i) {
		bool flg = false;
		const sxModelData::Material* pMtl = pMdl->get_batch_material(i);
		if (pMtl) flg = pMtl->is_alpha();
		if (flg) {
			obj_bat_draw(this, i, discard ? Draw::DRWMODE_DISCARD : Draw::DRWMODE_STD);
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
