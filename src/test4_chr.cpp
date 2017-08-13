#include "crossdata.hpp"
#include "gex.hpp"
#include "keyctrl.hpp"
#include "util.hpp"
#include "obstacle.hpp"

#include "test4_chr.hpp"

void cTest4Character::create() {
	sxData* pData;

	pData = nxData::load(DATA_PATH("test3_chr_B.xtex"));
	if (pData) {
		sxTextureData* pTex = pData->as<sxTextureData>();
		if (pTex) {
			mpTexB = gexTexCreate(*pTex);
		}
		nxData::unload(pData);
	}

	pData = nxData::load(DATA_PATH("test3_chr_S.xtex"));
	if (pData) {
		sxTextureData* pTex = pData->as<sxTextureData>();
		if (pTex) {
			mpTexS = gexTexCreate(*pTex);
		}
		nxData::unload(pData);
	}

	pData = nxData::load(DATA_PATH("test3_chr_N.xtex"));
	if (pData) {
		sxTextureData* pTex = pData->as<sxTextureData>();
		if (pTex) {
			mpTexN = gexTexCreate(*pTex);
		}
		nxData::unload(pData);
	}

	pData = nxData::load(DATA_PATH("test3_chr.xgeo"));
	if (!pData) return;
	sxGeometryData* pGeo = pData->as<sxGeometryData>();
	if (!pGeo) {
		nxData::unload(pData);
		return;
	}
	GEX_OBJ* pObj = gexObjCreate(*pGeo, "xbat_");
	if (pObj) {
		obj_shadow_mode(*pObj, true, true);
		obj_shadow_params(*pObj, 1.0f/1.1f, 500.0f, false);
		obj_diff_mode(*pObj, GEX_DIFF_MODE::OREN_NAYAR);
	} else {
		nxData::unload(pGeo);
		return;
	}
	mpObj = pObj;

	pData = nxData::load(DATA_PATH("test3_chr_mtl.xval"));
	if (pData) {
		sxValuesData* pMtlVals = pData->as<sxValuesData>();
		if (pMtlVals && pObj) {
			init_materials(*pObj, *pMtlVals);
			//obj_tesselation(*pObj, GEX_TESS_MODE::PNTRI, 8.0f);
			int nmtl = gexObjMtlNum(pObj);
			for (int i = 0; i < nmtl; ++i) {
				GEX_MTL* pMtl = gexObjMaterial(pObj, i);
				if (pMtl) {
					const char* pMtlName = gexMtlName(pMtl);
					if (nxCore::str_ends_with(pMtlName, "skin")) {
						gexMtlSHReflectionColor(pMtl, cxColor(0.1f));
						//gexMtlSHDiffuseDetail(pMtl, 6.0f);
					} else {
						gexMtlSHReflectionColor(pMtl, cxColor(0.025f));
					}
					gexMtlUpdate(pMtl);
				}
			}
		}
		nxData::unload(pData);
	}

	pData = nxData::load(DATA_PATH("test3_chr.xrig"));
	if (!pData) return;
	mpRig = pData->as<sxRigData>();
	if (!mpRig) {
		nxData::unload(pData);
		nxData::unload(pGeo);
		return;
	}
	mRootNodeId = mpRig->find_node("root");
	mMovementNodeId = mpRig->find_node("n_Move");

	mMotLib.init(DATA_PATH("test3_chr_mot"), *mpRig);

	int nnodes = mpRig->get_nodes_num();
	mRigNodesNum = nnodes;
	mpRigMtxL = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'L'));
	mpRigMtxW = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'W'));
	mpBlendMtxL = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'B'));
	for (int i = 0; i < nnodes; ++i) {
		if (mpRigMtxL) {
			mpRigMtxL[i] = mpRig->get_lmtx(i);
		}
		if (mpRigMtxW) {
			mpRigMtxW[i] = mpRig->get_wmtx(i);
		}
		if (mpBlendMtxL && mpRigMtxL) {
			mpBlendMtxL[i] = mpRigMtxL[i];
		}
	}
	int nskin = pGeo->get_skin_nodes_num();
	mpObjMtxW = (cxMtx*)nxCore::mem_alloc(nskin * sizeof(cxMtx), XD_FOURCC('O', 'b', 'j', 'W'));
	mpObjToRig = (int*)nxCore::mem_alloc(nskin * sizeof(int), XD_FOURCC('O', '2', 'R', '_'));
	mSkinNodesNum = nskin;
	cxMtx* pRestIW = (cxMtx*)nxCore::mem_alloc(nskin * sizeof(cxMtx), XD_TMP_MEM_TAG);
	if (pRestIW) {
		for (int i = 0; i < nskin; ++i) {
			const char* pNodeName = pGeo->get_skin_node_name(i);
			int nodeIdx = mpRig->find_node(pNodeName);
			if (mpObjToRig) {
				mpObjToRig[i] = nodeIdx;
			}
			pRestIW[i] = mpRig->get_imtx(nodeIdx);
		}
		gexObjSkinInit(pObj, pRestIW);
		nxCore::mem_free(pRestIW);
	}

	nxData::unload(pGeo);

	mMotIdHandsDefault = mMotLib.find_motion("HandsDefault");
	mMotIdStand = mMotLib.find_motion("Stand");
	mMotIdWalk = mMotLib.find_motion("WalkSlow");
	mMotIdRun = mMotLib.find_motion("Run");

	mInitFlg = true;

	mWorldPos.set(-1.7f, 0.0f, 13.0f);
	mWorldRot.set(0.0f, 30.0f, 0.0f);

	mWorldPos.set(-5.7f, 0.0f, 18.0f);
	mWorldRot.set(0.0f, -45.0f, 0.0f);

	mWorldPos.set(1.85f, -0.01f, 15.74f);
	mWorldRot.set(0.0f, 50.0f, 0.0f);

	//mWorldPos.set(0.0f, -0.001f, 15.4f);
	//mWorldRot.set(0.0f, 60.0f, 0.0f);

	mWorldPos.set(-3.85f, -0.0f, 17.74f);
	mWorldRot.set(0.0f, 120.0f, 0.0f);

	//
	mWorldPos.set(-8.0f, -0.0f, 20.0f);
	mWorldRot.set(0.0f, 120.0f, 0.0f);

	mWorldPos.set(-8.0f, -0.0f, 20.0f);
	cxQuat qdir;
	mWorldRot.set(0.0f, 120.0f, 0.0f);
	qdir.set_rot_degrees(mWorldRot);
	cxVec vdir = qdir.apply(nxVec::get_axis(exAxis::PLUS_Z));
	mWorldPos += vdir * 2.75f;

	mWorldPos.set(-1.0f, -0.0f, 16.5f);
	mWorldRot.set(0.0f, 120.0f, 0.0f);
}

