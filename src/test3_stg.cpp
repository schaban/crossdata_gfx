#include "crossdata.hpp"
#include "gex.hpp"
#include "keyctrl.hpp"
#include "remote.hpp"
#include "util.hpp"

#include "test3_chr.hpp"
#include "test3_stg.hpp"

void cStage3::create() {
	mpSkyLit = make_const_lit("test3_sky_lit");

	mTexLib.init(DATA_PATH("test3_stg_tex"));

	sxData* pData = nxData::load(DATA_PATH("test3_stg.xgeo"));
	if (!pData) return;
	sxGeometryData* pGeo = pData->as<sxGeometryData>();
	if (!pGeo) {
		nxData::unload(pData);
		return;
	}
	GEX_OBJ* pObj = gexObjCreate(*pGeo, "xbat_");
	if (pObj) {
		obj_shadow_mode(*pObj, false, true); // receiver
	} else {
		nxData::unload(pGeo);
		return;
	}
	mpObj = pObj;

	// fence
	pData = nxData::load(DATA_PATH("test3_fnc.xgeo"));
	if (pData) {
		sxGeometryData* pFenceGeo = pData->as<sxGeometryData>();
		if (pFenceGeo) {
			mpFenceObj = gexObjCreate(*pFenceGeo);
		}
		nxData::unload(pData);
	}

	pData = nxData::load(DATA_PATH("test3_stg_mtl.xval"));
	if (pData) {
		sxValuesData* pMtlVals = pData->as<sxValuesData>();
		if (pMtlVals && pObj) {
			init_materials(*pObj, *pMtlVals);
		}
		int nmtl = gexObjMtlNum(pObj);
		for (int i = 0; i < nmtl; ++i) {
			GEX_MTL* pMtl = gexObjMaterial(pObj, i);
			gexMtlDiffMode(pMtl, GEX_DIFF_MODE::OREN_NAYAR);
			//gexMtlShadowCulling(pMtl, true);
			gexMtlUpdate(pMtl);
		}

		if (mpFenceObj) {
			if (pMtlVals) {
				init_materials(*mpFenceObj, *pMtlVals);
			}
			obj_shadow_mode(*mpFenceObj, true, true); // caster + receiver
			obj_shadow_params(*mpFenceObj, 1.0f, 200.0f, false);

			nmtl = gexObjMtlNum(mpFenceObj);
			for (int i = 0; i < nmtl; ++i) {
				GEX_MTL* pMtl = gexObjMaterial(mpFenceObj, i);
				//gexMtlDiffMode(pMtl, GEX_DIFF_MODE::OREN_NAYAR);
				gexMtlUpdate(pMtl);
			}
		}

		nxData::unload(pData);
	}

	nxData::unload(pGeo);



	// sky
	pData = nxData::load(DATA_PATH("test3_sky.xgeo"));
	if (pData) {
		sxGeometryData* pSkyGeo = pData->as<sxGeometryData>();
		if (pSkyGeo) {
			mpSkyObj = gexObjCreate(*pSkyGeo);
			nxData::unload(pData);
			if (mpSkyObj) {
				pData = nxData::load(DATA_PATH("test3_sky_mtl.xval"));
				if (pData) {
					sxValuesData* pMtlVals = pData->as<sxValuesData>();
					if (pMtlVals) {
						init_materials(*mpSkyObj, *pMtlVals);
					}
					if (1) {
						obj_sort_mode(*mpSkyObj, GEX_SORT_MODE::CENTER);
						obj_sort_bias(*mpSkyObj, 0.0f, 1.0f); // set rel z-bias to make sure that the dome will be drawn last
					} else {
						obj_sort_mode(*mpSkyObj, GEX_SORT_MODE::FAR_POS);
					}
					nxData::unload(pData);
				}
			}
		} else {
			nxData::unload(pData);
		}
	}

	pData = nxData::load(DATA_PATH("test3_obst.xgeo"));
	if (pData) {
		mpObstGeo = pData->as<sxGeometryData>();
	}

	mInitFlg = true;
}

void cStage3::destroy() {
	gexObjDestroy(mpObj);
	mpObj = nullptr;
	gexObjDestroy(mpFenceObj);
	mpFenceObj = nullptr;
	gexObjDestroy(mpSkyObj);
	mpSkyObj = nullptr;
	mTexLib.reset();
	gexLitDestroy(mpSkyLit);
	mpSkyLit = nullptr;
	nxData::unload(mpObstGeo);
	mpObstGeo = nullptr;
}

void cStage3::update() {
}

void cStage3::disp(GEX_LIT* pLit) {
	if (!mInitFlg) return;
	gexObjDisp(mpObj, pLit);
	if (mpFenceObj) {
		gexObjDisp(mpFenceObj, pLit);
	}
	if (mpSkyObj) {
		gexObjDisp(mpSkyObj, mpSkyLit);
	}
}

void cStage3::sky_fog_enable(bool enable) {
	if (!mpSkyLit) return;
	if (enable) {
		gexFog(mpSkyLit, cxColor(0.75f, 0.75f, 0.965f, 1.0f), 10.0f, 1000.0f, 0.1f, 0.1f);
	} else {
		gexFog(mpSkyLit, cxColor(0.0f, 0.0f, 0.0f, 0.0f), 0.0f, 0.0f);
	}
	gexLitUpdate(mpSkyLit);
}

class cStgHitFn : public sxGeometryData::HitFunc {
public:
	cxVec mPos;
	cxVec mNrm;
	float mDist;
	int mPolId;
	bool mHitFlg;

	cStgHitFn() : mHitFlg(false), mPolId(-1), mDist(FLT_MAX) {}

	virtual bool operator()(const sxGeometryData::Polygon& pol, const cxVec& hitPos, const cxVec& hitNrm, float hitDist) {
		if (hitDist < mDist) {
			mPos = hitPos;
			mNrm = hitNrm;
			mDist = hitDist;
			mPolId = pol.get_id();
			mHitFlg = true;
		}
		return true;
	}
};

bool cStage3::hit_ck(const cxVec& pos0, const cxVec& pos1, cxVec* pHitPos, cxVec* pHitNrm) {
	if (!mpObstGeo) return false;
	cStgHitFn hitFn;
	cxLineSeg seg(pos0, pos1);
	mpObstGeo->hit_query(seg, hitFn);
	if (hitFn.mHitFlg) {
		if (pHitPos) {
			*pHitPos = hitFn.mPos;
		}
		if (pHitNrm) {
			*pHitNrm = hitFn.mNrm;
		}
	}
	return hitFn.mHitFlg;
}

