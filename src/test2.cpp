#include "crossdata.hpp"
#include "gex.hpp"
#include "keyctrl.hpp"

#include "util.hpp"
#include "test.hpp"


#define D_KEY_BOX_SW 0

static struct TEST2_WK {
	CAM_INFO camInfo[10];
	GEX_CAM* pCam;
	int camSrc;
	int camDst;
	int camNum;
	float camT;

	GEX_LIT* pLit;
	GEX_LIT* pConstLit;

	GEX_OBJ* pDancerObj;
	GEX_TEX* pDancerTexB;
	GEX_TEX* pDancerTexS;
	sxRigData* pRig;
	cxMtx* pRigMtxL;
	cxMtx* pRigMtxW;
	cxMtx* pObjMtxW;
	int* pObjToRig;
	int skinNodesNum;
	sxKeyframesData* pAnim;
	sxKeyframesData::RigLink* pAnimLink;
	float animFrame;
	cxAABB dancerBBox;
	GEX_POL* pDancerSolidBoxPol;
	GEX_POL* pDancerWireBoxPol;

	GEX_OBJ* pWallsObj;
	GEX_OBJ* pFloorObj;
	float wallsRY;
	float floorRY;

	KEY_CTRL* pKeyCtrl;
	bool dispBoxFlg;

	void clear() {
		pCam = nullptr;
		pLit = nullptr;
		pConstLit = nullptr;

		pDancerObj = nullptr;
		pDancerTexB = nullptr;
		pDancerTexS = nullptr;
		pRig = nullptr;
		pRigMtxL = nullptr;
		pRigMtxW = nullptr;
		pObjMtxW = nullptr;
		pObjToRig = nullptr;
		pAnim = nullptr;
		pAnimLink = nullptr;
		animFrame = 0.0f;

		pDancerSolidBoxPol = nullptr;
		pDancerWireBoxPol = nullptr;;

		pWallsObj = nullptr;
		pFloorObj = nullptr;
		wallsRY = 0.0f;
		floorRY = 0.0f;

		camSrc = 0;
		camDst = 1;
		camNum = 2;
		camT = 0.0f;

		pKeyCtrl = nullptr;
		dispBoxFlg = false;
	}

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

} WK;

static bool s_initFlg = false;

