#include "crossdata.hpp"
#include "gex.hpp"
#include "keyctrl.hpp"
#include "util.hpp"
#include "test.hpp"

#define D_KEY_F1 20
#define D_KEY_F2 21
#define D_KEY_F3 22
#define D_KEY_F4 24

static struct TEST_CAR_WK {
	KEY_CTRL* pKeys;
	GEX_CAM* pCam;
	GEX_TEX* pPano;
	GEX_LIT* pConstLit;

	struct CAR {
		GEX_OBJ* mpObj;
		TEXTURE_LIB mTexLib;

		void clear() {
			mpObj = nullptr;
		}

		void tess_enable(bool flg) {
			if (!mpObj) return;
			if (flg) {
				//obj_tesselation(*mpObj, GEX_TESS_MODE::PNTRI, 8.0f);
				obj_tesselation(*mpObj, GEX_TESS_MODE::PNTRI, 20.0f);
			} else {
				obj_tesselation(*mpObj, GEX_TESS_MODE::NONE, 0.0f);
			}
		}

		void set_refl_pano(GEX_TEX* pPano) {
			if (!pPano) return;
			if (!mpObj) return;
			int nmtl = gexObjMtlNum(mpObj);
			for (int i = 0; i < nmtl; ++i) {
				GEX_MTL* pMtl = gexObjMaterial(mpObj, i);
				if (pMtl) {
					if (nxCore::str_eq(gexMtlName(pMtl), "glass")) {
						gexMtlReflectionLevel(pMtl, 2.0f);
						gexMtlReflectionColor(pMtl, cxColor(1.95f));
						gexMtlShadowAlphaThreshold(pMtl, 0.1f);
					} else if (nxCore::str_eq(gexMtlName(pMtl), "tires")) {
						gexMtlReflectionLevel(pMtl, 6.0f);
						gexMtlReflectionColor(pMtl, cxColor(0.01f));
					} else {
						gexMtlReflectionLevel(pMtl, 4.0f);
						gexMtlReflectionColor(pMtl, cxColor(0.05f));
					}
					gexMtlReflectionTexture(pMtl, pPano);

					gexMtlUpdate(pMtl);
				}
			}
		}

		void init() {
			const char* pTexLibPath = DATA_PATH("test_car_tex");
			const char* pGeoPath = DATA_PATH("dcar.xgeo");
			const char* pMtlPath = DATA_PATH("dcar_mtl.xval");

			mTexLib.init(pTexLibPath);

			sxData* pData = nxData::load(pGeoPath);
			if (pData && pData->is<sxGeometryData>()) {
				mpObj = gexObjCreate(*pData->as<sxGeometryData>());
			}
			nxData::unload(pData);

			if (mpObj) {
				pData = nxData::load(pMtlPath);
				if (pData && pData->is<sxValuesData>()) {
					init_materials(*mpObj, *pData->as<sxValuesData>());
				}
				nxData::unload(pData);

				obj_shadow_mode(*mpObj, true, true); // caster + receiver
				obj_shadow_params(*mpObj, 1.0f, 100.0f, false);
			}
		}

		void reset() {
			mTexLib.reset();
			gexObjDestroy(mpObj);
			mpObj = nullptr;
		}
	} car;

	struct STG {
		GEX_OBJ* mpObj;

		void clear() {
			mpObj = nullptr;
		}

		void init() {
			sxData* pData = nxData::load(DATA_PATH("dcar_stg.xgeo"));
			if (pData && pData->is<sxGeometryData>()) {
				mpObj = gexObjCreate(*pData->as<sxGeometryData>());
			}
			nxData::unload(pData);
			if (mpObj) {
				obj_shadow_mode(*mpObj, false, true); // receiver
				obj_shadow_params(*mpObj, 1.0f, 0.0f, false);
			}
		}

		void reset() {
			gexObjDestroy(mpObj);
			mpObj = nullptr;
		}
	} stg;

	void clear() {
		pKeys = nullptr;
		pCam = nullptr;
		pPano = nullptr;
		pConstLit = nullptr;
		car.clear();
		stg.clear();
	}
} WK;

void test_car_init() {
	WK.clear();

	static struct KEY_INFO keys[] = {
		{ "[F1]", D_KEY_F1 },
		{ "[F2]", D_KEY_F2 },
		{ "[F3]", D_KEY_F3 },
		{ "[F4]", D_KEY_F4 },
	};
	WK.pKeys = keyCreate(keys, XD_ARY_LEN(keys));

	WK.pCam = gexCamCreate("obst_car_cam");
	gexCamUpdate(WK.pCam, cxVec(5.5f, 1.4f, 2.5f), cxVec(0.0f, 1.0f, 1.0f), nxVec::get_axis(exAxis::PLUS_Y), XD_DEG2RAD(40.0f), 0.1f, 500.0f);

	WK.pConstLit = make_const_lit("const_lit", 0.25f);

	sxData* pData = nxData::load(DATA_PATH("def_pano.xtex"));
	if (pData) {
		sxTextureData* pTex = pData->as<sxTextureData>();
		if (pTex) {
			WK.pPano = gexTexCreate(*pTex);
		}
	}
	nxData::unload(pData);

	WK.car.init();
	WK.car.set_refl_pano(WK.pPano);

	WK.stg.init();

	gexShadowColor(cxColor(0.0f, 0.0f, 0.0f, 1.5f)); // increase default shadow density
	gexShadowViewSize(8.0f);

	gexLinearWhiteRGB(0.25f, 0.225f, 0.97f);
	gexLinearGain(1.75f);
	gexLinearBias(-0.005f);

	gexUpdateGlobals();
}

static void cam_ctrl() {
	TRACKBALL* pTBall = get_trackball();
	if (!pTBall) return;
	GEX_CAM* pCam = WK.pCam;
	if (!pCam) return;

	float dist = nxCalc::fit(get_wheel_val(), 0, 1, 1.0f, 4.0f);
	cxVec tgt = cxVec(0.0f, 1.0f, 0.0f);
	cxVec rel = cxVec(0.5f, 1.0f, 3.0f) * dist;
	cxVec vec = tgt - rel;
	cxQuat q = pTBall->mQuat;
	vec = q.apply(vec);
	cxVec pos = tgt - vec;
	cxVec up = q.apply(nxVec::get_axis(exAxis::PLUS_Y));
	gexCamUpdate(WK.pCam, pos, tgt, up, XD_DEG2RAD(40.0f), 0.1f, 500.0f);
}

void test_car_loop() {
	keyUpdate(WK.pKeys);
	cam_ctrl();

	static bool tessFlg = false;
	if (keyCkTrg(WK.pKeys, D_KEY_F2)) {
		tessFlg ^= true;
		WK.car.tess_enable(tessFlg);
		if (tessFlg) {
			::printf("Tesselation: ON\n");
		} else {
			::printf("Tesselation: OFF\n");
		}
	}

	//gexClearTarget(cxColor(0.44f, 0.55f, 0.66f));
	gexClearTarget(cxColor(0.1f));
	gexClearDepth();

	GEX_CAM* pCam = WK.pCam;
	GEX_LIT* pLit = nullptr;

	gexBeginScene(pCam);
	gexObjDisp(WK.car.mpObj, pLit);
	gexObjDisp(WK.stg.mpObj, WK.pConstLit);
	gexEndScene();

	gexSwap();
}

void test_car_end() {
	gexCamDestroy(WK.pCam);
	gexLitDestroy(WK.pConstLit);
	WK.car.reset();
	WK.stg.reset();
	gexTexDestroy(WK.pPano);
	keyDestroy(WK.pKeys);
	WK.clear();
}

