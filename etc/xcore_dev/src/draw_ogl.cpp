#include "crosscore.hpp"
#include "oglsys.hpp"
#include "draw.hpp"

#include "ogl/gpu_defs.h"

#define DRW_CACHE_PARAMS 1
#define DRW_CACHE_PROGS 1
#define DRW_CACHE_BLEND 1
#define DRW_CACHE_DSIDED 1
#define DRW_LIMIT_JMAP 1
#define DRW_USE_VAO 1

DRW_IMPL_BEGIN

static bool s_useMipmaps = true;

static cxResourceManager* s_pRsrcMgr = nullptr;

struct GPUProg;
static const GPUProg* s_pNowProg = nullptr;

static bool s_drwInitFlg = false;

static int s_shadowSize = 0;
static GLuint s_shadowFBO = 0;
static GLuint s_shadowTex = 0;
static GLuint s_shadowDepthBuf = 0;
static bool s_shadowCastDepthTest = true;

static int s_frameBufMode = 0; // 0: def, 1: shadow, 2: sprites
static int s_batDrwCnt = 0;
static int s_shadowCastCnt = 0;

static GLuint s_sprVBO = 0;
static GLuint s_sprIBO = 0;

static void def_tex_lod_bias() {
#if !OGLSYS_ES
	static bool flg = false;
	static int bias = 0;
	if (!flg) {
		bias = nxApp::get_int_opt("tlod_bias", -3);
		flg = true;
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, bias);
#endif
}

