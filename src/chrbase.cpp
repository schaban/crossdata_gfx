// Author: Sergey Chaban <sergey.chaban@gmail.com>

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
	mpParams = (NodeParams*)nxCore::mem_alloc(nnodes * sizeof(NodeParams), XD_FOURCC('R', 'i', 'g', 'P'));
	mpMtxL = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'L'));
	mpMtxW = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'W'));
	mpPrevMtxW = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'w'));
	mpMtxBlendL = (cxMtx*)nxCore::mem_alloc(nnodes * sizeof(cxMtx), XD_FOURCC('R', 'i', 'g', 'B'));
	mpLocalFuncs = (LocalFunc*)nxCore::mem_alloc(nnodes * sizeof(LocalFunc), XD_FOURCC('R', 'i', 'g', 'F'));
	for (int i = 0; i < nnodes; ++i) {
		if (mpParams) {
			mpParams[i].mPos = mpData->get_lpos(i);
			mpParams[i].mRot = mpData->get_lrot(i);
			mpParams[i].mScl = mpData->get_lscl(i);
		}
		if (mpMtxL) {
			mpMtxL[i] = mpData->get_lmtx(i);
		}
		if (mpMtxW) {
			mpMtxW[i] = mpData->get_wmtx(i);
		}
		if (mpMtxBlendL && mpMtxL) {
			mpMtxBlendL[i] = mpMtxL[i];
		}
		mpLocalFuncs[i] = nullptr;
	}
	save_prev_w();
	clear_anim_status();
	clear_expr_status();
	mNodesNum = nnodes;
	mRootNodeId = mpData->find_node("root");
	mMovementNodeId = mpData->find_node("n_Move");
	if (!mpData->ck_node_idx(mMovementNodeId)) {
		mMovementNodeId = mRootNodeId;
	}
	mExprCtx.init(*this);
}

void cBaseRig::reset() {
	if (!mpData) return;
	mExprCtx.reset();
	nxCore::mem_free(mpMtxL);
	mpMtxL = nullptr;
	nxCore::mem_free(mpMtxW);
	mpMtxW = nullptr;
	nxCore::mem_free(mpPrevMtxW);
	mpPrevMtxW = nullptr;
	nxCore::mem_free(mpMtxBlendL);
	mpMtxBlendL = nullptr;
	nxCore::mem_free(mpParams);
	mpParams = nullptr;
	nxCore::mem_free(mpLocalFuncs);
	mpLocalFuncs = nullptr;
}

void cBaseRig::ExprCtx::init(cBaseRig& rig) {
	mpRig = &rig;
	sxRigData* pRigData = rig.mpData;
	if (pRigData) {
		int nexpr = pRigData->get_expr_num();
		if (nexpr > 0) {
			mExprNum = nexpr;
			int stksize = pRigData->get_expr_stack_size();
			mStack.alloc(stksize);
			mppInfo = (sxRigData::ExprInfo**)nxCore::mem_alloc(nexpr * sizeof(sxRigData::ExprInfo*), XD_FOURCC('R', 'E', 'x', 'p'));
			if (mppInfo) {
				for (int i = 0; i < nexpr; ++i) {
					mppInfo[i] = pRigData->get_expr_info(i);
				}
			}
		}
	}
}

void cBaseRig::ExprCtx::reset() {
	nxCore::mem_free(mppInfo);
	mppInfo = nullptr;
	mStack.free();
	mExprNum = 0;
	mpRig = nullptr;
}

float cBaseRig::ExprCtx::ch(const sxCompiledExpression::String& path) {
	float val = 0.0f;
	if (mpRig) {
		ExprChInfo chi = mpRig->parse_ch_path(path);
		val = mpRig->eval_ch(chi);
	}
	return val;
}

void cBaseRig::clear_anim_status() {
	if (is_valid() && mpParams) {
		for (int i = 0; i < mNodesNum; ++i) {
			mpParams[i].mAnimStatus.clear();
		}
	}
}

void cBaseRig::clear_expr_status() {
	if (is_valid() && mpParams) {
		for (int i = 0; i < mNodesNum; ++i) {
			mpParams[i].mExprStatus.clear();
		}
	}
}

