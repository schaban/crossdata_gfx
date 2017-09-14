#include "crossdata.hpp"
#include "gex.hpp"
#include "keyctrl.hpp"
#include "util.hpp"
#include "test.hpp"

#include "test4_chr.hpp"
#include "test4_stg.hpp"

#define D_KEY_F1 20
#define D_KEY_F2 21

static struct TEST4_WK {
	KEY_CTRL* pKeys;

	CAM_INFO camInfo[10];
	int camNum;
	GEX_CAM* pCam;

	GEX_LIT* pLit;

	cTest4Character chr;
	cStage4 stg;

	cTest4Panda panda;

	bool tessFlg;

	SH_COEFS envSH;
	GEX_OBJ* pSHObj;

	void reset_cam_info() {
		for (int i = 0; i < XD_ARY_LEN(camInfo); ++i) {
			camInfo[i].set_defaults();
		}
	}

	void apply_cam_info(int i) {
		if ((uint32_t)i < (uint32_t)XD_ARY_LEN(camInfo)) {
			CAM_INFO* pInfo = &camInfo[i];
			if (pCam) {
				cxVec tgt = pInfo->mPos + pInfo->get_dir()*5.0f;
				gexCamUpdate(pCam, pInfo->mPos, tgt, pInfo->mUp, pInfo->mFOVY, pInfo->mNear, pInfo->mFar);
			}
		}
	}

	void clear() {
		pKeys = nullptr;
		pCam = nullptr;
		tessFlg = true;
	}
} WK;


void set_obj_tess(GEX_OBJ* pObj) {
	if (!pObj) return;
	if (WK.tessFlg) {
		obj_tesselation(*pObj, GEX_TESS_MODE::PNTRI, 8.0f);
	} else {
		obj_tesselation(*pObj, GEX_TESS_MODE::NONE, 0.0f);
	}
}

void test4_init() {
	WK.clear();

	static struct KEY_INFO keys[] = {
		{ "[F1]", D_KEY_F1 },
		{ "[F2]", D_KEY_F2 }
	};
	WK.pKeys = keyCreate(keys, XD_ARY_LEN(keys));

	WK.pCam = gexCamCreate("test4_camera");
	WK.reset_cam_info();
	sxData* pData = nxData::load(DATA_PATH("test4_cams.xval"));
	if (pData) {
		sxValuesData* pCamVals = pData->as<sxValuesData>();
		if (pCamVals) {
			static const char* cams[] = {
				"cam0", "cam1", "cam2", "cam3", "cam4", "cam5"
			};
			WK.camNum = XD_ARY_LEN(cams);
			for (int i = 0; i < WK.camNum; ++i) {
				WK.camInfo[i] = get_cam_info(*pCamVals, cams[i]);
			}
		}
		nxData::unload(pData);
	}
	WK.apply_cam_info(4);

	// TODO: export
	cxVec sunDir(0.632425f, -0.771139f, 0.073367f);
	cxColor skyClr(0.3f, 0.3f, 0.31f);
	cxColor gndClr(0.157281f, 0.25084f, 0.315763f);
	cxColor sunClr(1.0f/4);
	//skyClr.scl(2.0f);
	//gndClr.scl(2.0f);

	pData = nxData::load(DATA_PATH("test4_env.xgeo"));
	if (pData) {
		sxGeometryData* pEnvGeo = pData->as<sxGeometryData>();
		if (pEnvGeo) {
			WK.envSH.from_geo(*pEnvGeo, 1.5f);
			WK.envSH.scl(cxColor(0.6f, 0.6f, 0.4f));

			WK.pSHObj = gexObjCreate(*pEnvGeo);
			if (WK.pSHObj) {
				obj_shadow_mode(*WK.pSHObj, true, true);
				obj_shadow_params(*WK.pSHObj, 1.2f, 50.0f, false);
				obj_diff_mode(*WK.pSHObj, GEX_DIFF_MODE::OREN_NAYAR);
				int nmtl = gexObjMtlNum(WK.pSHObj);
				for (int i = 0; i < nmtl; ++i) {
					GEX_MTL* pMtl = gexObjMaterial(WK.pSHObj, i);
					gexMtlSHDiffuseColor(pMtl, cxColor(0.97f));
					gexMtlSHDiffuseDetail(pMtl, 4.0f);
					gexMtlSHReflectionColor(pMtl, cxColor(0.045f));
					gexMtlSHReflectionDetail(pMtl, 8.0f);
					gexMtlUpdate(pMtl);
				}
			}
		}
		nxData::unload(pData);
	}

	WK.pLit = gexLitCreate("test4_chr_lit");
	if (0) {
		gexHemi(WK.pLit, skyClr, gndClr);
	} else {
		gexAmbient(WK.pLit, cxColor(0.0f));
		gexSHL(WK.pLit, GEX_SHL_MODE::DIFF_REFL, SH_COEFS::ORDER, WK.envSH.mR, WK.envSH.mG, WK.envSH.mB);
		//gexSHLW(WK.pLit, GEX_SHL_MODE::DIFF_REFL, SH_COEFS::ORDER, WK.envSH.mR, WK.envSH.mG, WK.envSH.mB, WK.envSH.mWgtDiff, WK.envSH.mWgtRefl);
		//gexSHLW(WK.pLit, GEX_SHL_MODE::DIFF, SH_COEFS::ORDER, WK.envSH.mR, WK.envSH.mG, WK.envSH.mB, nullptr, WK.envSH.mWgtRefl);
	}
	gexLightDir(WK.pLit, 0, sunDir);
	gexLightColor(WK.pLit, 0, sunClr);
	gexLitUpdate(WK.pLit);
	set_scene_fog(WK.pLit);

	WK.stg.create();
	WK.chr.create();
	WK.panda.create();

	gexShadowDir(sunDir);
	gexShadowColor(cxColor(0.0f, 0.0f, 0.0f, 1.5f)); // A = shadow density
	gexShadowViewSize(24.0f);
	gexShadowProjection(GEX_SHADOW_PROJ::PERSPECTIVE);

	if (1) {
		gexLinearWhiteRGB(0.75f, 0.75f, 0.67f);
		gexLinearGain(1.0f);
		gexLinearBias(0.0f);

		gexLinearWhiteRGB(0.25f, 0.225f, 0.97f);
		gexLinearGain(1.5f);
		gexLinearBias(-0.005f);
	}
	gexUpdateGlobals();
}