static void def_tex_params(const bool mipmapEnabled) {
	if (s_useMipmaps && mipmapEnabled) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		def_tex_lod_bias();
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

static GLuint load_shader(const char* pName) {
	GLuint sid = 0;
	const char* pDataPath = s_pRsrcMgr ? s_pRsrcMgr->get_data_path() : nullptr;
	if (pName) {
		char path[1024];
		XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/ogl/%s", pDataPath ? pDataPath : ".", pName);
		size_t srcSize = 0;
		char* pSrc = (char*)nxCore::bin_load(path, &srcSize, false, true);
		if (pSrc) {
			GLenum kind = nxCore::str_ends_with(pName, ".vert") ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
			sid = OGLSys::compile_shader_str(pSrc, srcSize, kind);
			nxCore::bin_unload(pSrc);
		}
	}
	return sid;
}

struct VtxLink {
	GLint Pos;
	GLint Oct;
	GLint RelPosW0;
	GLint OctW1W2;
	GLint Clr;
	GLint Tex;
	GLint Wgt;
	GLint Jnt;
	GLint Id;

	static int num_attrs() {
		return int(sizeof(VtxLink) / sizeof(GLint));
	}

	void reset() { ::memset(this, 0xFF, sizeof(*this)); }

	void disable_all() const {
		int n = num_attrs();
		const GLint* p = reinterpret_cast<const GLint*>(this);
		for (int i = 0; i < n; ++i) {
			GLint iattr = p[i];
			if (iattr >= 0) {
				glDisableVertexAttribArray(iattr);
			}
		}
	}

	uint32_t get_mask() const {
		uint32_t msk = 0;
		int n = num_attrs();
		const GLint* p = reinterpret_cast<const GLint*>(this);
		for (int i = 0; i < n; ++i) {
			GLint iattr = p[i];
			msk |= (iattr >= 0 ? 1 : 0) << i;
		}
		return msk;
	}
};

struct ParamLink {
	GLint PosBase;
	GLint PosScale;
	GLint SkinMtx;
	GLint SkinMap;
	GLint World;
	GLint ViewProj;
	GLint ViewPos;
	GLint ScrXform;
	GLint HemiUp;
	GLint HemiUpper;
	GLint HemiLower;
	GLint HemiParam;
	GLint SpecLightDir;
	GLint SpecLightColor;
	GLint ShadowMtx;
	GLint ShadowSize;
	GLint ShadowCtrl;
	GLint ShadowFade;
	GLint BaseColor;
	GLint SpecColor;
	GLint SurfParam;
	GLint BumpParam;
	GLint AlphaCtrl;
	GLint BumpPatUV;
	GLint BumpPatParam;
	GLint FogColor;
	GLint FogParam;
	GLint InvWhite;
	GLint LClrGain;
	GLint LClrBias;
	GLint Exposure;
	GLint InvGamma;
	GLint SprVtxPos;
	GLint SprVtxTex;
	GLint SprVtxClr;

	void reset() { ::memset(this, 0xFF, sizeof(*this)); }
};

struct SmpLink {
	GLint Base;
	GLint Spec;
	GLint Bump;
	GLint Surf;
	GLint BumpPat;
	GLint Shadow;

	void reset() { ::memset(this, 0xFF, sizeof(*this)); }
};

#define VTX_LINK(_name) mVtxLink._name = glGetAttribLocation(mProgId, "vtx" #_name)
#define PARAM_LINK(_name) mParamLink._name = glGetUniformLocation(mProgId, "gp" #_name)
#define SMP_LINK(_name) mSmpLink._name = glGetUniformLocation(mProgId, "smp" #_name)
#define SET_TEX_UNIT(_name) if (mSmpLink._name >= 0) { glUniform1i(mSmpLink._name, Draw::TEXUNIT_##_name); }

static void gl_param(const GLint loc, const xt_mtx& m) {
	glUniformMatrix4fv(loc, 1, GL_FALSE, m);
}

static void gl_param(const GLint loc, const xt_xmtx& wm) {
	glUniform4fv(loc, 3, wm);
}

static void gl_param(const GLint loc, const xt_float3& v) {
	glUniform3fv(loc, 1, v);
}

static void gl_param(const GLint loc, const xt_float4& v) {
	glUniform4fv(loc, 1, v);
}

enum VtxFmt {
	VtxFmt_none,
	VtxFmt_rigid0,
	VtxFmt_rigid1,
	VtxFmt_skin0,
	VtxFmt_skin1,
	VtxFmt_sprite
};

struct GPUProg {
	GLuint mVertSID;
	GLuint mFragSID;
	GLuint mProgId;
	VtxLink mVtxLink;
	ParamLink mParamLink;
	SmpLink mSmpLink;
	VtxFmt mVtxFmt;
	GLuint mVAO;

	template<typename T> struct CachedParam {
		T mVal;
		bool mFlg;

		void reset() {
			mFlg = false;
		}

		void set(const GLint loc, const T& val) {
#if DRW_CACHE_PARAMS
			if (loc >= 0) {
				if (mFlg) {
					if (::memcmp(&mVal, &val, sizeof(T)) != 0) {
						gl_param(loc, val);
						mVal = val;
					}
				} else {
					gl_param(loc, val);
					mVal = val;
					mFlg = true;
				}
			}
#else
			if (loc >= 0) {
				gl_param(loc, val);
			}
#endif
		}
	};

	struct Cache {
		CachedParam<xt_mtx> mViewProj;
		CachedParam<xt_xmtx> mWorld;
		CachedParam<xt_mtx> mShadowMtx;

		CachedParam<xt_float3> mViewPos;

		CachedParam<xt_float3> mPosBase;
		CachedParam<xt_float3> mPosScale;

		CachedParam<xt_float3> mHemiUpper;
		CachedParam<xt_float3> mHemiLower;
		CachedParam<xt_float3> mHemiUp;
		CachedParam<xt_float3> mHemiParam;

		CachedParam<xt_float3> mSpecLightDir;
		CachedParam<xt_float4> mSpecLightColor;

		CachedParam<xt_float3> mBaseColor;
		CachedParam<xt_float3> mSpecColor;
		CachedParam<xt_float4> mSurfParam;
		CachedParam<xt_float4> mBumpParam;
		CachedParam<xt_float3> mAlphaCtrl;

		CachedParam<xt_float4> mBumpPatUV;
		CachedParam<xt_float4> mBumpPatParam;

		CachedParam<xt_float4> mFogColor;
		CachedParam<xt_float4> mFogParam;

		CachedParam<xt_float4> mShadowSize;
		CachedParam<xt_float4> mShadowCtrl;
		CachedParam<xt_float4> mShadowFade;

		CachedParam<xt_float3> mInvWhite;
		CachedParam<xt_float3> mLClrGain;
		CachedParam<xt_float3> mLClrBias;
		CachedParam<xt_float3> mExposure;
		CachedParam<xt_float3> mInvGamma;


		void reset() {
			mViewProj.reset();
			mWorld.reset();
			mShadowMtx.reset();
			mViewPos.reset();
			mPosBase.reset();
			mPosScale.reset();
			mHemiUpper.reset();
			mHemiLower.reset();
			mHemiUp.reset();
			mHemiParam.reset();
			mSpecLightDir.reset();
			mSpecLightColor.reset();
			mBaseColor.reset();
			mSpecColor.reset();
			mSurfParam.reset();
			mBumpParam.reset();
			mAlphaCtrl.reset();
			mBumpPatUV.reset();
			mBumpPatParam.reset();
			mFogColor.reset();
			mFogParam.reset();
			mShadowSize.reset();
			mShadowCtrl.reset();
			mShadowFade.reset();
			mInvWhite.reset();
			mLClrGain.reset();
			mLClrBias.reset();
			mExposure.reset();
			mInvGamma.reset();
		}
	} mCache;


	void init(VtxFmt vfmt, GLuint vertSID, GLuint fragSID) {
		mVertSID = vertSID;
		mFragSID = fragSID;
		mProgId = OGLSys::link_draw_prog(mVertSID, mFragSID);
		mVtxLink.reset();
		mParamLink.reset();
		mSmpLink.reset();
		mCache.reset();
		mVtxFmt = vfmt;

		if (is_valid()) {
			VTX_LINK(Pos);
			VTX_LINK(Oct);
			VTX_LINK(RelPosW0);
			VTX_LINK(OctW1W2);
			VTX_LINK(Clr);
			VTX_LINK(Tex);
			VTX_LINK(Wgt);
			VTX_LINK(Jnt);
			VTX_LINK(Id);

			PARAM_LINK(PosBase);
			PARAM_LINK(PosScale);
			PARAM_LINK(SkinMtx);
			PARAM_LINK(SkinMap);
			PARAM_LINK(World);
			PARAM_LINK(ViewProj);
			PARAM_LINK(ViewPos);
			PARAM_LINK(ScrXform);
			PARAM_LINK(HemiUp);
			PARAM_LINK(HemiUpper);
			PARAM_LINK(HemiLower);
			PARAM_LINK(HemiParam);
			PARAM_LINK(SpecLightDir);
			PARAM_LINK(SpecLightColor);
			PARAM_LINK(ShadowMtx);
			PARAM_LINK(ShadowSize);
			PARAM_LINK(ShadowCtrl);
			PARAM_LINK(ShadowFade);
			PARAM_LINK(BaseColor);
			PARAM_LINK(SpecColor);
			PARAM_LINK(SurfParam);
			PARAM_LINK(BumpParam);
			PARAM_LINK(AlphaCtrl);
			PARAM_LINK(BumpPatUV);
			PARAM_LINK(BumpPatParam);
			PARAM_LINK(FogColor);
			PARAM_LINK(FogParam);
			PARAM_LINK(InvWhite);
			PARAM_LINK(LClrGain);
			PARAM_LINK(LClrBias);
			PARAM_LINK(Exposure);
			PARAM_LINK(InvGamma);
			PARAM_LINK(SprVtxPos);
			PARAM_LINK(SprVtxTex);
			PARAM_LINK(SprVtxClr);

			SMP_LINK(Base);
			SMP_LINK(Bump);
			SMP_LINK(Spec);
			SMP_LINK(Surf);
			SMP_LINK(Shadow);

			glUseProgram(mProgId);
			SET_TEX_UNIT(Base);
			SET_TEX_UNIT(Bump);
			SET_TEX_UNIT(Spec);
			SET_TEX_UNIT(Surf);
			SET_TEX_UNIT(BumpPat);
			SET_TEX_UNIT(Shadow);
			glUseProgram(0);

			mVAO = DRW_USE_VAO ? OGLSys::gen_vao() : 0;
		}
	}

	void reset() {
		if (!is_valid()) return;
		glDetachShader(mProgId, mVertSID);
		glDetachShader(mProgId, mFragSID);
		glDeleteProgram(mProgId);
		mVertSID = 0;
		mFragSID = 0;
		mProgId = 0;
		mVtxLink.reset();
		mParamLink.reset();
		mSmpLink.reset();
		mCache.reset();
		if (mVAO) {
			OGLSys::del_vao(mVAO);
			mVAO = 0;
		}
	}

	bool is_valid() const { return mProgId != 0; }

	bool is_skin() const {
		return mVtxLink.Jnt >= 0;
	}

	void use() const {
#if DRW_CACHE_PROGS
		if (s_pNowProg != this) {
			glUseProgram(mProgId);
			s_pNowProg = this;
		}
#else
		glUseProgram(mProgId);
#endif
	}

	void enable_attrs(const int minIdx, const size_t vtxSize = 0) const;

	void disable_attrs() const {
		mVtxLink.disable_all();
	}

	void set_view_proj(const xt_mtx& m) {
		mCache.mViewProj.set(mParamLink.ViewProj, m);
	}

	void set_world(const xt_xmtx& wm) {
		mCache.mWorld.set(mParamLink.World, wm);
	}

	void set_shadow_mtx(const xt_mtx& sm) {
		mCache.mShadowMtx.set(mParamLink.ShadowMtx, sm);
	}

	void set_view_pos(const xt_float3& pos) {
		mCache.mViewPos.set(mParamLink.ViewPos, pos);
	}

	void set_pos_base(const xt_float3& base) {
		mCache.mPosBase.set(mParamLink.PosBase, base);
	}

	void set_pos_scale(const xt_float3& scl) {
		mCache.mPosScale.set(mParamLink.PosScale, scl);
	}

	void set_hemi_upper(const xt_float3& hupr) {
		mCache.mHemiUpper.set(mParamLink.HemiUpper, hupr);
	}

	void set_hemi_lower(const xt_float3& hlwr) {
		mCache.mHemiLower.set(mParamLink.HemiLower, hlwr);
	}

	void set_hemi_up(const xt_float3& upvec) {
		mCache.mHemiUp.set(mParamLink.HemiUp, upvec);
	}

	void set_hemi_param(const xt_float3& hprm) {
		mCache.mHemiParam.set(mParamLink.HemiParam, hprm);
	}

	void set_spec_light_dir(const xt_float3& dir) {
		mCache.mSpecLightDir.set(mParamLink.SpecLightDir, dir);
	}

	void set_spec_light_color(const xt_float4& c) {
		mCache.mSpecLightColor.set(mParamLink.SpecLightColor, c);
	}

	void set_base_color(const xt_float3& c) {
		mCache.mBaseColor.set(mParamLink.BaseColor, c);
	}

	void set_spec_color(const xt_float3& c) {
		mCache.mSpecColor.set(mParamLink.SpecColor, c);
	}

	void set_surf_param(const xt_float4& sprm) {
		mCache.mSurfParam.set(mParamLink.SurfParam, sprm);
	}

	void set_bump_param(const xt_float4& bprm) {
		mCache.mBumpParam.set(mParamLink.BumpParam, bprm);
	}

	void set_alpha_ctrl(const xt_float3& actrl) {
		mCache.mAlphaCtrl.set(mParamLink.AlphaCtrl, actrl);
	}

	void set_bump_pat_uv(sxModelData::Material::NormPatternExt* pExt) {
		if (!pExt) return;
		xt_float4 uv;
		uv.set(pExt->offs.x, pExt->offs.y, pExt->scl.x, pExt->scl.y);
		mCache.mBumpPatUV.set(mParamLink.BumpPatUV, uv);
	}

	void set_bump_pat_param(sxModelData::Material::NormPatternExt* pExt) {
		if (!pExt) return;
		xt_float4 param;
		param.set(pExt->factor.x, pExt->factor.y, 0.0f, 0.0f);
		mCache.mBumpPatParam.set(mParamLink.BumpPatParam, param);
	}

	void set_bump_pat(sxModelData::Material::NormPatternExt* pExt) {
		set_bump_pat_uv(pExt);
		set_bump_pat_param(pExt);
	}

	void set_fog_color(const xt_float4& fclr) {
		mCache.mFogColor.set(mParamLink.FogColor, fclr);
	}

	void set_fog_param(const xt_float4 fprm) {
		mCache.mFogParam.set(mParamLink.FogParam, fprm);
	}

	void set_shadow_size(const xt_float4& size) {
		mCache.mShadowSize.set(mParamLink.ShadowSize, size);
	}

	void set_shadow_ctrl(const xt_float4& ctrl) {
		mCache.mShadowCtrl.set(mParamLink.ShadowCtrl, ctrl);
	}

	void set_shadow_fade(const xt_float4& fade) {
		mCache.mShadowFade.set(mParamLink.ShadowFade, fade);
	}

	void set_inv_white(const xt_float3& iw) {
		mCache.mInvWhite.set(mParamLink.InvWhite, iw);
	}

	void set_lclr_gain(const xt_float3& gain) {
		mCache.mLClrGain.set(mParamLink.LClrGain, gain);
	}

	void set_lclr_bias(const xt_float3& bias) {
		mCache.mLClrBias.set(mParamLink.LClrBias, bias);
	}

	void set_exposure(const xt_float3& e) {
		mCache.mExposure.set(mParamLink.Exposure, e);
	}

	void set_inv_gamma(const xt_float3& ig) {
		mCache.mInvGamma.set(mParamLink.InvGamma, ig);
	}
};

void GPUProg::enable_attrs(const int minIdx, const size_t vtxSize) const {
	GLsizei stride = (GLsizei)vtxSize;
	if (stride <= 0) {
		switch (mVtxFmt) {
			case VtxFmt_skin0: stride = sizeof(sxModelData::VtxSkinHalf); break;
			case VtxFmt_skin1: stride = sizeof(sxModelData::VtxSkinShort); break;
			case VtxFmt_rigid0: stride = sizeof(sxModelData::VtxRigidHalf); break;
			case VtxFmt_rigid1: stride = sizeof(sxModelData::VtxRigidShort); break;
			case VtxFmt_sprite: stride = sizeof(float); break;
			default: break;
		}
	}
	size_t top = minIdx * stride;
	switch (mVtxFmt) {
		case VtxFmt_skin0:
			if (mVtxLink.Pos >= 0) {
				glEnableVertexAttribArray(mVtxLink.Pos);
				glVertexAttribPointer(mVtxLink.Pos, 3, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxSkinHalf, pos)));
			} else {
				return;
			}
			if (mVtxLink.Oct >= 0) {
				glEnableVertexAttribArray(mVtxLink.Oct);
				glVertexAttribPointer(mVtxLink.Oct, 2, GL_HALF_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxSkinHalf, oct)));
			}
			if (mVtxLink.Tex >= 0) {
				glEnableVertexAttribArray(mVtxLink.Tex);
				glVertexAttribPointer(mVtxLink.Tex, 2, GL_HALF_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxSkinHalf, tex)));
			}
			if (mVtxLink.Clr >= 0) {
				glEnableVertexAttribArray(mVtxLink.Clr);
				glVertexAttribPointer(mVtxLink.Clr, 4, GL_HALF_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxSkinHalf, clr)));
			}
			if (mVtxLink.Wgt >= 0) {
				glEnableVertexAttribArray(mVtxLink.Wgt);
				glVertexAttribPointer(mVtxLink.Wgt, 4, GL_UNSIGNED_SHORT, GL_TRUE, stride, (const void*)(top + offsetof(sxModelData::VtxSkinHalf, wgt)));
			}
			if (mVtxLink.Jnt >= 0) {
				glEnableVertexAttribArray(mVtxLink.Jnt);
				glVertexAttribPointer(mVtxLink.Jnt, 4, GL_UNSIGNED_BYTE, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxSkinHalf, jnt)));
			}
			break;

		case VtxFmt_rigid0:
			if (mVtxLink.Pos >= 0) {
				glEnableVertexAttribArray(mVtxLink.Pos);
				glVertexAttribPointer(mVtxLink.Pos, 3, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxRigidHalf, pos)));
			} else {
				return;
			}
			if (mVtxLink.Oct >= 0) {
				glEnableVertexAttribArray(mVtxLink.Oct);
				glVertexAttribPointer(mVtxLink.Oct, 2, GL_HALF_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxRigidHalf, oct)));
			}
			if (mVtxLink.Tex >= 0) {
				glEnableVertexAttribArray(mVtxLink.Tex);
				glVertexAttribPointer(mVtxLink.Tex, 2, GL_HALF_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxRigidHalf, tex)));
			}
			if (mVtxLink.Clr >= 0) {
				glEnableVertexAttribArray(mVtxLink.Clr);
				glVertexAttribPointer(mVtxLink.Clr, 4, GL_HALF_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxRigidHalf, clr)));
			}
			break;

		case VtxFmt_skin1:
			if (mVtxLink.RelPosW0 >= 0) {
				glEnableVertexAttribArray(mVtxLink.RelPosW0);
				glVertexAttribPointer(mVtxLink.RelPosW0, 4, GL_UNSIGNED_SHORT, GL_TRUE, stride, (const void*)(top + offsetof(sxModelData::VtxSkinShort, qpos)));
			} else {
				return;
			}
			if (mVtxLink.OctW1W2 >= 0) {
				glEnableVertexAttribArray(mVtxLink.OctW1W2);
				glVertexAttribPointer(mVtxLink.OctW1W2, 4, GL_UNSIGNED_SHORT, GL_TRUE, stride, (const void*)(top + offsetof(sxModelData::VtxSkinShort, oct)));
			}
			if (mVtxLink.Clr >= 0) {
				glEnableVertexAttribArray(mVtxLink.Clr);
				glVertexAttribPointer(mVtxLink.Clr, 4, GL_UNSIGNED_SHORT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxSkinShort, clr)));
			}
			if (mVtxLink.Tex >= 0) {
				glEnableVertexAttribArray(mVtxLink.Tex);
				glVertexAttribPointer(mVtxLink.Tex, 2, GL_SHORT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxSkinShort, tex)));
			}
			if (mVtxLink.Jnt >= 0) {
				glEnableVertexAttribArray(mVtxLink.Jnt);
				glVertexAttribPointer(mVtxLink.Jnt, 4, GL_UNSIGNED_BYTE, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxSkinShort, jnt)));
			}
			break;

		case VtxFmt_rigid1:
			if (mVtxLink.Pos >= 0) {
				glEnableVertexAttribArray(mVtxLink.Pos);
				glVertexAttribPointer(mVtxLink.Pos, 3, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxRigidShort, pos)));
			} else {
				return;
			}
			if (mVtxLink.Oct >= 0) {
				glEnableVertexAttribArray(mVtxLink.Oct);
				glVertexAttribPointer(mVtxLink.Oct, 2, GL_SHORT, GL_TRUE, stride, (const void*)(top + offsetof(sxModelData::VtxRigidShort, oct)));
			}
			if (mVtxLink.Clr >= 0) {
				glEnableVertexAttribArray(mVtxLink.Clr);
				glVertexAttribPointer(mVtxLink.Clr, 4, GL_UNSIGNED_SHORT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxRigidShort, clr)));
			}
			if (mVtxLink.Tex >= 0) {
				glEnableVertexAttribArray(mVtxLink.Tex);
				glVertexAttribPointer(mVtxLink.Tex, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(sxModelData::VtxRigidShort, tex)));
			}
			break;

		case VtxFmt_sprite:
			if (mVtxLink.Id >= 0) {
				glEnableVertexAttribArray(mVtxLink.Id);
				glVertexAttribPointer(mVtxLink.Id, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(top));
			}
			break;

		default:
			break;
	}
}