static void init_dancer() {
	sxData* pData;

	pData = nxData::load(DATA_PATH("test2_dancer_B.xtex"));
	if (pData) {
		sxTextureData* pTex = pData->as<sxTextureData>();
		if (pTex) {
			WK.pDancerTexB = gexTexCreate(*pTex);
		}
		nxData::unload(pData);
	}

	pData = nxData::load(DATA_PATH("test2_dancer_S.xtex"));
	if (pData) {
		sxTextureData* pTex = pData->as<sxTextureData>();
		if (pTex) {
			WK.pDancerTexS = gexTexCreate(*pTex);
		}
		nxData::unload(pData);
	}

	pData = nxData::load(DATA_PATH("test2_dancer.xgeo"));
	if (!pData) return;
	sxGeometryData* pGeo = pData->as<sxGeometryData>();
	if (!pGeo) {
		nxData::unload(pData);
		return;
	}
	GEX_OBJ* pObj = gexObjCreate(*pGeo, "xbat_");
	WK.pDancerObj = pObj;
	if (WK.pDancerObj) {
		obj_shadow_mode(*WK.pDancerObj, true, !false);
		obj_shadow_params(*WK.pDancerObj, 1.0f, 500.0f, false);
		//obj_tesselation(*WK.pDancerObj, GEX_TESS_MODE::PNTRI, 10.0f);
	}

	pData = nxData::load(DATA_PATH("test2_dancer_mtl.xval"));
	if (pData) {
		sxValuesData* pMtlVals = pData->as<sxValuesData>();
		if (pMtlVals && pObj) {
			init_materials(*pObj, *pMtlVals);
		}
		nxData::unload(pData);
	}

	pData = nxData::load(DATA_PATH("test2_dancer.xrig"));
	if (!pData) return;
	sxRigData* pRig = pData->as<sxRigData>();
	WK.pRig = pRig;
	if (pRig) {
		int nnodes = pRig->get_nodes_num();
		WK.pRigMtxL = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'L'));
		WK.pRigMtxW = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'W'));
		for (int i = 0; i < nnodes; ++i) {
			if (WK.pRigMtxL) {
				WK.pRigMtxL[i] = pRig->get_lmtx(i);
			}
			if (WK.pRigMtxW) {
				WK.pRigMtxW[i] = pRig->get_wmtx(i);
			}
		}
		int nskin = pGeo->get_skin_nodes_num();
		WK.pObjMtxW = (cxMtx*)nxCore::mem_alloc(nskin * sizeof(cxMtx), XD_FOURCC('O', 'b', 'j', 'W'));
		WK.pObjToRig = (int*)nxCore::mem_alloc(nskin * sizeof(int), XD_FOURCC('O', '2', 'R', '_'));
		WK.skinNodesNum = nskin;
		cxMtx* pRestIW = (cxMtx*)nxCore::mem_alloc(nskin * sizeof(cxMtx), XD_TMP_MEM_TAG);
		if (pRestIW) {
			for (int i = 0; i < nskin; ++i) {
				const char* pNodeName = pGeo->get_skin_node_name(i);
				int nodeIdx = pRig->find_node(pNodeName);
				if (WK.pObjToRig) {
					WK.pObjToRig[i] = nodeIdx;
				}
				pRestIW[i] = pRig->get_imtx(nodeIdx);
			}
			gexObjSkinInit(pObj, pRestIW);
			nxCore::mem_free(pRestIW);
		}
	}

	nxData::unload(pGeo);

	pData = nxData::load(DATA_PATH("test2_dancer_mot.xkfr"));
	if (!pData) return;
	WK.pAnim = pData->as<sxKeyframesData>();
	if (WK.pAnim) {
		if (pRig) {
			WK.pAnimLink = WK.pAnim->make_rig_link(*pRig);
		}
	}

	WK.pDancerSolidBoxPol = gexPolCreate(48, "solid_box");
	if (0) {
		gexPolSortMode(WK.pDancerSolidBoxPol, GEX_SORT_MODE::CENTER);
		gexPolSortBias(WK.pDancerSolidBoxPol, 0.0f, -1.0f); // ... and their villages in ruins.
	}

	WK.pDancerWireBoxPol = gexPolCreate(24, "wire_box");
}

