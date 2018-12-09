#include "crossdata.hpp"
#include "gex.hpp"

#include "util.hpp"

class cExprCtx : public sxCompiledExpression::ExecIfc {
protected:
	sxCompiledExpression::Stack mStk;

public:
	float mRes;
	float mRoughness;
public:
	cExprCtx() {}

	void init() {
		mStk.alloc(128);
		mRes = 0.0f;
		mRoughness = 0.0f;
	}
	void reset() {
		mStk.free();
	}

	sxCompiledExpression::Stack* get_stack() { return &mStk; }
	void set_result(float val) { mRes = val; }
	float ch(const sxCompiledExpression::String& path) {
		float res = 0.0f;
		if (path.is_valid()) {
			if (nxCore::str_eq("rough", path.mpChars)) {
				res = mRoughness;
			}
		}
		return res;
	}
};

static struct _WK {
	sxExprLibData* pLib;
	cExprCtx ctx;
	int eidRoughCvt;

	void clear() {
		pLib = nullptr;
		eidRoughCvt = -1;
	}
} WK;

void util_init() {
	WK.clear();
	const char* pLibPath = DATA_PATH("exprlib.xcel");
	sxData* pData = nxData::load(pLibPath);
	if (!pData) return;
	WK.pLib = pData->as<sxExprLibData>();
	if (!WK.pLib) return;
	WK.ctx.init();
	WK.eidRoughCvt = WK.pLib->find_expr_idx("EXPRLIB", "roughCvt");
}

void util_reset() {
	WK.ctx.reset();
	nxData::unload(WK.pLib);
	WK.clear();
}

cxMtx TD_CAM_INFO::calc_xform() const {
	cxMtx tns = nxMtx::mk_translation(cxVec(mX, mY, mZ));
	cxMtx scl = nxMtx::mk_scl(1.0f);
	cxMtx rot;
	rot.set_rot_degrees(cxVec(mRotX, mRotY, mRotZ), rord());
	cxMtx xform;
	xform.calc_xform(tns, rot, scl, xord());
	cxVec piv(mPivotX, mPivotY, mPivotZ);
	xform = nxMtx::mk_translation(piv.neg_val()) * xform * nxMtx::mk_translation(piv);
	return xform;
}

void TD_CAM_INFO::calc_pos_tgt(cxVec* pPos, cxVec* pTgt, float dist) const {
	if (dist == 0.0f) {
		dist = mFar - mNear;
	}
	cxMtx xform = calc_xform();
	cxVec pos = xform.get_translation();
	cxVec dir = xform.get_row_vec(2).neg_val();
	cxVec tgt = pos + dir*dist;
	if (pPos) {
		*pPos = pos;
	}
	if (pTgt) {
		*pTgt = tgt;
	}
}

void MOTION_LIB::init(const char* pBasePath, const sxRigData& rig) {
	bool dbgInfo = true;
	if (!pBasePath) return;
	char path[XD_MAX_PATH];
	XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/motlib.fcat", pBasePath);
	sxData* pData = nxData::load(path);
	if (!pData) return;
	mpCat = pData->as<sxFileCatalogue>();
	if (!mpCat) return;
	int nmot = get_motions_num();
	if (nmot <= 0) return;
	if (dbgInfo) nxCore::dbg_msg("Motion Lib @ %s\n", pBasePath);
	mppKfrs = (sxKeyframesData**)nxCore::mem_alloc(nmot * sizeof(sxKeyframesData*), XD_FOURCC('M', 'K', 'f', 'r'));
	if (!mppKfrs) return;
	mppRigLinks = (sxKeyframesData::RigLink**)nxCore::mem_alloc(nmot * sizeof(sxKeyframesData::RigLink*), XD_FOURCC('M', 'R', 'i', 'g'));
	if (!mppRigLinks) return;
	int upperBodyRootIdx = rig.find_node("j_Spine");
	int lowerBodyRootIdx = rig.find_node("j_Pelvis");
	int armRootIdL = rig.find_node("j_Shoulder_L");
	int armRootIdR = rig.find_node("j_Shoulder_R");
	int slerpNodes[] = {
		upperBodyRootIdx,
		lowerBodyRootIdx,
		armRootIdL,
		armRootIdR
	};
	for (int i = 0; i < nmot; ++i) {
		const char* pKfrFileName = mpCat->get_file_name(i);
		XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/%s", pBasePath, pKfrFileName);
		pData = nxData::load(path);
		if (pData) {
			mppKfrs[i] = pData->as<sxKeyframesData>();
			if (mppKfrs[i]) {
				if (dbgInfo) {
					nxCore::dbg_msg("[%d]: %s, #frames=%d\n", i, mpCat->get_name(i), mppKfrs[i]->get_frame_count());
				}
				mppRigLinks[i] = mppKfrs[i]->make_rig_link(rig);
				if (mppRigLinks[i]) {
					for (int j = 0; j < XD_ARY_LEN(slerpNodes); ++j) {
						if (rig.ck_node_idx(slerpNodes[j])) {
							int linkIdx = mppRigLinks[i]->get_rig_map()[slerpNodes[j]];
							if (linkIdx >= 0) {
								mppRigLinks[i]->mNodes[linkIdx].mUseSlerp = true;
							}
						}
					}
				}
			}
		}
	}
}