#define GPU_SHADER(_name, _kind) static GLuint s_sdr_##_name##_##_kind = 0;
#include "ogl/shaders.inc"
#undef GPU_SHADER

#define GPU_PROG(_vert, _frag) static GPUProg s_prg_##_vert##_##_frag = {};
#include "ogl/progs.inc"
#undef GPU_PROG


static void prepare_texture(sxTextureData* pTex) {
	if (!pTex) return;
	GLuint* pHandle = pTex->get_gpu_wk<GLuint>();
	if (!pHandle) return;
	if (*pHandle) return;
	glGenTextures(1, pHandle);
	if (*pHandle) {
		glBindTexture(GL_TEXTURE_2D, *pHandle);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pTex->mWidth, pTex->mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pTex->get_data_ptr());
		bool mipmapEnabled = pTex->mipmap_enabled();
		if (s_useMipmaps && mipmapEnabled) {
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		def_tex_params(mipmapEnabled);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

static void release_texture(sxTextureData* pTex) {
	if (!pTex) return;
	GLuint* pHandle = pTex->get_gpu_wk<GLuint>();
	if (pHandle) {
		if (*pHandle) {
			glDeleteTextures(1, pHandle);
		}
		*pHandle = 0;
	}
}

static bool is_tex_prepared(sxTextureData* pTex) {
	if (!pTex) return false;
	GLuint* pHandle = pTex->get_gpu_wk<GLuint>();
	if (!pHandle) return false;
	return *pHandle != 0;
}

static GLuint get_tex_handle(sxTextureData* pTex) {
	if (!pTex) return 0;
	prepare_texture(pTex);
	GLuint* pHandle = pTex->get_gpu_wk<GLuint>();
	return pHandle ? *pHandle : 0;
}

static void prepare_model(sxModelData* pMdl) {
	if (!pMdl) return;
	GLuint* pBufIds = pMdl->get_gpu_wk<GLuint>();
	GLuint* pBufVB = &pBufIds[0];
	GLuint* pBufIB16 = &pBufIds[1];
	GLuint* pBufIB32 = &pBufIds[2];
	if (*pBufVB && (*pBufIB16 || *pBufIB32)) {
		return;
	}
	if (pMdl->mSknNum > MAX_JNT) {
		const char* pMdlName = pMdl->get_name();
		nxCore::dbg_msg("Model \"%s\": too many skin nodes (%d).\n", pMdlName, pMdl->mSknNum);
	}
	size_t vsize = pMdl->get_vtx_size();
	if (!(*pBufVB) && vsize > 0) {
		const void* pPntData = pMdl->get_pnt_data_top();
		if (pPntData) {
			glGenBuffers(1, pBufVB);
			if (*pBufVB) {
				glBindBuffer(GL_ARRAY_BUFFER, *pBufVB);
				glBufferData(GL_ARRAY_BUFFER, pMdl->mPntNum * vsize, pPntData, GL_STATIC_DRAW);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}
		}
	}
	if (!(*pBufIB16) && pMdl->mIdx16Num > 0) {
		const uint16_t* pIdxData = pMdl->get_idx16_top();
		if (pIdxData) {
			glGenBuffers(1, pBufIB16);
			if (*pBufIB16) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *pBufIB16);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, pMdl->mIdx16Num * sizeof(uint16_t), pIdxData, GL_STATIC_DRAW);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			}
		}
	}
	if (!(*pBufIB32) && pMdl->mIdx32Num > 0) {
		const uint32_t* pIdxData = pMdl->get_idx32_top();
		if (pIdxData) {
			glGenBuffers(1, pBufIB32);
			if (*pBufIB32) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *pBufIB32);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, pMdl->mIdx32Num * sizeof(uint32_t), pIdxData, GL_STATIC_DRAW);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			}
		}
	}
}

