#include "crosscore.hpp"
#include "oglsys.hpp"
#include "draw.hpp"

#define MAX_XFORMS 128

static cxResourceManager* s_pRsrcMgr = nullptr;

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

static GLuint s_progId = 0;
static GLint s_locPos = -1;
static GLint s_locNrm = -1;
static GLint s_locTex = -1;
static GLint s_locClr = -1;
static GLint s_locWgt = -1;
static GLint s_locIdx = -1;

static GLuint s_gpIdxXform = (GLuint)-1;
static GLuint s_xformBufId = 0;
static GPXform s_gpXform;

static void prepare_model(sxModelData* pMdl) {
	if (!pMdl) return;
	GLuint* pBufIds = pMdl->get_gpu_wk<GLuint>();
	GLuint* pBufVB = &pBufIds[0];
	GLuint* pBufIB16 = &pBufIds[1];
	GLuint* pBufIB32 = &pBufIds[2];
	if (*pBufVB && (*pBufIB16 || *pBufIB32)) {
		return;
	}
	size_t vsize = pMdl->get_vtx_size();
	if (!(*pBufVB) && vsize > 0) {
		glGenBuffers(1, pBufVB);
		if (*pBufVB) {
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
					pVtx[i].idx.scl(3);
				}
			
				glBindBuffer(GL_ARRAY_BUFFER, *pBufVB);
				glBufferData(GL_ARRAY_BUFFER, npnt * sizeof(GPUVtx), pVtx, GL_STATIC_DRAW);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				nxCore::tMem<GPUVtx>::free(pVtx);
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

static void prepare_texture(sxTextureData* pTex) {
}

static void release_texture(sxTextureData* pTex) {

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
	s_pRsrcMgr->set_gfx_ifc(rsrcGfxIfc);
}

static void init_gpu_prog() {
	GLuint sidVert = load_shader("vtx.vert");
	GLuint sidFrag = load_shader("pix.frag");
	s_progId = OGLSys::link_draw_prog(sidVert, sidFrag);
	if (s_progId) {
		glDetachShader(s_progId, sidVert);
		glDetachShader(s_progId, sidFrag);
		s_locPos = glGetAttribLocation(s_progId, "vtxPos");
		s_locNrm = glGetAttribLocation(s_progId, "vtxNrm");
		s_locTex = glGetAttribLocation(s_progId, "vtxTex");
		s_locClr = glGetAttribLocation(s_progId, "vtxClr");
		s_locWgt = glGetAttribLocation(s_progId, "vtxWgt");
		s_locIdx = glGetAttribLocation(s_progId, "vtxIdx");

		glGenBuffers(1, &s_xformBufId);
		s_gpIdxXform = glGetUniformBlockIndex(s_progId, "GPXform");
		const int gpBindingXform = 0;
		glUniformBlockBinding(s_progId, s_gpIdxXform, gpBindingXform);
		glBindBufferBase(GL_UNIFORM_BUFFER, gpBindingXform, s_xformBufId);
		glBindBuffer(GL_UNIFORM_BUFFER, s_xformBufId);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(s_gpXform), nullptr, GL_DYNAMIC_DRAW);
	}
	glDeleteShader(sidVert);
	glDeleteShader(sidFrag);
}

static void init(int shadowSize, cxResourceManager* pRsrcMgr) {
	s_pRsrcMgr = pRsrcMgr;
	if (!pRsrcMgr) return;

	init_rsrc_mgr();
	init_gpu_prog();
}

static void reset() {
	glDeleteBuffers(1, &s_xformBufId);
	s_xformBufId = 0;
	glDeleteProgram(s_progId);
	s_progId = 0;
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
	if (!s_progId) return;
	if (!s_xformBufId) return;

	if (mode == Draw::DRWMODE_SHADOW_CAST) return;

	const sxModelData::Batch* pBat = pMdl->get_batch_ptr(ibat);
	if (!pBat) return;

	prepare_model(pMdl);

	GLuint* pBufIds = pMdl->get_gpu_wk<GLuint>();
	GLuint bufVB = pBufIds[0];
	if (!bufVB) return;
	GLuint bufIB16 = pBufIds[1];
	GLuint bufIB32 = pBufIds[2];

	glUseProgram(s_progId);

	s_gpXform.viewProj = pCtx->view.viewProjMtx;
	if (pWk->mpSkinXforms) {
		int xformsNum = pMdl->mSknNum;
		if (xformsNum > MAX_XFORMS) xformsNum = MAX_XFORMS;
		::memcpy(s_gpXform.xforms, pWk->mpSkinXforms, xformsNum * sizeof(xt_xmtx));
	} else if (pWk->mpWorldXform) {
		s_gpXform.xforms[0] = *pWk->mpWorldXform;
	} else {
		s_gpXform.xforms[0].identity();
	}

	size_t xformSize = sizeof(s_gpXform);
	glBindBuffer(GL_UNIFORM_BUFFER, s_xformBufId);
	void* pXformDst = glMapBufferRange(GL_UNIFORM_BUFFER, 0, xformSize, GL_MAP_WRITE_BIT);
	if (pXformDst) {
		::memcpy(pXformDst, &s_gpXform, xformSize);
	}
	glUnmapBuffer(GL_UNIFORM_BUFFER);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, bufVB);

	GLsizei stride = (GLsizei)sizeof(GPUVtx);
	size_t top = pBat->mMinIdx * stride;
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

	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);

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
}

static void end() {
	OGLSys::swap();
}

namespace Draw {

Ifc get_impl_ogl_x() {
	Ifc ifc;
	::memset(&ifc, 0, sizeof(ifc));
	ifc.init = init;
	ifc.reset = reset;
	ifc.get_screen_width = get_screen_width;
	ifc.get_screen_height = get_screen_height;
	ifc.get_shadow_bias_mtx = get_shadow_bias_mtx;
	ifc.begin = begin;
	ifc.end = end;
	ifc.batch = batch;
	return ifc;
}

} // Draw
