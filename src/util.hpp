#define DATA_PATH(_fname) "../data/"##_fname

void util_init();
void util_reset();

struct TD_CAM_INFO {
	float mX, mY, mZ;
	float mRotX, mRotY, mRotZ;
	float mPivotX, mPivotY, mPivotZ;
	float mXOrd;
	float mROrd;
	float mFOVType;
	float mFOV;
	float mFocal;
	float mAperture;
	float mNear;
	float mFar;

	exTransformOrd xord() const {
		int i = (int)mXOrd;
		if (i >= 0 && i <= 5) return (exTransformOrd)i;
		return exTransformOrd::SRT;
	}

	exRotOrd rord() const {
		int i = (int)mROrd;
		if (i >= 0 && i <= 5) return (exRotOrd)i;
		return exRotOrd::XYZ;
	}

	cxMtx calc_xform() const;
	void calc_pos_tgt(cxVec* pPos, cxVec* pTgt, float dist = 0.0f) const;
};

struct CAM_INFO {
	cxVec mPos;
	cxQuat mQuat;
	cxVec mUp;
	float mFOVY;
	float mNear;
	float mFar;

	void set_defaults() {
		mPos.set(3.0f, 1.5f, 3.0f);
		mQuat.set_rot_degrees(cxVec(-10.0f, 45.0f, 0.0f));
		mUp = nxVec::get_axis(exAxis::PLUS_Y);
		mFOVY = XD_DEG2RAD(40.0f);
		mNear = 0.001f;
		mFar = 1000.0f;
	}

	cxVec get_dir() const {
		return mQuat.apply(nxVec::get_axis(exAxis::MINUS_Z));
	}
};

struct MOTION_LIB {
	sxFileCatalogue* mpCat;
	sxKeyframesData** mppKfrs;
	sxKeyframesData::RigLink** mppRigLinks;

	MOTION_LIB() : mpCat(nullptr), mppKfrs(nullptr), mppRigLinks(nullptr) {}

	void init(const char* pBasePath, const sxRigData& rig);
	void reset();
	int get_motions_num() const { return mpCat ? mpCat->mFilesNum : 0; }
	int find_motion(const char* pName) const { return mpCat ? mpCat->find_name_idx(pName) : -1; };
	sxKeyframesData* get_keyframes(int idx) const { return mpCat && mppKfrs ? (mpCat->ck_file_idx(idx) ? mppKfrs[idx] : nullptr) : nullptr; }
	sxKeyframesData::RigLink* get_riglink(int idx) const { return mpCat && mppRigLinks ? (mpCat->ck_file_idx(idx) ? mppRigLinks[idx] : nullptr) : nullptr; }
};

struct TEXTURE_LIB {
	sxFileCatalogue* mpCat;
	GEX_TEX** mppTexs;

	TEXTURE_LIB() : mpCat(nullptr), mppTexs(nullptr) {}

	void init(const char* pBasePath);
	void reset();
	int get_tex_num() const { return mpCat ? mpCat->mFilesNum : 0; }
};

struct TEXDATA_LIB {
	sxFileCatalogue* mpCat;
	sxTextureData** mppTexs;

	TEXDATA_LIB() : mpCat(nullptr), mppTexs(nullptr) {}

	void init(const char* pBasePath);
	void reset();
	int get_tex_num() const { return mpCat ? mpCat->mFilesNum : 0; }
};

struct TRACKBALL {
	cxQuat mSpin;
	cxQuat mQuat;
	float mRadius;

	void init() {
		mRadius = 0.5f;
		reset();
	}

	void reset() {
		mSpin.identity();
		mQuat.identity();
	}

	void update(float x0, float y0, float x1, float y1);
};

struct SH_COEFS {
	static const int ORDER = 6;
	static const int NCOEF = ORDER*ORDER;
	float mR[NCOEF];
	float mG[NCOEF];
	float mB[NCOEF];
	float mWgtDiff[ORDER];
	float mWgtRefl[ORDER];

	SH_COEFS();

	void clear();
	void from_geo(sxGeometryData& geo, float scl = 1.0f);
	void from_pano(const cxColor* pClr, int w, int h);

	float* get_channel(int i) {
		float* pData = nullptr;
		switch (i) {
			case 0: pData = mR; break;
			case 1: pData = mG; break;
			case 2: pData = mB; break;
		}
		return pData;
	}