static void release_model(sxModelData* pMdl) {
	if (!pMdl) return;
	GLuint* pBufIds = pMdl->get_gpu_wk<GLuint>();
	GLuint* pBufVB = &pBufIds[0];
	GLuint* pBufIB16 = &pBufIds[1];
	GLuint* pBufIB32 = &pBufIds[2];
	if (*pBufVB) {
		glDeleteBuffers(1, pBufVB);
		*pBufVB = 0;
	}
	if (*pBufIB16) {
		glDeleteBuffers(1, pBufIB16);
		*pBufIB16 = 0;
	}
	if (*pBufIB32) {
		glDeleteBuffers(1, pBufIB32);
		*pBufIB32 = 0;
	}
	pMdl->clear_tex_wk();
}

bool is_mdl_prepared(sxModelData* pMdl) {
	if (!pMdl) return false;
	GLuint* pBufIds = pMdl->get_gpu_wk<GLuint>();
	GLuint bufVB = pBufIds[0];
	GLuint bufIB16 = pBufIds[1];
	GLuint bufIB32 = pBufIds[2];
	return bufVB && (bufIB16 || bufIB32);
}

static void batch_draw_exec(const sxModelData* pMdl, int ibat, int baseVtx = 0) {
	if (!pMdl) return;
	const sxModelData::Batch* pBat = pMdl->get_batch_ptr(ibat);
	if (!pBat) return;
	const GLuint* pBufIds = pMdl->get_gpu_wk<GLuint>();
	GLuint bufVB = pBufIds[0];
	GLuint bufIB16 = pBufIds[1];
	GLuint bufIB32 = pBufIds[2];
	if (!bufVB) return;
	intptr_t org = 0;
	GLenum typ = GL_NONE;
	if (pBat->is_idx16()) {
		if (!bufIB16) return;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufIB16);
		org = pBat->mIdxOrg * sizeof(uint16_t);
		typ = GL_UNSIGNED_SHORT;
	} else {
		if (!bufIB32) return;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufIB32);
		org = pBat->mIdxOrg * sizeof(uint32_t);
		typ = GL_UNSIGNED_INT;
	}
	if (baseVtx > 0) {
		OGLSys::draw_tris_base_vtx(pBat->mTriNum, typ, org, baseVtx);
	} else {
		glDrawElements(GL_TRIANGLES, pBat->mTriNum * 3, typ, (const void*)org);
	}
}

static void set_def_framebuf(const bool useDepth = true) {
	if (s_frameBufMode != 0) {
		int w = OGLSys::get_width();
		int h = OGLSys::get_height();
		OGLSys::bind_def_framebuf();
		glViewport(0, 0, w, h);
		glScissor(0, 0, w, h);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		s_frameBufMode = 0;
	}
}

static void set_shadow_framebuf() {
	if (s_shadowFBO) {
		if (s_frameBufMode != 1) {
			glBindFramebuffer(GL_FRAMEBUFFER, s_shadowFBO);
			glViewport(0, 0, s_shadowSize, s_shadowSize);
			glScissor(0, 0, s_shadowSize, s_shadowSize);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			if (s_shadowDepthBuf && s_shadowCastDepthTest) {
				glDepthMask(GL_TRUE);
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LEQUAL);
			} else {
				glDepthMask(GL_FALSE);
				glDisable(GL_DEPTH_TEST);
			}
			s_frameBufMode = 1;
		}
	}
}

static void set_spr_framebuf() {
	if (s_frameBufMode != 2) {
		int w = OGLSys::get_width();
		int h = OGLSys::get_height();
		OGLSys::bind_def_framebuf();
		glViewport(0, 0, w, h);
		glScissor(0, 0, w, h);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_FALSE);
		glDisable(GL_DEPTH_TEST);
		s_frameBufMode = 2;
	}
}

static bool s_nowSemi = false;

static void gl_opaq() {
	glDisable(GL_BLEND);
}

