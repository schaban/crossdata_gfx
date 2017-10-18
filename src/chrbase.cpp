#include "crossdata.hpp"
#include "gex.hpp"
#include "util.hpp"
#include "chrbase.hpp"

void cBaseRig::load(const char* pDataPath) {
	sxData* pData = nxData::load(pDataPath);
	if (!pData) return;
	sxRigData* pRigData = pData->as<sxRigData>();
	if (!pRigData) {
		nxData::unload(pData);
		return;
	}
	init(pRigData);
	mOwnData = true;
}

void cBaseRig::unload() {
	reset();
	if (mpData && mOwnData) {
		nxData::unload(mpData);
		mpData = nullptr;
	}
	mOwnData = false;
}

void cBaseRig::init(sxRigData* pRigData) {
	if (!pRigData) return;
	mpData = pRigData;
	int nnodes = mpData->get_nodes_num();
	mpMtxL = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'L'));
	mpMtxW = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'W'));
	mpPrevMtxW = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'w'));
	mpMtxBlendL = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'B'));
	for (int i = 0; i < nnodes; ++i) {
		if (mpMtxL) {
			mpMtxL[i] = mpData->get_lmtx(i);
		}
		if (mpMtxW) {
			mpMtxW[i] = mpData->get_wmtx(i);
		}
		if (mpMtxBlendL && mpMtxL) {
			mpMtxBlendL[i] = mpMtxL[i];
		}
	}
	save_prev_w();
	mNodesNum = nnodes;
	mRootNodeId = mpData->find_node("root");
	mMovementNodeId = mpData->find_node("n_Move");
	if (!mpData->ck_node_idx(mMovementNodeId)) {
		mMovementNodeId = mRootNodeId;
	}
}

void cBaseRig::reset() {
	if (!mpData) return;
	nxCore::mem_free(mpMtxL);
	mpMtxL = nullptr;
	nxCore::mem_free(mpMtxW);
	mpMtxW = nullptr;
	nxCore::mem_free(mpPrevMtxW);
	mpPrevMtxW = nullptr;
	nxCore::mem_free(mpMtxBlendL);
	mpMtxBlendL = nullptr;
}

float cBaseRig::animate(sxKeyframesData* pKfr, sxKeyframesData::RigLink* pLnk, float frameNow, float frameAdd, bool* pLoopFlg) {
	float frame = frameNow;
	if (is_valid() && pKfr && pLnk) {
		float frmax = (float)pKfr->get_max_fno();
		mAnimFrame = frame;
		pKfr->eval_rig_link(pLnk, frame, mpData, mpMtxL);
		mMoveVel = mConstMoveVel * frameAdd;
		if (mMoveMode == eMoveMode::FCURVES && mpData->ck_node_idx(mMovementNodeId)) {
			int16_t* pMotToRig = pLnk->get_rig_map();
			if (pMotToRig) {
				int moveGrpId = pMotToRig[mMovementNodeId];
				if (moveGrpId >= 0) {
					sxKeyframesData::RigLink::Val* pVal = pLnk->mNodes[moveGrpId].get_pos_val();
					cxVec vel = pVal->get_vec();
					if (frame > 0.0f) {
						float prevFrame = nxCalc::max(frame - frameAdd, 0.0f);
						cxVec prevPos;
						for (int i = 0; i < 3; ++i) {
							sxKeyframesData::FCurve fcv = pKfr->get_fcv(pVal->fcvId[i]);
							prevPos.set_at(i, fcv.is_valid() ? fcv.eval(prevFrame) : 0.0f);
						}
						vel -= prevPos;
					} else {
						vel *= frameAdd;
					}
					mMoveVel = vel;
				}
			}
		}
		bool loopFlg = false;
		frame += frameAdd;
		if (frame >= frmax) {
			frame = ::fmodf(frame, frmax);
			loopFlg = true;
		} else if (frame < 0.0f) {
			frame = frmax - ::fmodf(-frame, frmax);
			loopFlg = true;
		}
		if (pLoopFlg) {
			*pLoopFlg = loopFlg;
		}
	}
	return frame;
}

void cBaseRig::blend_init(int duration) {
	::memcpy(mpMtxBlendL, mpMtxL, mNodesNum * sizeof(cxMtx));
	mBlendDuration = (float)duration;
	mBlendCount = mBlendDuration;
}