cBaseRig::ExprChInfo cBaseRig::parse_ch_path(const sxCompiledExpression::String& path) const {
	ExprChInfo chi;
	const char* pPath = path.mpChars;
	int pathLen = path.mLen;
	short pathParts[2];
	int splitN = 0;
	for (int i = pathLen; --i;) {
		if (pPath[i] == '/') {
			pathParts[splitN] = (short)i;
			++splitN;
			if (splitN >= XD_ARY_LEN(pathParts)) break;
		}
	}
	if (splitN > 0) {
		const char* pChanName = &pPath[pathParts[0] + 1];
		const char* pNodeName = splitN < 2 ? pPath : &pPath[pathParts[1] + 1];
		size_t nodeNameLen = splitN < 2 ? pathParts[0] : pathParts[0] - pathParts[1] - 1;
		char nodeName[128];
		::memcpy(nodeName, pNodeName, nodeNameLen);
		nodeName[nodeNameLen] = 0;
		sxRigData* pRig = get_data();
		chi.mNodeId = pRig ? pRig->find_node(nodeName) : -1;
		chi.mChanId = nxDataUtil::anim_chan_from_str(pChanName);
	}
	return chi;
}

bool cBaseRig::NodeStatus::ck(exAnimChan ch) {
	bool res = false;
	switch (ch) {
		case exAnimChan::TX: res = !!tx; break;
		case exAnimChan::TY: res = !!ty; break;
		case exAnimChan::TZ: res = !!tz; break;
		case exAnimChan::RX: res = !!rx; break;
		case exAnimChan::RY: res = !!ry; break;
		case exAnimChan::RZ: res = !!rz; break;
		case exAnimChan::SX: res = !!sx; break;
		case exAnimChan::SY: res = !!sy; break;
		case exAnimChan::SZ: res = !!sz; break;
		default: break;
	}
	return res;
}

float cBaseRig::eval_ch(ExprChInfo chi) const {
	float val = 0.0f;
	int nodeId = chi.mNodeId;
	if (is_valid() && mpData->ck_node_idx(nodeId)) {
		bool useAnim = false;
		if (mpParams) {
			useAnim = mpParams[nodeId].mAnimStatus.ck(chi.mChanId);
		}
		if (useAnim) {
			val = mpParams[nodeId].get_ch(chi.mChanId);
		} else if (mpMtxL) {
			exRotOrd rord = mpData->get_rot_order(nodeId);
			exTransformOrd xord = mpData->get_xform_order(nodeId);
			cxMtx lm = mpMtxL[nodeId];
			cxMtx rt;
			cxVec scl = lm.get_scl(&rt, xord);
			switch (chi.mChanId) {
				case exAnimChan::TX: val = rt.get_translation().x; break;
				case exAnimChan::TY: val = rt.get_translation().y; break;
				case exAnimChan::TZ: val = rt.get_translation().z; break;
				case exAnimChan::RX: val = rt.get_rot_degrees(rord).x; break;
				case exAnimChan::RY: val = rt.get_rot_degrees(rord).y; break;
				case exAnimChan::RZ: val = rt.get_rot_degrees(rord).z; break;
				case exAnimChan::SX: val = scl.x; break;
				case exAnimChan::SY: val = scl.y; break;
				case exAnimChan::SZ: val = scl.z; break;
				default: break;
			}
		}
	}
	return val;
}

static void update_prm_ch(cBaseRig::NodeParams* pPrm, exAnimChan ch, float val, bool isExpr) {
	cBaseRig::NodeStatus* pStt = isExpr ? &pPrm->mExprStatus : &pPrm->mAnimStatus;
	switch (ch) {
		case exAnimChan::TX: pPrm->mPos.x = val; pStt->tx = 1; break;
		case exAnimChan::TY: pPrm->mPos.y = val; pStt->ty = 1; break;
		case exAnimChan::TZ: pPrm->mPos.z = val; pStt->tz = 1; break;
		case exAnimChan::RX: pPrm->mRot.x = val; pStt->rx = 1; break;
		case exAnimChan::RY: pPrm->mRot.y = val; pStt->ry = 1; break;
		case exAnimChan::RZ: pPrm->mRot.z = val; pStt->rz = 1; break;
		case exAnimChan::SX: pPrm->mScl.x = val; pStt->sx = 1; break;
		case exAnimChan::SY: pPrm->mScl.y = val; pStt->sy = 1; break;
		case exAnimChan::SZ: pPrm->mScl.z = val; pStt->sz = 1; break;
		default: break;
	}
}