static void gl_semi() {
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void set_opaq() {
#if DRW_CACHE_BLEND
	if (s_nowSemi) {
		gl_opaq();
		s_nowSemi = false;
	}
#else
	gl_opaq();
#endif
}

static void set_semi() {
#if DRW_CACHE_BLEND
	if (!s_nowSemi) {
		gl_semi();
		s_nowSemi = true;
	}
#else
	gl_semi();
#endif
}

static bool s_nowDblSided = false;

static void gl_dbl_sided() {
	glDisable(GL_CULL_FACE);
}

static void gl_face_cull() {
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
}

static void set_dbl_sided() {
#if DRW_CACHE_DSIDED
	if (!s_nowDblSided) {
		gl_dbl_sided();
		s_nowDblSided = true;
	}
#else
	gl_dbl_sided();
#endif
}

static void set_face_cull() {
#if DRW_CACHE_DSIDED
	if (s_nowDblSided) {
		gl_face_cull();
		s_nowDblSided = false;
	}
#else
	gl_face_cull();
#endif
}


static void init(int shadowSize, cxResourceManager* pRsrcMgr) {
	if (s_drwInitFlg) return;

	if (!pRsrcMgr) return;
	s_pRsrcMgr = pRsrcMgr;
	cxResourceManager::GfxIfc rsrcGfxIfc;
	rsrcGfxIfc.reset();
	rsrcGfxIfc.prepareTexture = prepare_texture;
	rsrcGfxIfc.releaseTexture = release_texture;
	rsrcGfxIfc.prepareModel = prepare_model;
	rsrcGfxIfc.releaseModel = release_model;
	s_pRsrcMgr->set_gfx_ifc(rsrcGfxIfc);

#define GPU_SHADER(_name, _kind) s_sdr_##_name##_##_kind = load_shader(#_name "." #_kind);
#include "ogl/shaders.inc"
#undef GPU_SHADER

	int prgCnt = 0;
	int prgOK = 0;
#define GPU_PROG(_vert_name, _frag_name) s_prg_##_vert_name##_##_frag_name.init(VtxFmt_##_vert_name, s_sdr_##_vert_name##_vert, s_sdr_##_frag_name##_frag); ++prgCnt; if (s_prg_##_vert_name##_##_frag_name.is_valid()) {++prgOK;} else { nxCore::dbg_msg("GPUProg init error: %s + %s\n", #_vert_name, #_frag_name); }
#include "ogl/progs.inc"
#undef GPU_PROG

	nxCore::dbg_msg("GPU progs: %d/%d\n", prgOK, prgCnt);

	glGenBuffers(1, &s_sprVBO);
	glGenBuffers(1, &s_sprIBO);
	if (s_sprVBO && s_sprIBO) {
		static GLfloat sprVBData[] = { 0.0f, 1.0f, 2.0f, 3.0f };
		static uint16_t sprIBData[] = { 0, 1, 3, 2 };
		glBindBuffer(GL_ARRAY_BUFFER, s_sprVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(sprVBData), sprVBData, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_sprIBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(sprIBData), sprIBData, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	if (shadowSize > 0) {
		s_shadowSize = shadowSize;
		glGenFramebuffers(1, &s_shadowFBO);
		if (s_shadowFBO) {
			glGenTextures(1, &s_shadowTex);
			if (s_shadowTex) {
				glBindFramebuffer(GL_FRAMEBUFFER, s_shadowFBO);
				glBindTexture(GL_TEXTURE_2D, s_shadowTex);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s_shadowSize, s_shadowSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_shadowTex, 0);
				glBindTexture(GL_TEXTURE_2D, 0);
				glGenRenderbuffers(1, &s_shadowDepthBuf);
				if (s_shadowDepthBuf) {
					glBindRenderbuffer(GL_RENDERBUFFER, s_shadowDepthBuf);
					glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, s_shadowSize, s_shadowSize);
					glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_shadowDepthBuf);
				}
				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
					glDeleteTextures(1, &s_shadowTex);
					s_shadowTex = 0;
					OGLSys::bind_def_framebuf();
					if (s_shadowDepthBuf) {
						glDeleteRenderbuffers(1, &s_shadowDepthBuf);
						s_shadowDepthBuf = 0;
					}
					glDeleteFramebuffers(1, &s_shadowFBO);
					s_shadowFBO = 0;
				}
			} else {
				glDeleteFramebuffers(1, &s_shadowFBO);
				s_shadowFBO = 0;
			}
		}
	}
	s_shadowCastDepthTest = true;
	s_frameBufMode = -1;
	set_def_framebuf();

	gl_opaq();
	s_nowSemi = false;

	gl_face_cull();
	s_nowDblSided = false;

	s_pNowProg = nullptr;

	s_drwInitFlg = true;
}

static void reset() {
	if (!s_drwInitFlg) return;

	OGLSys::bind_def_framebuf();

	if (s_shadowDepthBuf) {
		glDeleteRenderbuffers(1, &s_shadowDepthBuf);
		s_shadowDepthBuf = 0;
	}
	if (s_shadowTex) {
		glDeleteTextures(1, &s_shadowTex);
		s_shadowTex = 0;
	}
	if (s_shadowFBO) {
		glDeleteFramebuffers(1, &s_shadowFBO);
		s_shadowFBO = 0;
	}

	if (s_sprIBO) {
		glDeleteBuffers(1, &s_sprIBO);
		s_sprIBO = 0;
	}
	if (s_sprVBO) {
		glDeleteBuffers(1, &s_sprVBO);
		s_sprVBO = 0;
	}

#define GPU_PROG(_vert_name, _frag_name) s_prg_##_vert_name##_##_frag_name.reset();
#include "ogl/progs.inc"
#undef GPU_PROG

#define GPU_SHADER(_name, _kind) glDeleteShader(s_sdr_##_name##_##_kind); s_sdr_##_name##_##_kind = 0;
#include "ogl/shaders.inc"
#undef GPU_SHADER

	s_pNowProg = nullptr;

	s_drwInitFlg = false;
}

static int get_screen_width() {
	return OGLSys::get_width();
}

static int get_screen_height() {
	return OGLSys::get_height();
}

static cxMtx get_shadow_bias_mtx() {
	static const float bias[4 * 4] = {
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	};
	return nxMtx::from_mem(bias);
}


