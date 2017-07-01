#include "crossdata.hpp"
#include "gex.hpp"
#include "util.hpp"
#include "chrbase.hpp"

void cChrBase::blend_init(int duration) {
	::memcpy(mpBlendMtxL, mpRigMtxL, mRigNodesNum * sizeof(cxMtx));
	mBlendDuration = (float)duration;
	mBlendCount = mBlendDuration;
}

void cChrBase::calc_blend() {
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

float cChrBase::calc_motion(int motId, float frame, float frameStep) {
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

void cChrBase::update_coord() {
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

void cChrBase::calc_world() {
	if (!mInitFlg) return;
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