void cTest4Character::destroy() {
	mMotLib.reset();
	nxCore::mem_free(mpRigMtxL);
	mpRigMtxL = nullptr;
	nxCore::mem_free(mpRigMtxW);
	mpRigMtxW = nullptr;
	nxCore::mem_free(mpBlendMtxL);
	mpBlendMtxL = nullptr;
	nxCore::mem_free(mpObjMtxW);
	mpObjMtxW = nullptr;
	nxCore::mem_free(mpObjToRig);
	mpObjToRig = nullptr;
	nxData::unload(mpRig);
	mpRig = nullptr;
	gexTexDestroy(mpTexB);
	mpTexB = nullptr;
	gexTexDestroy(mpTexS);
	mpTexS = nullptr;
	gexTexDestroy(mpTexN);
	mpTexN = nullptr;
	gexObjDestroy(mpObj);
	mpObj = nullptr;
	mInitFlg = false;
}

void cTest4Character::blend_init(int duration) {
	::memcpy(mpBlendMtxL, mpRigMtxL, mRigNodesNum * sizeof(cxMtx));
	mBlendDuration = (float)duration;
	mBlendCount = mBlendDuration;
}

void cTest4Character::calc_blend() {
	if (mBlendCount <= 0.0f) return;
	int n = mRigNodesNum;
	float t = nxCalc::div0(mBlendDuration - mBlendCount, mBlendDuration);
	for (int i = 0; i < n; ++i) {
		cxQuat qsrc;
		qsrc.from_mtx(mpBlendMtxL[i]);
		cxVec tsrc = mpBlendMtxL[i].get_translation();

		cxQuat qdst;
		qdst.from_mtx(mpRigMtxL[i]);
		cxVec tdst = mpRigMtxL[i].get_translation();

		cxQuat qblend = nxQuat::slerp(qsrc, qdst, t);
		cxVec tblend = nxVec::lerp(tsrc, tdst, t);
		mpRigMtxL[i].from_quat_and_pos(qblend, tblend);
	}
	--mBlendCount;
	mBlendCount = nxCalc::max(0.0f, mBlendCount);
}