void MOTION_LIB::reset() {
	if (!mpCat) return;
	int nmot = get_motions_num();
	for (int i = 0; i < nmot; ++i) {
		if (mppKfrs && mppKfrs[i]) {
			nxData::unload(mppKfrs[i]);
		}
		if (mppRigLinks && mppRigLinks[i]) {
			nxCore::mem_free(mppRigLinks[i]);
		}
	}
	if (mppKfrs) {
		nxCore::mem_free(mppKfrs);
		mppKfrs = nullptr;
	}
	if (mppRigLinks) {
		nxCore::mem_free(mppRigLinks);
		mppRigLinks = nullptr;
	}
	nxData::unload(mpCat);
	mpCat = nullptr;
}


void TEXTURE_LIB::init(const char* pBasePath) {
	if (!pBasePath) return;
	char path[XD_MAX_PATH];
	XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/texlib.fcat", pBasePath);
	sxData* pData = nxData::load(path);
	if (!pData) return;
	mpCat = pData->as<sxFileCatalogue>();
	if (!mpCat) return;
	int ntex = mpCat->mFilesNum;
	size_t memsize = ntex * sizeof(GEX_TEX*);
	mppTexs = (GEX_TEX**)nxCore::mem_alloc(memsize, XD_FOURCC('T', 'L', 'i', 'b'));
	if (!mppTexs) return;
	::memset(mppTexs, 0, memsize);
	for (int i = 0; i < ntex; ++i) {
		mppTexs[i] = nullptr;
		const char* pTexFileName = mpCat->get_file_name(i);
		XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/%s", pBasePath, pTexFileName);
		pData = nxData::load(path);
		if (pData) {
			sxTextureData* pTex = pData->as<sxTextureData>();
			if (pTex) {
				mppTexs[i] = gexTexCreate(*pTex);
			}
			nxData::unload(pData);
		}
	}
}

void TEXTURE_LIB::reset() {
	if (mpCat && mppTexs) {
		int ntex = mpCat->mFilesNum;
		for (int i = 0; i < ntex; ++i) {
			gexTexDestroy(mppTexs[i]);
		}
	}
	nxCore::mem_free(mppTexs);
	mppTexs = nullptr;
	nxData::unload(mpCat);
	mpCat = nullptr;
}


void TEXDATA_LIB::init(const char* pBasePath) {
	if (!pBasePath) return;
	char path[XD_MAX_PATH];
	XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/texlib.fcat", pBasePath);
	sxData* pData = nxData::load(path);
	if (!pData) return;
	mpCat = pData->as<sxFileCatalogue>();
	if (!mpCat) return;
	int ntex = mpCat->mFilesNum;
	size_t memsize = ntex * sizeof(sxTextureData*);
	mppTexs = (sxTextureData**)nxCore::mem_alloc(memsize, XD_FOURCC('T', 'X', 'l', 'b'));
	if (!mppTexs) return;
	::memset(mppTexs, 0, memsize);
	for (int i = 0; i < ntex; ++i) {
		mppTexs[i] = nullptr;
		const char* pTexFileName = mpCat->get_file_name(i);
		XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/%s", pBasePath, pTexFileName);
		pData = nxData::load(path);
		if (pData) {
			sxTextureData* pTex = pData->as<sxTextureData>();
			if (pTex) {
				mppTexs[i] = pTex;
			}
		}
	}
}

void TEXDATA_LIB::reset() {
	if (mpCat && mppTexs) {
		int ntex = mpCat->mFilesNum;
		for (int i = 0; i < ntex; ++i) {
			nxData::unload(mppTexs[i]);
		}
	}
	nxCore::mem_free(mppTexs);
	mppTexs = nullptr;
	nxData::unload(mpCat);
	mpCat = nullptr;
}

static cxVec tball_proj(float x, float y, float r) {
	float d = ::hypotf(x, y);
	float t = r / ::sqrtf(2.0f);
	float z;
	if (d < t) {
		z = ::sqrtf(r*r - d*d);
	} else {
		z = (t*t) / d;
	}
	return cxVec(x, y, z);
}

void TRACKBALL::update(float x0, float y0, float x1, float y1) {
	cxVec tp0 = tball_proj(x0, y0, mRadius);
	cxVec tp1 = tball_proj(x1, y1, mRadius);
	cxVec dir = tp0 - tp1;
	cxVec axis = nxVec::cross(tp1, tp0);
	float t = nxCalc::clamp(dir.mag() / (2.0f*mRadius), -1.0f, 1.0f);
	float ang = 2.0f * ::asinf(t);
	mSpin.set_rot(axis, ang);
	mQuat.mul(mSpin);
	mQuat.normalize();
}


SH_COEFS::SH_COEFS() {
	clear();
	calc_diff_wgt(4.0f, 1.0f);
	calc_refl_wgt(8.0f, 0.025f);
}

void SH_COEFS::clear() {
	for (int i = 0; i < NCOEF; ++i) {
		mR[i] = 0.0f;
		mG[i] = 0.0f;
		mB[i] = 0.0f;
	}
	for (int i = 0; i < ORDER; ++i) {
		mWgtDiff[i] = 0.0f;
		mWgtRefl[i] = 0.0f;
	}
}