void test2_init() {
	WK.clear();

	WK.pCam = gexCamCreate("test2_camera");
	WK.pConstLit = make_const_lit();

	sxData* pData;

	pData = nxData::load(DATA_PATH("test2_lits.xval"));
	if (pData) {
		sxValuesData* pLitVals = pData->as<sxValuesData>();
		if (pLitVals) {
			cxVec dominantDir;
			WK.pLit = make_lights(*pLitVals, &dominantDir);
			if (WK.pLit) {
				//dominantDir = vec_rot_deg(dominantDir, 15, -150, 0);
				dominantDir = vec_rot_deg(dominantDir, 45, 30, 0);
				gexShadowDir(dominantDir);
			}
		} else {
			WK.pLit = gexLitCreate();
		}
		nxData::unload(pData);
	}

	if (1) {
		//gexFog(WK.pLit, cxColor(0.7f, 0.65f, 0.25f, 1.0f), 4.0f, 10.0f,  0.05f, 0.123f);
		gexFog(WK.pLit, cxColor(0.25f, 0.4f, 2.8f, 1.0f), 4.0f, 10.0f, 0.05f, 0.123f);
		gexLitUpdate(WK.pLit);
	}

	if (0) {
		gexLinearWhiteRGB(0.3025f, 0.3025f, 0.25f);
		gexUpdateGlobals();
	}

	WK.reset_cam_info();
	pData = nxData::load(DATA_PATH("test2_cams.xval"));
	if (pData) {
		sxValuesData* pCamVals = pData->as<sxValuesData>();
		if (pCamVals) {
			static const char* cams[] = {
				"cam0", "cam1", "cam2", "cam3", "cam4", "cam5", "cam6", "cam7"
			};
			WK.camNum = XD_ARY_LEN(cams);
			for (int i = 0; i < WK.camNum; ++i) {
				WK.camInfo[i] = get_cam_info(*pCamVals, cams[i]);
			}
		}
		nxData::unload(pData);
	}
	WK.apply_cam_info(0);

	init_dancer();

	pData = nxData::load(DATA_PATH("test2_walls.xgeo"));
	if (pData) {
		sxGeometryData* pWallsGeo = pData->as<sxGeometryData>();
		if (pWallsGeo) {
			WK.pWallsObj = gexObjCreate(*pWallsGeo);
			if (WK.pWallsObj) {
				obj_shadow_mode(*WK.pWallsObj, false, true);
			}
		}
		nxData::unload(pData);
	}
	pData = nxData::load(DATA_PATH("test2_walls_mtl.xval"));
	if (pData) {
		sxValuesData* pMtlVals = pData->as<sxValuesData>();
		if (pMtlVals && WK.pWallsObj) {
			init_materials(*WK.pWallsObj, *pMtlVals);
		}
		nxData::unload(pData);
	}

	pData = nxData::load(DATA_PATH("test2_floor.xgeo"));
	if (pData) {
		sxGeometryData* pFloorGeo = pData->as<sxGeometryData>();
		if (pFloorGeo) {
			WK.pFloorObj = gexObjCreate(*pFloorGeo);
			if (WK.pFloorObj) {
				obj_shadow_mode(*WK.pFloorObj, false, true);
			}
		}
		nxData::unload(pData);
	}
	pData = nxData::load(DATA_PATH("test2_floor_mtl.xval"));
	if (pData) {
		sxValuesData* pMtlVals = pData->as<sxValuesData>();
		if (pMtlVals && WK.pFloorObj) {
			init_materials(*WK.pFloorObj, *pMtlVals);
		}
		nxData::unload(pData);
	}

	static struct KEY_INFO keys[] = {
		{"[F5]", D_KEY_BOX_SW}
	};
	WK.pKeyCtrl = keyCreate(keys, XD_ARY_LEN(keys));

	s_initFlg = true;
}

static void exec_dancer() {
	if (WK.pAnim) {
		WK.pAnim->eval_rig_link(WK.pAnimLink, WK.animFrame, WK.pRig, WK.pRigMtxL);
		WK.animFrame += 0.5f;
		if (WK.animFrame >= WK.pAnim->get_max_fno()) {
			WK.animFrame = 0.0f;
		}
	}

	if (WK.pRig) {
		int n = WK.pRig->get_nodes_num();
		for (int i = 0; i < n; ++i) {
			int parentId = WK.pRig->get_parent_idx(i);
			if (WK.pRig->ck_node_idx(parentId)) {
				gexMtxMul(&WK.pRigMtxW[i], &WK.pRigMtxL[i], &WK.pRigMtxW[parentId]);
			} else {
				WK.pRigMtxW[i] = WK.pRigMtxL[i];
			}
		}
	}

	for (int i = 0; i < WK.skinNodesNum; ++i) {
		WK.pObjMtxW[i] = WK.pRigMtxW[WK.pObjToRig[i]];
	}
	gexObjTransform(WK.pDancerObj, WK.pObjMtxW);
	WK.dancerBBox = gexObjBBox(WK.pDancerObj);
}

static void exec_stage() {
	cxMtx m;

	m.set_rot_y(WK.wallsRY);
	gexObjTransform(WK.pWallsObj, &m);
	WK.wallsRY += XD_DEG2RAD(0.5f);

	m.set_rot_y(WK.floorRY);
	gexObjTransform(WK.pFloorObj, &m);
	WK.floorRY += XD_DEG2RAD(-0.5f);
}

