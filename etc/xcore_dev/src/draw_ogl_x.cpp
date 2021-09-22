#include "crosscore.hpp"
#include "oglsys.hpp"
#include "draw.hpp"

#include "ogl_x/_inc_def.glsl"

DRW_IMPL_BEGIN

static bool s_useMipmaps = true;

static bool s_useSharedXformBuf = true;

static bool s_useFSTriVB = true;
static GLuint s_fsTriVBO = 0;

static cxResourceManager* s_pRsrcMgr = nullptr;

static const int c_maxCtx = 100;
static int s_ctxStoreCnt = 0;
static Draw::Context s_ctxStore[c_maxCtx];
static int s_ctxUseCnt[c_maxCtx];

struct GPUVtx {
	xt_float3   pos;
	xt_float3   nrm;
	xt_texcoord tex;
	xt_rgba     clr;
	xt_float4   wgt;
	xt_int4     idx;
};

struct GPXform {
	xt_mtx viewProj;
	xt_xmtx xforms[MAX_XFORMS];
};

static struct GPUInfo {
	GLint mMaxUniformBlkSize;
	GLint mMaxVertUniformComps;
	GLint mMaxFragUniformComps;
	GLint mMaxVertUniformVecs;
	GLint mMaxFragUniformVecs;
	GLint mMaxVertUniformBlocks;
	GLint mMaxFragUniformBlocks;
	GLint mMaxDrawBufs;

	void init() {
		glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &mMaxUniformBlkSize);
		glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &mMaxVertUniformComps);
		glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &mMaxFragUniformComps);
		glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &mMaxVertUniformVecs);
		glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &mMaxFragUniformVecs);
		glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS, &mMaxVertUniformBlocks);
		glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &mMaxFragUniformBlocks);
		glGetIntegerv(GL_MAX_DRAW_BUFFERS, &mMaxDrawBufs);
		if (1) {
			nxCore::dbg_msg("GPU info:\n");
			nxCore::dbg_msg(" max uniform block size: %d\n", mMaxUniformBlkSize);
			nxCore::dbg_msg(" max uniform vertex components: %d\n", mMaxVertUniformComps);
			nxCore::dbg_msg(" max uniform fragment components: %d\n", mMaxFragUniformComps);
			nxCore::dbg_msg(" max uniform vertex vectors: %d\n", mMaxVertUniformVecs);
			nxCore::dbg_msg(" max uniform fragment vectors: %d\n", mMaxFragUniformVecs);
			nxCore::dbg_msg(" max render targets: %d\n", mMaxDrawBufs);
			nxCore::dbg_msg(" need jnt map: %s\n", need_jnt_map() ? "true" : "false");
		}
	}

	bool need_jnt_map() const { return mMaxVertUniformVecs < 4 + MAX_XFORMS*3; }

} s_GPUInfo = {};

struct GPUParamsBuf : public OGLSysParamsBuf {
	using OGLSysParamsBuf::set;
	void set(const int idx, const xt_float3& v) { set(idx, v.x, v.y, v.z); }
	void set(const char* pName, const xt_float3& v) { set(find_field(pName), v); }
	void set(const int idx, const xt_float4& v) { set(idx, v.x, v.y, v.z, v.w); }
	void set(const char* pName, const xt_float4& v) { set(find_field(pName), v); }
};

static GLuint s_pidGBuf = 0;
static GLint s_locPos = -1;
static GLint s_locNrm = -1;
static GLint s_locTex = -1;
static GLint s_locClr = -1;
static GLint s_locWgt = -1;
static GLint s_locIdx = -1;

static GLint s_gpLocViewProj = -1;
static GLint s_gpLocXforms = -1;
static GLint s_gpLocXformMap = -1;

static GPUParamsBuf s_gbufParamsGPWPos = {};

static GLuint s_gpIdxXform = (GLuint)-1;
static GPXform s_gpXform;

static const GLuint gpBindingXform = 0;
static const GLuint gpBindingView = 1;
static const GLuint gpBindingModel = 2;
static const GLuint gpBindingMaterial = 3;

static const GLuint gpBindingHemi = 0;
static const GLuint gpBindingColor = 1;

static GLint s_locSmpBase = -1;
static GLint s_locSmpNorm = -1;

static GLuint s_pidImgCpy = 0;
static GLint s_locSmpImgCpy = -1;

static GLuint s_pidDispClr = 0;
static GLint s_locSmpDispClr = -1;
static GLint s_locSmpClrDispClr = -1;
static GPUParamsBuf s_dispClrParams = {};

static GLuint s_pidDispNrm = 0;
static GLint s_locSmpDispNrm = -1;

static GLuint s_pidDispFog = 0;
static GLint s_locSmpDispFog = -1;
static GPUParamsBuf s_dispFogParamsGPWPos = {};
static GPUParamsBuf s_dispFogParamsGPFog = {};

static GLuint s_pidHemi = 0;
static GLint s_locSmpHemiDiff = -1;
static GLint s_locSmpHemiClr = -1;
static GLint s_locSmpHemiNrm = -1;
static GPUParamsBuf s_hemiParamsGPHemi = {};

static GLuint s_pidHemiCc = 0;
static GLint s_locSmpHemiCcDiff = -1;
static GLint s_locSmpHemiCcClr = -1;
static GLint s_locSmpHemiCcNrm = -1;
static GPUParamsBuf s_hemiccParamsGPHemi = {};
static GPUParamsBuf s_hemiccParamsGPColor = {};

static GLuint s_pidHemiFogCc = 0;
static GLint s_locSmpHemiFogCcDiff = -1;
static GLint s_locSmpHemiFogCcClr = -1;
static GLint s_locSmpHemiFogCcNrm = -1;
static GLint s_locSmpHemiFogCcPos = -1;
static GPUParamsBuf s_hemifogccParamsGPHemi = {};
static GPUParamsBuf s_hemifogccParamsGPWPos = {};
static GPUParamsBuf s_hemifogccParamsGPFog = {};
static GPUParamsBuf s_hemifogccParamsGPColor = {};

static int s_gbW = 0;
static int s_gbH = 0;
static GLuint s_gbuf = 0;
static GLuint s_gbDiffTex = 0;
static GLuint s_gbClrTex = 0;
static GLuint s_gbNrmTex = 0;
static GLuint s_gbPosTex = 0;
static GLuint s_gbDepthBuf = 0;

static GLuint s_imgbufDiv2 = 0;
static GLuint s_ibDiv2Tex = 0;

struct MdlGPURsrc {
	xt_xmtx xforms[MAX_XFORMS];
	size_t xformMtxSize;
	sxModelData* pMdl;
	GLuint bufVB;
	GLuint bufIB16;
	GLuint bufIB32;
	GLuint bufGPXform;
	bool xformUpdateFlg;
	GPUParamsBuf paramsGPModel;
	int nmtl;
	GPUParamsBuf* pParamsGPMaterial;
};

struct MdlGPUWk {
	cxModelWork* pMdlWk;
	GPXform gpXform;
	GLuint bufGPXform;
	bool xformUpdateFlg;
};

typedef cxPlexList<MdlGPURsrc> MdlGPURsrcList;
static MdlGPURsrcList* s_pMdlGPURsrcList = nullptr;

