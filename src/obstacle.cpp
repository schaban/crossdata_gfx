#include "crossdata.hpp"
#include "obstacle.hpp"


void sObstGeoWk::alloc(const sxGeometryData& geo) {
	int npol = geo.get_pol_num();
	if (npol > 0) {
		mPolNum = npol;
		int stampBytes = obstCalcStampBytes(npol);
		mpStamps = (uint32_t*)nxCore::mem_alloc(stampBytes, XD_FOURCC('o', 'g', 'w', '1'));
		mpPolLst = (int*)nxCore::mem_alloc(npol * sizeof(int), XD_FOURCC('o', 'g', 'w', '2'));
	}
}

void sObstGeoWk::free() {
	if (mpStamps) {
		nxCore::mem_free(mpStamps);
		mpStamps = nullptr;
	}
	if (mpPolLst) {
		nxCore::mem_free(mpPolLst);
		mpPolLst = nullptr;
	}
	mPolNum = 0;
}

struct sObstPolyWk {
	cxPlane mPlane;
	cxAABB mBBox;
	cxVec mVtx[4];
	int mIdx;
	bool mTriFlg;

	sObstPolyWk() : mIdx(-1) {}

	void init(const sxGeometryData& geo, int idx) {
		mIdx = idx;
		sxGeometryData::Polygon pol = geo.get_pol(idx);
		int nvtx = pol.get_vtx_num();
		mTriFlg = false;
		if (nvtx >= 4) {
			mVtx[0] = pol.get_vtx_pos(3);
			mVtx[1] = pol.get_vtx_pos(2);
			mVtx[2] = pol.get_vtx_pos(1);
			mVtx[3] = pol.get_vtx_pos(0);
		} else if (nvtx == 3) {
			mVtx[0] = pol.get_vtx_pos(2);
			mVtx[1] = pol.get_vtx_pos(1);
			mVtx[2] = pol.get_vtx_pos(0);
			mVtx[3] = pol.get_vtx_pos(0);
			mTriFlg = true;
		}
		cxVec nrm = nxGeom::tri_normal_ccw(mVtx[0], mVtx[1], mVtx[2]);
		mPlane.calc(mVtx[0], nrm);
		mBBox.set(mVtx[0]);
		mBBox.add_pnt(mVtx[1]);
		mBBox.add_pnt(mVtx[2]);
		if (!mTriFlg) {
			mBBox.add_pnt(mVtx[3]);
		}
	}

	int get_vtx_num() const { return mTriFlg ? 3 : 4; }
	int get_next_lst() const { return mTriFlg ? 0x1021 : 0x0321; }
	cxVec get_normal() const { return mPlane.get_normal(); }
	float get_min_y() const { return mBBox.get_min_pos().y; }
	float get_max_y() const { return mBBox.get_max_pos().y; }
	bool in_y_range(float y) const { return y >= get_min_y() && y <= get_max_y(); }

	bool is_pnt_inside(const cxVec& pnt) const {
		int n = get_vtx_num();
		int next = get_next_lst();
		cxVec nrm = get_normal();
		for (int i = 0; i < n; ++i) {
			cxVec v = mVtx[next & 3] - mVtx[i];
			cxVec p = pnt - mVtx[i];
			cxVec t = nxVec::cross(v, p);
			if (t.dot(nrm) < 0.0f) return false;
			next >>= 4;
		}
		return true;
	}

	bool seg_intersect(const cxVec& p0, const cxVec& p1, cxVec* pIsPos) {
		return nxGeom::seg_quad_intersect_ccw_n(p0, p1, mVtx[0], mVtx[1], mVtx[2], mVtx[3], get_normal(), pIsPos);
	}

	bool seg_intersect_pln(const cxVec& p0, const cxVec& p1, cxVec* pIsPos) {
		float t = 0.0f;
		bool res = mPlane.seg_intersect(p0, p1, &t);
		if (res) {
			if (pIsPos) {
				*pIsPos = cxLineSeg(p0, p1).get_inner_pos(t);
			}
		}
		return res;
	}
};

struct sObstBallWk {
	cxVec mNewPos;
	cxVec mOldPos;
	cxVec mAdjPos;
	float mRadius;
	float mRange;
	float mSqDist;
	uint32_t* mpStamps;
	int mAdjIdx;
	int8_t mState;
	int8_t mObstState;

	sObstBallWk(const cxVec& newPos, const cxVec& oldPos, float radius, float range, uint32_t* pStamps) {
		mNewPos = newPos;
		mOldPos = oldPos;
		mRadius = radius;
		mRange = range;
		mSqDist = FLT_MAX;
		mAdjPos = mNewPos;
		mpStamps = pStamps;
		mAdjIdx = -1;
		mState = 0;
		mObstState = 0;
	}