float cBaseRig::NodeParams::get_ch(exAnimChan ch) {
	float val = 0.0f;
	switch (ch) {
		case exAnimChan::TX: val = mPos.x; break;
		case exAnimChan::TY: val = mPos.y; break;
		case exAnimChan::TZ: val = mPos.z; break;
		case exAnimChan::RX: val = mRot.x; break;
		case exAnimChan::RY: val = mRot.y; break;
		case exAnimChan::RZ: val = mRot.z; break;
		case exAnimChan::SX: val = mScl.x; break;
		case exAnimChan::SY: val = mScl.y; break;
		case exAnimChan::SZ: val = mScl.z; break;
		default: break;
	}
	return val;
}

void cBaseRig::NodeParams::update_anim_ch(exAnimChan ch, float val) {
	update_prm_ch(this, ch, val, false);
}

void cBaseRig::NodeParams::update_expr_ch(exAnimChan ch, float val) {
	update_prm_ch(this, ch, val, true);
}

void cBaseRig::exec_exprs() {
	clear_expr_status();
	if (is_valid()) {
		int n = mExprCtx.get_expr_num();
		for (int i = 0; i < n; ++i) {
			sxRigData::ExprInfo* pInfo = mExprCtx.get_expr_info(i);
			if (pInfo) {
				int nodeId = pInfo->mNodeId;
				if (mpData->ck_node_idx(nodeId)) {
					pInfo->get_code()->exec(mExprCtx);
					float res = mExprCtx.get_result();
					if (mpParams) {
						mpParams[nodeId].update_expr_ch(pInfo->get_chan_id(), res);
					}
				}
			}
		}
		if (mpParams && mpMtxL) {
			for (int i = 0; i < mNodesNum; ++i) {
				if (mpParams[i].mExprStatus.any()) {
					exTransformOrd xord = mpData->get_xform_order(i);
					exRotOrd rord = mpData->get_rot_order(i);
					cxMtx tm;
					tm.mk_translation(mpParams[i].mPos);
					cxMtx rm;
					rm.set_rot_degrees(mpParams[i].mRot, rord);
					cxMtx sm;
					sm.mk_scl(mpParams[i].mScl);
					mpMtxL[i].calc_xform(tm, rm, sm, xord);
				}
			}
		}
	}
}