void SH_COEFS::from_geo(sxGeometryData& geo, float scl) {
	char name[32];
	for (int ch = 0; ch < 3; ++ch) {
		float* pCh = get_channel(ch);
		char chName = "rgb"[ch];
		for (int i = 0; i < NCOEF; ++i) {
			float val = 0.0f;
			XD_SPRINTF(XD_SPRINTF_BUF(name, sizeof(name)), "SHC_%c%d", chName, i);
			int attrIdx = geo.find_glb_attr(name);
			float* pVal = geo.get_attr_data_f(attrIdx, sxGeometryData::eAttrClass::GLOBAL, 0);
			if (pVal) {
				val = *pVal;
			}
			val *= scl;
			pCh[i] = val;
		}
	}
}

static struct SH {
	float consts[nxSH::calc_consts_num(SH_COEFS::ORDER)];

	SH() {
		nxSH::calc_consts(SH_COEFS::ORDER, consts);
	}
} SH;

void SH_COEFS::from_pano(const cxColor* pClr, int w, int h) {
	struct PANO_SH_FN {
		const cxColor* pPano;
		int w, h;
		SH_COEFS* pCoefs;
		XD_FORCEINLINE void operator ()(int x, int y, float dx, float dy, float dz, float dw) {
			float dirCoefs[NCOEF];
			nxSH::eval(ORDER, dirCoefs, dx, dy, dz, SH.consts);
			cxColor c = pPano[y*w + x];
			for (int i = 0; i < NCOEF; ++i) {
				dirCoefs[i] *= dw;
			}
			float r = c.r;
			for (int i = 0; i < NCOEF; ++i) {
				pCoefs->mR[i] += dirCoefs[i] * r;
			}
			float g = c.g;
			for (int i = 0; i < NCOEF; ++i) {
				pCoefs->mG[i] += dirCoefs[i] * g;
			}
			float b = c.b;
			for (int i = 0; i < NCOEF; ++i) {
				pCoefs->mB[i] += dirCoefs[i] * b;
			}
		}
	} shf = { pClr, w, h, this };
	clear();
	float wscl = nxCalc::panorama_scan(shf, shf.w, shf.h);
}

cxColor SH_COEFS::calc_color(const cxVec dir) const {
	float dirCoefs[NCOEF];
	nxSH::eval(ORDER, dirCoefs, dir.x, dir.y, dir.z, SH.consts);
	float r = 0;
	for (int i = 0; i < NCOEF; ++i) {
		r += dirCoefs[i] * mR[i];
	}
	float g = 0;
	for (int i = 0; i < NCOEF; ++i) {
		g += dirCoefs[i] * mG[i];
	}
	float b = 0;
	for (int i = 0; i < NCOEF; ++i) {
		b += dirCoefs[i] * mB[i];
	}
	return cxColor(r, g, b);
}

void SH_COEFS::scl(const cxColor& clr) {
	for (int i = 0; i < NCOEF; ++i) {
		mR[i] *= clr.r;
		mG[i] *= clr.g;
		mB[i] *= clr.b;
	}
}

void ENV_LIGHT::init(const sxTextureData* pTex) {
	if (!pTex) return;
	mpTex = pTex;
	sxTextureData::DDS panoDDS = mpTex->get_dds();
	mSH.from_pano((cxColor*)(panoDDS.get_data_ptr() + 1), panoDDS.get_width(), panoDDS.get_height());
	panoDDS.free();
	mSH.calc_diff_wgt(2.0f, 1.0f);
	mSH.calc_refl_wgt(5.0f, 1.0f);
	mDominantDir = mSH.calc_dominant_dir();
	mDominantClr = mSH.calc_color(mDominantDir);
	mDominantLum = mDominantClr.luminance();
	mDominantClr.scl_rgb(nxCalc::rcp0(mDominantClr.max()));
	mAmbientClr = mSH.get_ambient();
	mAmbientLum = mAmbientClr.luminance();
	mAmbientClr.scl_rgb(nxCalc::rcp0(mAmbientClr.max()));
}

void ENV_LIGHT::apply(GEX_LIT* pLit, float diffRate, float specRate) {
	if (!pLit) return;
	gexAmbient(pLit, cxColor(0));
	gexSHLW(pLit, GEX_SHL_MODE::DIFF_REFL, SH_COEFS::ORDER, mSH.mR, mSH.mG, mSH.mB, mSH.mWgtDiff, mSH.mWgtRefl);
	for (int i = 0; i < D_GEX_MAX_DYN_LIGHTS; ++i) {
		gexLightMode(pLit, i, GEX_LIGHT_MODE::NONE);
	}
	gexLightDir(pLit, 0, mDominantDir.neg_val());
	gexLightColor(pLit, 0, mDominantClr, diffRate, specRate); // spec
	if (diffRate != 0 || specRate != 0) {
		gexLightMode(pLit, 0, GEX_LIGHT_MODE::DIST);
	}
	gexLitUpdate(pLit);
	gexShadowDir(mDominantDir.neg_val());
}