	void stamp_st(int idx) { XD_BIT_ARY_ST(uint32_t, mpStamps, idx); }
	bool stamp_ck(int idx) const { return XD_BIT_ARY_CK(uint32_t, mpStamps, idx); }

	void adjust(sObstPolyWk& poly, float margin = 0.005f);
};

void sObstBallWk::adjust(sObstPolyWk& poly, float margin) {
	float sdist = poly.mPlane.signed_dist(mNewPos);
	float adist = ::fabsf(sdist);
	if (sdist > 0.0f && adist > mRadius) {
		if (adist > mRange) {
			stamp_st(poly.mIdx);
		}
		return;
	}
	cxVec nrm = poly.get_normal();
	cxVec isect;
	if (adist > mRadius) {
		if (adist > mRange) {
			stamp_st(poly.mIdx);
		}
		if (!poly.seg_intersect_pln(mNewPos, mOldPos, &isect)) {
			return;
		}
	} else {
		isect = mNewPos + (sdist > 0.0f ? nrm.neg_val() : nrm) * adist;
	}

	float dist2;
	float r = mRadius;
	cxVec adj;
	const float eps = 1e-6f;
	if (poly.is_pnt_inside(isect)) {
		mState = 1;
		adist = (r - adist) + margin;
		adj = nrm;
		if (sdist <= 0.0f) {
			adj.neg();
		}
		adj.scl(adist);
	} else {
		if (mState == 1) {
			return;
		}
		cxVec vec;
		vec.zero();
		float mov = 0.0f;
		float rr = nxCalc::sq(r);
		adist = FLT_MAX;
		bool flg = false;
		int nvtx = poly.get_vtx_num();
		int next = poly.get_next_lst();
		for (int i = 0; i < nvtx; ++i) {
			cxVec vtx = poly.mVtx[i];
			cxVec ev = mNewPos - vtx;
			dist2 = ev.mag2();
			if (dist2 <= rr && dist2 < adist) {
				vec = ev.get_normalized();
				mov = ::sqrtf(dist2);
				if (vec.dot(mOldPos - vtx) < 0.0f) {
					vec.neg();
				} else {
					mov = -mov;
				}
				flg = true;
			}
			cxVec nv = poly.mVtx[next & 3] - vtx;
			dist2 = nv.mag2();
			if (dist2 > eps) {
				float nd = ev.dot(nv);
				if (nd >= eps && dist2 >= nd) {
					isect = vtx + nv*(nd / dist2);
					cxVec av = mNewPos - isect;
					dist2 = av.mag2();
					if (dist2 <= rr && dist2 < adist) {
						mov = ::sqrtf(dist2);
						vec = av.get_normalized();
						if (vec.dot(mOldPos - isect) < 0.0f) {
							vec.neg();
						} else {
							mov = -mov;
						}
						adist = dist2;
						flg = true;
					}
				}
			}
			next >>= 4;
		}
		if (flg) {
			mState = 2;
			adj = vec * (r + mov + margin);
		} else {
			return;
		}
	}

	adj.add(mNewPos);
	dist2 = nxVec::dist2(adj, mOldPos);
	if (dist2 > mSqDist) {
		if (mObstState == 1) return;
		if (mState != 1) return;
	}
	mObstState = mState;
	mSqDist = dist2;
	mAdjPos = adj;
	mAdjIdx = poly.mIdx;
}

class cWallRangeFn : public sxGeometryData::RangeFunc {
public:
	int* mpPolIds;
	int mCount;
	int mMaxPolIds;
	float mSlopeLim;

	cWallRangeFn(int* pLst, int lstSize, float slopeLim) : mpPolIds(pLst), mMaxPolIds(lstSize), mSlopeLim(slopeLim), mCount(0) {}

	virtual bool operator()(const sxGeometryData::Polygon& pol) {
		if (!mpPolIds) return false;
		if (mCount >= mMaxPolIds) {
			nxCore::dbg_msg("OBST range query: too many walls.\n");
			return false;
		}
		bool flg = true;
		if (mSlopeLim > 0.0f) {
			int nvtx = pol.get_vtx_num();
			if (nvtx == 3 || nvtx == 4) {
				cxVec nrm = pol.calc_normal();
				flg = ::fabsf(nrm.y) < mSlopeLim;
			} else {
				flg = false;
			}
		}
		if (flg) {
			mpPolIds[mCount++] = pol.get_id();
		}
		return true;
	}
};