void cam_ctrl() {
	GEX_CAM* pCam = WK.pCam;
	if (!pCam) return;
	cxVec tgt = WK.chr.get_world_pos();
	tgt.y += 1.5f;
	cxVec pos = tgt + cxVec(1.0f, 0.2f, 5.0f);

#if 1
	static bool camFlg = false;
	static cxVec camPrev;
	if (camFlg) {
		pos = approach(camPrev, pos, 2);
	} else {
		camFlg = true;
	}
	camPrev = pos;
#endif

	gexCamUpdate(pCam, pos, tgt, nxVec::get_axis(exAxis::PLUS_Y), XD_DEG2RAD(40.0f), 0.1f, 1000.0f);
}

void test4_loop() {
	keyUpdate(WK.pKeys);
	if (keyCkTrg(WK.pKeys, D_KEY_F2)) {
		WK.tessFlg ^= true;
		::printf("Tesselation: %s\n", WK.tessFlg ? "ON" : "OFF");
	}
	set_obj_tess(WK.chr.get_obj());
	set_obj_tess(WK.panda.get_obj());

	gexClearTarget(cxColor(0.44f, 0.55f, 0.66f));
	gexClearDepth();

	GEX_CAM* pCam = WK.pCam;
	GEX_LIT* pLit = WK.pLit;

	WK.chr.update();
	WK.panda.update();
	cam_ctrl();

	gexBeginScene(pCam);
	WK.stg.disp();
	WK.chr.disp(pLit);
	WK.panda.disp(pLit);
	//gexObjDisp(WK.pSHObj, pLit);
	gexEndScene();

	gexSwap();
}

void test4_end() {
	WK.chr.destroy();
	WK.stg.destroy();
	WK.panda.destroy();
	gexObjDestroy(WK.pSHObj);
	gexCamDestroy(WK.pCam);
	keyDestroy(WK.pKeys);
	WK.clear();
}
