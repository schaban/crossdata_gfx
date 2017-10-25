class cBaseRig {
public:
	enum eMoveMode {
		CONSTANT,
		FCURVES
	};

	struct NodeStatus {
		uint16_t tx : 1;
		uint16_t ty : 1;
		uint16_t tz : 1;
		uint16_t rx : 1;
		uint16_t ry : 1;
		uint16_t rz : 1;
		uint16_t sx : 1;
		uint16_t sy : 1;
		uint16_t sz : 1;

		void clear() {
			*((uint16_t*)this) = 0;
		}

		bool any_t() const { return tx || ty || tz; }
		bool any_r() const { return rx || ry || rz; }
		bool any_s() const { return sx || sy || sz; }
		bool any() const { return any_t() || any_r() || any_s(); }

		bool all_t() const { return tx && ty && tz; }
		bool all_r() const { return rx && ry && rz; }

		bool ck(exAnimChan ch);
	};

	struct NodeParams {
		cxVec mPos;
		cxVec mRot;
		cxVec mScl;
		NodeStatus mAnimStatus;
		NodeStatus mExprStatus;

		float get_ch(exAnimChan ch);
		void update_anim_ch(exAnimChan ch, float val);
		void update_expr_ch(exAnimChan ch, float val);
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

	struct ExprChInfo {
		int16_t mNodeId;
		exAnimChan mChanId;

		ExprChInfo() : mNodeId(-1), mChanId(exAnimChan::UNKNOWN) {}
	};

	class ExprCtx : public sxCompiledExpression::ExecIfc {
	private:
		cBaseRig* mpRig;
		sxRigData::ExprInfo** mppInfo;
		sxCompiledExpression::Stack mStack;
		int mExprNum;
		float mRes;
	public:
		ExprCtx() : mppInfo(nullptr), mExprNum(0), mRes(0.0f) {}

		void init(cBaseRig& rig);
		void reset();

		bool ck_expr_idx(int idx) const { return (uint32_t)idx < (uint32_t)mExprNum; }
		int get_expr_num() const { return mExprNum; }
		sxRigData::ExprInfo* get_expr_info(int idx) { return (mppInfo && ck_expr_idx(idx)) ? mppInfo[idx] : nullptr; }
		sxCompiledExpression::Stack* get_stack() { return &mStack; }
		void set_result(float val) { mRes = val; }
		float get_result() const { return mRes; }
		float ch(const sxCompiledExpression::String& path);
	};

	ExprCtx mExprCtx;
	NodeParams* mpParams;

	float mBlendDuration;
	float mBlendCount;
	float mAnimFrame;
	bool mOwnData;

	void save_prev_w() {
		if (mpMtxW && mpPrevMtxW) {
			::memcpy(mpPrevMtxW, mpMtxW, mNodesNum * sizeof(cxMtx));
		}
	}

	void clear_anim_status();
	void clear_expr_status();

	ExprChInfo parse_ch_path(const sxCompiledExpression::String& path) const;
	float eval_ch(ExprChInfo chi) const;
	void exec_exprs();

public:
	cBaseRig()
	:
	mpData(nullptr),
	mpParams(nullptr),
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

	virtual float animate(sxKeyframesData* pKfr, sxKeyframesData::RigLink* pLnk, float frameNow, float frameAdd, bool* pLoopFlg = nullptr, bool evalExprs = true);

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
	virtual float animate(sxKeyframesData* pKfr, sxKeyframesData::RigLink* pLnk, float frameNow, float frameAdd, bool* pLoopFlg = nullptr, bool evalExprs = true);
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