void cBaseRig::blend_exec() {
	if (mBlendCount <= 0.0f) return;
	int n = mNodesNum;
	float t = nxCalc::div0(mBlendDuration - mBlendCount, mBlendDuration);
	for (int i = 0; i < n; ++i) {
		cxQuat qsrc;
		qsrc.from_mtx(mpMtxBlendL[i]);
		cxVec tsrc = mpMtxBlendL[i].get_translation();

		cxQuat qdst;
		qdst.from_mtx(mpMtxL[i]);
		cxVec tdst = mpMtxL[i].get_translation();

		cxQuat qblend = nxQuat::slerp(qsrc, qdst, t);
		cxVec tblend = nxVec::lerp(tsrc, tdst, t);
		mpMtxL[i].from_quat_and_pos(qblend, tblend);
	}
	--mBlendCount;
	mBlendCount = nxCalc::max(0.0f, mBlendCount);
}


void cBaseRig::update_coord() {
	if (!is_valid()) return;
	mPrevWorldPos = mWorldPos;
	cxMtx m;
	m.set_rot_degrees(mWorldRot);
	cxVec v = m.calc_vec(mMoveVel);
	mWorldPos.add(v);
	if (mpData->ck_node_idx(mMovementNodeId)) {
		mpMtxL[mMovementNodeId].identity();
	}
	if (mpData->ck_node_idx(mRootNodeId)) {
		cxMtx* pRootMtx = &mpMtxL[mRootNodeId];
		pRootMtx->set_rot_degrees(mWorldRot);
		pRootMtx->set_translation(mWorldPos);
	}
}

void cBaseRig::calc_world() {
	if (!is_valid()) return;
	save_prev_w();
	for (int i = 0; i < mNodesNum; ++i) {
		int parentId = mpData->get_parent_idx(i);
		if (mpData->ck_node_idx(parentId)) {
			gexMtxMul(&mpMtxW[i], &mpMtxL[i], &mpMtxW[parentId]);
		} else {
			mpMtxW[i] = mpMtxL[i];
		}
	}
}

void cBaseRig::print_local() const {
	if (!is_valid()) return;
	for (int i = 0; i < mNodesNum; ++i) {
		const char* pNodeName = mpData->get_node_name(i);
		cxMtx m = mpMtxL[i];
		cxVec rot(-1000);
		if (m.is_valid_rot()) {
			rot = m.get_rot_degrees();
		}
		cxVec pos = m.get_translation();
		::printf("[%d] %s\n", i, pNodeName);
		::printf("\tpos:%f, %f, %f\n", pos.x, pos.y, pos.z);
		::printf("\trot:%f, %f, %f\n", rot.x, rot.y, rot.z);
	}
}

void cHumanoidRig::init(sxRigData* pRigData) {
	cBaseRig::init(pRigData);
	if (mpData) {
		for (int i = 0; i < 4; ++i) {
			mLimbs[i].mpInfo = mpData->find_limb((sxRigData::eLimbType)i);
			mLimbs[i].mChain.set(mLimbs[i].mpInfo);
		}
	}
}

void cHumanoidRig::reset() {
	cBaseRig::reset();
	reset_limbs();
}

float cHumanoidRig::animate(sxKeyframesData* pKfr, sxKeyframesData::RigLink* pLnk, float frameNow, float frameAdd, bool* pLoopFlg) {
	float fnext = frameNow;
	if (is_valid()) {
		fnext = cBaseRig::animate(pKfr, pLnk, frameNow, frameAdd, pLoopFlg);
		update_coord();
		for (int i = 0; i < 4; ++i) {
			if (mLimbs[i].mpInfo) {
				mpData->calc_limb_local(&mLimbs[i].mSolution, mLimbs[i].mChain, mpMtxL);
				mpData->copy_limb_solution(mpMtxL, mLimbs[i].mChain, mLimbs[i].mSolution);
			}
		}
	}
	return fnext;
}