static void clear_shadow() {
	if (s_shadowFBO && s_frameBufMode == 1) {
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
}

static void begin(const cxColor& clearColor) {
	s_frameBufMode = -1;
	set_shadow_framebuf();
	clear_shadow();
	set_def_framebuf();
	glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	s_batDrwCnt = 0;
	s_shadowCastCnt = 0;
}

static void end() {
	OGLSys::swap();
}

static GPUProg* prog_sel(const cxModelWork* pWk, const int ibat, const sxModelData::Material* pMtl, const Draw::Mode mode, const Draw::Context* pCtx) {
	GPUProg* pProg = nullptr;
	sxModelData* pMdl = pWk->mpData;
	if (mode == Draw::DRWMODE_SHADOW_CAST) {
		if (s_shadowFBO && pMtl->mFlags.shadowCast) {
			if (pMdl->half_encoding()) {
				if (pMdl->has_skin()) {
					if (pMtl->mFlags.alpha) {
						pProg = &s_prg_skin0_cast_semi;
					} else {
						pProg = &s_prg_skin0_cast_opaq;
					}
				} else {
					if (pMtl->mFlags.alpha) {
						pProg = &s_prg_rigid0_cast_semi;
					} else {
						pProg = &s_prg_rigid0_cast_opaq;
					}
				}
			} else {
				if (pMdl->has_skin()) {
					if (pMtl->mFlags.alpha) {
						pProg = &s_prg_skin1_cast_semi;
					} else {
						pProg = &s_prg_skin1_cast_opaq;
					}
				} else {
					if (pMtl->mFlags.alpha) {
						pProg = &s_prg_rigid1_cast_semi;
					} else {
						pProg = &s_prg_rigid1_cast_opaq;
					}
				}
			}
		}
	} else {
		bool recvFlg = pMtl->mFlags.shadowRecv && (s_shadowFBO != 0) && (s_shadowCastCnt > 0) && (pCtx->shadow.get_density() > 0.0f);
		bool specFlg = pCtx->glb.useSpec && pCtx->spec.enabled && pMtl->mFlags.baseMapSpecAlpha;
		if (recvFlg) {
			if (pWk->mBoundsValid && pWk->mpBatBBoxes) {
				recvFlg = pCtx->ck_bbox_shadow_receive(pWk->mpBatBBoxes[ibat]);
			}
		}
		bool bumpFlg = false;
		if (pCtx->glb.useBump && OGLSys::ext_ck_derivatives()) {
			if (pMtl->mBumpScale > 0.0f) {
				int tid = pMtl->mBumpTexId;
				if (tid >= 0) {
					sxModelData::TexInfo* pTexInfo = pMdl->get_tex_info(tid);
					GLuint* pTexHandle = pTexInfo->get_wk<GLuint>();
					if (*pTexHandle == 0) {
						const char* pTexName = pMdl->get_tex_name(tid);
						sxTextureData* pTex = s_pRsrcMgr->find_texture_for_model(pMdl, pTexName);
						*pTexHandle = get_tex_handle(pTex);
					}
					if (*pTexHandle) {
						bumpFlg = true;
					}
				}
			}
		}
		if ((!pMtl->mFlags.alpha) || (mode == Draw::DRWMODE_DISCARD && !pMtl->mFlags.forceBlend)) {
			if (pMdl->half_encoding()) {
				if (bumpFlg) {
					// fmt0 bump
					if (specFlg) {
						if (pMdl->has_skin()) {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_skin0_hemi_spec_bump_discard_sdw : &s_prg_skin0_hemi_spec_bump_discard;
							} else {
								pProg = recvFlg ? &s_prg_skin0_hemi_spec_bump_opaq_sdw : &s_prg_skin0_hemi_spec_bump_opaq;
							}
						} else {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_rigid0_hemi_spec_bump_discard_sdw : &s_prg_rigid0_hemi_spec_bump_discard;
							} else {
								pProg = recvFlg ? &s_prg_rigid0_hemi_spec_bump_opaq_sdw : &s_prg_rigid0_hemi_spec_bump_opaq;
							}
						}
					} else {
						if (pMdl->has_skin()) {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_skin0_hemi_bump_discard_sdw : &s_prg_skin0_hemi_bump_discard;
							} else {
								pProg = recvFlg ? &s_prg_skin0_hemi_bump_opaq_sdw : &s_prg_skin0_hemi_bump_opaq;
							}
						} else {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_rigid0_hemi_bump_discard_sdw : &s_prg_rigid0_hemi_bump_discard;
							} else {
								pProg = recvFlg ? &s_prg_rigid0_hemi_bump_opaq_sdw : &s_prg_rigid0_hemi_bump_opaq;
							}
						}
					}
				} else {
					// fmt0 !bump
					if (specFlg) {
						if (pMdl->has_skin()) {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_skin0_hemi_spec_discard_sdw : &s_prg_skin0_hemi_spec_discard;
							} else {
								pProg = recvFlg ? &s_prg_skin0_hemi_spec_opaq_sdw : &s_prg_skin0_hemi_spec_opaq;
							}
						} else {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_rigid0_hemi_spec_discard_sdw : &s_prg_rigid0_hemi_spec_discard;
							} else {
								pProg = recvFlg ? &s_prg_rigid0_hemi_spec_opaq_sdw : &s_prg_rigid0_hemi_spec_opaq;
							}
						}
					} else {
						if (pMdl->has_skin()) {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_skin0_hemi_discard_sdw : &s_prg_skin0_hemi_discard;
							} else {
								pProg = recvFlg ? &s_prg_skin0_hemi_opaq_sdw : &s_prg_skin0_hemi_opaq;
							}
						} else {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_rigid0_hemi_discard_sdw : &s_prg_rigid0_hemi_discard;
							} else {
								pProg = recvFlg ? &s_prg_rigid0_hemi_opaq_sdw : &s_prg_rigid0_hemi_opaq;
							}
						}
					}
				}
			} else {
				if (bumpFlg) {
					// fmt1 bump
					if (specFlg) {
						if (pMdl->has_skin()) {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_skin1_hemi_spec_bump_discard_sdw : &s_prg_skin1_hemi_spec_bump_discard;
							} else {
								pProg = recvFlg ? &s_prg_skin1_hemi_spec_bump_opaq_sdw : &s_prg_skin1_hemi_spec_bump_opaq;
							}
						} else {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_rigid1_hemi_spec_bump_discard_sdw : &s_prg_rigid1_hemi_spec_bump_discard;
							} else {
								pProg = recvFlg ? &s_prg_rigid1_hemi_spec_bump_opaq_sdw : &s_prg_rigid1_hemi_spec_bump_opaq;
							}
						}
					} else {
						if (pMdl->has_skin()) {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_skin1_hemi_bump_discard_sdw : &s_prg_skin1_hemi_bump_discard;
							} else {
								pProg = recvFlg ? &s_prg_skin1_hemi_bump_opaq_sdw : &s_prg_skin1_hemi_bump_opaq;
							}
						} else {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_rigid1_hemi_bump_discard_sdw : &s_prg_rigid1_hemi_bump_discard;
							} else {
								pProg = recvFlg ? &s_prg_rigid1_hemi_bump_opaq_sdw : &s_prg_rigid1_hemi_bump_opaq;
							}
						}
					}
				} else {
					// fmt1 !bump
					if (specFlg) {
						if (pMdl->has_skin()) {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_skin1_hemi_spec_discard_sdw : &s_prg_skin1_hemi_spec_discard;
							} else {
								pProg = recvFlg ? &s_prg_skin1_hemi_spec_opaq_sdw : &s_prg_skin1_hemi_spec_opaq;
							}
						} else {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_rigid1_hemi_spec_discard_sdw : &s_prg_rigid1_hemi_spec_discard;
							} else {
								pProg = recvFlg ? &s_prg_rigid1_hemi_spec_opaq_sdw : &s_prg_rigid1_hemi_spec_opaq;
							}
						}
					} else {
						if (pMdl->has_skin()) {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_skin1_hemi_discard_sdw : &s_prg_skin1_hemi_discard;
							} else {
								pProg = recvFlg ? &s_prg_skin1_hemi_opaq_sdw : &s_prg_skin1_hemi_opaq;
							}
						} else {
							if (pMtl->mFlags.alpha) {
								pProg = recvFlg ? &s_prg_rigid1_hemi_discard_sdw : &s_prg_rigid1_hemi_discard;
							} else {
								pProg = recvFlg ? &s_prg_rigid1_hemi_opaq_sdw : &s_prg_rigid1_hemi_opaq;
							}
						}
					}
				}
			}
		} else {
			if (pMtl->mFlags.alpha) {
				if (pMdl->half_encoding()) {
					if (specFlg) {
						if (pMtl->is_cutout()) {
							if (pMdl->has_skin()) {
								pProg = recvFlg ? &s_prg_skin0_hemi_spec_limit_sdw : &s_prg_skin0_hemi_spec_limit;
							} else {
								pProg = recvFlg ? &s_prg_rigid0_hemi_spec_limit_sdw : &s_prg_rigid0_hemi_spec_limit;
							}
						} else {
							if (pMdl->has_skin()) {
								pProg = recvFlg ? &s_prg_skin0_hemi_spec_semi_sdw : &s_prg_skin0_hemi_spec_semi;
							} else {
								pProg = recvFlg ? &s_prg_rigid0_hemi_spec_semi_sdw : &s_prg_rigid0_hemi_spec_semi;
							}
						}
					} else {
						if (pMtl->is_cutout()) {
							if (pMdl->has_skin()) {
								pProg = recvFlg ? &s_prg_skin0_hemi_limit_sdw : &s_prg_skin0_hemi_limit;
							} else {
								pProg = recvFlg ? &s_prg_rigid0_hemi_limit_sdw : &s_prg_rigid0_hemi_limit;
							}
						} else {
							if (pMdl->has_skin()) {
								pProg = recvFlg ? &s_prg_skin0_hemi_semi_sdw : &s_prg_skin0_hemi_semi;
							} else {
								pProg = recvFlg ? &s_prg_rigid0_hemi_semi_sdw : &s_prg_rigid0_hemi_semi;
							}
						}
					}
				} else {
					if (specFlg) {
						if (pMtl->mAlphaLim > 0.0f) {
							if (pMdl->has_skin()) {
								pProg = recvFlg ? &s_prg_skin1_hemi_spec_limit_sdw : &s_prg_skin1_hemi_spec_limit;
							} else {
								pProg = recvFlg ? &s_prg_rigid1_hemi_spec_limit_sdw : &s_prg_rigid1_hemi_spec_limit;
							}
						} else {
							if (pMdl->has_skin()) {
								pProg = recvFlg ? &s_prg_skin1_hemi_spec_semi_sdw : &s_prg_skin1_hemi_spec_semi;
							} else {
								pProg = recvFlg ? &s_prg_rigid1_hemi_spec_semi_sdw : &s_prg_rigid1_hemi_spec_semi;
							}
						}
					} else {
						if (pMtl->mAlphaLim > 0.0f) {
							if (pMdl->has_skin()) {
								pProg = recvFlg ? &s_prg_skin1_hemi_limit_sdw : &s_prg_skin1_hemi_limit;
							} else {
								pProg = recvFlg ? &s_prg_rigid1_hemi_limit_sdw : &s_prg_rigid1_hemi_limit;
							}
						} else {
							if (pMdl->has_skin()) {
								pProg = recvFlg ? &s_prg_skin1_hemi_semi_sdw : &s_prg_skin1_hemi_semi;
							} else {
								pProg = recvFlg ? &s_prg_rigid1_hemi_semi_sdw : &s_prg_rigid1_hemi_semi;
							}
						}
					}
				}
			} else {
				if (specFlg) {
					if (pMdl->half_encoding()) {
						if (pMdl->has_skin()) {
							pProg = recvFlg ? &s_prg_skin0_hemi_spec_opaq_sdw : &s_prg_skin0_hemi_spec_opaq;
						} else {
							pProg = recvFlg ? &s_prg_rigid0_hemi_spec_opaq_sdw : &s_prg_rigid0_hemi_spec_opaq;
						}
					} else {
						if (pMdl->has_skin()) {
							pProg = recvFlg ? &s_prg_skin1_hemi_spec_opaq_sdw : &s_prg_skin1_hemi_spec_opaq;
						} else {
							pProg = recvFlg ? &s_prg_rigid1_hemi_spec_opaq_sdw : &s_prg_rigid1_hemi_spec_opaq;
						}
					}
				} else {
					if (pMdl->half_encoding()) {
						if (pMdl->has_skin()) {
							pProg = recvFlg ? &s_prg_skin0_hemi_opaq_sdw : &s_prg_skin0_hemi_opaq;
						} else {
							pProg = recvFlg ? &s_prg_rigid0_hemi_opaq_sdw : &s_prg_rigid0_hemi_opaq;
						}
					} else {
						if (pMdl->has_skin()) {
							pProg = recvFlg ? &s_prg_skin1_hemi_opaq_sdw : &s_prg_skin1_hemi_opaq;
						} else {
							pProg = recvFlg ? &s_prg_rigid1_hemi_opaq_sdw : &s_prg_rigid1_hemi_opaq;
						}
					}
				}
			}
		}
	}
	return pProg;
}