typedef cxPlexList<MdlGPUWk> MdlGPUWkList;
static MdlGPUWkList* s_pMdlGPUWkList = nullptr;

static void prepare_model(sxModelData* pMdl) {
	if (!s_pMdlGPURsrcList) return;
	if (!pMdl) return;
	MdlGPURsrc** ppGPURsrc = pMdl->get_gpu_wk<MdlGPURsrc*>();
	if (*ppGPURsrc) return;
	MdlGPURsrc* pGPURsrc = s_pMdlGPURsrcList->new_item();
	if (!pGPURsrc) return;
	*ppGPURsrc = pGPURsrc;
	pGPURsrc->pMdl = pMdl;
	size_t vsize = pMdl->get_vtx_size();
	if (vsize > 0) {
		glGenBuffers(1, &pGPURsrc->bufVB);
		if (pGPURsrc->bufVB) {
			int npnt = pMdl->mPntNum;
			GPUVtx* pVtx = nxCore::tMem<GPUVtx>::alloc(npnt, "VtxTemp");
			if (pVtx) {
				for (int i = 0; i < npnt; ++i) {
					pVtx[i].pos = pMdl->get_pnt_pos(i);
					pVtx[i].nrm = pMdl->get_pnt_nrm(i);
					pVtx[i].tex = pMdl->get_pnt_tex(i);
					pVtx[i].clr = pMdl->get_pnt_clr(i);
					pVtx[i].wgt.fill(0.0f);
					pVtx[i].idx.fill(0);
					sxModelData::PntSkin skn = pMdl->get_pnt_skin(i);
					if (skn.num > 0) {
						for (int j = 0; j < skn.num; ++j) {
							pVtx[i].wgt.set_at(j, skn.wgt[j]);
							pVtx[i].idx.set_at(j, skn.idx[j]);
						}
					} else {
						pVtx[i].wgt[0] = 1.0f;
					}
					if (!s_GPUInfo.need_jnt_map()) {
						pVtx[i].idx.scl(3);
					}
				}
				glBindBuffer(GL_ARRAY_BUFFER, pGPURsrc->bufVB);
				glBufferData(GL_ARRAY_BUFFER, npnt * sizeof(GPUVtx), pVtx, GL_STATIC_DRAW);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				nxCore::tMem<GPUVtx>::free(pVtx);
			}
		}
	}
	if (pMdl->mIdx16Num > 0) {
		const uint16_t* pIdxData = pMdl->get_idx16_top();
		if (pIdxData) {
			glGenBuffers(1, &pGPURsrc->bufIB16);
			if (pGPURsrc->bufIB16) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pGPURsrc->bufIB16);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, pMdl->mIdx16Num * sizeof(uint16_t), pIdxData, GL_STATIC_DRAW);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			}
		}
	}
	if (pMdl->mIdx32Num > 0) {
		const uint32_t* pIdxData = pMdl->get_idx32_top();
		if (pIdxData) {
			glGenBuffers(1, &pGPURsrc->bufIB32);
			if (pGPURsrc->bufIB32) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pGPURsrc->bufIB32);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, pMdl->mIdx32Num * sizeof(uint32_t), pIdxData, GL_STATIC_DRAW);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			}
		}
	}
	glGenBuffers(1, &pGPURsrc->bufGPXform);
	if (pGPURsrc->bufGPXform) {
		glBindBuffer(GL_UNIFORM_BUFFER, pGPURsrc->bufGPXform);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(GPXform), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	static const GLchar* fldNamesGPModel[] = {
		"gpRdrMask"
	};
	pGPURsrc->paramsGPModel.init(s_pidGBuf, "GPModel", gpBindingModel, fldNamesGPModel, XD_ARY_LEN(fldNamesGPModel));

	pGPURsrc->nmtl = pMdl->mMtlNum;
	static const GLchar* fldNamesGPMaterial[] = {
			"gpBaseColor",
			"gpBumpParam",
			"gpAlphaLim",
			"gpSpecScl"
	};
	pGPURsrc->pParamsGPMaterial = nxCore::tMem<GPUParamsBuf>::alloc(pGPURsrc->nmtl, "GPMaterial");
	if (pGPURsrc->pParamsGPMaterial) {
		for (int i = 0; i < pGPURsrc->nmtl; ++i) {
			pGPURsrc->pParamsGPMaterial[i].init(s_pidGBuf, "GPMaterial", gpBindingMaterial, fldNamesGPMaterial, XD_ARY_LEN(fldNamesGPMaterial));
		}
	}
}

static void release_model(sxModelData* pMdl) {
	if (!pMdl) return;
	MdlGPURsrc** ppGPURsrc = pMdl->get_gpu_wk<MdlGPURsrc*>();
	if (!*ppGPURsrc) return;
	MdlGPURsrc* pGPURsrc = *ppGPURsrc;
	if (!pGPURsrc) return;
	if (pGPURsrc->bufVB) {
		glDeleteBuffers(1, &pGPURsrc->bufVB);
		pGPURsrc->bufVB = 0;
	}
	if (pGPURsrc->bufIB16) {
		glDeleteBuffers(1, &pGPURsrc->bufIB16);
		pGPURsrc->bufIB16 = 0;
	}
	if (pGPURsrc->bufIB32) {
		glDeleteBuffers(1, &pGPURsrc->bufIB32);
		pGPURsrc->bufIB32 = 0;
	}
	if (pGPURsrc->bufGPXform) {
		glDeleteBuffers(1, &pGPURsrc->bufGPXform);
		pGPURsrc->bufGPXform = 0;
	}
	s_gbufParamsGPWPos.reset();
	pGPURsrc->paramsGPModel.reset();
	if (pGPURsrc->pParamsGPMaterial) {
		for (int i = 0; i < pGPURsrc->nmtl; ++i) {
			pGPURsrc->pParamsGPMaterial[i].reset();
		}
		nxCore::tMem<GPUParamsBuf>::free(pGPURsrc->pParamsGPMaterial);
		pGPURsrc->pParamsGPMaterial = nullptr;
	}
	pMdl->clear_tex_wk();
	if (s_pMdlGPURsrcList) {
		s_pMdlGPURsrcList->remove(pGPURsrc);
	}
	*ppGPURsrc = nullptr;
}

static void def_tex_params(const bool mipmapEnabled) {
	if (s_useMipmaps && mipmapEnabled) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		OGLSys::set_tex2d_lod_bias(-1);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

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

static void prepare_model_work(cxModelWork* pWk) {
	if (!pWk) return;
	if (pWk->mpGPUWk) return;
	if (!s_pMdlGPUWkList) return;
	MdlGPUWk* pGPUWk = s_pMdlGPUWkList->new_item();
	pWk->mpGPUWk = pGPUWk;
	if (pGPUWk) {
		::memset(pGPUWk, 0, sizeof(MdlGPUWk));
		pGPUWk->pMdlWk = pWk;
		if (!s_useSharedXformBuf) {
			glGenBuffers(1, &pGPUWk->bufGPXform);
			if (pGPUWk->bufGPXform) {
				glBindBuffer(GL_UNIFORM_BUFFER, pGPUWk->bufGPXform);
				glBufferData(GL_UNIFORM_BUFFER, sizeof(GPXform), nullptr, GL_DYNAMIC_DRAW);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);
			}
		}
	}
}

