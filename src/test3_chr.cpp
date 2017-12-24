#include "crossdata.hpp"
#include "gex.hpp"
#include "keyctrl.hpp"
#include "remote.hpp"
#include "task.hpp"
#include "util.hpp"
#include "obstacle.hpp"

#include "test3_chr.hpp"

void cCharacter3::create() {
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
		obj_shadow_params(*pObj, 1.0f, 500.0f, false);
		//obj_sort_mode(*pObj, GEX_SORT_MODE::CENTER);
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

	mpMotEvalJobs = tskJobsAlloc(nnodes);
	mpMotEvalQueue = tskQueueCreate(nnodes);
	if (mpMotEvalJobs) {
		for (int i = 0; i < nnodes; ++i) {
			mpMotEvalJobs[i].mFunc = mot_node_eval_job;
			mpMotEvalJobs[i].mpData = this;
		}
		if (mpMotEvalQueue) {
			for (int i = 0; i < nnodes; ++i) {
				tskQueueAdd(mpMotEvalQueue, &mpMotEvalJobs[i]);
			}
		}
	}	

	//mtl_sort_bias(*pObj, "lashes", 0.2f, 0.0f);

	nxData::unload(pGeo);

	mMotIdHandsDefault = mMotLib.find_motion("HandsDefault");
	mMotIdDanceLoop = mMotLib.find_motion("DanceLoop");
	mMotIdStand = mMotLib.find_motion("Stand");
	mMotIdWalk = mMotLib.find_motion("Walk");
	mMotIdRun = mMotLib.find_motion("Run");

	mStateMain = get_test_mode() ? STATE::DANCE_LOOP : STATE::IDLE;
	mStateSub = 0;

	mInitFlg = true;
}

void cCharacter3::destroy() {
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
	tskQueueDestroy(mpMotEvalQueue);
	mpMotEvalQueue = nullptr;
	tskJobsFree(mpMotEvalJobs);
	mpMotEvalJobs = nullptr;
	mInitFlg = false;
}

void cCharacter3::blend_init(int duration) {
	::memcpy(mpBlendMtxL, mpRigMtxL, mRigNodesNum * sizeof(cxMtx));
	mBlendDuration = (float)duration;
	mBlendCount = mBlendDuration;
}