float frand01() {
	uxVal32 uf;
	uint32_t r0 = ::rand() & 0xFF;
	uint32_t r1 = ::rand() & 0xFF;
	uint32_t r2 = ::rand() & 0xFF;
	uf.f = 1.0f;
	uf.u |= (r0 | (r1 << 8) | ((r2 & 0x7F) << 16));
	uf.f -= 1.0f;
	return uf.f;
}

float frand11() {
	float r = frand01() - 0.5f;
	return r + r;
}

float cvt_roughness(float rough) {
	float res = rough;
	if (WK.pLib && WK.eidRoughCvt >= 0) {
		sxExprLibData::Entry ent = WK.pLib->get_entry(WK.eidRoughCvt);
		if (ent.is_valid()) {
			const sxCompiledExpression* pExp = ent.get_expr();
			if (pExp && pExp->is_valid()) {
				WK.ctx.mRoughness = rough;
				pExp->exec(WK.ctx);
				res = WK.ctx.mRes;
			}
		}
	}
	return res;
}

cxQuat get_val_grp_quat(sxValuesData::Group& grp) {
	cxVec rot = grp.get_vec("r");
	exRotOrd rord = nxDataUtil::rot_ord_from_str(grp.get_str("rOrd"));
	cxQuat q;
	q.set_rot_degrees(rot, rord);
	return q;
}

cxVec vec_rot_deg(const cxVec& vsrc, float angX, float angY, float angZ, exRotOrd rord) {
	cxQuat q;
	q.set_rot_degrees(cxVec(angX, angY, angZ), rord);
	return q.apply(vsrc);
}

GEX_SPEC_MODE parse_spec_mode(const char* pMode) {
	GEX_SPEC_MODE mode = GEX_SPEC_MODE::PHONG;
	if (nxCore::str_eq(pMode, "phong")) {
		mode = GEX_SPEC_MODE::PHONG;
	} else if (nxCore::str_eq(pMode, "blinn")) {
		mode = GEX_SPEC_MODE::BLINN;
	} else if (nxCore::str_eq(pMode, "ggx")) {
		mode = GEX_SPEC_MODE::GGX;
	}
	return mode;
}

static cxVec fcv_grp_eval(const sxKeyframesData& kfr, const char* pNodeName, float frame, const char** ppChTbl) {
	cxVec val(0.0f);
	if (pNodeName && ppChTbl) {
		for (int i = 0; i < 3; ++i) {
			int idx = kfr.find_fcv_idx(pNodeName, ppChTbl[i]);
			if (idx >= 0) {
				sxKeyframesData::FCurve fcv = kfr.get_fcv(idx);
				if (fcv.is_valid()) {
					val.set_at(i, fcv.eval(frame));
				}
			}
		}
	}
	return val;
}

cxVec eval_pos(const sxKeyframesData& kfr, const char* pNodeName, float frame) {
	static const char* chTbl[] = { "tx", "ty", "tz" };
	return fcv_grp_eval(kfr, pNodeName, frame, chTbl);
}

cxVec eval_rot(const sxKeyframesData& kfr, const char* pNodeName, float frame) {
	static const char* chTbl[] = { "rx", "ry", "rz" };
	return fcv_grp_eval(kfr, pNodeName, frame, chTbl);
}

GEX_LIT* make_lights(const sxValuesData& vals, cxVec* pDominantDir) {
	GEX_LIT* pLit = gexLitCreate();
	cxVec dominantDir = nxVec::get_axis(exAxis::MINUS_Z);
	if (pLit) {
		float maxLum = 0.0f;
		int n = vals.get_grp_num();
		int lidx = 0;
		for (int i = 0; i < n; ++i) {
			sxValuesData::Group grp = vals.get_grp(i);
			if (grp.is_valid()) {
				const char* pTypeName = grp.get_type();
				if (pTypeName == "ambient") {
					cxColor ambClr = grp.get_rgb("light_color", cxColor(0.1f));
					float ambInt = grp.get_float("light_intensity", 1.0f);
					ambClr.scl_rgb(ambInt);
					gexAmbient(pLit, ambClr);
				} else if (nxCore::str_starts_with(pTypeName, "hlight")) {
					if (lidx < D_GEX_MAX_DYN_LIGHTS) {
						cxVec pos = grp.get_vec("t");
						cxColor clr = grp.get_rgb("light_color", cxColor(1.0f));
						float val = grp.get_float("light_intensity", 0.1f);
						clr.scl_rgb(val);
						cxQuat q = get_val_grp_quat(grp);
						cxVec dir = q.apply(nxVec::get_axis(exAxis::MINUS_Z));
						gexLightDir(pLit, lidx, dir);
						gexLightColor(pLit, lidx, clr);
						gexLightMode(pLit, lidx, GEX_LIGHT_MODE::DIST);
						float lum = clr.luminance();
						if (lum > maxLum) {
							dominantDir = dir;
							maxLum = lum;
						}
						++lidx;
					} else {
						nxCore::dbg_msg("Skipping light entry @ %s:%s.", vals.get_name(), grp.get_name());
					}
				}
			}
		}
		gexLitUpdate(pLit);
	}
	if (pDominantDir) {
		*pDominantDir = dominantDir;
	}
	return pLit;
}

