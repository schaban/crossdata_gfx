#include "crossdata.hpp"
#include "gex.hpp"
#include "keyctrl.hpp"
#include "obstacle.hpp"
#include "util.hpp"
#include "test.hpp"


float get_prim_radius(sxGeometryData& geo) {
	float radius = 1.0f;
	int attrId = geo.find_pnt_attr("pscale");
	if (attrId >= 0) {
		radius = geo.get_pnt_attr_val_f(attrId, 0);
	}
	return radius;
}

struct OBST_PRIM {
	GEX_OBJ* mpMdl;

	OBST_PRIM() : mpMdl(nullptr) {}

	void load_mdl(const char* pPath) {
		sxData* pData = nxData::load(pPath);
		if (pData) {
			if (pData->is<sxGeometryData>()) {
				mpMdl = gexObjCreate(*pData->as<sxGeometryData>(), "xbat_");
				//gexDoubleSidedAll(mpMdl, true);
				if (mpMdl) {
					obj_shadow_mode(*mpMdl, true, true);
					obj_shadow_params(*mpMdl, 1.0f, 500.0f, false);
				}
			}
			nxData::unload(pData);
		}
	}

	void disp(GEX_LIT* pLit) {
		gexObjDisp(mpMdl, pLit);
	}

	void destroy_mdl() {
		gexObjDestroy(mpMdl);
	}
};

struct OBST_SPHERE : public OBST_PRIM {
	cxSphere mSph;

	cxVec get_center() const { return mSph.get_center(); }
	float get_radius() const { return mSph.get_radius(); }

	void init_param(sxGeometryData& geo) {
		mSph.set_center(geo.get_pnt(0));
		mSph.set_radius(get_prim_radius(geo));
	}

	void load(const char* pPathPrm, const char* pPathMdl) {
		sxData* pData = nxData::load(pPathPrm);
		if (pData) {
			if (pData->is<sxGeometryData>()) {
				init_param(*pData->as<sxGeometryData>());
			}
			nxData::unload(pData);
		}
		load_mdl(pPathMdl);
	}
};

struct OBST_CAPSULE : public OBST_PRIM {
	cxCapsule mCap;

	cxVec get_pos0() const { return mCap.get_pos0(); }
	cxVec get_pos1() const { return mCap.get_pos1(); }
	float get_radius() const { return mCap.get_radius(); }

	void init_param(sxGeometryData& geo) {
		mCap.set(geo.get_pnt(0), geo.get_pnt(1), get_prim_radius(geo));
	}

	void load(const char* pPathPrm, const char* pPathMdl) {
		sxData* pData = nxData::load(pPathPrm);
		if (pData) {
			if (pData->is<sxGeometryData>()) {
				init_param(*pData->as<sxGeometryData>());
			}
			nxData::unload(pData);
		}
		load_mdl(pPathMdl);
	}
};

static struct TEST_OBST_WK {
	GEX_CAM* pCam;
	GEX_OBJ* pBgObj;
	int state;
	int count;

	struct {
		OBST_SPHERE sph1;
		OBST_SPHERE sph2;
		cxVec pos;
		cxVec vel;
		int cnt;

		void init() {
			sph1.load(DATA_PATH("test_obst/SPH_SPH_sph1_prm.xgeo"), DATA_PATH("test_obst/SPH_SPH_sph1_mdl.xgeo"));
			sph2.load(DATA_PATH("test_obst/SPH_SPH_sph2_prm.xgeo"), DATA_PATH("test_obst/SPH_SPH_sph2_mdl.xgeo"));
			cnt = 0;
			reset();
			cnt = 0;
		}

		void reset() {
			pos.zero();

			switch (cnt & 1) {
				case 0:
					vel.set(0.01f, 0.0f, -0.01f);
					break;
				case 1:
					vel.set(0.0025f, 0.001f, -0.01f);
					break;
			}
			++cnt;
		}

		void clean() {
			sph1.destroy_mdl();
			sph2.destroy_mdl();
		}

		void disp(GEX_LIT* pLit) {
			sph1.disp(pLit);
			sph2.disp(pLit);
		}

		void update() {
			cxVec oldPos = pos;
			cxVec newPos = oldPos + vel;

			cxVec offs = sph1.get_center();
			cxVec tstOld = oldPos + offs;
			cxVec tstNew = newPos + offs;
			float tstRadius = sph1.get_radius();
			cxVec adjPos;
			bool adjFlg = obstBallToBall(tstNew, tstOld, tstRadius, sph2.get_center(), sph2.get_radius(), &adjPos);
			if (adjFlg) {
				::printf("!!!!!!\n");
				newPos = adjPos - offs;
				newPos = approach(oldPos, newPos, 4);
			} else {
				::printf("--\n");
			}

			cxMtx mtx;
			mtx.identity();
			mtx.set_translation(newPos);
			gexObjTransform(sph1.mpMdl, &mtx);
			pos = newPos;
		}
	} SPH_SPH;