static void release_model_work(cxModelWork* pWk) {
	if (!pWk) return;
	MdlGPUWk* pGPUWk = (MdlGPUWk*)pWk->mpGPUWk;
	if (!pGPUWk) return;
	if (!s_pMdlGPUWkList) return;
	if (pGPUWk->bufGPXform) {
		glDeleteBuffers(1, &pGPUWk->bufGPXform);
		pGPUWk->bufGPXform = 0;
	}
	s_pMdlGPUWkList->remove(pGPUWk);
	pWk->mpGPUWk = nullptr;
}

static MdlGPUWk* get_mdl_gpu_wk(cxModelWork* pWk) {
	MdlGPUWk* pGPUWk = nullptr;
	if (pWk && pWk->mpGPUWk) {
		pGPUWk = (MdlGPUWk*)pWk->mpGPUWk;
	}
	return pGPUWk;
}

static GLuint load_shader(const char* pName) {
	GLuint sid = 0;
	const char* pDataPath = s_pRsrcMgr ? s_pRsrcMgr->get_data_path() : nullptr;
	if (pName) {
		char path[1024];
		XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/ogl_x/%s", pDataPath ? pDataPath : ".", pName);
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

static void init_rsrc_mgr() {
	cxResourceManager::GfxIfc rsrcGfxIfc;
	rsrcGfxIfc.reset();
	rsrcGfxIfc.prepareTexture = prepare_texture;
	rsrcGfxIfc.releaseTexture = release_texture;
	rsrcGfxIfc.prepareModel = prepare_model;
	rsrcGfxIfc.releaseModel = release_model;
	rsrcGfxIfc.prepareModelWork = prepare_model_work;
	rsrcGfxIfc.releaseModelWork = release_model_work;
	if (s_pRsrcMgr) {
		s_pRsrcMgr->set_gfx_ifc(rsrcGfxIfc);
	}
}

static GLuint link_prog(GLuint sidVert, GLuint sidFrag) {
	GLuint pid = OGLSys::link_draw_prog(sidVert, sidFrag);
	if (pid) {
		glDetachShader(pid, sidVert);
		glDetachShader(pid, sidFrag);
	}
	return pid;
}

static void init_gpu_prog() {
	static const GLchar* fldNamesGPHemi[] = {
		"gpHemiUpper",
		"gpHemiLower",
		"gpHemiUp",
		"gpHemiExp",
		"gpHemiGain"
	};
	static const GLchar* fldNamesGPColor[] = {
		"gpInvWhite",
		"gpLClrGain",
		"gpLClrBias",
		"gpExposure",
		"gpInvGamma"
	};
	static const GLchar* fldNamesGPWPos[] = {
		"gpWPosOffs"
	};
	static const GLchar* fldNamesGPFog[] = {
		"gpFogColor",
		"gpFogParam",
		"gpFogViewPos"
	};
	bool gbufVtxJMap = s_GPUInfo.need_jnt_map();
	GLuint sidVertGBuf = gbufVtxJMap ? load_shader("gbuf_vtx_jmap.vert") : load_shader("gbuf_vtx.vert");
	GLuint sidFragGBuf = load_shader("gbuf_pix.frag");
	s_pidGBuf = OGLSys::link_draw_prog(sidVertGBuf, sidFragGBuf);
	if (s_pidGBuf) {
		glDetachShader(s_pidGBuf, sidVertGBuf);
		glDetachShader(s_pidGBuf, sidFragGBuf);
		s_locPos = glGetAttribLocation(s_pidGBuf, "vtxPos");
		s_locNrm = glGetAttribLocation(s_pidGBuf, "vtxNrm");
		s_locTex = glGetAttribLocation(s_pidGBuf, "vtxTex");
		s_locClr = glGetAttribLocation(s_pidGBuf, "vtxClr");
		s_locWgt = glGetAttribLocation(s_pidGBuf, "vtxWgt");
		s_locIdx = glGetAttribLocation(s_pidGBuf, "vtxIdx");

		if (gbufVtxJMap) {
			s_gpLocViewProj = glGetUniformLocation(s_pidGBuf, "gpViewProj");
			s_gpLocXforms = glGetUniformLocation(s_pidGBuf, "gpXforms");
			s_gpLocXformMap = glGetUniformLocation(s_pidGBuf, "gpXformMap");
		} else {
			s_gpIdxXform = glGetUniformBlockIndex(s_pidGBuf, "GPXform");
			glUniformBlockBinding(s_pidGBuf, s_gpIdxXform, gpBindingXform);
		}

		s_locSmpBase = glGetUniformLocation(s_pidGBuf, "smpBase");
		s_locSmpNorm = glGetUniformLocation(s_pidGBuf, "smpNorm");

		s_gbufParamsGPWPos.init(s_pidGBuf, "GPWPos", gpBindingView, fldNamesGPWPos, XD_ARY_LEN(fldNamesGPWPos));
	} else {
		nxCore::dbg_msg("Failed to create GBuf program.\n");
	}
	glDeleteShader(sidVertGBuf);
	glDeleteShader(sidFragGBuf);

	GLuint sidVertFSTri = s_fsTriVBO ? load_shader("fstri_vb.vert") : load_shader("fstri.vert");

	GLuint sidFragImgCpy = load_shader("img_cpy.frag");
	s_pidImgCpy = link_prog(sidVertFSTri, sidFragImgCpy);
	if (s_pidImgCpy) {
		s_locSmpImgCpy = glGetUniformLocation(s_pidImgCpy, "smpImg");
	}

	GLuint sidFragDispClr = load_shader("disp_clr.frag");
	s_pidDispClr = link_prog(sidVertFSTri, sidFragDispClr);
	if (s_pidDispClr) {
		s_locSmpDispClr = glGetUniformLocation(s_pidDispClr, "smpDiff");
		s_locSmpClrDispClr = glGetUniformLocation(s_pidDispClr, "smpClr");
		static const GLchar* fldNamesGPDispClr[] = {
			"gpDiffColorScl"
		};
		s_dispClrParams.init(s_pidDispClr, "GPDispDiff", 0, fldNamesGPDispClr, XD_ARY_LEN(fldNamesGPDispClr));
	}
	GLuint sidFragDispNrm = load_shader("disp_nrm.frag");
	s_pidDispNrm = link_prog(sidVertFSTri, sidFragDispNrm);
	if (s_pidDispNrm) {
		s_locSmpDispNrm = glGetUniformLocation(s_pidDispNrm, "smpNrm");
	}
	GLuint sidFragDispFog = load_shader("disp_fog.frag");
	s_pidDispFog = link_prog(sidVertFSTri, sidFragDispFog);
	if (s_pidDispFog) {
		s_locSmpDispFog = glGetUniformLocation(s_pidDispFog, "smpPos");
		s_dispFogParamsGPWPos.init(s_pidDispFog, "GPWPos", 0, fldNamesGPWPos, XD_ARY_LEN(fldNamesGPWPos));
		s_dispFogParamsGPFog.init(s_pidDispFog, "GPFog", 1, fldNamesGPFog, XD_ARY_LEN(fldNamesGPFog));
	}

	GLuint sidFragHemi = load_shader("hemi.frag");
	s_pidHemi = link_prog(sidVertFSTri, sidFragHemi);
	if (s_pidHemi) {
		s_locSmpHemiDiff = glGetUniformLocation(s_pidHemi, "smpDiff");
		s_locSmpHemiClr = glGetUniformLocation(s_pidHemi, "smpClr");
		s_locSmpHemiNrm = glGetUniformLocation(s_pidHemi, "smpNrm");
		s_hemiParamsGPHemi.init(s_pidHemi, "GPHemi", gpBindingHemi, fldNamesGPHemi, XD_ARY_LEN(fldNamesGPHemi));
	}

	GLuint sidFragHemiCc = load_shader("hemicc.frag");
	s_pidHemiCc = link_prog(sidVertFSTri, sidFragHemiCc);
	if (s_pidHemiCc) {
		s_locSmpHemiCcDiff = glGetUniformLocation(s_pidHemiCc, "smpDiff");
		s_locSmpHemiCcClr = glGetUniformLocation(s_pidHemiCc, "smpClr");
		s_locSmpHemiCcNrm = glGetUniformLocation(s_pidHemiCc, "smpNrm");
		s_hemiccParamsGPHemi.init(s_pidHemiCc, "GPHemi", gpBindingHemi, fldNamesGPHemi, XD_ARY_LEN(fldNamesGPHemi));
		s_hemiccParamsGPColor.init(s_pidHemiCc, "GPColor", gpBindingColor, fldNamesGPColor, XD_ARY_LEN(fldNamesGPColor));
	}

	GLuint sidFragHemiFogCc = load_shader("hemifogcc.frag");
	s_pidHemiFogCc = link_prog(sidVertFSTri, sidFragHemiFogCc);
	if (s_pidHemiFogCc) {
		s_locSmpHemiFogCcDiff = glGetUniformLocation(s_pidHemiFogCc, "smpDiff");
		s_locSmpHemiFogCcClr = glGetUniformLocation(s_pidHemiFogCc, "smpClr");
		s_locSmpHemiFogCcNrm = glGetUniformLocation(s_pidHemiFogCc, "smpNrm");
		s_locSmpHemiFogCcPos = glGetUniformLocation(s_pidHemiFogCc, "smpPos");
		s_hemifogccParamsGPHemi.init(s_pidHemiFogCc, "GPHemi", 0, fldNamesGPHemi, XD_ARY_LEN(fldNamesGPHemi));
		s_hemifogccParamsGPWPos.init(s_pidHemiFogCc, "GPWPos", 1, fldNamesGPWPos, XD_ARY_LEN(fldNamesGPWPos));
		s_hemifogccParamsGPFog.init(s_pidHemiFogCc, "GPFog", 2, fldNamesGPFog, XD_ARY_LEN(fldNamesGPFog));
		s_hemifogccParamsGPColor.init(s_pidHemiFogCc, "GPColor", 3, fldNamesGPColor, XD_ARY_LEN(fldNamesGPColor));
	}

	glDeleteShader(sidVertFSTri);
	glDeleteShader(sidFragImgCpy);
	glDeleteShader(sidFragDispClr);
	glDeleteShader(sidFragDispNrm);
	glDeleteShader(sidFragDispFog);
	glDeleteShader(sidFragHemi);
	glDeleteShader(sidFragHemiCc);
	glDeleteShader(sidFragHemiFogCc);
}

static bool s_useF32RT = false;

static void fb_tex_init(GLuint tex, const bool high, const int w, const int h, const bool linear) {
	glBindTexture(GL_TEXTURE_2D, tex);
#if defined(OGLSYS_VIVANTE_FB)
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
#else
	if (high) {
		if (s_useF32RT) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
		}
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	}
#endif
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void init_gbuf_rt(GLuint tex, int idx) {
	int w = s_gbW;
	int h = s_gbH;
	fb_tex_init(tex, idx != 0, w, h, false);
	int attach = GL_COLOR_ATTACHMENT0;
	switch (idx) {
		case 1: attach = GL_COLOR_ATTACHMENT1; break;
		case 2: attach = GL_COLOR_ATTACHMENT2; break;
		case 3: attach = GL_COLOR_ATTACHMENT3; break;
		default: break;
	}
	glFramebufferTexture2D(GL_FRAMEBUFFER, attach, GL_TEXTURE_2D, tex, 0);
}

static void init_gbuf() {
	float gbScl = 1.0f;
	s_gbW = int(float(OGLSys::get_width()) * gbScl);
	s_gbH = int(float(OGLSys::get_height()) * gbScl);
	glGenFramebuffers(1, &s_gbuf);
	if (s_gbuf) {
		glBindFramebuffer(GL_FRAMEBUFFER, s_gbuf);
		//
		glGenTextures(1, &s_gbDiffTex);
		init_gbuf_rt(s_gbDiffTex, 0);
		//
		glGenTextures(1, &s_gbClrTex);
		init_gbuf_rt(s_gbClrTex, 1);
		//
		glGenTextures(1, &s_gbNrmTex);
		init_gbuf_rt(s_gbNrmTex, 2);
		//
		glGenTextures(1, &s_gbPosTex);
		init_gbuf_rt(s_gbPosTex, 3);
		glBindTexture(GL_TEXTURE_2D, 0);
		GLuint att[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
		glDrawBuffers(XD_ARY_LEN(att), att);
		glGenRenderbuffers(1, &s_gbDepthBuf);
		glBindRenderbuffer(GL_RENDERBUFFER, s_gbDepthBuf);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, s_gbW, s_gbH);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_gbDepthBuf);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			nxCore::dbg_msg("Failed to create MRT.\n");
		}
	}
	OGLSys::bind_def_framebuf();
}

