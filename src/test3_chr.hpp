#define D_KEY_UP 0
#define D_KEY_DOWN 1
#define D_KEY_LEFT 2
#define D_KEY_RIGHT 3
#define D_KEY_L1 4
#define D_KEY_L2 5
#define D_KEY_R1 6
#define D_KEY_R2 7
#define D_KEY_A 8
#define D_KEY_B 9

KEY_CTRL* get_ctrl_keys();
TD_JOYSTICK_CHANNELS* get_td_joy();
float get_anim_speed();
bool stg_hit_ck(const cxVec& pos0, const cxVec& pos1, cxVec* pHitPos, cxVec* pHitNrm);
sxGeometryData* get_stg_obst_geo();

struct TSK_QUEUE;
struct TSK_JOB;
struct TSK_CONTEXT;

class cCharacter3 {
protected:
	enum class STATE : uint8_t {
		DANCE_LOOP = 0,
		DANCE_ACT,
		IDLE,
		TURN,
		WALK,
		RUN
	};

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
	int mMotIdDanceLoop;
	int mMotIdStand;
	int mMotIdWalk;
	int mMotIdRun;
	float mMotVelFrame;
	cxVec mMotVel;

	cxVec mPrevWorldPos;
	cxVec mWorldPos;
	cxVec mWorldRot;

	TSK_JOB* mpMotEvalJobs;
	TSK_QUEUE* mpMotEvalQueue;
	sxKeyframesData::RigLink* mpMotEvalLink;
	sxKeyframesData* mpMotEvalKfr;
	float mMotEvalFrame;

	bool mInitFlg;

	STATE mStateMain;
	uint8_t mStateSub;
	int mStateParams[4];
	float mStateParamsF[4];

	typedef void (cCharacter3::*StateFunc)();

	void blend_init(int duration);
	void calc_blend();
	float calc_motion(int motId, float frame, float frameStep);
	void update_coord();
	void calc_world();

	void dance_loop_ctrl();
	void dance_loop_exec();
	void dance_act_ctrl();
	void dance_act_exec();

	void idle_ctrl();
	void idle_exec();
	void turn_ctrl();
	void turn_exec();
	void walk_ctrl();
	void walk_exec();
	void run_ctrl();
	void run_exec();

	void trans_to_state(STATE st, int subSt = 0, float frame = 0.0f) {
		mMotFrame = frame;
		mStateMain = st;
		mStateSub = subSt;
	}

	bool wall_adj_line();
	bool wall_adj_ball();
	bool wall_adj();

	bool prop_adj();

	static void mot_node_eval_job(TSK_CONTEXT* pCtx);

public:
	cCharacter3()
	: mInitFlg(false),
	mpObj(nullptr), mpTexB(nullptr), mpTexS(nullptr), mpTexN(nullptr), mpRig(nullptr),
	mpRigMtxL(nullptr), mpRigMtxW(nullptr), mpObjMtxW(nullptr), mpBlendMtxL(nullptr), mpObjToRig(nullptr),
	mSkinNodesNum(0), mRigNodesNum(0), mRootNodeId(-1), mMovementNodeId(-1),
	mpMotEvalJobs(nullptr), mpMotEvalQueue(nullptr), mpMotEvalLink(nullptr), mpMotEvalKfr(nullptr), mMotEvalFrame(0.0f),
	mStateMain(STATE::DANCE_LOOP), mStateSub(0),
	mMotFrame(0.0f), mBlendDuration(0.0f), mBlendCount(0.0f), mMotVel(0.0f),
	mPrevWorldPos(0.0f), mWorldPos(0.0f), mWorldRot(0.0f)
	{}

	void create();
	void destroy();

	void update();

	void disp(GEX_LIT* pLit = nullptr);


	cxMtx get_movement_mtx() const;
	cxVec get_world_pos() const { return mWorldPos; }
};