void cSkinGeo::init(sxGeometryData* pGeoData, cBaseRig* pRig, const char* pBatchGrpPrefix) {
	if (!pGeoData) return;
	if (!pRig || !pRig->is_valid()) return;
	mpDispObj = gexObjCreate(*pGeoData, pBatchGrpPrefix);
	if (!mpDispObj) return;
	mpRig = pRig;
	int nskin = pGeoData->get_skin_nodes_num();
	mpObjMtxW = (cxMtx*)nxCore::mem_alloc(nskin * sizeof(cxMtx), XD_FOURCC('O', 'b', 'j', 'W'));
	mpObjToRig = (int*)nxCore::mem_alloc(nskin * sizeof(int), XD_FOURCC('O', '2', 'R', '_'));
	mSkinNodesNum = nskin;
	cxMtx* pRestIW = (cxMtx*)nxCore::mem_alloc(nskin * sizeof(cxMtx), XD_TMP_MEM_TAG);
	if (pRestIW) {
		for (int i = 0; i < nskin; ++i) {
			const char* pNodeName = pGeoData->get_skin_node_name(i);
			int nodeIdx = pRig->mpData->find_node(pNodeName);
			if (mpObjToRig) {
				mpObjToRig[i] = nodeIdx;
			}
			pRestIW[i] = pRig->mpData->get_imtx(nodeIdx);
		}
		gexObjSkinInit(mpDispObj, pRestIW);
		nxCore::mem_free(pRestIW);
		pRestIW = nullptr;
	}
	update_world();
}

void cSkinGeo::reset() {
	nxCore::mem_free(mpObjMtxW);
	mpObjMtxW = nullptr;
	nxCore::mem_free(mpObjToRig);
	mpObjToRig = nullptr;
	gexObjDestroy(mpDispObj);
	mpDispObj = nullptr;
	mpRig = nullptr;
}

void cSkinGeo::update_world() {
	if (mpRig && mpObjMtxW && mpObjToRig) {
		for (int i = 0; i < mSkinNodesNum; ++i) {
			int isrc = mpObjToRig[i];
			if (isrc >= 0) {
				mpObjMtxW[i] = mpRig->mpMtxW[isrc];
			}
		}
	}
}

void cSkinGeo::disp(GEX_LIT* pLit) {
	if (!is_valid()) return;
	update_world();
	gexObjTransform(mpDispObj, mpObjMtxW);
	gexObjDisp(mpDispObj, pLit);
}


void cHumanoid::load(const char* pGeoPath, const char* pRigPath, const char* pMtlPath, const char* pBatchGrpPrefix) {
	mInternalRig.load(pRigPath);
	if (!mInternalRig.is_valid()) {
		return;
	}
	sxData* pData = nxData::load(pGeoPath);
	if (!pData || !pData->is<sxGeometryData>()) {
		nxData::unload(pData);
		mInternalRig.unload();
		return;
	}
	sxGeometryData* pGeoData = pData->as<sxGeometryData>();
	pData = nxData::load(pMtlPath);
	sxValuesData* pMtlData = pData ? pData->as<sxValuesData>() : nullptr;
	init(pGeoData, &mInternalRig, pMtlData, pBatchGrpPrefix);
	nxData::unload(pGeoData);
	nxData::unload(pMtlData);
	mOwnData = true;
}

void cHumanoid::init(sxGeometryData* pGeoData, cHumanoidRig* pRig, sxValuesData* pMtlData, const char* pBatchGrpPrefix) {
	if (!pGeoData) return;
	if (!pRig) return;
	mpRig = pRig;
	mSkin.init(pGeoData, pRig, pBatchGrpPrefix);
	if (mSkin.mpDispObj && pMtlData) {
		init_materials(*mSkin.mpDispObj, *pMtlData);
	}
}

void cHumanoid::reset() {
	mSkin.reset();
	if (mOwnData) {
		mInternalRig.unload();
	}
	mpRig = nullptr;
	mOwnData = false;
}

float cHumanoid::animate(sxKeyframesData* pKfr, sxKeyframesData::RigLink* pLnk, float frameNow, float frameAdd) {
	float frame = frameNow;
	if (mpRig) {
		frame = mpRig->animate(pKfr, pLnk, frameNow, frameAdd);
		mpRig->blend_exec();
		mpRig->calc_world();
	}
	return frame;
}

void cHumanoid::disp(GEX_LIT* pLit) {
	if (!is_valid()) return;
	mSkin.disp(pLit);
}