static void init_imgbuf() {
	int scrW = OGLSys::get_width();
	int scrH = OGLSys::get_height();
	glGenFramebuffers(1, &s_imgbufDiv2);
	glBindFramebuffer(GL_FRAMEBUFFER, s_imgbufDiv2);
	glGenTextures(1, &s_ibDiv2Tex);
	fb_tex_init(s_ibDiv2Tex, true, scrW / 2, scrH / 2, true);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_ibDiv2Tex, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		nxCore::dbg_msg("Failed to create ImgBuf/2.\n");
	}
	OGLSys::bind_def_framebuf();
}

static void init(int shadowSize, cxResourceManager* pRsrcMgr, Draw::Font* pFont) {
	s_pRsrcMgr = pRsrcMgr;
	if (!pRsrcMgr) return;

	s_GPUInfo.init();

	s_pMdlGPURsrcList = MdlGPURsrcList::create("MdlGPURsrcList", false, false);
	s_pMdlGPUWkList = MdlGPUWkList::create("MdlGPUWkList", false, false);

	if (s_useFSTriVB) {
		glGenBuffers(1, &s_fsTriVBO);
		if (s_fsTriVBO) {
			static GLfloat fsTriVBData[] = { 0.0f, 1.0f, 2.0f };
			glBindBuffer(GL_ARRAY_BUFFER, s_fsTriVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(fsTriVBData), fsTriVBData, GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}

	init_rsrc_mgr();
	init_gpu_prog();
	init_gbuf();
	init_imgbuf();
}

static void reset_gbuf() {
	OGLSys::bind_def_framebuf();
	if (s_gbDiffTex) {
		glDeleteTextures(1, &s_gbDiffTex);
		s_gbDiffTex = 0;
	}
	if (s_gbClrTex) {
		glDeleteTextures(1, &s_gbClrTex);
		s_gbClrTex = 0;
	}
	if (s_gbNrmTex) {
		glDeleteTextures(1, &s_gbNrmTex);
		s_gbNrmTex = 0;
	}
	if (s_gbPosTex) {
		glDeleteTextures(1, &s_gbPosTex);
		s_gbPosTex = 0;
	}
	if (s_gbDepthBuf) {
		glDeleteRenderbuffers(1, &s_gbDepthBuf);
		s_gbDepthBuf = 0;
	}
	if (s_gbuf) {
		glDeleteFramebuffers(1, &s_gbuf);
		s_gbuf = 0;
	}
}

static void reset_imgbuf() {
	OGLSys::bind_def_framebuf();
	if (s_ibDiv2Tex) {
		glDeleteTextures(1, &s_ibDiv2Tex);
		s_ibDiv2Tex = 0;
	}
	if (s_imgbufDiv2) {
		glDeleteFramebuffers(1, &s_imgbufDiv2);
		s_imgbufDiv2 = 0;
	}
}

static void reset() {
	glDeleteProgram(s_pidGBuf);
	s_pidGBuf = 0;
	glDeleteProgram(s_pidImgCpy);
	s_pidImgCpy = 0;
	glDeleteProgram(s_pidDispClr);
	s_pidDispClr = 0;
	glDeleteProgram(s_pidDispNrm);
	s_pidDispNrm = 0;
	glDeleteProgram(s_pidDispFog);
	s_pidDispFog = 0;
	glDeleteProgram(s_pidHemi);
	s_pidHemi = 0;
	glDeleteProgram(s_pidHemiCc);
	s_pidHemiCc = 0;
	s_dispClrParams.reset();
	s_hemiParamsGPHemi.reset();
	s_hemiccParamsGPHemi.reset();
	s_hemiccParamsGPColor.reset();
	s_dispFogParamsGPWPos.reset();
	s_dispFogParamsGPFog.reset();
	s_hemifogccParamsGPHemi.reset();
	s_hemifogccParamsGPWPos.reset();
	s_hemifogccParamsGPFog.reset();
	s_hemifogccParamsGPColor.reset();
	MdlGPURsrcList::destroy(s_pMdlGPURsrcList);
	s_pMdlGPURsrcList = nullptr;
	MdlGPUWkList::destroy(s_pMdlGPUWkList);
	s_pMdlGPUWkList = nullptr;
	reset_gbuf();
	reset_imgbuf();
	if (s_fsTriVBO) {
		glDeleteBuffers(1, &s_fsTriVBO);
		s_fsTriVBO = 0;
	}
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

static void batch(cxModelWork* pWk, const int ibat, const Draw::Mode mode, const Draw::Context* pCtx) {
	if (!pCtx) return;
	if (!pWk) return;
	sxModelData* pMdl = pWk->mpData;
	if (!pMdl) return;
	if (!s_pidGBuf) return;

	if (mode == Draw::DRWMODE_SHADOW_CAST) return;

	const sxModelData::Batch* pBat = pMdl->get_batch_ptr(ibat);
	if (!pBat) return;

	const sxModelData::Material* pMtl = pMdl->get_material(pBat->mMtlId);
	if (pMtl->mFlags.forceBlend) {
		//return;
	}
	if (pWk->mVariation != 0) {
		if (pMdl->mtl_has_swaps(pBat->mMtlId)) {
			const sxModelData::Material* pSwapMtl = pMdl->get_swap_material(pBat->mMtlId, pWk->mVariation);
			if (pSwapMtl) {
				pMtl = pSwapMtl;
			}
		}
	}
	if (!pMtl) return;

	prepare_model(pMdl);
	prepare_model_work(pWk);

	MdlGPURsrc** ppGPURsrc = pMdl->get_gpu_wk<MdlGPURsrc*>();
	if (!*ppGPURsrc) return;
	MdlGPURsrc* pGPURsrc = *ppGPURsrc;
	if (!pGPURsrc) return;
	GLuint bufVB = pGPURsrc->bufVB;
	if (!bufVB) return;
	GLuint bufIB16 = pGPURsrc->bufIB16;
	GLuint bufIB32 = pGPURsrc->bufIB32;

	MdlGPUWk* pGPUWk = get_mdl_gpu_wk(pWk);

	bool vtxJMap = s_GPUInfo.need_jnt_map();

	int ctxIdx = 0;
	if (s_ctxStoreCnt > 0) {
		int foundIdx = -1;
		for (int i = 0; i < s_ctxStoreCnt; ++i) {
			if (::memcmp(&s_ctxStore[i], pCtx, sizeof(Draw::Context)) == 0) {
				foundIdx = i;
				break;
			}
		}
		if (foundIdx < 0) {
			ctxIdx = s_ctxStoreCnt;
			++s_ctxStoreCnt;
		} else {
			ctxIdx = foundIdx;
			++s_ctxUseCnt[ctxIdx];
		}
	} else {
		++s_ctxUseCnt[ctxIdx];
		++s_ctxStoreCnt;
	}
	s_ctxStore[ctxIdx] = *pCtx;

	glUseProgram(s_pidGBuf);

	if (vtxJMap) {
		if (s_gpLocViewProj >= 0) {
			glUniformMatrix4fv(s_gpLocViewProj, 1, GL_FALSE, pCtx->view.mViewProjMtx);
		}
		if (s_gpLocXforms >= 0) {
			xt_xmtx xforms[MAX_BAT_XFORMS];
			float xmap[MAX_XFORMS];
			for (int i = 0; i < MAX_XFORMS; ++i) {
				xmap[i] = 0;
			}
			if (pWk->mpSkinXforms) {
				const int32_t* pJntLst = pMdl->get_batch_jnt_list(ibat);
				int njnt = pBat->mJntNum;
				for (int i = 0; i < njnt; ++i) {
					xforms[i] = pWk->mpSkinXforms[pJntLst[i]];
				}
				for (int i = 0; i < njnt; ++i) {
					xmap[pJntLst[i]] = float(i * 3);
				}
			} else if (pWk->mpWorldXform) {
				xforms[0] = *pWk->mpWorldXform;
			} else {
				xforms[0].identity();
			}
			glUniform4fv(s_gpLocXforms, MAX_XFORMS * 3, xforms[0]);
			glUniform4fv(s_gpLocXformMap, MAX_XFORMS / 4, xmap);
		}
	} else if (pGPUWk && pGPUWk->bufGPXform && pGPUWk->xformUpdateFlg) {
		int xformsNum = 1;
		pGPUWk->gpXform.viewProj = pCtx->view.mViewProjMtx;
		if (pWk->mpSkinXforms) {
			xformsNum = pMdl->mSknNum;
			if (xformsNum > MAX_XFORMS) xformsNum = MAX_XFORMS;
			::memcpy(pGPUWk->gpXform.xforms, pWk->mpSkinXforms, xformsNum * sizeof(xt_xmtx));
		} else if (pWk->mpWorldXform) {
			pGPUWk->gpXform.xforms[0] = *pWk->mpWorldXform;
		} else {
			pGPUWk->gpXform.xforms[0].identity();
		}
		size_t xformMtxSize = sizeof(xt_xmtx) * xformsNum;
		size_t xformSize = sizeof(xt_mtx) + xformMtxSize;
		glBindBuffer(GL_UNIFORM_BUFFER, pGPUWk->bufGPXform);
		void* pXformDst = glMapBufferRange(GL_UNIFORM_BUFFER, 0, xformSize, GL_MAP_WRITE_BIT);
		if (pXformDst) {
			::memcpy(pXformDst, &pGPUWk->gpXform, xformSize);
		}
		glUnmapBuffer(GL_UNIFORM_BUFFER);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		glBindBufferBase(GL_UNIFORM_BUFFER, gpBindingXform, pGPUWk->bufGPXform);
		pGPUWk->xformUpdateFlg = false;
	} else {
		s_gpXform.viewProj = pCtx->view.mViewProjMtx;
		int xformsNum = 1;
		if (pWk->mpSkinXforms) {
			xformsNum = pMdl->mSknNum;
			if (xformsNum > MAX_XFORMS) xformsNum = MAX_XFORMS;
			::memcpy(s_gpXform.xforms, pWk->mpSkinXforms, xformsNum * sizeof(xt_xmtx));
		} else if (pWk->mpWorldXform) {
			s_gpXform.xforms[0] = *pWk->mpWorldXform;
		} else {
			s_gpXform.xforms[0].identity();
		}

		size_t xformMtxSize = sizeof(xt_xmtx) * xformsNum;
		size_t xformSize = sizeof(xt_mtx) + xformMtxSize;
		glBindBufferBase(GL_UNIFORM_BUFFER, gpBindingXform, pGPURsrc->bufGPXform);
		bool xformsUpdate = pGPURsrc->xformUpdateFlg;
		if (!xformsUpdate) {
			xformsUpdate = xformMtxSize != pGPURsrc->xformMtxSize;
			if (!xformsUpdate) {
				xformsUpdate = (::memcmp(&s_gpXform.xforms, pGPURsrc->xforms, xformMtxSize) != 0);
			}
		}
		if (xformsUpdate) {
			pGPURsrc->xformMtxSize = xformMtxSize;
			::memcpy(pGPURsrc->xforms, &s_gpXform.xforms, xformMtxSize);
			glBindBuffer(GL_UNIFORM_BUFFER, pGPURsrc->bufGPXform);
			void* pXformDst = glMapBufferRange(GL_UNIFORM_BUFFER, 0, xformSize, GL_MAP_WRITE_BIT);
			if (pXformDst) {
				::memcpy(pXformDst, &s_gpXform, xformSize);
			}
			glUnmapBuffer(GL_UNIFORM_BUFFER);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
			pGPURsrc->xformUpdateFlg = false;
		}
	}

	s_gbufParamsGPWPos.set("gpWPosOffs", pCtx->view.mPos);
	s_gbufParamsGPWPos.send();
	s_gbufParamsGPWPos.bind();

	pGPURsrc->paramsGPModel.set("gpRdrMask", pWk->mRenderMask);
	pGPURsrc->paramsGPModel.send();
	pGPURsrc->paramsGPModel.bind();

	float alphaLim = 0.0f;
	if (pMtl->mFlags.alpha) {
		alphaLim = pMtl->mAlphaLim;
		if (alphaLim <= 0.0f) {
			alphaLim = 0.5f;
		}
	}

	bool specFlg = pCtx->glb.useSpec && pCtx->spec.mEnabled && pMtl->mFlags.baseMapSpecAlpha;
	float specScl = 0.0f;
	if (specFlg) {
		specScl = (pMtl->mSpecColor.x + pMtl->mSpecColor.y + pMtl->mSpecColor.z) / 3.0f;
	}

	bool bumpFlg = false;
	float bumpScl = 0.0f;
	float bumpSclT = pMtl->mFlags.flipTangent ? -1.0f : 1.0f;
	float bumpSclB = -(pMtl->mFlags.flipBitangent ? -1.0f : 1.0f);
	GLuint bumpTexHandle = 0;
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
				bumpTexHandle = *pTexHandle;
				if (bumpTexHandle) {
					bumpFlg = true;
					bumpScl = pMtl->mBumpScale;
				}
			}
		}
	}

	GPUParamsBuf* pGPMaterial = &pGPURsrc->pParamsGPMaterial[pBat->mMtlId];
	xt_float3 baseClr = pMtl->mBaseColor;
	pGPMaterial->set("gpBaseColor", baseClr);
	pGPMaterial->set("gpAlphaLim", alphaLim);
	pGPMaterial->set("gpSpecScl", specScl);
	pGPMaterial->set("gpBumpParam", bumpScl, bumpSclT, bumpSclB);
	pGPMaterial->send();
	pGPMaterial->bind();

	if (pMtl && s_locSmpBase >= 0) {
		GLuint htex = 0;
		int tid = pMtl->mBaseTexId;
		if (tid >= 0) {
			sxModelData::TexInfo* pTexInfo = pMdl->get_tex_info(tid);
			GLuint* pMtlTexHandle = pTexInfo->get_wk<GLuint>();
			if (*pMtlTexHandle == 0) {
				const char* pTexName = pMdl->get_tex_name(tid);
				sxTextureData* pTex = s_pRsrcMgr->find_texture_for_model(pMdl, pTexName);
				prepare_texture(pTex);
				GLuint* pTexHandle = pTex->get_gpu_wk<GLuint>();
				*pMtlTexHandle = *pTexHandle;
			}
			htex = *pMtlTexHandle;
		}
		if (!htex) {
			htex = OGLSys::get_white_tex();
		}
		glUniform1i(s_locSmpBase, Draw::TEXUNIT_Base);
		glActiveTexture(GL_TEXTURE0 + Draw::TEXUNIT_Base);
		glBindTexture(GL_TEXTURE_2D, htex);
	}

	if (s_locSmpNorm >= 0) {
		glUniform1i(s_locSmpNorm, Draw::TEXUNIT_Bump);
		glActiveTexture(GL_TEXTURE0 + Draw::TEXUNIT_Bump);
		glBindTexture(GL_TEXTURE_2D, bumpTexHandle);
	}

	glBindBuffer(GL_ARRAY_BUFFER, bufVB);

	GLsizei stride = (GLsizei)sizeof(GPUVtx);
	size_t top = size_t(pBat->mMinIdx) * stride;
	if (s_locPos >= 0) {
		glEnableVertexAttribArray(s_locPos);
		glVertexAttribPointer(s_locPos, 3, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(GPUVtx, pos)));
	}
	if (s_locNrm >= 0) {
		glEnableVertexAttribArray(s_locNrm);
		glVertexAttribPointer(s_locNrm, 3, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(GPUVtx, nrm)));
	}
	if (s_locTex >= 0) {
		glEnableVertexAttribArray(s_locTex);
		glVertexAttribPointer(s_locTex, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(GPUVtx, tex)));
	}
	if (s_locClr >= 0) {
		glEnableVertexAttribArray(s_locClr);
		glVertexAttribPointer(s_locClr, 4, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(GPUVtx, clr)));
	}
	if (s_locWgt >= 0) {
		glEnableVertexAttribArray(s_locWgt);
		glVertexAttribPointer(s_locWgt, 4, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(GPUVtx, wgt)));
	}
	if (s_locIdx >= 0) {
		glEnableVertexAttribArray(s_locIdx);
		glVertexAttribPointer(s_locIdx, 4, GL_INT, GL_FALSE, stride, (const void*)(top + offsetof(GPUVtx, idx)));
	}

	if (pMtl->mFlags.dblSided) {
		glDisable(GL_CULL_FACE);
	} else {
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);
		glCullFace(GL_BACK);
	}

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
	glDrawElements(GL_TRIANGLES, pBat->mTriNum * 3, typ, (const void*)org);

	if (s_locPos >= 0) {
		glDisableVertexAttribArray(s_locPos);
	}
	if (s_locNrm >= 0) {
		glDisableVertexAttribArray(s_locNrm);
	}
	if (s_locTex >= 0) {
		glDisableVertexAttribArray(s_locTex);
	}
	if (s_locClr >= 0) {
		glDisableVertexAttribArray(s_locClr);
	}
	if (s_locWgt >= 0) {
		glDisableVertexAttribArray(s_locWgt);
	}
	if (s_locIdx >= 0) {
		glDisableVertexAttribArray(s_locIdx);
	}

}