float cTest4Character::calc_motion(int motId, float frame, float frameStep) {
	if (!mInitFlg) return frame;
	sxKeyframesData* pMot = mMotLib.get_keyframes(motId);
	if (!pMot) return frame;
	sxKeyframesData::RigLink* pLink = mMotLib.get_riglink(motId);
	if (!pLink) return frame;
	pMot->eval_rig_link(pLink, frame, mpRig, mpRigMtxL);
	if (mpRig->ck_node_idx(mMovementNodeId)) {
		mMotVelFrame = frame;
		int16_t* pMotToRig = pLink->get_rig_map();
		if (pMotToRig) {
			int moveGrpId = pMotToRig[mMovementNodeId];
			if (moveGrpId >= 0) {
				sxKeyframesData::RigLink::Val* pVal = pLink->mNodes[moveGrpId].get_pos_val();
				cxVec vel = pVal->get_vec();
				if (frame > 0.0f) {
					float prevFrame = nxCalc::max(frame - frameStep, 0.0f);
					cxVec prevPos;
					for (int i = 0; i < 3; ++i) {
						sxKeyframesData::FCurve fcv = pMot->get_fcv(pVal->fcvId[i]);
						prevPos.set_at(i, fcv.is_valid() ? fcv.eval(prevFrame) : 0.0f);
					}
					vel -= prevPos;
				} else {
					vel *= frameStep;
				}
				mMotVel = vel;
			}
		}
	}
	frame += frameStep;
	if (frame >= pMot->get_max_fno()) {
		frame = 0.0f;
	}
	return frame;
}

void cTest4Character::update_coord() {
	if (!mInitFlg) return;
	mPrevWorldPos = mWorldPos;
	cxMtx m;
	m.set_rot_degrees(mWorldRot);
	//::printf("%f: %f\n", mMotVelFrame, mMotVel.z);//////////////////////////////
	cxVec v = m.calc_vec(mMotVel);
	mWorldPos.add(v);
	if (mpRig->ck_node_idx(mMovementNodeId)) {
		mpRigMtxL[mMovementNodeId].identity();
	}
}

void cTest4Character::calc_world() {
	if (mpRig->ck_node_idx(mRootNodeId)) {
		cxMtx* pRootMtx = &mpRigMtxL[mRootNodeId];
		pRootMtx->set_rot_degrees(mWorldRot);
		pRootMtx->set_translation(mWorldPos);
	}
	int n = mpRig->get_nodes_num();
	for (int i = 0; i < n; ++i) {
		int parentId = mpRig->get_parent_idx(i);
		if (mpRig->ck_node_idx(parentId)) {
			gexMtxMul(&mpRigMtxW[i], &mpRigMtxL[i], &mpRigMtxW[parentId]);
		} else {
			mpRigMtxW[i] = mpRigMtxL[i];
		}
	}
	for (int i = 0; i < mSkinNodesNum; ++i) {
		mpObjMtxW[i] = mpRigMtxW[mpObjToRig[i]];
	}
}

void cTest4Character::update() {
	//mWorldPos.x += -0.01f;///////////////
	mWorldRot.y += -0.25f;
	calc_motion(mMotIdHandsDefault, 0, 0);
	//mMotFrame = calc_motion(mMotIdStand, mMotFrame, get_anim_speed());
	mMotFrame = calc_motion(mMotIdWalk, mMotFrame, get_anim_speed()*1.5f);
	update_coord();
	calc_blend();
	calc_world();
}

void cTest4Character::disp(GEX_LIT* pLit) {
	if (!mInitFlg) return;
	gexObjTransform(mpObj, mpObjMtxW);
	gexObjDisp(mpObj, pLit);
}