bool obstBallToWalls(const cxVec& newPos, const cxVec& oldPos, float radius, const sxGeometryData& geo, cxVec* pAdjPos, sObstGeoWk* pGeoWk, float wallSlopeLim) {
	int npol = geo.get_pol_num();
	if (npol <= 0) return false;
	uint32_t* pStamps = pGeoWk ? pGeoWk->mpStamps : nullptr;
	int stampBytes = obstCalcStampBytes(npol);
	if (!pStamps) {
		pStamps = (uint32_t*)::_alloca(stampBytes);
	}
	if (!pStamps) return false;

	int adjCount = 0;
	float maxRange = radius + nxVec::dist(oldPos, newPos)*2.0f;
	cxAABB range;
	range.from_sph(cxSphere(newPos, maxRange));
	const int maxStkPolys = 128;
	int stkPolList[maxStkPolys];
	int maxPolys = pGeoWk ? pGeoWk->mPolNum : maxStkPolys;
	int* pPolLst = pGeoWk ? pGeoWk->mpPolLst : stkPolList;
	if (!pPolLst) return false;
	cWallRangeFn func(pPolLst, maxPolys, wallSlopeLim);
	geo.range_query(range, func);
	int n = func.mCount;
	if (n <= 0) return false;

	::memset(pStamps, 0, stampBytes);
	sObstBallWk ball(newPos, oldPos, radius, maxRange, pStamps);

	const int itrMax = 15;
	int itrCnt = -1;
	sObstPolyWk poly;
	do {
		++itrCnt;
		for (int i = 0; i < n; ++i) {
			int wkPolIdx = pPolLst[i];
			if (ball.mAdjIdx != wkPolIdx) {
				poly.init(geo, wkPolIdx);
				bool calcFlg = true;
				if (itrCnt > 0) {
					calcFlg = !ball.stamp_ck(wkPolIdx);
				} else {
					float ny = newPos.y;
					if (!poly.in_y_range(ny)) {
						ball.stamp_st(wkPolIdx);
						calcFlg = false;
					}
				}
				if (calcFlg) {
					ball.adjust(poly);
				}
			}
		}
		if (ball.mState) {
			ball.mNewPos = ball.mAdjPos;
			++adjCount;
		}
	} while (ball.mState && itrCnt < itrMax);

	if (pAdjPos) {
		*pAdjPos = ball.mNewPos;
	}

	return adjCount > 0;
}

bool obstBallToBall(const cxVec& newPos, const cxVec& oldPos, float radius, const cxVec& staticPos, float staticRadius, cxVec* pAdjPos, float reflectFactor, float margin) {
	cxVec sepVec;
	cxVec tstPos = newPos;
	cxVec adjPos = tstPos;
	cxVec vel = newPos - oldPos;
	cxSphere stSph(staticPos, staticRadius);
	bool flg = obstSeparateSpheres(cxSphere(newPos, radius), vel, stSph, &sepVec, margin);
	if (flg) {
		cxVec nv = (tstPos + sepVec - staticPos).get_normalized();
		cxVec rv = nxVec::reflect(vel, nv) * reflectFactor;
		adjPos += rv;
		if (cxSphere(adjPos, radius).overlaps(stSph)) {
			adjPos = tstPos + sepVec;
		}
	}
	if (pAdjPos) {
		*pAdjPos = adjPos;
	}
	return flg;
}

bool obstBallToCap(const cxVec& newPos, const cxVec& oldPos, float radius, const cxVec& staticPos0, const cxVec& staticPos1, float staticRadius, cxVec* pAdjPos, float reflectFactor, float margin) {
	cxVec sepVec;
	cxVec axisPnt;
	cxVec tstPos = newPos;
	cxVec adjPos = tstPos;
	cxVec vel = newPos - oldPos;
	cxCapsule stCap(staticPos0, staticPos1, staticRadius);
	bool flg = obstSeparateSphereCapsule(cxSphere(newPos, radius), vel, stCap, &sepVec, &axisPnt, margin);
	if (flg) {
		cxVec nv = (tstPos + sepVec - axisPnt).get_normalized();
		cxVec rv = nxVec::reflect(vel, nv) * reflectFactor;
		if (0) {
			cxVec nrv = rv.get_normalized();
			if (nv.dot(nrv) > 0.99f) {
				rv += cxVec(rv.x < 0.0f ? -1.0f : 1.0f, 0.0f, rv.z < 0.0f ? -1.0f : 1.0f) * 0.001f;
			}
		}
		adjPos += rv;
		if (cxSphere(adjPos, radius).overlaps(stCap)) {
			adjPos = tstPos + sepVec;
		}
	}
	if (pAdjPos) {
		*pAdjPos = adjPos;
	}
	return flg;
}