GEX_LIT* make_const_lit(const char* pName, float val) {
	GEX_LIT* pLit = gexLitCreate(pName ? pName : "const_lit");
	if (pLit) {
		gexAmbient(pLit, cxColor(val));
		for (int i = 0; i < D_GEX_MAX_DYN_LIGHTS; ++i) {
			gexLightMode(pLit, i, GEX_LIGHT_MODE::NONE);
		}
		gexLitUpdate(pLit);
	}
	return pLit;
}

static GEX_TEX* find_tex(const char* pPath) {
	GEX_TEX* pTex = nullptr;
	if (pPath) {
		size_t len = ::strlen(pPath);
		if (len > 0) {
			int nameOffs = (int)len;
			for (; --nameOffs >= 0;) {
				if (pPath[nameOffs] == '/') break;
			}
			if (nameOffs > 0) {
				pTex = gexTexFind(&pPath[nameOffs + 1]);
			}
		}
	}
	return pTex;
}

namespace MtlParamNames {

	static const char* baseTexFlg[] = {
		"diff_colorUseTexture", /* Classic Shader */
		"basecolor_useTexture" /* Principled Shader */
	};

	static const char* baseTexName[] = {
		"diff_colorTexture", /* Classic Shader */
		"basecolor_texture" /* Principled Shader */
	};

	static const char* baseColor[] = {
		"diff_color", /* Classic Shader */
		"basecolor" /* Principled Shader */
	};

	static const char* specTexFlg[] = {
		"refl_colorUseTexture", /* Classic Shader */
		"reflect_useTexture" /* Principled Shader */
	};

	static const char* specTexName[] = {
		"refl_colorTexture", /* Classic Shader */
		"reflect_texture" /* Principled Shader */
	};

	static const char* specColor[] = {
		"spec_color" /* Classic Shader */
	};

	static const char* specModel[] = {
		"spec_model", /* Classic Shader */
		"ogl_spec_model" /* Principled Shader */
	};

	static const char* roughness[] = {
		"spec_rough", /* Classic Shader */
		"rough" /* Principled Shader */
	};

	static const char* IOR[] = {
		"ior_in", /* Classic Shader */
		"ogl_ior_inner" /* Principled Shader */
	};

	static const char* bumpTexFlg[] = {
		"baseBumpAndNormal_enable", /* Classic Shader */
		"enableBumpOrNormalTexture" /* Principled Shader */
	};

	static const char* useAlpha[] = {
		"diff_colorTextureUseAlpha", /* Classic Shader */
		"ogl_cutout" /* Principled Shader */
	};

	static const char* bumpTexName[] = {
		"baseNormal_texture", /* Classic Shader */
		"normalTexture" /* Principled Shader */
	};

	static const char* bumpScale[] = {
		"baseNormal_scale", /* Classic Shader */
		"normalTexScale" /* Principled Shader */
	};
};

#define D_MTL_PRM(_grp, _typ, _name, _def) (_grp).get_##_typ##_any(MtlParamNames::_name, XD_ARY_LEN(MtlParamNames::_name), _def)
#define D_MTL_INT(_grp, _name, _def) D_MTL_PRM(_grp, int, _name, _def)
#define D_MTL_FLOAT(_grp, _name, _def) D_MTL_PRM(_grp, float, _name, _def)
#define D_MTL_RGB(_grp, _name, _def) D_MTL_PRM(_grp, rgb, _name, _def)
#define D_MTL_STR(_grp, _name, _def) D_MTL_PRM(_grp, str, _name, _def)
#define D_MTL_BOOL(_grp, _name, _def) ( !!(D_MTL_INT(_grp, _name, _def)) )
#define D_MTL_BOOL_(_grp, _name) D_MTL_BOOL(_grp, _name, 0)