	struct {
		OBST_SPHERE sph;
		OBST_CAPSULE cap;
		cxVec pos;
		cxVec vel;
		int cnt;

		void init() {
			sph.load(DATA_PATH("test_obst/SPH_CAP_sph_prm.xgeo"), DATA_PATH("test_obst/SPH_CAP_sph_mdl.xgeo"));
			cap.load(DATA_PATH("test_obst/SPH_CAP_cap_prm.xgeo"), DATA_PATH("test_obst/SPH_CAP_cap_mdl.xgeo"));
			cnt = 0;
			reset();
		}

		void reset() {
			pos.zero();

			switch (cnt & 1) {
				case 0:
					vel.set(0.0035f, 0.0f, -0.005f);
					break;
				case 1:
					vel.set(0.003f, 0.001f, -0.005f);
					break;
			}
			++cnt;
		}

		void clean() {
			sph.destroy_mdl();
			cap.destroy_mdl();
		}

		void disp(GEX_LIT* pLit) {
			sph.disp(pLit);
			cap.disp(pLit);
		}

		void update() {
			cxVec oldPos = pos;
			cxVec newPos = oldPos + vel;

			cxVec offs = sph.get_center();
			cxVec tstOld = oldPos + offs;
			cxVec tstNew = newPos + offs;
			float tstRadius = sph.get_radius();
			cxVec adjPos;
			bool adjFlg = obstBallToCap(tstNew, tstOld, tstRadius, cap.get_pos0(), cap.get_pos1(), cap.get_radius(), &adjPos, 2.0f);
			if (adjFlg) {
				::printf("!!!!!!\n");
				newPos = adjPos - offs;
				//newPos = approach(oldPos, newPos, 4);
			} else {
				::printf("--\n");
			}

			cxMtx mtx;
			mtx.identity();
			mtx.set_translation(newPos);
			gexObjTransform(sph.mpMdl, &mtx);
			pos = newPos;
		}
	} SPH_CAP;
} WK;

static void init_bg() {
	WK.pBgObj = nullptr;
	sxData* pData = nxData::load(DATA_PATH("test_obst/bg.xgeo"));
	if (pData) {
		if (pData->is<sxGeometryData>()) {
			WK.pBgObj = gexObjCreate(*pData->as<sxGeometryData>(), "xbat_");
			if (WK.pBgObj) {
				obj_shadow_mode(*WK.pBgObj, false, true);
				obj_shadow_params(*WK.pBgObj, 1.0f, 500.0f, false);
			}
		}
		nxData::unload(pData);
	}
}

void test_obst_init() {
	WK.pCam = gexCamCreate("obst_test_cam");
	gexCamUpdate(WK.pCam, cxVec(0.5f, 1.0f, 3.0f), cxVec(0.0f, 1.0f, 0.0f), nxVec::get_axis(exAxis::PLUS_Y), XD_DEG2RAD(40.0f), 0.001f, 1000.0f);

	init_bg();

	WK.SPH_SPH.init();
	WK.SPH_CAP.init();

	WK.state = 1;
	WK.count = 0;
}

static void cam_ctrl() {
	TRACKBALL* pTBall = get_trackball();
	if (!pTBall) return;
	GEX_CAM* pCam = WK.pCam;
	if (!pCam) return;

	cxVec tgt = cxVec(0.0f, 1.0f, 0.0f);
	cxVec vec = tgt - cxVec(0.5f, 1.0f, 3.0f);
	cxQuat q = pTBall->mQuat;
	vec = q.apply(vec);
	cxVec pos = tgt - vec;
	cxVec up = q.apply(nxVec::get_axis(exAxis::PLUS_Y));
	gexCamUpdate(WK.pCam, pos, tgt, up, XD_DEG2RAD(40.0f), 0.001f, 1000.0f);
}

void test_obst_loop() {
	gexClearTarget(cxColor(0.44f, 0.55f, 0.66f));
	gexClearDepth();

	cam_ctrl();

	GEX_CAM* pCam = WK.pCam;
	GEX_LIT* pLit = nullptr;

	gexBeginScene(pCam);
	gexObjDisp(WK.pBgObj);
	if (WK.state == 0) {
		WK.SPH_SPH.update();
		WK.SPH_SPH.disp(pLit);
		if (WK.count > 320) {
			WK.state = 1;
			WK.count = 0;
			WK.SPH_CAP.reset();
		}
	} else {
		WK.SPH_CAP.update();
		WK.SPH_CAP.disp(pLit);
		if (WK.count > 500) {
			WK.state = 0;
			WK.count = 0;
			WK.SPH_SPH.reset();
		}
	}
	++WK.count;
	gexEndScene();

	gexSwap();
}

void test_obst_end() {
	gexCamDestroy(WK.pCam);
	gexObjDestroy(WK.pBgObj);
	WK.SPH_SPH.clean();
	WK.SPH_CAP.clean();
}

