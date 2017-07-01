#include "crossdata.hpp"
#include "gex.hpp"
#include "keyctrl.hpp"
#include "util.hpp"

#include "test4_stg.hpp"

void set_scene_fog(GEX_LIT* pLit) {
	if (!pLit) return;
	gexFog(pLit, cxColor(1.25f, 1.2f, 0.8f, 1.0f), 4.0f, 500.0f, 0.01f, 0.3f);
	gexLitUpdate(pLit);
}

static void set_ground_shadow_params(GEX_OBJ* pObj, const char* pMtlName) {
	GEX_MTL* pMtl = gexFindMaterial(pObj, pMtlName);
	if (!pMtl) return;
	gexMtlShadowMode(pMtl, false, true);
	gexMtlSelfShadowFactor(pMtl, 0.0f);
}

void cStage4::create() {
	mpConstLit = make_const_lit("test4_const_lit");
	set_scene_fog(mpConstLit);

	mTexLib.init(DATA_PATH("test4_stg_tex"));

	sxData* pData = nxData::load(DATA_PATH("test4_stg.xgeo"));
	if (!pData) return;
	sxGeometryData* pGeo = pData->as<sxGeometryData>();
	if (!pGeo) {
		nxData::unload(pData);
		return;
	}
	GEX_OBJ* pObj = gexObjCreate(*pGeo);
	if (pObj) {
		obj_shadow_mode(*pObj, true, true); // caster + receiver
		obj_shadow_params(*pObj, 1.2f, 200.0f, false);
		gexMtlShadowDensity(gexFindMaterial(pObj, "tower"), 1.0f);
		gexMtlSelfShadowFactor(gexFindMaterial(pObj, "tower"), 50.0f);
		set_ground_shadow_params(pObj, "groundsand");
		set_ground_shadow_params(pObj, "groundsqr");
	} else {
		nxData::unload(pGeo);
		return;
	}
	mpObj = pObj;

	pData = nxData::load(DATA_PATH("test4_stg_mtl.xval"));
	if (pData) {
		sxValuesData* pMtlVals = pData->as<sxValuesData>();
		if (pMtlVals && mpObj) {
			init_materials(*mpObj, *pMtlVals);
		}
		nxData::unload(pData);
	}

	gexDoubleSidedAll(mpObj, true);

	pData = nxData::load(DATA_PATH("test4_sky.xgeo"));
	if (pData) {
		sxGeometryData* pSkyGeo = pData->as<sxGeometryData>();
		if (pSkyGeo) {
			mpSkyObj = gexObjCreate(*pSkyGeo);
			nxData::unload(pData);
		} else {
			nxData::unload(pData);
		}
	}

	nxData::unload(pGeo);

	mInitFlg = true;
}

void cStage4::destroy() {
	gexObjDestroy(mpObj);
	mpObj = nullptr;
	gexObjDestroy(mpSkyObj);
	mpSkyObj = nullptr;
	mTexLib.reset();
}

void cStage4::disp() {
	if (!mInitFlg) return;
	gexObjDisp(mpSkyObj, mpConstLit);
	gexObjDisp(mpObj, mpConstLit);
}