void init_materials(GEX_OBJ& obj, const sxValuesData& vals, bool useReflectColor) {
	int n = vals.get_grp_num();
	for (int i = 0; i < n; ++i) {
		sxValuesData::Group grp = vals.get_grp(i);
		if (grp.is_valid()) {
			const char* pMtlName = grp.get_name();
			if (pMtlName) {
				GEX_MTL* pMtl = gexFindMaterial(&obj, pMtlName);
				if (pMtl) {
					float roughness = cvt_roughness(D_MTL_FLOAT(grp, roughness, 0.3f));
					cxColor baseClr = D_MTL_RGB(grp, baseColor, cxColor(0.2f));
					cxColor specClr = D_MTL_RGB(grp, specColor, cxColor(1.0f));
					GEX_SPEC_MODE specMode = parse_spec_mode(D_MTL_STR(grp, specModel, "phong"));
					float IOR = D_MTL_FLOAT(grp, IOR, 1.4f);
					gexMtlBaseColor(pMtl, baseClr);
					gexMtlSpecularColor(pMtl, specClr);
					gexMtlRoughness(pMtl, roughness);
					gexMtlSpecMode(pMtl, specMode);
					gexMtlIOR(pMtl, IOR);
					if (specMode == GEX_SPEC_MODE::GGX) {
						gexMtlSpecFresnelMode(pMtl, 1);
					}
					GEX_UV_MODE uvMode = grp.get_int("ogl_clamping_mode1", 0) ? GEX_UV_MODE::CLAMP : GEX_UV_MODE::WRAP;
					gexMtlUVMode(pMtl, uvMode);
					bool baseTexFlg = D_MTL_BOOL_(grp, baseTexFlg);
					if (baseTexFlg) {
						const char* pBaseTexName = D_MTL_STR(grp, baseTexName, "");
						GEX_TEX* pBaseTex = find_tex(pBaseTexName);
						if (pBaseTex) {
							gexMtlBaseTexture(pMtl, pBaseTex);
						}
					}
					const char* pSHDiffDtlName = "gex_shDiffDtl";
					if (grp.find_val_idx(pSHDiffDtlName) >= 0) {
						gexMtlSHDiffuseDetail(pMtl, grp.get_float(pSHDiffDtlName));
					}
					const char* pSHReflDtlName = "gex_shReflDtl";
					if (grp.find_val_idx(pSHReflDtlName) >= 0) {
						gexMtlSHReflectionDetail(pMtl, grp.get_float(pSHReflDtlName));
					}
					bool specTexFlg = D_MTL_BOOL_(grp, specTexFlg);
					if (specTexFlg) {
						const char* pSpecTexName = D_MTL_STR(grp, specTexName, "");
						GEX_TEX* pSpecTex = find_tex(pSpecTexName);
						if (pSpecTex) {
							gexMtlSpecularTexture(pMtl, pSpecTex);
						}
					}
					if (useReflectColor) {
						gexMtlSpecularColor(pMtl, cxColor(grp.get_float("reflect", 0.5f)));
					}
					const char* pRoughnessMinName = "gex_specRoughnessMin";
					if (grp.find_val_idx(pRoughnessMinName) >= 0) {
						gexMtlSpecularRoughnessMin(pMtl, grp.get_float(pRoughnessMinName));
					}
					gexMtlSHReflectionFresnelRate(pMtl, grp.get_float("gex_shReflFrRate", 1.0f));
					gexMtlReflDownFadeRate(pMtl, grp.get_float("gex_reflDownFadeRate", 0.0f));
					bool alphaFlg = D_MTL_BOOL_(grp, useAlpha);
					gexMtlAlpha(pMtl, alphaFlg);
					bool alphaCovFlg = !!grp.get_int("gex_alphaCov", false);
					gexMtlAlphaToCoverage(pMtl, alphaCovFlg);
					bool bumpTexFlg = D_MTL_BOOL_(grp, bumpTexFlg);
					if (bumpTexFlg) {
						gexMtlBumpFactor(pMtl, D_MTL_FLOAT(grp, bumpScale, 1.0f));
						if (1) {
							gexMtlTangentMode(pMtl, GEX_TANGENT_MODE::AUTO);
						} else {
							gexMtlTangentMode(pMtl, GEX_TANGENT_MODE::GEOM);
							gexMtlTangentOptions(pMtl, false, true);
						}
						const char* pBumpTexName = D_MTL_STR(grp, bumpTexName, nullptr);
						GEX_TEX* pBumpTex = find_tex(pBumpTexName);
						if (pBumpTex) {
							gexMtlBumpTexture(pMtl, pBumpTex);
							gexMtlBumpMode(pMtl, GEX_BUMP_MODE::NMAP);
						}
					}
					const char* pDblSidedName = "gex_dblSided";
					if (grp.find_val_idx(pDblSidedName) >= 0) {
						gexMtlDoubleSided(pMtl, grp.get_int(pDblSidedName) != 0);
					}
					gexMtlUpdate(pMtl);
				}
			}
		}
	}
}

CAM_INFO get_cam_info(const sxValuesData& vals, const char* pCamName) {
	CAM_INFO info;
	info.set_defaults();
	sxValuesData::Group grp = vals.find_grp(pCamName);
	if (grp.is_valid()) {
		info.mPos = grp.get_vec("t", info.mPos);
		info.mQuat = get_val_grp_quat(grp);
		info.mUp = grp.get_vec("up", info.mUp);
		float focal = grp.get_float("focal", 50.0f);
		float aperture = grp.get_float("aperture", 41.4214f);
		info.mFOVY = gexCalcFOVY(focal, aperture);
		info.mNear = grp.get_float("near", info.mNear);
		info.mFar = grp.get_float("far", info.mFar);
	}
	return info;
}

