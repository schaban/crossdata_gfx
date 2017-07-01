class cChrBase {
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

	bool mInitFlg;

	void blend_init(int duration);
	void calc_blend();
	float calc_motion(int motId, float frame, float frameStep);
	void update_coord();
	void calc_world();

public:
	cChrBase()
	: mInitFlg(false),
	mpObj(nullptr), mpRig(nullptr),
	mpRigMtxL(nullptr), mpRigMtxW(nullptr), mpObjMtxW(nullptr), mpBlendMtxL(nullptr), mpObjToRig(nullptr),
	mSkinNodesNum(0), mRigNodesNum(0), mRootNodeId(-1), mMovementNodeId(-1),
	mMotFrame(0.0f), mBlendDuration(0.0f), mBlendCount(0.0f), mMotVel(0.0f),
	mPrevWorldPos(0.0f), mWorldPos(0.0f), mWorldRot(0.0f)
	{}
};

