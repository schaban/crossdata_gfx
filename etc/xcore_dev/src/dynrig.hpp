struct Ponytail {
	struct Adj {
		cxSphere mSph;
		int mJntId;
	};

	struct Node {
		cxMtx mWMtx;
		cxMtx mPrevWMtx;
		cxVec mPos;
		cxVec mPrevPos;
		cxVec mAdd;
		float mMaxDist;
		float mForce;
		float mRate;
		float mLimitTheta;
		float mLimitPhi;
		int mJntId;
		uint32_t mAttr;
	};

	ScnObj* mpObj;
	Node mNodes[12];
	Adj mAdjs[8];
	int mNumNodes;
	int mNumAdjs;

	void init(ScnObj* pObj, const char* pPrefix = nullptr);
	void reset_nodes();
	void move();
};

struct LegInfo {
	float effY;
	float effOffsX;
	float effOffsZ;
	int inodeTop;
	int inodeRot;
	int inodeEnd;
	int inodeExt;
	int inodeEff;

	void init(const ScnObj* pObj, const char side, const bool ext);
};

namespace DynRig {

void calc_forearm_twist(ScnObj* pObj, const int wristId, const int forearmId, const float wristInfluence = 0.5f);
void calc_forearm_twist(ScnObj* pObj, const char* pWristName, const char* pForearmName, const float wristInfluence = 0.5f);
void calc_forearm_twist_l(ScnObj* pObj, const float wristInfluence = 0.5f);
void calc_forearm_twist_r(ScnObj* pObj, const float wristInfluence = 0.5f);

void reset_shoulder_rot(ScnObj* pObj, const int shoulderId);
void reset_shoulder_rot(ScnObj* pObj, const char* pName);
void reset_shoulder_rot_l(ScnObj* pObj);
void reset_shoulder_rot_r(ScnObj* pObj);

void calc_shoulder_axis_rot(ScnObj* pObj, const int shoulderJntId, const int shoulderId, const float shoulderJntInfluence, const int axisIdx, const bool useQuats = false);
void calc_shoulder_axis_rot(ScnObj* pObj, const char* pShoulderJntName, const char* pShoulderName, const float shoulderJntInfluence, const int axisIdx);
void calc_shoulder_axis_rot_l(ScnObj* pObj, const float shoulderJntInfluence, const int axisIdx);
void calc_shoulder_axis_rot_r(ScnObj* pObj, const float shoulderJntInfluence, const int axisIdx);

void calc_eyelids_blink(ScnObj* pObj, const float yopen, const float yclosed, const float t, const float p1 = 0.65f, const float p2 = 0.95f);

void adjust_leg(ScnObj* pObj, sxCollisionData* pCol, LegInfo* pLeg);

} // DynRig