TD_CAM_INFO parse_td_cam(const char* pData, size_t dataSize) {
	TD_CAM_INFO info = {};
	if (pData && dataSize > 0) {
		const char* pWk = pData;
		const char* pEnd = pData + dataSize;
		const char* pSym = pWk;
		int state = 0;
		int cnt = 0;
		int idx = -1;
		static struct MAP {
			const char* pName;
			float TD_CAM_INFO::* pField;
		} map[] = {
			{ "tx", &TD_CAM_INFO::mX },
			{ "ty", &TD_CAM_INFO::mY },
			{ "tz", &TD_CAM_INFO::mZ },
			{ "rx", &TD_CAM_INFO::mRotX },
			{ "ry", &TD_CAM_INFO::mRotY },
			{ "rz", &TD_CAM_INFO::mRotZ },
			{ "px", &TD_CAM_INFO::mPivotX },
			{ "py", &TD_CAM_INFO::mPivotY },
			{ "pz", &TD_CAM_INFO::mPivotZ },
			{ "xord", &TD_CAM_INFO::mXOrd },
			{ "rord", &TD_CAM_INFO::mROrd },
			{ "viewanglemethod", &TD_CAM_INFO::mFOVType },
			{ "fov", &TD_CAM_INFO::mFOV },
			{ "focal", &TD_CAM_INFO::mFocal },
			{ "aperture", &TD_CAM_INFO::mAperture },
			{ "near", &TD_CAM_INFO::mNear },
			{ "far", &TD_CAM_INFO::mFar },
			{ nullptr, nullptr }
		};
		while (pWk < pEnd) {
			char c = *pWk++;
			if (c == '\n' || c == '\r') {
				if (idx >= 0 && pSym && cnt > 0) {
					char vbuf[128];
					if (cnt < sizeof(vbuf) - 1) {
						::memcpy(vbuf, pSym, cnt);
						vbuf[cnt] = 0;
						float val = (float)::atof(vbuf);
						info.*map[idx].pField = val;
					}
				}
				state = 0;
				cnt = 0;
				idx = -1;
				pSym = pWk;
			} else if (c == '\t') {
				idx = -1;
				if (pSym && cnt > 0) {
					for (int i = 0; map[i].pName; ++i) {
						if (cnt == ::strlen(map[i].pName) && ::memcmp(map[i].pName, pSym, cnt) == 0) {
							idx = i;
							break;
						}
					}
				}
				pSym = pWk;
				cnt = 0;
				state = 1;
			} else {
				++cnt;
			}
		}
	}
	return info;
}

TD_CAM_INFO load_td_cam(const char* pPath) {
	TD_CAM_INFO info = {};
	XD_FHANDLE fh = nxSys::fopen(pPath);
	if (fh) {
		size_t fsize = nxSys::fsize(fh);
		void* pData = nxCore::mem_alloc(fsize, XD_TMP_MEM_TAG);
		if (pData) {
			size_t size = nxSys::fread(fh, pData, fsize);
			nxSys::fclose(fh);
			info = parse_td_cam((const char*)pData, size);
			nxCore::mem_free(pData);
		}
	}
	return info;
}

void obj_shadow_mode(const GEX_OBJ& obj, bool castShadows, bool receiveShadows) {
	int n = gexObjMtlNum(&obj);
	for (int i = 0; i < n; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlShadowMode(pMtl, castShadows, receiveShadows);
	}
}

void obj_shadow_params(const GEX_OBJ& obj, float density, float selfShadowFactor, bool cullShadows) {
	int n = gexObjMtlNum(&obj);
	for (int i = 0; i < n; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlShadowDensity(pMtl, density);
		gexMtlSelfShadowFactor(pMtl, selfShadowFactor);
		gexMtlShadowCulling(pMtl, cullShadows);
		gexMtlUpdate(pMtl);
	}
}

void obj_tesselation(const GEX_OBJ& obj, GEX_TESS_MODE mode, float factor) {
	int nmtl = gexObjMtlNum(&obj);
	for (int i = 0; i < nmtl; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlTesselationMode(pMtl, mode);
		gexMtlTesselationFactor(pMtl, factor);
		gexMtlUpdate(pMtl);
	}
}

void obj_spec_fresnel_mode(const GEX_OBJ& obj, int mode) {
	int nmtl = gexObjMtlNum(&obj);
	for (int i = 0; i < nmtl; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlSpecFresnelMode(pMtl, mode);
		gexMtlUpdate(pMtl);
	}
}

void obj_diff_roughness(const GEX_OBJ& obj, const cxColor& rgb) {
	int nmtl = gexObjMtlNum(&obj);
	for (int i = 0; i < nmtl; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlDiffuseRoughness(pMtl, rgb);
		gexMtlUpdate(pMtl);
	}
}

void obj_diff_mode(const GEX_OBJ& obj, GEX_DIFF_MODE mode) {
	int nmtl = gexObjMtlNum(&obj);
	for (int i = 0; i < nmtl; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlDiffMode(pMtl, mode);
		gexMtlUpdate(pMtl);
	}
}

void obj_spec_mode(const GEX_OBJ& obj, GEX_SPEC_MODE mode) {
	int nmtl = gexObjMtlNum(&obj);
	for (int i = 0; i < nmtl; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlSpecMode(pMtl, mode);
		gexMtlUpdate(pMtl);
	}
}

void obj_sort_mode(const GEX_OBJ& obj, GEX_SORT_MODE mode) {
	int n = gexObjMtlNum(&obj);
	for (int i = 0; i < n; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlSortMode(pMtl, mode);
	}
}

void obj_sort_bias(const GEX_OBJ& obj, float absBias, float relBias) {
	int n = gexObjMtlNum(&obj);
	for (int i = 0; i < n; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlSortBias(pMtl, absBias, relBias);
	}
}