void cTest4Panda::create() {
	sxData* pData = nxData::load(DATA_PATH("panda.xrig"));
	if (!pData) return;
	mpRig = pData->as<sxRigData>();
	if (!mpRig) {
		nxData::unload(pData);
		return;
	}


	mRootNodeId = mpRig->find_node("root");
	mMovementNodeId = mpRig->find_node("nd000");

	mTexLib.init(DATA_PATH("panda_tex"));
	mMotLib.init(DATA_PATH("panda_mot"), *mpRig);

	pData = nxData::load(DATA_PATH("panda.xgeo"));
	if (!pData) return;
	sxGeometryData* pGeo = pData->as<sxGeometryData>();
	if (!pGeo) {
		nxData::unload(pData);
		return;
	}
	mpObj = gexObjCreate(*pGeo);
	if (mpObj) {
		obj_shadow_mode(*mpObj, true, true);
		obj_shadow_params(*mpObj, 1.0f / 1.1f, 500.0f, false);
		obj_diff_mode(*mpObj, GEX_DIFF_MODE::OREN_NAYAR);
	} else {
		nxData::unload(pData);
		return;
	}

	int nnodes = mpRig->get_nodes_num();
	mRigNodesNum = nnodes;
	mpRigMtxL = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'L'));
	mpRigMtxW = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'W'));
	mpBlendMtxL = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'B'));
	for (int i = 0; i < nnodes; ++i) {
		if (mpRigMtxL) {
			mpRigMtxL[i] = mpRig->get_lmtx(i);
		}
		if (mpRigMtxW) {
			mpRigMtxW[i] = mpRig->get_wmtx(i);
		}
		if (mpBlendMtxL) {
			mpBlendMtxL[i] = mpRigMtxL[i];
		}
	}
	int nskin = pGeo->get_skin_nodes_num();
	mpObjMtxW = (cxMtx*)nxCore::mem_alloc(nskin * sizeof(cxMtx), XD_FOURCC('O', 'b', 'j', 'W'));
	mpObjToRig = (int*)nxCore::mem_alloc(nskin * sizeof(int), XD_FOURCC('O', '2', 'R', '_'));
	mSkinNodesNum = nskin;
	cxMtx* pRestIW = (cxMtx*)nxCore::mem_alloc(nskin * sizeof(cxMtx), XD_TMP_MEM_TAG);
	if (pRestIW) {
		for (int i = 0; i < nskin; ++i) {
			const char* pNodeName = pGeo->get_skin_node_name(i);
			int nodeIdx = mpRig->find_node(pNodeName);
			if (mpObjToRig) {
				mpObjToRig[i] = nodeIdx;
			}
			pRestIW[i] = mpRig->get_imtx(nodeIdx);
		}
		gexObjSkinInit(mpObj, pRestIW);
		nxCore::mem_free(pRestIW);
	}

	nxData::unload(pGeo);
	pGeo = nullptr;

	pData = nxData::load(DATA_PATH("panda_mtl.xval"));
	if (pData) {
		sxValuesData* pMtlVals = pData->as<sxValuesData>();
		if (pMtlVals && mpObj) {
			init_materials(*mpObj, *pMtlVals);
			//obj_tesselation(*mpObj, GEX_TESS_MODE::PNTRI, 8.0f);
			int nmtl = gexObjMtlNum(mpObj);
			for (int i = 0; i < nmtl; ++i) {
				GEX_MTL* pMtl = gexObjMaterial(mpObj, i);
				if (pMtl) {
					const char* pMtlName = gexMtlName(pMtl);
					gexMtlSHReflectionColor(pMtl, cxColor(0.25f));
					gexMtlUpdate(pMtl);
				}
			}
		}
		nxData::unload(pData);
	}

	mInitFlg = true;

	mWorldPos.set(-2.2f, 0.0f, 12.0f);
	mWorldRot.set(0.0f, 30.0f, 0.0f);

	//mWorldPos.set(-5.75f, 0.0f, 15.5f);
	//mWorldRot.set(0.0f, 30.0f, 0.0f);
}

void cTest4Panda::destroy() {
	mMotLib.reset();
	mTexLib.reset();
	nxCore::mem_free(mpRigMtxL);
	mpRigMtxL = nullptr;
	nxCore::mem_free(mpRigMtxW);
	mpRigMtxW = nullptr;
	nxCore::mem_free(mpBlendMtxL);
	mpBlendMtxL = nullptr;
	nxCore::mem_free(mpObjMtxW);
	mpObjMtxW = nullptr;
	nxCore::mem_free(mpObjToRig);
	mpObjToRig = nullptr;
	nxData::unload(mpRig);
	mpRig = nullptr;
	gexObjDestroy(mpObj);
	mpObj = nullptr;
	mInitFlg = false;
}

void cTest4Panda::blend_init(int duration) {
	::memcpy(mpBlendMtxL, mpRigMtxL, mRigNodesNum * sizeof(cxMtx));
	mBlendDuration = (float)duration;
	mBlendCount = mBlendDuration;
}