	cxColor get_ambient() const {
		return cxColor(mR[0], mG[0], mB[0]);
	}

	cxVec calc_dominant_dir() const {
		int idx = nxSH::calc_ary_idx(1, 1);
		float lx = cxColor(mR[idx], mG[idx], mB[idx]).luminance();
		idx = nxSH::calc_ary_idx(1, -1);
		float ly = cxColor(mR[idx], mG[idx], mB[idx]).luminance();
		idx = nxSH::calc_ary_idx(1, 0);
		float lz = cxColor(mR[idx], mG[idx], mB[idx]).luminance();
		return cxVec(-lx, -ly, lz).get_normalized();
	}

	cxColor calc_color(const cxVec dir) const;

	void calc_diff_wgt(float s, float scl) { nxSH::calc_weights(mWgtDiff, ORDER, s, scl); }
	void calc_refl_wgt(float s, float scl) { nxSH::calc_weights(mWgtRefl, ORDER, s, scl); }

	void scl(const cxColor& clr);
};

struct ENV_LIGHT {
	SH_COEFS mSH;
	const sxTextureData* mpTex;
	cxVec mDominantDir;
	cxColor mDominantClr;
	float mDominantLum;
	cxColor mAmbientClr;
	float mAmbientLum;

	ENV_LIGHT() : mpTex(nullptr) {}

	void init(const sxTextureData* pTex);
	void apply(GEX_LIT* pLit, float diffRate = 0.0f, float specRate = 0.75f);
};

TRACKBALL* get_trackball();
float get_wheel_val();

int get_test_mode();
int get_max_workers();
void con_locate(int x, int y);
void con_text_color(const cxColor& clr = cxColor(0.5f));

struct TSK_BRIGADE;
TSK_BRIGADE* get_brigade();

template<typename T> inline T approach(const T& val, const T& dst, int time) {
	float flen = (float)(time < 1 ? 1 : time);
	return (val*(time - 1.0f) + dst) * (1.0f / time);
}

float frand01();
float frand11();
float cvt_roughness(float rough);
cxQuat get_val_grp_quat(sxValuesData::Group& grp);
cxVec vec_rot_deg(const cxVec& vsrc, float angX, float angY, float angZ, exRotOrd rord = exRotOrd::XYZ);
GEX_SPEC_MODE parse_spec_mode(const char* pMode);
cxVec eval_pos(const sxKeyframesData& kfr, const char* pNodeName, float frame);
cxVec eval_rot(const sxKeyframesData& kfr, const char* pNodeName, float frame);
GEX_LIT* make_lights(const sxValuesData& vals, cxVec* pDominantDir = nullptr);
GEX_LIT* make_const_lit(const char* pName = nullptr, float val = 1.0f);
void init_materials(GEX_OBJ& obj, const sxValuesData& vals, bool useReflectColor = false);
CAM_INFO get_cam_info(const sxValuesData& vals, const char* pCamName);
TD_CAM_INFO parse_td_cam(const char* pData, size_t dataSize);
TD_CAM_INFO load_td_cam(const char* pPath);
void obj_shadow_mode(const GEX_OBJ& obj, bool castShadows, bool receiveShadows);
void obj_shadow_params(const GEX_OBJ& obj, float density, float selfShadowFactor, bool cullShadows);
void obj_tesselation(const GEX_OBJ& obj, GEX_TESS_MODE mode, float factor);
void obj_diff_roughness(const GEX_OBJ& obj, const cxColor& rgb);
void obj_diff_mode(const GEX_OBJ& obj, GEX_DIFF_MODE mode);
void obj_spec_mode(const GEX_OBJ& obj, GEX_SPEC_MODE mode);
void obj_sort_mode(const GEX_OBJ& obj, GEX_SORT_MODE mode);
void obj_sort_bias(const GEX_OBJ& obj, float absBias, float relBias);
void mtl_sort_mode(const GEX_OBJ& obj, const char* pMtlName, GEX_SORT_MODE mode);
void mtl_sort_bias(const GEX_OBJ& obj, const char* pMtlName, float absBias, float relBias);
void mtl_sh_diff_detail(const GEX_OBJ& obj, const char* pMtlName, float dtl);
void mtl_sh_refl_detail(const GEX_OBJ& obj, const char* pMtlName, float dtl);

void dump_riglink_info(sxKeyframesData* pKfr, sxKeyframesData::RigLink* pLink, FILE* pOut = nullptr);