void mtl_sort_mode(const GEX_OBJ& obj, const char* pMtlName, GEX_SORT_MODE mode) {
	GEX_MTL* pMtl = gexFindMaterial(&obj, pMtlName);
	if (!pMtl) return;
	gexMtlSortMode(pMtl, mode);
}

void mtl_sort_bias(const GEX_OBJ& obj, const char* pMtlName, float absBias, float relBias) {
	GEX_MTL* pMtl = gexFindMaterial(&obj, pMtlName);
	if (!pMtl) return;
	gexMtlSortBias(pMtl, absBias, relBias);
}

void mtl_sh_diff_detail(const GEX_OBJ& obj, const char* pMtlName, float dtl) {
	GEX_MTL* pMtl = gexFindMaterial(&obj, pMtlName);
	if (!pMtl) return;
	gexMtlSHDiffuseDetail(pMtl, dtl);
	gexMtlUpdate(pMtl);
}

void mtl_sh_refl_detail(const GEX_OBJ& obj, const char* pMtlName, float dtl) {
	GEX_MTL* pMtl = gexFindMaterial(&obj, pMtlName);
	if (!pMtl) return;
	gexMtlSHReflectionDetail(pMtl, dtl);
	gexMtlUpdate(pMtl);
}

void mtl_shadow_density(const GEX_OBJ& obj, const char* pMtlName, float density) {
	GEX_MTL* pMtl = gexFindMaterial(&obj, pMtlName);
	if (!pMtl) return;
	gexMtlShadowDensity(pMtl, density);
	gexMtlUpdate(pMtl);
}


void dump_riglink_info(sxKeyframesData* pKfr, sxKeyframesData::RigLink* pLink, FILE* pOut) {
	if (!pKfr) return;
	if (!pLink) return;
	if (!pOut) {
		pOut = stdout;
	}
	::fprintf(pOut, "#frames = %d\n", pKfr->get_frame_count());
	int nnodes = pLink->mNodeNum;
	for (int i = 0; i < nnodes; ++i) {
		sxKeyframesData::RigLink::Node* pNode = &pLink->mNodes[i];
		int nodeId = pNode->mKfrNodeId;
		const char* pNodeName = pKfr->get_node_name(nodeId);
		sxKeyframesData::RigLink::Val* pPosVal = pNode->get_pos_val();
		int nchPos = 0;
		int nchKeyPos = 0;
		int nkfPos[3] = {};
		if (pPosVal) {
			for (int i = 0; i < 3; ++i) {
				if (pKfr->ck_fcv_idx(pPosVal->fcvId[i])) {
					++nchPos;
					sxKeyframesData::FCurve fcv = pKfr->get_fcv(pPosVal->fcvId[i]);
					nkfPos[i] = fcv.get_key_num();
					if (!fcv.is_const()) {
						++nchKeyPos;
					}
				} else {
					nkfPos[i] = 0;
				}
			}
		}
		sxKeyframesData::RigLink::Val* pRotVal = pNode->get_rot_val();
		int nchRot = 0;
		int nchKeyRot = 0;
		int nkfRot[3] = {};
		if (pRotVal) {
			for (int i = 0; i < 3; ++i) {
				if (pKfr->ck_fcv_idx(pRotVal->fcvId[i])) {
					++nchRot;
					sxKeyframesData::FCurve fcv = pKfr->get_fcv(pRotVal->fcvId[i]);
					nkfRot[i] = fcv.get_key_num();
					if (!fcv.is_const()) {
						++nchKeyRot;
					}
				} else {
					nkfRot[i] = 0;
				}
			}
		}
		sxKeyframesData::RigLink::Val* pSclVal = pNode->get_scl_val();
		int nchScl = 0;
		int nchKeyScl = 0;
		int nkfScl[3] = {};
		if (pSclVal) {
			for (int i = 0; i < 3; ++i) {
				if (pKfr->ck_fcv_idx(pSclVal->fcvId[i])) {
					++nchScl;
					sxKeyframesData::FCurve fcv = pKfr->get_fcv(pSclVal->fcvId[i]);
					nkfScl[i] = fcv.get_key_num();
					if (!fcv.is_const()) {
						++nchKeyScl;
					}
				} else {
					nkfScl[i] = 0;
				}
			}
		}
		::fprintf(pOut, "[%d] %s\n", i, pNodeName);
		::fprintf(pOut, " # channels: %d pos, %d rot, %d scl, total = %d\n", nchPos, nchRot, nchScl, nchPos + nchRot + nchScl);
		::fprintf(pOut, " # non-const: %d pos, %d rot, %d scl, total = %d\n", nchKeyPos, nchKeyRot, nchKeyScl, nchKeyPos + nchKeyRot + nchKeyScl);
		if (pPosVal) {
			::fprintf(pOut, " # pos keys: %d %d %d\n", nkfPos[0], nkfPos[1], nkfPos[2]);
		}
		if (pRotVal) {
			::fprintf(pOut, " # rot keys: %d %d %d\n", nkfRot[0], nkfRot[1], nkfRot[2]);
		}
		if (pSclVal) {
			::fprintf(pOut, " # scl keys: %d %d %d\n", nkfScl[0], nkfScl[1], nkfScl[2]);
		}
	}
}