static void exec_cam() {
	CAM_INFO* pSrc = &WK.camInfo[WK.camSrc];
	CAM_INFO* pDst = &WK.camInfo[WK.camDst];
	float t = nxCalc::saturate(WK.camT);
	cxVec pos = nxVec::lerp(pSrc->mPos, pDst->mPos, t);
	cxVec up = nxVec::lerp(pSrc->mUp, pDst->mUp, t).get_normalized();
#if 1
	cxVec dirSrc = pSrc->get_dir();
	cxVec dirDst = pDst->get_dir();
	cxVec tgtSrc = pSrc->mPos + dirSrc*5.0f;
	cxVec tgtDst = pDst->mPos + dirDst*5.0f;
	cxVec tgt = nxVec::lerp(tgtSrc, tgtDst, t);
#else
	cxQuat qdir = nxQuat::slerp(pSrc->mQuat, pDst->mQuat, t);
	cxVec tgt = pos + qdir.apply(nxVec::get_axis(exAxis::MINUS_Z))*5.0f;
#endif
	pos -= (tgt - pos).get_normalized() * 1.1f; ////////////////
	gexCamUpdate(WK.pCam, pos, tgt, up, WK.camInfo[0].mFOVY, WK.camInfo[0].mNear, WK.camInfo[0].mFar);
	if (t < 1.0f) {
		t += 0.01f;
		WK.camT = nxCalc::saturate(t);
	} else {
		WK.camSrc = WK.camDst;
		++WK.camDst;
		if (WK.camDst >= WK.camNum) {
			WK.camDst = 0;
		}
		WK.camT = 0.0f;
	}
}

void disp_solid_box() {
	GEX_POL* pPol = WK.pDancerSolidBoxPol;
	if (!pPol) return;
	GEX_LIT* pLit = WK.pLit;
	GEX_MTL* pMtl = gexPolMaterial(pPol);
	if (!pMtl) return;

	gexMtlAlpha(pMtl, true);
	gexMtlRoughness(pMtl, 0.1f);
	gexMtlSpecularColor(pMtl, cxColor(0.2f));
	gexMtlUpdate(pMtl);

	cxVec vmin = WK.dancerBBox.get_min_pos();
	cxVec vmax = WK.dancerBBox.get_max_pos();
	cxVec npx = nxVec::get_axis(exAxis::PLUS_X);
	cxVec nmx = nxVec::get_axis(exAxis::MINUS_X);
	cxVec npy = nxVec::get_axis(exAxis::PLUS_Y);
	cxVec nmy = nxVec::get_axis(exAxis::MINUS_Y);
	cxVec npz = nxVec::get_axis(exAxis::PLUS_Z);
	cxVec nmz = nxVec::get_axis(exAxis::MINUS_Z);
	xt_texcoord uv;
	uv.fill(0.0f);
	cxVec zv(0.0f);
	cxColor clr(0.1f, 1.0f, 0.15f, 0.25f);

	gexPolEditBegin(pPol);

	// back
	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmin.z), nmz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmin.z), nmz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmin.z), nmz, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmin.z), nmz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmin.z), nmz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmin.z), nmz, zv, clr, uv, uv);

	// front
	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmax.z), npz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmax.z), npz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmax.z), npz, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmax.z), npz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmax.z), npz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmax.z), npz, zv, clr, uv, uv);

	// left
	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmin.z), nmx, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmin.z), nmx, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmax.z), nmx, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmax.z), nmx, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmin.z), nmx, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmax.z), nmx, zv, clr, uv, uv);

	// right
	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmax.z), npx, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmax.z), npx, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmin.z), npx, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmin.z), npx, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmax.z), npx, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmin.z), npx, zv, clr, uv, uv);

	// top
	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmax.z), npy, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmin.z), npy, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmax.z), npy, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmin.z), npy, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmin.z), npy, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmax.z), npy, zv, clr, uv, uv);

	// bottom
	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmax.z), nmy, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmax.z), nmy, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmin.z), nmy, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmax.z), nmy, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmin.z), nmy, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmin.z), nmy, zv, clr, uv, uv);

	gexPolEditEnd(pPol);

	gexPolDisp(pPol, GEX_POL_TYPE::TRILIST, pLit);
}