void cTest4Panda::calc_blend() {
	if (mBlendCount <= 0.0f) return;
	int n = mRigNodesNum;
	float t = nxCalc::div0(mBlendDuration - mBlendCount, mBlendDuration);
	for (int i = 0; i < n; ++i) {
		cxQuat qsrc;
		qsrc.from_mtx(mpBlendMtxL[i]);
		cxVec tsrc = mpBlendMtxL[i].get_translation();

		cxQuat qdst;
		qdst.from_mtx(mpRigMtxL[i]);
		cxVec tdst = mpRigMtxL[i].get_translation();

		cxQuat qblend = nxQuat::slerp(qsrc, qdst, t);
		cxVec tblend = nxVec::lerp(tsrc, tdst, t);
		mpRigMtxL[i].from_quat_and_pos(qblend, tblend);
	}
	--mBlendCount;
	mBlendCount = nxCalc::max(0.0f, mBlendCount);
}

float cTest4Panda::calc_motion(int motId, float frame, float frameStep) {
	if (!mInitFlg) return frame;
	sxKeyframesData* pMot = mMotLib.get_keyframes(motId);
	if (!pMot) return frame;
	sxKeyframesData::RigLink* pLink = mMotLib.get_riglink(motId);
	if (!pLink) return frame;
	pMot->eval_rig_link(pLink, frame, mpRig, mpRigMtxL);
	if (mpRig->ck_node_idx(mMovementNodeId)) {
		mMotVelFrame = frame;
		int16_t* pMotToRig = pLink->get_rig_map();
		if (pMotToRig) {
			int moveGrpId = pMotToRig[mMovementNodeId];
			if (moveGrpId >= 0) {
				sxKeyframesData::RigLink::Val* pVal = pLink->mNodes[moveGrpId].get_pos_val();
				cxVec vel = pVal->get_vec();
				if (frame > 0.0f) {
					float prevFrame = nxCalc::max(frame - frameStep, 0.0f);
					cxVec prevPos;
					for (int i = 0; i < 3; ++i) {
						sxKeyframesData::FCurve fcv = pMot->get_fcv(pVal->fcvId[i]);
						prevPos.set_at(i, fcv.is_valid() ? fcv.eval(prevFrame) : 0.0f);
					}
					vel -= prevPos;
				} else {
					vel *= frameStep;
				}
				mMotVel = vel;
			}
		}
	}
	frame += frameStep;
	if (frame >= pMot->get_max_fno()) {
		frame = 0.0f;
	}
	return frame;
}

void cTest4Panda::update_coord() {
	if (!mInitFlg) return;
	mPrevWorldPos = mWorldPos;
	cxMtx m;
	m.set_rot_degrees(mWorldRot);
	cxVec v = m.calc_vec(mMotVel);
	mWorldPos.add(v);
	if (mpRig->ck_node_idx(mMovementNodeId)) {
		mpRigMtxL[mMovementNodeId].identity();
	}
}

void cTest4Panda::calc_world() {
	if (mpRig->ck_node_idx(mRootNodeId)) {
		cxMtx* pRootMtx = &mpRigMtxL[mRootNodeId];
		pRootMtx->set_rot_degrees(mWorldRot);
		pRootMtx->set_translation(mWorldPos);
	}
	int n = mpRig->get_nodes_num();
	for (int i = 0; i < n; ++i) {
		int parentId = mpRig->get_parent_idx(i);
		if (mpRig->ck_node_idx(parentId)) {
			gexMtxMul(&mpRigMtxW[i], &mpRigMtxL[i], &mpRigMtxW[parentId]);
		} else {
			mpRigMtxW[i] = mpRigMtxL[i];
		}
	}
	for (int i = 0; i < mSkinNodesNum; ++i) {
		mpObjMtxW[i] = mpRigMtxW[mpObjToRig[i]];
	}
}

void cTest4Panda::update() {
	float animSpeed = get_anim_speed() * 2;
	const char* pMotName = nullptr;
	++mCount;
	if (mState == 0) {
		if (mCount > 300) {
			blend_init(12);
			mState = 1;
			mCount = 0;
			pMotName = "Roll";
		} else {
			pMotName = "Sit";
		}
	} else {
		if (mCount > 266) {
			blend_init(4);
			mState = 0;
			mCount = 0;
			pMotName = "Sit";
		} else {
			pMotName = "Roll";
		}
	}
	if (!pMotName) return;
	int motId = mMotLib.find_motion(pMotName);
	mMotFrame = calc_motion(motId, mMotFrame, animSpeed);
	update_coord();
	calc_blend();
	calc_world();
}

void cTest4Panda::disp(GEX_LIT* pLit) {
	if (!mInitFlg) return;
	gexObjTransform(mpObj, mpObjMtxW);
	gexObjDisp(mpObj, pLit);
}