bool obstPillarToPillar(const cxVec& newPos, const cxVec& oldPos, float radius, float height, const cxVec& staticPos, float staticRadius, float staticHeight, cxVec* pAdjPos, float reflectFactor, float margin) {
	cxVec adjPos = newPos;
	float ymin = newPos.y;
	float ymax = ymin + height;
	float yminSt = staticPos.y;
	float ymaxSt = yminSt + staticHeight;
	bool flg = (ymin <= ymaxSt && ymax >= yminSt);
	if (flg) {
		cxVec sphNewPos = newPos;
		sphNewPos.y = 0.0f;
		cxVec sphOldPos = oldPos;
		sphOldPos.y = 0.0f;
		cxVec capStaticPos0 = staticPos;
		capStaticPos0.y = -1.0f;
		cxVec capStaticPos1 = staticPos;
		capStaticPos1.y = 1.0f;
		cxVec sphAdjPos;
		flg = obstBallToCap(sphNewPos, sphOldPos, radius, capStaticPos0, capStaticPos1, staticRadius, &sphAdjPos, reflectFactor, margin);
		if (flg) {
			adjPos.x = sphAdjPos.x;
			adjPos.z = sphAdjPos.z;
		}
	}
	if (pAdjPos) {
		*pAdjPos = adjPos;
	}
	return flg;
}

bool obstSeparateSpheres(const cxSphere& movSph, const cxVec& vel, const cxSphere& staticSph, cxVec* pSepVec, float margin) {
	cxVec sepVec(0.0f);
	bool flg = movSph.overlaps(staticSph);
	if (flg) {
		float sepDist = movSph.get_radius() + staticSph.get_radius() + margin;
		cxVec sepDir = vel.neg_val();
		cxVec dv = movSph.get_center() - staticSph.get_center();
		cxVec vec = dv + sepDir*dv.mag();
		float len = vec.mag();
		if (len < 1e-5f) {
			vec = sepDir.get_normalized();
		} else {
			sepDist /= len;
		}
		sepVec = vec*sepDist - dv;
	}
	if (pSepVec) {
		*pSepVec = sepVec;
	}
	return flg;
}

bool obstSeparateSphereCapsule(const cxSphere& movSph, const cxVec& vel, const cxCapsule& staticCap, cxVec* pSepVec, cxVec* pAxisPnt, float margin) {
	cxVec sepVec(0.0f);
	cxVec axisPnt(0.0f);
	bool flg = movSph.overlaps(staticCap, &axisPnt);
	if (flg) {
		float sepDist = movSph.get_radius() + staticCap.get_radius() + margin;
		cxVec sepDir = vel.neg_val();
		cxVec dv = movSph.get_center() - axisPnt;
		cxVec vec = dv + sepDir*dv.mag();
		float len = vec.mag();
		if (len < 1e-5f) {
			vec = sepDir.get_normalized();
		} else {
			sepDist /= len;
		}
		sepVec = vec*sepDist - dv;
	}
	if (pSepVec) {
		*pSepVec = sepVec;
	}
	if (pAxisPnt) {
		*pAxisPnt = axisPnt;
	}
	return flg;
}

class cGroundHitFn : public sxGeometryData::HitFunc {
public:
	cxVec mPos;
	cxVec mNrm;
	float mDist;
	int mPolId;
	bool mHitFlg;
	float mSlopeLim;

	cGroundHitFn(float slopeLim) : mSlopeLim(slopeLim), mHitFlg(false), mPolId(-1), mDist(FLT_MAX) {}

	virtual bool operator()(const sxGeometryData::Polygon& pol, const cxVec& hitPos, const cxVec& hitNrm, float hitDist) {
		if (hitNrm.y > mSlopeLim) {
			if (hitDist < mDist) {
				mPos = hitPos;
				mNrm = hitNrm;
				mDist = hitDist;
				mPolId = pol.get_id();
				mHitFlg = true;
			}
		}
		return true;
	}
};

int obstGround(const cxVec& pos, const sxGeometryData& geo, cxVec* pPos, cxVec* pNrm, float offsUp, float offsDown, float slopeLim) {
	int polId = -1;
	cGroundHitFn hitFn(slopeLim);
	cxVec posUp = pos;
	posUp.y += offsUp;
	cxVec posDn = pos;
	posDn.y += offsDown;
	cxLineSeg seg(posUp, posDn);
	geo.hit_query(seg, hitFn);
	if (hitFn.mHitFlg) {
		if (pPos) {
			*pPos = hitFn.mPos;
		}
		if (pNrm) {
			*pNrm = hitFn.mNrm;
		}
		polId = hitFn.mPolId;
	} else {
		if (pPos) {
			*pPos = pos;
		}
		if (pNrm) {
			*pNrm = nxVec::get_axis(exAxis::PLUS_Y);
		}
	}
	return polId;
}

