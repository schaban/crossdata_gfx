float get_anim_speed();

class cTest4Character {
protected:
	GEX_OBJ* mpObj;
	GEX_TEX* mpTexB;
	GEX_TEX* mpTexS;
	GEX_TEX* mpTexN;
	sxRigData* mpRig;
	cxMtx* mpRigMtxL;
	cxMtx* mpRigMtxW;
	cxMtx* mpObjMtxW;
	cxMtx* mpBlendMtxL;
	int* mpObjToRig;
	int mRigNodesNum;
	int mSkinNodesNum;
	int mRootNodeId;
	int mMovementNodeId;

	float mBlendDuration;
	float mBlendCount;

	MOTION_LIB mMotLib;
	float mMotFrame;
	int mMotIdHandsDefault;
	int mMotIdStand;
	int mMotIdWalk;
	int mMotIdRun;
	float mMotVelFrame;
	cxVec mMotVel;

	cxVec mPrevWorldPos;
	cxVec mWorldPos;
	cxVec mWorldRot;

	bool mInitFlg;

	void blend_init(int duration);
	void calc_blend();
	float calc_motion(int motId, float frame, float frameStep);
	void update_coord();
	void calc_world();

public:
	cTest4Character()
	: mInitFlg(false),
	mpObj(nullptr), mpTexB(nullptr), mpTexS(nullptr), mpTexN(nullptr), mpRig(nullptr),
	mpRigMtxL(nullptr), mpRigMtxW(nullptr), mpObjMtxW(nullptr), mpBlendMtxL(nullptr), mpObjToRig(nullptr),
	mSkinNodesNum(0), mRigNodesNum(0), mRootNodeId(-1), mMovementNodeId(-1),
	mMotFrame(0.0f), mBlendDuration(0.0f), mBlendCount(0.0f), mMotVel(0.0f),
	mPrevWorldPos(0.0f), mWorldPos(0.0f), mWorldRot(0.0f)
	{}

	void create();
	void destroy();

	void update();

	void disp(GEX_LIT* pLit = nullptr);

	cxVec get_world_pos() const { return mWorldPos; }
};


class cTest4Panda {
protected:
	GEX_OBJ* mpObj;
	sxRigData* mpRig;
	TEXTURE_LIB mTexLib;
	MOTION_LIB mMotLib;

	cxMtx* mpRigMtxL;
	cxMtx* mpRigMtxW;
	cxMtx* mpObjMtxW;
	cxMtx* mpBlendMtxL;
	int* mpObjToRig;
	int mRigNodesNum;
	int mSkinNodesNum;
	int mRootNodeId;
	int mMovementNodeId;

	float mMotFrame;
	float mBlendDuration;
	float mBlendCount;
	float mMotVelFrame;
	cxVec mMotVel;

	cxVec mPrevWorldPos;
	cxVec mWorldPos;
	cxVec mWorldRot;

	int mState;
	int mCount;

	bool mInitFlg;

	void blend_init(int duration);
	void calc_blend();
	float calc_motion(int motId, float frame, float frameStep);
	void update_coord();
	void calc_world();

public:
	cTest4Panda() : mpObj(nullptr), mpRig(nullptr), mInitFlg(false), mState(0), mCount(0) {}

	void create();
	void destroy();

	void update();
	void disp(GEX_LIT* pLit = nullptr);
};