void disp_wire_box() {
	GEX_POL* pPol = WK.pDancerWireBoxPol;
	if (!pPol) return;
	GEX_LIT* pLit = WK.pConstLit;
	GEX_MTL* pMtl = gexPolMaterial(pPol);
	if (!pMtl) return;

	gexMtlShadowMode(pMtl, true, true);

	cxVec vmin = WK.dancerBBox.get_min_pos();
	cxVec vmax = WK.dancerBBox.get_max_pos();
	cxVec npx = nxVec::get_axis(exAxis::PLUS_X);
	cxVec nmx = nxVec::get_axis(exAxis::MINUS_X);
	cxVec npy = nxVec::get_axis(exAxis::PLUS_Y);
	cxVec nmy = nxVec::get_axis(exAxis::MINUS_Y);
	cxVec npz = nxVec::get_axis(exAxis::PLUS_Z);
	cxVec nmz = nxVec::get_axis(exAxis::MINUS_Z);
	xt_texcoord uv;
	uv.fill(0.0f);
	cxVec zv(0.0f);
	cxColor clr(0.64f, 0.25f, 0.2f, 1.0f);

	gexPolEditBegin(pPol);

	// zmin
	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmin.z), nmz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmin.z), nmz, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmin.z), nmz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmin.z), nmz, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmin.z), nmz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmin.z), nmz, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmin.z), nmz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmin.z), nmz, zv, clr, uv, uv);

	// zmax
	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmax.z), npz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmax.z), npz, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmax.z), npz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmax.z), npz, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmax.z), npz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmax.z), npz, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmax.z), npz, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmax.z), npz, zv, clr, uv, uv);

	// left
	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmin.z), nmx, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmin.y, vmax.z), nmx, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmin.z), nmx, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmin.x, vmax.y, vmax.z), nmx, zv, clr, uv, uv);

	// right
	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmin.z), npx, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmin.y, vmax.z), npx, zv, clr, uv, uv);

	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmin.z), npx, zv, clr, uv, uv);
	gexPolAddVertex(pPol, cxVec(vmax.x, vmax.y, vmax.z), npx, zv, clr, uv, uv);

	gexPolEditEnd(pPol);

	gexPolDisp(pPol, GEX_POL_TYPE::SEGLIST, pLit);
}

void test2_loop() {
	keyUpdate(WK.pKeyCtrl);
	if (keyCkTrg(WK.pKeyCtrl, D_KEY_BOX_SW)) {
		WK.dispBoxFlg ^= true;
	}
	
	gexClearTarget();
	gexClearDepth();

	if (s_initFlg) {
		exec_cam();
		exec_dancer();
		exec_stage();

		GEX_CAM* pCam = WK.pCam;
		GEX_LIT* pLit = WK.pLit;

		gexBeginScene(pCam);
		if (WK.dispBoxFlg) {
			disp_solid_box();
			disp_wire_box();
		}
		gexObjDisp(WK.pDancerObj, pLit);
		gexObjDisp(WK.pWallsObj, pLit);
		gexObjDisp(WK.pFloorObj, pLit);
		gexEndScene();
	}

	gexSwap();
}

void test2_end() {
	nxData::unload(WK.pAnim);
	nxCore::mem_free(WK.pAnimLink);
	nxCore::mem_free(WK.pObjToRig);
	nxCore::mem_free(WK.pObjMtxW);
	nxCore::mem_free(WK.pRigMtxW);
	nxCore::mem_free(WK.pRigMtxL);
	gexObjDestroy(WK.pDancerObj);
	nxData::unload(WK.pRig);
	gexTexDestroy(WK.pDancerTexB);
	gexTexDestroy(WK.pDancerTexS);
	gexObjDestroy(WK.pWallsObj);
	gexObjDestroy(WK.pFloorObj);
	gexPolDestroy(WK.pDancerSolidBoxPol);
	gexPolDestroy(WK.pDancerWireBoxPol);
	gexLitDestroy(WK.pConstLit);
	gexLitDestroy(WK.pLit);
	gexCamDestroy(WK.pCam);
	keyDestroy(WK.pKeyCtrl);
	WK.clear();
	s_initFlg = false;
}

