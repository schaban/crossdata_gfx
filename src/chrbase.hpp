class cBaseRig {
public:
	enum eMoveMode {
		CONSTANT,
		FCURVES
	};

	sxRigData* mpData;
	cxMtx* mpMtxL;
	cxMtx* mpMtxW;
	cxMtx* mpPrevMtxW;
	cxMtx* mpMtxBlendL;
	cxVec mMoveVel;
	cxVec mConstMoveVel;
	cxVec mPrevWorldPos;
	cxVec mWorldPos;
	cxVec mWorldRot;
	int mNodesNum;
	int mRootNodeId;
	int mMovementNodeId;
	eMoveMode mMoveMode;

protected:
	float mBlendDuration;
	float mBlendCount;
	float mAnimFrame;
	bool mOwnData;

	void save_prev_w() {
		if (mpMtxW && mpPrevMtxW) {
			::memcpy(mpPrevMtxW, mpMtxW, mNodesNum * sizeof(cxMtx));
		}
	}
public:
	cBaseRig()
	:
	mpData(nullptr),
	mpMtxL(nullptr), mpMtxW(nullptr),
	mpPrevMtxW(nullptr), mpMtxBlendL(nullptr),
	mMoveMode(eMoveMode::FCURVES),
	mMoveVel(0.0f), mConstMoveVel(0.0f),
	mPrevWorldPos(0.0f), mWorldPos(0.0f), mWorldRot(0.0f),
	mNodesNum(0), mRootNodeId(-1), mMovementNodeId(-1),
	mBlendDuration(0), mBlendCount(0), mAnimFrame(0),
	mOwnData(false) {}

	virtual ~cBaseRig() { unload(); }

	bool is_valid() const { return mpData != nullptr; }

	void load(const char* pDataPath);
	void unload();

	virtual void init(sxRigData* pRigData);
	virtual void reset();

	virtual float animate(sxKeyframesData* pKfr, sxKeyframesData::RigLink* pLnk, float frameNow, float frameAdd, bool* pLoopFlg = nullptr);

	void blend_init(int duration);
	void blend_exec();

	void update_coord();
	void calc_world();

	sxRigData* get_data() const { return mpData; }

	void print_local() const;
};

class cHumanoidRig : public cBaseRig {
public:
	struct {
		sxRigData::LimbInfo* mpInfo;
		sxRigData::LimbChain mChain;
		sxRigData::LimbChain::Solution mSolution;
	} mLimbs[4];

protected:
	void reset_limbs() {
		for (int i = 0; i < 4; ++i) {
			mLimbs[i].mpInfo = nullptr;
		}
	}

public:
	cHumanoidRig() {
		reset_limbs();
	}

	virtual void init(sxRigData* pRigData);
	virtual void reset();
	virtual float animate(sxKeyframesData* pKfr, sxKeyframesData::RigLink* pLnk, float frameNow, float frameAdd, bool* pLoopFlg = nullptr);
};

class cSkinGeo {
public:
	GEX_OBJ* mpDispObj;
	cBaseRig* mpRig;
	int mSkinNodesNum;
	cxMtx* mpObjMtxW;
	int* mpObjToRig;

public:
	cSkinGeo() {}
	~cSkinGeo() { reset(); }

	bool is_valid() const { return mpDispObj != nullptr; }

	void init(sxGeometryData* pGeoData, cBaseRig* pRig, const char* pBatchGrpPrefix = nullptr);
	void reset();

	void update_world();
	void disp(GEX_LIT* pLit = nullptr);
};

class cHumanoid {
protected:
	cHumanoidRig* mpRig;
	cSkinGeo mSkin;
	cHumanoidRig mInternalRig;
	bool mOwnData;

public:
	cHumanoid() : mpRig(nullptr), mOwnData(false) {}
	~cHumanoid() { reset(); }

	bool is_valid() const { return mpRig != nullptr && mpRig->is_valid() && mSkin.is_valid(); }

	void load(const char* pGeoPath, const char* pRigPath, const char* pMtlPath, const char* pBatchGrpPrefix = nullptr);
	void unload() { reset(); }

	void init(sxGeometryData* pGeoData, cHumanoidRig* pRig, sxValuesData* pMtlData, const char* pBatchGrpPrefix = nullptr);
	void reset();

	GEX_OBJ* get_disp_obj() const { return mSkin.mpDispObj; }
	cHumanoidRig* get_rig() const { return mpRig; }

	float animate(sxKeyframesData* pKfr, sxKeyframesData::RigLink* pLnk, float frameNow, float frameAdd);
	void disp(GEX_LIT* pLit = nullptr);
};