static void begin(const cxColor& clearColor) {
	int w = get_screen_width();
	int h = get_screen_height();
	OGLSys::bind_def_framebuf();
	glViewport(0, 0, w, h);
	glScissor(0, 0, w, h);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	if (s_gbuf) {
		glBindFramebuffer(GL_FRAMEBUFFER, s_gbuf);
		glViewport(0, 0, s_gbW, s_gbH);
		glScissor(0, 0, s_gbW, s_gbH);
		cxColor gbClearColor = clearColor;
		xt_float4 cval;
		cval.set(gbClearColor.r, gbClearColor.g, gbClearColor.b, 0.0f);
		glClearBufferfv(GL_COLOR, 0, cval);
		cval.fill(1.0f);
		cval.w = 0.0f; /* light/cc mask */
		glClearBufferfv(GL_COLOR, 1, cval);
		cval.fill(0.0f);
		glClearBufferfv(GL_COLOR, 2, cval);
		glClearBufferfv(GL_COLOR, 3, cval);
		glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	if (s_pMdlGPURsrcList) {
		for (MdlGPURsrcList::Itr itr = s_pMdlGPURsrcList->get_itr(); !itr.end(); itr.next()) {
			MdlGPURsrc* pRsrc = itr.item();
			pRsrc->xformUpdateFlg = true;
		}
	}
	if (s_pMdlGPUWkList) {
		for (MdlGPUWkList::Itr itr = s_pMdlGPUWkList->get_itr(); !itr.end(); itr.next()) {
			MdlGPUWk* pWk = itr.item();
			pWk->xformUpdateFlg = true;
		}
	}

	s_ctxStoreCnt = 0;
	for (int i = 0; i < c_maxCtx; ++i) {
		s_ctxUseCnt[i] = 0;
	}
}

static void set_fstri_vbo() {
	if (s_fsTriVBO) {
		glBindBuffer(GL_ARRAY_BUFFER, s_fsTriVBO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat), (const void*)0);
	} else {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static void cpy_img() {
	glBindFramebuffer(GL_FRAMEBUFFER, s_imgbufDiv2);
	int w = get_screen_width();
	int h = get_screen_height();
	int wd2 = w / 2;
	int hd2 = h / 2;
	glViewport(0, 0, wd2, hd2);
	glScissor(0, 0, wd2, hd2);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);

	glUseProgram(s_pidImgCpy);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_gbDiffTex);
	glUniform1i(s_locSmpImgCpy, 0);
	set_fstri_vbo();
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

static void end() {
	int ctxIdx = -1;
	int maxUseCnt = -1;
	for (int i = 0; i < s_ctxStoreCnt; ++i) {
		if (s_ctxUseCnt[i] > maxUseCnt) {
			maxUseCnt = s_ctxUseCnt[i];
			ctxIdx = i;
		}
	}
	//cpy_img();
	Draw::Context* pCtx = ctxIdx < 0 ? nullptr : &s_ctxStore[ctxIdx];
	int w = get_screen_width();
	int h = get_screen_height();
	OGLSys::bind_def_framebuf();
	glViewport(0, 0, w, h);
	glScissor(0, 0, w, h);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	set_fstri_vbo();
	int mode = 5; // 0:clr, 1:nrm, 2:fog, 3:hemi, 4:hemicc, 5: hemifogcc
	if (mode == 0) {
		if (s_pidDispClr) {
			glUseProgram(s_pidDispClr);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, s_gbDiffTex);
			glUniform1i(s_locSmpDispClr, 0);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, s_gbClrTex);
			glUniform1i(s_locSmpClrDispClr, 1);
			s_dispClrParams.set("gpDiffColorScl", 1.0f, 1.0f, 1.0f);
			s_dispClrParams.bind();
			s_dispClrParams.send();
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	} else if (mode == 1) {
		if (s_pidDispNrm) {
			glUseProgram(s_pidDispNrm);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, s_gbNrmTex);
			glUniform1i(s_locSmpDispNrm, 0);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	} else if (mode == 2) {
		if (s_pidDispFog) {
			glUseProgram(s_pidDispFog);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, s_gbPosTex);
			glUniform1i(s_locSmpDispFog, 0);
			s_dispFogParamsGPWPos.set("gpWPosOffs", pCtx->view.mPos);
			s_dispFogParamsGPWPos.bind();
			s_dispFogParamsGPWPos.send();
			s_dispFogParamsGPFog.set("gpFogViewPos", pCtx->view.mPos);
			s_dispFogParamsGPFog.set("gpFogColor", pCtx->fog.mColor);
			s_dispFogParamsGPFog.set("gpFogParam", pCtx->fog.mParam);
			s_dispFogParamsGPFog.bind();
			s_dispFogParamsGPFog.send();
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	} else if (mode == 3) {
		if (s_pidHemi) {
			glUseProgram(s_pidHemi);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, s_gbDiffTex);
			glUniform1i(s_locSmpHemiDiff, 0);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, s_gbClrTex);
			glUniform1i(s_locSmpHemiClr, 1);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, s_gbNrmTex);
			glUniform1i(s_locSmpHemiNrm, 2);
			if (pCtx) {
				s_hemiParamsGPHemi.set("gpHemiUpper", pCtx->hemi.mUpper);
				s_hemiParamsGPHemi.set("gpHemiLower", pCtx->hemi.mLower);
				s_hemiParamsGPHemi.set("gpHemiUp", pCtx->hemi.mUp);
				s_hemiParamsGPHemi.set("gpHemiExp", pCtx->hemi.mExp);
				s_hemiParamsGPHemi.set("gpHemiGain", pCtx->hemi.mGain);
			} else {
				s_hemiParamsGPHemi.set("gpHemiUpper", 1.0f);
				s_hemiParamsGPHemi.set("gpHemiLower", 0.1f);
				s_hemiParamsGPHemi.set("gpHemiUp", 0.0f, 1.0f, 0.0f);
				s_hemiParamsGPHemi.set("gpHemiExp", 1.0f);
				s_hemiParamsGPHemi.set("gpHemiGain", 0.0f);
			}
			s_hemiParamsGPHemi.bind();
			s_hemiParamsGPHemi.send();
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	} else if (mode == 4) {
		if (s_pidHemiCc) {
			glUseProgram(s_pidHemiCc);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, s_gbDiffTex);
			glUniform1i(s_locSmpHemiCcDiff, 0);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, s_gbClrTex);
			glUniform1i(s_locSmpHemiCcClr, 1);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, s_gbNrmTex);
			glUniform1i(s_locSmpHemiCcNrm, 2);
			if (pCtx) {
				s_hemiccParamsGPHemi.set("gpHemiUpper", pCtx->hemi.mUpper);
				s_hemiccParamsGPHemi.set("gpHemiLower", pCtx->hemi.mLower);
				s_hemiccParamsGPHemi.set("gpHemiUp", pCtx->hemi.mUp);
				s_hemiccParamsGPHemi.set("gpHemiExp", pCtx->hemi.mExp);
				s_hemiccParamsGPHemi.set("gpHemiGain", pCtx->hemi.mGain);
			} else {
				s_hemiccParamsGPHemi.set("gpHemiUpper", 1.0f);
				s_hemiccParamsGPHemi.set("gpHemiLower", 0.1f);
				s_hemiccParamsGPHemi.set("gpHemiUp", 0.0f, 1.0f, 0.0f);
				s_hemiccParamsGPHemi.set("gpHemiExp", 1.0f);
				s_hemiccParamsGPHemi.set("gpHemiGain", 0.0f);
			}
			if (pCtx) {
				s_hemiccParamsGPColor.set("gpInvWhite", pCtx->cc.mToneMap.get_inv_white());
				s_hemiccParamsGPColor.set("gpLClrGain", pCtx->cc.mToneMap.mLinGain);
				s_hemiccParamsGPColor.set("gpLClrBias", pCtx->cc.mToneMap.mLinBias);
				s_hemiccParamsGPColor.set("gpExposure", pCtx->cc.mExposure);
				s_hemiccParamsGPColor.set("gpInvGamma", pCtx->cc.get_inv_gamma());
			} else {
				s_hemiccParamsGPColor.set("gpInvWhite", 1.0f);
				s_hemiccParamsGPColor.set("gpLClrGain", 2.0f);
				s_hemiccParamsGPColor.set("gpLClrBias", 0.0f);
				s_hemiccParamsGPColor.set("gpExposure", 4.0f);
				s_hemiccParamsGPColor.set("gpInvGamma", 0.5f);
			}
			s_hemiccParamsGPHemi.bind();
			s_hemiccParamsGPHemi.send();
			s_hemiccParamsGPColor.bind();
			s_hemiccParamsGPColor.send();
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	} else if (mode == 5) {
		if (s_pidHemiFogCc) {
			glUseProgram(s_pidHemiFogCc);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, s_gbDiffTex);
			glUniform1i(s_locSmpHemiFogCcDiff, 0);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, s_gbClrTex);
			glUniform1i(s_locSmpHemiFogCcClr, 1);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, s_gbNrmTex);
			glUniform1i(s_locSmpHemiFogCcNrm, 2);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, s_gbPosTex);
			glUniform1i(s_locSmpHemiFogCcPos, 3);
			if (pCtx) {
				s_hemifogccParamsGPHemi.set("gpHemiUpper", pCtx->hemi.mUpper);
				s_hemifogccParamsGPHemi.set("gpHemiLower", pCtx->hemi.mLower);
				s_hemifogccParamsGPHemi.set("gpHemiUp", pCtx->hemi.mUp);
				s_hemifogccParamsGPHemi.set("gpHemiExp", pCtx->hemi.mExp);
				s_hemifogccParamsGPHemi.set("gpHemiGain", pCtx->hemi.mGain);
			} else {
				s_hemifogccParamsGPHemi.set("gpHemiUpper", 1.0f);
				s_hemifogccParamsGPHemi.set("gpHemiLower", 0.1f);
				s_hemifogccParamsGPHemi.set("gpHemiUp", 0.0f, 1.0f, 0.0f);
				s_hemifogccParamsGPHemi.set("gpHemiExp", 1.0f);
				s_hemifogccParamsGPHemi.set("gpHemiGain", 0.0f);
			}
			s_hemifogccParamsGPHemi.bind();
			s_hemifogccParamsGPHemi.send();
			if (pCtx) {
				s_hemifogccParamsGPColor.set("gpInvWhite", pCtx->cc.mToneMap.get_inv_white());
				s_hemifogccParamsGPColor.set("gpLClrGain", pCtx->cc.mToneMap.mLinGain);
				s_hemifogccParamsGPColor.set("gpLClrBias", pCtx->cc.mToneMap.mLinBias);
				s_hemifogccParamsGPColor.set("gpExposure", pCtx->cc.mExposure);
				s_hemifogccParamsGPColor.set("gpInvGamma", pCtx->cc.get_inv_gamma());
			} else {
				s_hemifogccParamsGPColor.set("gpInvWhite", 1.0f);
				s_hemifogccParamsGPColor.set("gpLClrGain", 2.0f);
				s_hemifogccParamsGPColor.set("gpLClrBias", 0.0f);
				s_hemifogccParamsGPColor.set("gpExposure", 4.0f);
				s_hemifogccParamsGPColor.set("gpInvGamma", 0.5f);
			}
			s_hemifogccParamsGPColor.bind();
			s_hemifogccParamsGPColor.send();
			s_hemifogccParamsGPWPos.set("gpWPosOffs", pCtx->view.mPos);
			s_hemifogccParamsGPWPos.bind();
			s_hemifogccParamsGPWPos.send();
			s_hemifogccParamsGPFog.set("gpFogViewPos", pCtx->view.mPos);
			s_hemifogccParamsGPFog.set("gpFogColor", pCtx->fog.mColor);
			s_hemifogccParamsGPFog.set("gpFogParam", pCtx->fog.mParam);
			s_hemifogccParamsGPFog.bind();
			s_hemifogccParamsGPFog.send();
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	}
	if (s_fsTriVBO) {
		glDisableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	OGLSys::swap();
}

static Draw::Ifc s_ifc;

struct DrwInit {
	DrwInit() {
		::memset(&s_ifc, 0, sizeof(s_ifc));
		s_ifc.info.pName = "ogl_x";
		s_ifc.info.needOGLContext = true;
		s_ifc.init = init;
		s_ifc.reset = reset;
		s_ifc.get_screen_width = get_screen_width;
		s_ifc.get_screen_height = get_screen_height;
		s_ifc.get_shadow_bias_mtx = get_shadow_bias_mtx;
		s_ifc.begin = begin;
		s_ifc.end = end;
		s_ifc.batch = batch;
		Draw::register_ifc_impl(&s_ifc);
	}
} s_drwInit;

DRW_IMPL_END