void cCharacter3::calc_blend() {
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

/*static*/ void cCharacter3::mot_node_eval_job(TSK_CONTEXT* pCtx) {
	TSK_JOB* pJob = pCtx->mpJob;
	cCharacter3* pSelf = (cCharacter3*)pJob->mpData;
	sxKeyframesData::RigLink* pLink = pSelf->mpMotEvalLink;
	if (!pLink) return;
	sxKeyframesData* pKfr = pSelf->mpMotEvalKfr;
	if (!pKfr) return;
	pKfr->eval_rig_link_node(pLink, pJob->mId, pSelf->mMotEvalFrame, pSelf->mpRig, pSelf->mpRigMtxL);
}

float cCharacter3::calc_motion(int motId, float frame, float frameStep) {
	if (!mInitFlg) return frame;
	sxKeyframesData* pKfr = mMotLib.get_keyframes(motId);
	if (!pKfr) return frame;
	sxKeyframesData::RigLink* pLink = mMotLib.get_riglink(motId);
	if (!pLink) return frame;
	mpMotEvalKfr = pKfr;
	mpMotEvalLink = pLink;
	mMotEvalFrame = frame;
	TSK_BRIGADE* pBgd = get_brigade();
	bool tskFlg = pBgd && mpMotEvalJobs && mpMotEvalQueue;
	int njobs = pLink->mNodeNum;
	int nwrk = tskBrigadeGetNumWorkers(pBgd);
	if (tskFlg) {
		if (nwrk > 3) {
			nwrk = (int)::floorf((float)njobs / 7);
		}
		tskBrigadeSetActiveWorkers(pBgd, nwrk);
		tskQueueAdjust(mpMotEvalQueue, njobs);
		tskBrigadeExec(pBgd, mpMotEvalQueue);
	} else {
		pKfr->eval_rig_link(pLink, frame, mpRig, mpRigMtxL);
	}

	int moveGrpId = -1;
	sxKeyframesData::RigLink::Val* pMoveVal = nullptr;
	cxVec prevPos;
	float prevFrame = nxCalc::max(frame - frameStep, 0.0f);
	if (mpRig->ck_node_idx(mMovementNodeId)) {
		mMotVelFrame = frame;
		int16_t* pMotToRig = pLink->get_rig_map();
		if (pMotToRig) {
			moveGrpId = pMotToRig[mMovementNodeId];
			if (moveGrpId >= 0) {
				pMoveVal = pLink->mNodes[moveGrpId].get_pos_val();
				if (frame > 0.0f) {
					for (int i = 0; i < 3; ++i) {
						sxKeyframesData::FCurve fcv = pKfr->get_fcv(pMoveVal->fcvId[i]);
						prevPos.set_at(i, fcv.is_valid() ? fcv.eval(prevFrame) : 0.0f);
					}
				}
			}
		}
	}

	if (tskFlg) {
		tskBrigadeWait(pBgd);
	}

	if (pMoveVal) {
		cxVec vel = pMoveVal->get_vec();
		if (frame > 0.0f) {
			vel -= prevPos;
		} else {
			vel *= frameStep;
		}
		mMotVel = vel;
	}

	frame += frameStep;
	if (frame >= pKfr->get_max_fno()) {
		frame = 0.0f;
	}

	if (0 && pBgd) {
		::printf("%d/%d -> ", njobs, nwrk);
		int nwrk = tskBrigadeGetNumActiveWorkers(pBgd);
		for (int i = 0; i < nwrk; ++i) {
			TSK_CONTEXT* pCtx = tskBrigadeGetContext(pBgd, i);
			::printf("%d:%d ", i, pCtx->mJobsDone);
		}
		::printf("\n");
	}

	return frame;
}

void cCharacter3::update_coord() {
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

void cCharacter3::calc_world() {
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

void cCharacter3::dance_loop_ctrl() {
	KEY_CTRL* pKeys = get_ctrl_keys();
	if (!pKeys) return;
	bool flgU = keyCkTrg(pKeys, D_KEY_UP);
	bool flgD = keyCkTrg(pKeys, D_KEY_DOWN);
	bool flgL = keyCkTrg(pKeys, D_KEY_LEFT);
	bool flgR = keyCkTrg(pKeys, D_KEY_RIGHT);
	bool useTDJoy = true;
	if (useTDJoy) {
		flgU |= (get_td_joy()->b4 != 0);
		flgD |= (get_td_joy()->b1 != 0);
		flgL |= (get_td_joy()->b3 != 0);
		flgR |= (get_td_joy()->b2 != 0);
	}
	if (flgU) {
		mStateMain = STATE::DANCE_ACT;
		mStateSub = 0;
		if (keyCkNow(pKeys, D_KEY_L1)) {
			mStateParams[0] = mMotLib.find_motion("DanceAct_jump_L");
			mStateParams[1] = 12;
			mMotFrame = 0.0f;
			blend_init(12);
		} else if (keyCkNow(pKeys, D_KEY_R1)) {
			mStateParams[0] = mMotLib.find_motion("DanceAct_jump_R");
			mStateParams[1] = 12;
			mMotFrame = 0.0f;
			blend_init(12);
		} else {
			int actRand = ::rand() & 3;
			int actMotId = -1;
			int actBlendToTime = 10;
			int actBlendBackTime = 16;
			switch (actRand) {
				case 0:
					actMotId = mMotLib.find_motion("DanceAct1_U");
					break;
				case 1:
					actMotId = mMotLib.find_motion("DanceAct2_U");
					actBlendToTime = 14;
					actBlendBackTime = 20;
					break;
				case 2:
					actMotId = mMotLib.find_motion("DanceAct3_U");
					break;
				case 3:
					actMotId = mMotLib.find_motion("DanceAct4_U");
					break;
			}
			mStateParams[0] = actMotId;
			mStateParams[1] = actBlendBackTime;
			mMotFrame = 0.0f;
			blend_init(actBlendToTime);
		}
	} else if (flgD) {
		mStateMain = STATE::DANCE_ACT;
		mStateSub = 0;
		int actRand = ::rand() & 3;
		int actMotId = -1;
		int actBlendToTime = 10;
		int actBlendBackTime = 16;
		switch (actRand) {
			case 0:
				actMotId = mMotLib.find_motion("DanceAct1_D");
				break;
			case 1:
				actMotId = mMotLib.find_motion("DanceAct2_D");
				break;
			case 2:
				actMotId = mMotLib.find_motion("DanceAct3_D");
				break;
			case 3:
				actMotId = mMotLib.find_motion("DanceAct4_D");
				actBlendBackTime = 20;
				break;
		}
		mStateParams[0] = actMotId;
		mStateParams[1] = actBlendBackTime;
		mMotFrame = 0.0f;
		blend_init(actBlendToTime);
	} else if (flgL) {
		mStateMain = STATE::DANCE_ACT;
		mStateSub = 0;
		int actRand = ::rand() & 3;
		int actMotId = -1;
		int actBlendToTime = 10;
		int actBlendBackTime = 16;
		switch (actRand) {
			case 0:
				actMotId = mMotLib.find_motion("DanceAct1_L");
				actBlendBackTime = 20;
				break;
			case 1:
				actMotId = mMotLib.find_motion("DanceAct2_L");
				break;
			case 2:
				actMotId = mMotLib.find_motion("DanceAct3_L");
				break;
			case 3:
				actMotId = mMotLib.find_motion("DanceAct4_L");
				break;
		}
		mStateParams[0] = actMotId;
		mStateParams[1] = actBlendBackTime;
		mMotFrame = 0.0f;
		blend_init(actBlendToTime);
	} else if (flgR) {
		mStateMain = STATE::DANCE_ACT;
		mStateSub = 0;
		int actRand = ::rand() & 3;
		int actMotId = -1;
		int actBlendToTime = 10;
		int actBlendBackTime = 16;
		switch (actRand) {
			case 0:
				actMotId = mMotLib.find_motion("DanceAct1_R");
				actBlendBackTime = 20;
				break;
			case 1:
				actMotId = mMotLib.find_motion("DanceAct2_R");
				break;
			case 2:
				actMotId = mMotLib.find_motion("DanceAct3_R");
				break;
			case 3:
				actMotId = mMotLib.find_motion("DanceAct4_R");
				break;
		}
		mStateParams[0] = actMotId;
		mStateParams[1] = actBlendBackTime;
		mMotFrame = 0.0f;
		blend_init(actBlendToTime);
	} else if (keyCkTrg(pKeys, D_KEY_A)) {
		mStateMain = STATE::DANCE_ACT;
		mStateSub = 0;
		//mStateParams[0] = mMotLib.find_motion("MuayThai");
		mStateParams[0] = mMotLib.find_motion("Karate");
		mStateParams[1] = 12;
		mMotFrame = 0.0f;
		blend_init(20);
	}
}

void cCharacter3::dance_loop_exec() {
	if (mStateSub == 0) {
		calc_motion(mMotIdHandsDefault, 0, 0);
		++mStateSub;
	}
	mMotFrame = calc_motion(mMotIdDanceLoop, mMotFrame, get_anim_speed());
	calc_blend();
	calc_world();
}

void cCharacter3::dance_act_ctrl() {
}

void cCharacter3::dance_act_exec() {
	mMotFrame = calc_motion(mStateParams[0], mMotFrame, get_anim_speed());
	calc_blend();
	calc_world();
	if (mMotFrame == 0.0f) {
		blend_init(mStateParams[1]);
		mStateMain = STATE::DANCE_LOOP;
		mStateSub = 0;
	}
}

void cCharacter3::idle_ctrl() {
	KEY_CTRL* pKeys = get_ctrl_keys();
	if (!pKeys) return;
	if (keyCkTrg(pKeys, D_KEY_UP)) {
		if (keyCkNow(pKeys, D_KEY_L1)) {
			blend_init(12);
			trans_to_state(STATE::RUN);
		} else {
			blend_init(12);
			trans_to_state(STATE::WALK);
		}
	} else if (keyCkNow(pKeys, D_KEY_LEFT) || keyCkNow(pKeys, D_KEY_RIGHT)) {
		if (keyCkNow(pKeys, D_KEY_LEFT)) {
			mStateParams[0] = mMotLib.find_motion("TurnL");
			mStateParamsF[0] = 90.0f;
		} else {
			mStateParams[0] = mMotLib.find_motion("TurnR");
			mStateParamsF[0] = -90.0f;
		}
		blend_init(8);
		trans_to_state(STATE::TURN);
	} else if (keyCkTrg(pKeys, D_KEY_DOWN)) {
		cxMtx dirMtx;
		dirMtx.set_rot_degrees(mWorldRot);
		cxVec vecL = dirMtx.calc_vec(nxVec::get_axis(exAxis::PLUS_X));
		cxVec vecR = dirMtx.calc_vec(nxVec::get_axis(exAxis::MINUS_X));
		if (vecL.azimuth() < vecR.azimuth()) {
			mStateParams[0] = mMotLib.find_motion("TurnBL");
			mStateParamsF[0] = 180.0f;
		} else {
			mStateParams[0] = mMotLib.find_motion("TurnBR");
			mStateParamsF[0] = -180.0f;
		}
		blend_init(8);
		trans_to_state(STATE::TURN);
	}
}

void cCharacter3::idle_exec() {
	if (mStateSub == 0) {
		calc_motion(mMotIdHandsDefault, 0, 0);
		++mStateSub;
	}
	mMotFrame = calc_motion(mMotIdStand, mMotFrame, get_anim_speed());
	update_coord();
	calc_blend();
	calc_world();
}

void cCharacter3::turn_ctrl() {
	KEY_CTRL* pKeys = get_ctrl_keys();
	if (pKeys && keyCkTrg(pKeys, D_KEY_UP)) {
		blend_init(12);
		trans_to_state(STATE::WALK);
	} else {
		sxKeyframesData* pMot = mMotLib.get_keyframes(mStateParams[0]);
		if (pMot) {
			float fmax = (float)pMot->get_max_fno();
			float step = nxCalc::div0(get_anim_speed(), fmax - get_anim_speed());
			mWorldRot.y += step * mStateParamsF[0];
		}
	}
}

void cCharacter3::turn_exec() {
	mMotFrame = calc_motion(mStateParams[0], mMotFrame, get_anim_speed());
	update_coord();
	calc_blend();
	calc_world();
	if (0.0f == mMotFrame) {
		blend_init(8);
		trans_to_state(STATE::IDLE);
	}
}

void cCharacter3::walk_ctrl() {
	KEY_CTRL* pKeys = get_ctrl_keys();
	if (!pKeys) return;
	if (!keyCkNow(pKeys, D_KEY_UP)) {
		blend_init(12);
		trans_to_state(STATE::IDLE);
	} else {
		if (keyCkTrg(pKeys, D_KEY_L1)) {
			blend_init(12);
			trans_to_state(STATE::RUN);
		} else {
			float rotSpeed = 1.0f;
			if (keyCkNow(pKeys, D_KEY_LEFT)) {
				mWorldRot.y += rotSpeed * get_anim_speed();
			} else if (keyCkNow(pKeys, D_KEY_RIGHT)) {
				mWorldRot.y -= rotSpeed * get_anim_speed();
			}
		}
	}
}

void cCharacter3::walk_exec() {
	mMotFrame = calc_motion(mMotIdWalk, mMotFrame, get_anim_speed());
	update_coord();
	calc_blend();
	calc_world();
}

void cCharacter3::run_ctrl() {
	KEY_CTRL* pKeys = get_ctrl_keys();
	if (!pKeys) return;
	if (!keyCkNow(pKeys, D_KEY_UP)) {
		blend_init(12);
		trans_to_state(STATE::IDLE);
	} else {
		if (!keyCkNow(pKeys, D_KEY_L1)) {
			blend_init(12);
			trans_to_state(STATE::WALK);
		} else {
			float rotSpeed = 3.0f;
			if (keyCkNow(pKeys, D_KEY_LEFT)) {
				mWorldRot.y += rotSpeed * get_anim_speed();
			} else if (keyCkNow(pKeys, D_KEY_RIGHT)) {
				mWorldRot.y -= rotSpeed * get_anim_speed();
			}
		}
	}
}

void cCharacter3::run_exec() {
	mMotFrame = calc_motion(mMotIdRun, mMotFrame, get_anim_speed());
	update_coord();
	calc_blend();
	calc_world();
}

bool cCharacter3::wall_adj_line() {
	cxVec hitPos;
	cxVec hitNrm;
	cxVec oldPos = mPrevWorldPos;
	cxVec newPos = mWorldPos;
	float yoffs = 1.0f;
	oldPos.y += yoffs;
	newPos.y += yoffs;
	bool hitFlg = stg_hit_ck(oldPos, newPos, &hitPos, &hitNrm);
	if (hitFlg) {
		mWorldPos = mPrevWorldPos;
	}
	return hitFlg;
}

bool cCharacter3::wall_adj_ball() {
	sxGeometryData* pGeo = get_stg_obst_geo();
	if (!pGeo) return false;

	cxVec oldPos = mPrevWorldPos;
	cxVec newPos = mWorldPos;
	float yoffs = 1.0f;
	oldPos.y += yoffs;
	newPos.y += yoffs;
	float radius = 0.1f;
	cxVec adjPos = newPos;
	bool adjFlg = obstBallToWalls(newPos, oldPos, radius, *pGeo, &adjPos);
	if (adjFlg) {
		mWorldPos.x = adjPos.x;
		mWorldPos.y = adjPos.y - yoffs;
		mWorldPos.z = adjPos.z;
	}
	return adjFlg;
}

bool cCharacter3::wall_adj() {
	bool flg = false;
	if (0) {
		flg = wall_adj_line();
	} else {
		flg = wall_adj_ball();
	}
	return flg;
}

bool cCharacter3::prop_adj() {
	return false;
	/* test */
	bool adjFlg = false;
	cxVec oldPos = mPrevWorldPos;
	cxVec newPos = mWorldPos;
	float height = 2.0f;
	float radius = 0.1f;
	cxVec adjPos = newPos;
	cxVec propPos(3.5f, 0.0f, 0.0f);
	float propRad = 0.5f;
	float propH = 2.0f;
	int mode = 1;
	if (mode == 0) {
		adjFlg = obstBallToBall(newPos, oldPos, radius, propPos, propRad, &adjPos);
		if (adjFlg) {
			mWorldPos.x = adjPos.x;
			mWorldPos.y = adjPos.y;
			mWorldPos.z = adjPos.z;
		}
	} else if (mode == 1) {
		adjFlg = obstPillarToPillar(newPos, oldPos, radius, height, propPos, propRad, propH, &adjPos);
		if (adjFlg) {
			mWorldPos.x = adjPos.x;
			mWorldPos.y = adjPos.y;
			mWorldPos.z = adjPos.z;
		}
	}
	return adjFlg;
}

void cCharacter3::update() {
	static StateFunc ctrlTbl[] = {
		&cCharacter3::dance_loop_ctrl,
		&cCharacter3::dance_act_ctrl,
		&cCharacter3::idle_ctrl,
		&cCharacter3::turn_ctrl,
		&cCharacter3::walk_ctrl,
		&cCharacter3::run_ctrl
	};
	static StateFunc execTbl[] = {
		&cCharacter3::dance_loop_exec,
		&cCharacter3::dance_act_exec,
		&cCharacter3::idle_exec,
		&cCharacter3::turn_exec,
		&cCharacter3::walk_exec,
		&cCharacter3::run_exec
	};
	(this->*ctrlTbl[(int)mStateMain])();
	(this->*execTbl[(int)mStateMain])();

	bool adjFlg = false;
	adjFlg |= prop_adj();
	adjFlg |= wall_adj();
	if (adjFlg) {
		calc_world();
	}
}

void cCharacter3::disp(GEX_LIT* pLit) {
	if (!mInitFlg) return;
	gexObjTransform(mpObj, mpObjMtxW);
	gexObjDisp(mpObj, pLit);
}

cxMtx cCharacter3::get_movement_mtx() const {
	cxMtx m;
	m.identity();
	if (mpRigMtxW && mMovementNodeId >= 0) {
		m = mpRigMtxW[mMovementNodeId];
	}
	return m;
}