float cBaseRig::animate(sxKeyframesData* pKfr, sxKeyframesData::RigLink* pLnk, float frameNow, float frameAdd, bool* pLoopFlg, bool evalExprs) {
	float frame = frameNow;
	clear_anim_status();
	if (is_valid() && pKfr && pLnk) {
		int16_t* pRigMap = pLnk->get_rig_map();
		float frmax = (float)pKfr->get_max_fno();
		mAnimFrame = frame;
		pKfr->eval_rig_link(pLnk, frame, mpData, mpMtxL);
		if (pRigMap && mpParams) {
			for (int i = 0; i < pLnk->mNodeNum; ++i) {
				sxKeyframesData::RigLink::Node* pLnkNode = &pLnk->mNodes[i];
				NodeParams* pPrm = &mpParams[pLnkNode->mRigNodeId];
				static exAnimChan ctbl[] = {
					exAnimChan::TX, exAnimChan::TY, exAnimChan::TZ,
					exAnimChan::RX, exAnimChan::RY, exAnimChan::RZ,
					exAnimChan::SX, exAnimChan::SY, exAnimChan::SZ
				};
				for (int ci = 0; ci < XD_ARY_LEN(ctbl); ++ci) {
					exAnimChan ch = ctbl[ci];
					if (pLnkNode->ck_anim_chan(ch)) {
						pPrm->update_anim_ch(ch, pLnkNode->get_anim_chan(ch));
					}
				}
			}
		}
		mMoveVel = mConstMoveVel * frameAdd;
		if (mMoveMode == eMoveMode::FCURVES && mpData->ck_node_idx(mMovementNodeId)) {
			if (pRigMap) {
				int moveGrpId = pRigMap[mMovementNodeId];
				if (moveGrpId >= 0) {
					sxKeyframesData::RigLink::Val* pVal = pLnk->mNodes[moveGrpId].get_pos_val();
					if (pVal) {
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
					} else {
						mMoveVel.zero();
					}
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
	if (evalExprs) {
		exec_exprs();
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

static void clean_rot_mtx(cxMtx* pMtx, int n) {
	if (!pMtx) return;
	if (n <= 0) return;
	float rmA[3 * 3];
	float rmU[3 * 3];
	float rmS[3];
	float rmV[3 * 3];
	float rmWk[3];
	for (int i = 0; i < n; ++i) {
		cxMtx m = pMtx[i];
		cxVec t = m.get_translation();
		for (int j = 0; j < 3; ++j) {
			m.get_row_vec(j).to_mem(&rmA[j * 3]);
		}
		bool res = nxLA::sv_decomp(rmU, rmS, rmV, rmWk, rmA, 3, 3);
		if (res) {
			nxLA::transpose(rmV, 3, 3);
			nxLA::mul_mm(rmA, rmU, rmV, 3, 3, 3);
			cxVec av[3];
			for (int j = 0; j < 3; ++j) {
				av[j].from_mem(&rmA[j * 3]);
			}
			m.set_rot_frame(av[0], av[1], av[2]);
			m.set_translation(t);
			pMtx[i] = m;
		}
	}
}

void cBaseRig::clean_rot_local() {
	if (is_valid()) {
		clean_rot_mtx(mpMtxL, mNodesNum);
	}
}

void cBaseRig::clean_rot_world() {
	if (is_valid()) {
		clean_rot_mtx(mpMtxW, mNodesNum);
	}
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
	if (mpLocalFuncs) {
		for (int i = 0; i < mNodesNum; ++i) {
			LocalFunc func = mpLocalFuncs[i];
			if (func) {
				func(*this, i);
			}
		}
	}
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

int cBaseRig::find_node_idx(const char* pName) const {
	int idx = -1;
	if (mpData) {
		idx = mpData->find_node(pName);
	}
	return idx;
}

void cBaseRig::set_local_func(int idx, LocalFunc func) {
	if (is_valid() && mpLocalFuncs && mpData->ck_node_idx(idx)) {
		mpLocalFuncs[idx] = func;
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

float cHumanoidRig::animate(sxKeyframesData* pKfr, sxKeyframesData::RigLink* pLnk, float frameNow, float frameAdd, bool* pLoopFlg, bool evalExprs) {
	float fnext = frameNow;
	if (is_valid()) {
		fnext = cBaseRig::animate(pKfr, pLnk, frameNow, frameAdd, pLoopFlg, false);
		update_coord();
		for (int i = 0; i < 4; ++i) {
			if (mLimbs[i].mpInfo) {
				mpData->calc_limb_local(&mLimbs[i].mSolution, mLimbs[i].mChain, mpMtxL);
				mpData->copy_limb_solution(mpMtxL, mLimbs[i].mChain, mLimbs[i].mSolution);
			}
		}
		if (evalExprs) {
			exec_exprs();
		}
	}
	return fnext;
}


void cSkinGeo::init(sxGeometryData* pGeoData, cBaseRig* pRig, const char* pBatchGrpPrefix, const char* pSortMtlsAttr) {
	if (!pGeoData) return;
	if (!pRig || !pRig->is_valid()) return;
	mpDispObj = gexObjCreate(*pGeoData, pBatchGrpPrefix, pSortMtlsAttr);
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


void cHumanoid::load(const char* pGeoPath, const char* pRigPath, const char* pMtlPath, const char* pBatchGrpPrefix, const char* pSortMtlsAttr) {
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
	init(pGeoData, &mInternalRig, pMtlData, pBatchGrpPrefix, pSortMtlsAttr);
	nxData::unload(pGeoData);
	nxData::unload(pMtlData);
	mOwnData = true;
}

void cHumanoid::init(sxGeometryData* pGeoData, cHumanoidRig* pRig, sxValuesData* pMtlData, const char* pBatchGrpPrefix, const char* pSortMtlsAttr) {
	if (!pGeoData) return;
	if (!pRig) return;
	mpRig = pRig;
	mSkin.init(pGeoData, pRig, pBatchGrpPrefix, pSortMtlsAttr);
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