#define NFLT_JMTX (MAX_JNT_PER_BATCH * 3 * 4)
#define NFLT_JMAP (MAX_JNT)

#define HAS_PARAM(_name) (pProg->mParamLink._name >= 0)

static void batch(cxModelWork* pWk, const int ibat, const Draw::Mode mode, const Draw::Context* pCtx) {
	float ftmp[NFLT_JMTX > NFLT_JMAP ? NFLT_JMTX : NFLT_JMAP];

	if (!pCtx) return;

	if (!pWk) return;
	sxModelData* pMdl = pWk->mpData;
	if (!pMdl) return;
	if (!pMdl->ck_batch_id(ibat)) return;

	Draw::MdlParam* pParam = (Draw::MdlParam*)pWk->mpParamMem;

	bool isShadowCast = (mode == Draw::DRWMODE_SHADOW_CAST);
	bool isDiscard = (mode == Draw::DRWMODE_DISCARD);

	const sxModelData::Batch* pBat = pMdl->get_batch_ptr(ibat);
	const sxModelData::Material* pMtl = pMdl->get_material(pBat->mMtlId);
	if (pWk->mVariation != 0) {
		if (pMdl->mtl_has_swaps(pBat->mMtlId)) {
			const sxModelData::Material* pSwapMtl = pMdl->get_swap_material(pBat->mMtlId, pWk->mVariation);
			if (pSwapMtl) {
				pMtl = pSwapMtl;
			}
		}
	}
	if (!pMtl) return;

	GPUProg* pProg = prog_sel(pWk, ibat, pMtl, mode, pCtx);
	if (!pProg) return;
	if (!pProg->is_valid()) return;

	if (isShadowCast) {
		set_shadow_framebuf();
		OGLSys::enable_msaa(false);
	} else {
		set_def_framebuf();
		OGLSys::enable_msaa(true);
	}

	prepare_model(pMdl);

	pProg->use();

	pProg->set_view_proj(isShadowCast ? pCtx->shadow.viewProjMtx : pCtx->view.viewProjMtx);
	pProg->set_view_pos(pCtx->view.pos);

	if (HAS_PARAM(World)) {
		xt_xmtx wm;
		if (pWk->mpWorldXform) {
			wm = *pWk->mpWorldXform;
		} else {
			wm.identity();
		}
		pProg->set_world(wm);
	}

	if (HAS_PARAM(SkinMtx) && pWk->mpSkinXforms) {
		xt_xmtx* pSkin = (xt_xmtx*)ftmp;
		const int32_t* pJntLst = pMdl->get_batch_jnt_list(ibat);
		for (int i = 0; i < pBat->mJntNum; ++i) {
			pSkin[i] = pWk->mpSkinXforms[pJntLst[i]];
		}
		glUniform4fv(pProg->mParamLink.SkinMtx, JMTX_SIZE, (const GLfloat*)pSkin);
	}

	if (HAS_PARAM(SkinMap)) {
		const int32_t* pJntLst = pMdl->get_batch_jnt_list(ibat);
		if (pJntLst) {
			::memset(ftmp, 0, NFLT_JMAP * sizeof(float));
			int njnt = pBat->mJntNum;
			for (int i = 0; i < njnt; ++i) {
				ftmp[pJntLst[i]] = float(i);
			}
			for (int i = 0; i < NFLT_JMAP; ++i) {
				ftmp[i] *= 3.0f;
			}
#if DRW_LIMIT_JMAP
			int njmax = pJntLst[njnt - 1] + 1;
			int nv = (njmax >> 2) + ((njmax & 3) != 0 ? 1 : 0);
			glUniform4fv(pProg->mParamLink.SkinMap, nv, ftmp);
#else
			glUniform4fv(pProg->mParamLink.SkinMap, JMAP_SIZE, ftmp);
#endif
		}
	}

	if (HAS_PARAM(PosBase)) {
		cxVec posBase = pMdl->mBBox.get_min_pos();
		pProg->set_pos_base(posBase);
	}

	if (HAS_PARAM(PosScale)) {
		cxVec posScale = pMdl->mBBox.get_size_vec();
		pProg->set_pos_scale(posScale);
	}

	pProg->set_hemi_upper(pCtx->hemi.upper);
	pProg->set_hemi_lower(pCtx->hemi.lower);
	pProg->set_hemi_up(pCtx->hemi.up);
	if (HAS_PARAM(HemiParam)) {
		xt_float3 hparam;
		hparam.set(pCtx->hemi.exp, pCtx->hemi.gain, 0.0f);
		pProg->set_hemi_param(hparam);
	}

	pProg->set_spec_light_dir(pCtx->spec.dir);
	if (HAS_PARAM(SpecLightColor)) {
		xt_float4 sclr;
		::memcpy(sclr, pCtx->spec.clr, sizeof(xt_float3));
		sclr.w = pCtx->spec.shadowing;
		pProg->set_spec_light_color(sclr);
	}

	pProg->set_shadow_mtx(pCtx->shadow.mtx);

	if (HAS_PARAM(ShadowSize)) {
		float ss = float(s_shadowSize);
		xt_float4 shadowSize;
		shadowSize.set(ss, ss, nxCalc::rcp0(ss), nxCalc::rcp0(ss));
		pProg->set_shadow_size(shadowSize);
	}

	if (HAS_PARAM(ShadowCtrl)) {
		xt_float4 shadowCtrl;

		float swght = pMtl->mShadowWght;
		if (pParam) {
			swght += pParam->shadowWeightBias;
		}
		if (swght > 0.0f) {
			swght = nxCalc::max(swght + pCtx->shadow.wghtBias, 1.0f);
		}

		float soffs = pMtl->mShadowOffs + pCtx->shadow.offsBias;
		if (pParam) {
			soffs += pParam->shadowOffsBias;
		}

		float sdens = pMtl->mShadowDensity * pCtx->shadow.get_density();
		if (pParam) {
			sdens *= nxCalc::saturate(pParam->shadowDensScl);
		}

		shadowCtrl.set(soffs, swght, sdens, 0.0f);
		pProg->set_shadow_ctrl(shadowCtrl);
	}

	if (HAS_PARAM(ShadowFade)) {
		xt_float4 shadowFade;
		shadowFade.set(pCtx->shadow.fadeStart, nxCalc::rcp0(pCtx->shadow.fadeEnd - pCtx->shadow.fadeStart), 0.0f, 0.0f);
		pProg->set_shadow_fade(shadowFade);
	}

	if (HAS_PARAM(BaseColor)) {
		xt_float3 baseClr = pMtl->mBaseColor;
		if (pParam) {
			for (int i = 0; i < 3; ++i) {
				baseClr[i] *= pParam->baseColorScl[i];
			}
		}
		pProg->set_base_color(baseClr);
	}

	pProg->set_spec_color(pMtl->mSpecColor);

	if (HAS_PARAM(SurfParam)) {
		xt_float4 sprm;
		sprm.set(pMtl->mRoughness, pMtl->mFresnel, 0.0f, 0.0f);
		pProg->set_surf_param(sprm);
	}

	if (HAS_PARAM(BumpParam)) {
		xt_float4 bprm;
		float sclT = pMtl->mFlags.flipTangent ? -1.0f : 1.0f;
		float sclB = -(pMtl->mFlags.flipBitangent ? -1.0f : 1.0f);
		bprm.set(pMtl->mBumpScale, sclT, sclB, 0.0f);
		pProg->set_bump_param(bprm);
	}

	if (HAS_PARAM(AlphaCtrl)) {
		float alphaLim = isShadowCast ? pMtl->mShadowAlphaLim : pMtl->mAlphaLim;
		if (isDiscard) {
			if (alphaLim <= 0.0f) {
				alphaLim = pMtl->mShadowAlphaLim;
			}
		}
		xt_float3 alphaCtrl;
		alphaCtrl.set(alphaLim, 0.0f, 0.0f);
		pProg->set_alpha_ctrl(alphaCtrl);
	}

	pProg->set_fog_color(pCtx->fog.color);
	pProg->set_fog_param(pCtx->fog.param);

	if (HAS_PARAM(InvWhite)) {
		xt_float3 invWhite;
		invWhite.set(nxCalc::rcp0(pCtx->cc.linWhite.x), nxCalc::rcp0(pCtx->cc.linWhite.y), nxCalc::rcp0(pCtx->cc.linWhite.z));
		pProg->set_inv_white(invWhite);
	}

	pProg->set_lclr_gain(pCtx->cc.linGain);
	pProg->set_lclr_bias(pCtx->cc.linBias);
	pProg->set_exposure(pCtx->cc.exposure);

	if (HAS_PARAM(InvGamma)) {
		xt_float3 invGamma;
		invGamma.set(nxCalc::rcp0(pCtx->cc.gamma.x), nxCalc::rcp0(pCtx->cc.gamma.y), nxCalc::rcp0(pCtx->cc.gamma.z));
		pProg->set_inv_gamma(invGamma);
	}

	if (pProg->mSmpLink.Base >= 0) {
		GLuint htex = 0;
		if (s_pRsrcMgr) {
			int tid = pMtl->mBaseTexId;
			if (tid >= 0) {
				sxModelData::TexInfo* pTexInfo = pMdl->get_tex_info(tid);
				GLuint* pTexHandle = pTexInfo->get_wk<GLuint>();
				if (*pTexHandle == 0) {
					const char* pTexName = pMdl->get_tex_name(tid);
					sxTextureData* pTex = s_pRsrcMgr->find_texture_for_model(pMdl, pTexName);
					*pTexHandle = get_tex_handle(pTex);
				}
				htex = *pTexHandle;
			}
		}
		if (!htex) {
			htex = OGLSys::get_white_tex();
		}
		glActiveTexture(GL_TEXTURE0 + Draw::TEXUNIT_Base);
		glBindTexture(GL_TEXTURE_2D, htex);
	}

	if (pProg->mSmpLink.Bump >= 0 && s_pRsrcMgr) {
		int tid = pMtl->mBumpTexId;
		if (tid >= 0) {
			sxModelData::TexInfo* pTexInfo = pMdl->get_tex_info(tid);
			GLuint* pTexHandle = pTexInfo->get_wk<GLuint>();
			if (*pTexHandle == 0) {
				const char* pTexName = pMdl->get_tex_name(tid);
				sxTextureData* pTex = s_pRsrcMgr->find_texture_for_model(pMdl, pTexName);
				*pTexHandle = get_tex_handle(pTex);
			}
			if (*pTexHandle) {
				glActiveTexture(GL_TEXTURE0 + Draw::TEXUNIT_Bump);
				glBindTexture(GL_TEXTURE_2D, *pTexHandle);
			}
		}
	}

	if (pProg->mSmpLink.Shadow >= 0) {
		glActiveTexture(GL_TEXTURE0 + Draw::TEXUNIT_Shadow);
		glBindTexture(GL_TEXTURE_2D, s_shadowTex);
	}

	if (isShadowCast || (isDiscard && !pMtl->mFlags.forceBlend)) {
		set_opaq();
	} else {
		if (pMtl->mFlags.alpha) {
			set_semi();
		} else {
			set_opaq();
		}
	}

	if (pMtl->mFlags.dblSided) {
		set_dbl_sided();
	} else {
		set_face_cull();
	}

	GLuint* pBufIds = pMdl->get_gpu_wk<GLuint>();
	GLuint bufVB = pBufIds[0];
	if (!bufVB) return;
	if (pProg->mVAO) {
		OGLSys::bind_vao(pProg->mVAO);
		glBindBuffer(GL_ARRAY_BUFFER, bufVB);
		pProg->enable_attrs(pBat->mMinIdx, pMdl->get_vtx_size());
		batch_draw_exec(pMdl, ibat);
		OGLSys::bind_vao(0);
	} else {
		glBindBuffer(GL_ARRAY_BUFFER, bufVB);
		pProg->enable_attrs(pBat->mMinIdx, pMdl->get_vtx_size());
		batch_draw_exec(pMdl, ibat);
		pProg->disable_attrs();
	}

	++s_batDrwCnt;

	if (isShadowCast) {
		++s_shadowCastCnt;
	}
}

