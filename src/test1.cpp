#include "crossdata.hpp"
#include "gex.hpp"

#include "util.hpp"
#include "test.hpp"

static struct TEST1_WK {
	cxColor ambientClr;
	float ambientInt;

	cxVec lightPos;
	cxVec lightRot;
	cxColor lightClr;
	float lightInt;
	cxVec lightDir;

	cxVec camPos;
	cxVec camTgt;
	cxVec camUp;
	float camFocal;
	float camAperture;
	float camNear;
	float camFar;
	float camFOVY;

	float roughness;
	float reflect;
	cxColor baseClr;
	GEX_SPEC_MODE specMode;
	float IOR;

	sxKeyframesData* pAnm;
	float frame;

	GEX_CAM* pCam;
	GEX_LIT* pLit;
	GEX_OBJ* pObj;

	GEX_TEX* pPano;

	void clear() {
		pCam = nullptr;
		pLit = nullptr;
		pObj = nullptr;
		frame = 0.0f;
		pPano = nullptr;
	}
} WK;

static bool s_initFlg = false;

void test1_init() {
	sxGeometryData* pGeo;
	sxValuesData* pVal;
	sxData* pData;

	const char* pGeoPath = DATA_PATH("test1.xgeo");
	const char* pValPath = DATA_PATH("test1.xval");
	const char* pKfrPath = DATA_PATH("test1.xkfr");

	WK.clear();

	pData = nxData::load(DATA_PATH("def_pano.xtex"));
	if (pData) {
		sxTextureData* pTex = pData->as<sxTextureData>();
		if (pTex) {
			WK.pPano = gexTexCreate(*pTex);
		}
	}
	nxData::unload(pData);

	// init scene params
	pData = nxData::load(pValPath);
	if (!pData) return;
	pVal = pData->as<sxValuesData>();
	if (!pVal) {
		nxData::unload(pData);
		return;
	}

	int ambValsIdx = pVal->find_grp_idx("ambient1");
	if (ambValsIdx >= 0) {
		sxValuesData::Group ambVals = pVal->get_grp(ambValsIdx);
		WK.ambientClr = ambVals.get_rgb("light_color");
		WK.ambientInt = ambVals.get_float("light_intensity");
	}

	int litValsIdx = pVal->find_grp_idx("distantlight1");
	if (litValsIdx >= 0) {
		sxValuesData::Group litVals = pVal->get_grp(litValsIdx);
		WK.lightPos = litVals.get_vec("t");
		WK.lightRot = litVals.get_vec("r");
		WK.lightClr = litVals.get_rgb("light_color");
		WK.lightInt = litVals.get_float("light_intensity");
	}
	cxQuat q;
	q.set_rot_degrees(WK.lightRot);
	WK.lightDir = q.apply(nxVec::get_axis(exAxis::MINUS_Z));
	gexShadowDir(WK.lightDir);
	gexShadowDensity(0.25f);

	int mtlValsIdx = pVal->find_grp_idx("principledshader1");
	if (mtlValsIdx >= 0) {
		sxValuesData::Group mtlVals = pVal->get_grp(mtlValsIdx);
		if (mtlVals.is_valid()) {
			WK.roughness = cvt_roughness(mtlVals.get_float("rough", 0.3f));
			WK.reflect = mtlVals.get_float("reflect", 0.5f);
			WK.baseClr = mtlVals.get_rgb("basecolor");
			WK.specMode = parse_spec_mode(mtlVals.get_str("ogl_spec_model", "phong"));
			WK.IOR = mtlVals.get_float("ogl_ior_inner", 1.4f);
		}
	}

	int camValsIdx = pVal->find_grp_idx("cam1");
	if (camValsIdx >= 0) {
		sxValuesData::Group camVals = pVal->get_grp(camValsIdx);
		if (camVals.is_valid()) {
			WK.camPos = camVals.get_vec("t");
			WK.camUp = camVals.get_vec("up");
			WK.camFocal = camVals.get_float("focal");
			WK.camAperture = camVals.get_float("aperture");
			WK.camNear = camVals.get_float("near");
			WK.camFar = camVals.get_float("far");
			WK.camFOVY = gexCalcFOVY(WK.camFocal, WK.camAperture);
		}
	}

	int tgtValsIdx = pVal->find_grp_idx("tgt1");
	if (tgtValsIdx >= 0) {
		sxValuesData::Group tgtVals = pVal->get_grp(tgtValsIdx);
		if (tgtVals.is_valid()) {
			WK.camTgt = tgtVals.get_vec("t");
		}
	}
	nxData::unload(pVal);

	WK.pCam = gexCamCreate();
	gexCamUpdate(WK.pCam, WK.camPos, WK.camTgt, WK.camUp, WK.camFOVY, WK.camNear, WK.camFar);

	WK.pLit = gexLitCreate();
	if (WK.pLit) {
		cxColor amb = WK.ambientClr;
		amb.scl_rgb(WK.ambientInt);
		gexAmbient(WK.pLit, amb);
		gexLightDir(WK.pLit, 0, WK.lightDir);
		cxColor lclr = WK.lightClr;
		lclr.scl_rgb(WK.lightInt);
		gexLightColor(WK.pLit, 0, lclr);
		gexLitUpdate(WK.pLit);
	}

	pData = nxData::load(pGeoPath);
	if (!pData) return;
	pGeo = pData->as<sxGeometryData>();
	if (!pGeo) {
		nxData::unload(pData);
		return;
	}
	WK.pObj = gexObjCreate(*pGeo);

	nxData::unload(pGeo);

	if (WK.pObj) {
		GEX_MTL* pMtl = gexObjMaterial(WK.pObj, 0);
		if (pMtl) {
			gexMtlDoubleSided(pMtl, true);
			gexMtlBaseColor(pMtl, WK.baseClr);
			//gexMtlSpecularColor(pMtl, cxColor(WK.reflect));
			gexMtlRoughness(pMtl, WK.roughness);
			gexMtlSpecMode(pMtl, WK.specMode);
			gexMtlDiffMode(pMtl, GEX_DIFF_MODE::OREN_NAYAR);
			gexMtlIOR(pMtl, WK.IOR);
			if (WK.specMode == GEX_SPEC_MODE::GGX) {
				gexMtlSpecFresnelMode(pMtl, 1);
			}
			gexMtlShadowMode(pMtl, true, true);
			gexMtlShadowDensity(pMtl, 1.0f);
			gexMtlSelfShadowFactor(pMtl, 200.0f);
			gexMtlShadowCulling(pMtl, true);
			if (1) {
				gexMtlReflectionLevel(pMtl, 5.0f);
				gexMtlReflectionColor(pMtl, cxColor(0.75f));
				gexMtlReflectionTexture(pMtl, WK.pPano);
				gexMtlReflFresnel(pMtl, 0.2f, 0.0f);
			}
			//gexMtlViewFresnel(pMtl, 0.1f, 0.0f);
			gexMtlUpdate(pMtl);
		}
	}

	pData = nxData::load(pKfrPath);
	if (pData) {
		WK.pAnm = pData->as<sxKeyframesData>();
	}

	s_initFlg = true;
}


void test1_loop() {
	gexClearTarget();
	gexClearDepth();

	GEX_CAM* pCam = WK.pCam;
	GEX_OBJ* pObj = WK.pObj;
	GEX_LIT* pLit = WK.pLit;

	if (s_initFlg) {
		if (WK.pAnm) {
			cxVec pos = eval_pos(*WK.pAnm, "cam1", WK.frame);
			cxVec tgt = eval_pos(*WK.pAnm, "tgt1", WK.frame);
			gexCamUpdate(WK.pCam, pos, tgt, WK.camUp, WK.camFOVY, WK.camNear, WK.camFar);
			WK.frame += 1.0f;
			if (WK.frame >= WK.pAnm->get_max_fno()) {
				WK.frame = 0.0f;
			}
		}

		gexBeginScene(pCam);
		gexObjDisp(pObj, pLit);
		gexEndScene();
	}

	gexSwap();
}

void test1_end() {
	gexObjDestroy(WK.pObj);
	gexLitDestroy(WK.pLit);
	gexCamDestroy(WK.pCam);
	gexTexDestroy(WK.pPano);
	nxData::unload(WK.pAnm);
	WK.clear();
	s_initFlg = false;
}