void sprite(Draw::Sprite* pSpr) {
	if (!pSpr) return;
	if (pSpr->color.a == 0.0f) return;
	GLuint htex = get_tex_handle(pSpr->pTex);
	if (!htex) {
		htex = OGLSys::get_white_tex();
	}
	set_spr_framebuf();
	set_semi();
	set_face_cull();
	GPUProg* pProg = &s_prg_sprite_sprite;
	pProg->use();
	xt_float4 pos[2];
	float* pPosSrc = pSpr->pos[0];
	float* pPosDst = pos[0];
	for (int i = 0; i < 8; ++i) {
		pPosDst[i] = pPosSrc[i] + 0.5f;
	}
	float scl[2];
	scl[0] = nxCalc::rcp0(pSpr->refWidth);
	scl[1] = nxCalc::rcp0(pSpr->refHeight);
	for (int i = 0; i < 8; ++i) {
		pPosDst[i] *= scl[i & 1];
	}
	for (int i = 0; i < 4; ++i) {
		int idx = i*2 + 1;
		pPosDst[idx] = 1.0f - pPosDst[idx];
	}
	for (int i = 0; i < 8; ++i) {
		pPosDst[i] *= 2.0f;
	}
	for (int i = 0; i < 8; ++i) {
		pPosDst[i] -= 1.0f;
	}
	cxColor clr[4];
	float* pClrDst = clr[0].ch;
	float* pClr = pSpr->color.ch;
	if (pSpr->pClrs) {
		float* pClrSrc = pSpr->pClrs[0].ch;
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				*pClrDst++ = pClrSrc[j] * pClr[j];
			}
			pClrSrc += 4;
		}
	} else {
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				*pClrDst++ = pClr[j];
			}
		}
	}
	if (HAS_PARAM(SprVtxPos)) {
		glUniform4fv(pProg->mParamLink.SprVtxPos, 2, pos[0]);
	}
	if (HAS_PARAM(SprVtxTex)) {
		glUniform4fv(pProg->mParamLink.SprVtxTex, 2, pSpr->tex[0]);
	}
	if (HAS_PARAM(SprVtxClr)) {
		glUniform4fv(pProg->mParamLink.SprVtxClr, 4, clr[0].ch);
	}
	if (HAS_PARAM(InvGamma)) {
		xt_float3 invGamma;
		invGamma.set(nxCalc::rcp0(pSpr->gamma.x), nxCalc::rcp0(pSpr->gamma.y), nxCalc::rcp0(pSpr->gamma.z));
		pProg->set_inv_gamma(invGamma);
	}
	glActiveTexture(GL_TEXTURE0 + Draw::TEXUNIT_Base);
	glBindTexture(GL_TEXTURE_2D, htex);
	if (pProg->mVAO) {
		OGLSys::bind_vao(pProg->mVAO);
	}
	glBindBuffer(GL_ARRAY_BUFFER, s_sprVBO);
	pProg->enable_attrs(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_sprIBO);
	glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_SHORT, (const void*)0);
	if (pProg->mVAO) {
		OGLSys::bind_vao(0);
	} else {
		pProg->disable_attrs();
	}
}

static Draw::Ifc s_ifc;

struct DrwInit {
	DrwInit() {
		::memset(&s_ifc, 0, sizeof(s_ifc));
		s_ifc.info.pName = "ogl";
		s_ifc.info.needOGLContext = true;
		s_ifc.init = init;
		s_ifc.reset = reset;
		s_ifc.get_screen_width = get_screen_width;
		s_ifc.get_screen_height = get_screen_height;
		s_ifc.get_shadow_bias_mtx = get_shadow_bias_mtx;
		s_ifc.begin = begin;
		s_ifc.end = end;
		s_ifc.batch = batch;
		s_ifc.sprite = sprite;
		Draw::register_ifc_impl(&s_ifc);
	}
} s_drwInit;

DRW_IMPL_END

