#include "crosscore.hpp"

#include <QtWidgets/QtWidgets>

#include "shaders.inc"

#define MAX_XFORMS 128
#define GP_BIND_XFORM 0
#define GP_BIND_LIGHT 1
#define GP_BIND_COLOR 2
#define GP_BIND_MATERIAL 3

#define TU_BASE 0

struct GPXform {
	xt_mtx  viewProj;
	xt_xmtx xforms[MAX_XFORMS];
};

struct GPLight {
	xt_float3 hemiUpper;
	float hemiUpperPad;
	xt_float3 hemiLower;
	float hemiLowerPad;
	xt_float3 hemiUp;
	float hemiExp;
	float hemiGain;
	xt_float3 pad;

	void reset() {
		hemiUpper.set(0.75f, 0.75f, 0.8f);
		hemiLower.set(0.15f, 0.15f, 0.1f);
		hemiUp.set(0.0f, 1.0f, 0.0f);
		hemiExp = 1.0f;
		hemiGain = 1.0f;
	}
};

struct GPColor {
	xt_float3 invGamma;
	float invGammaPad;

	void reset() {
		invGamma.fill(1.0f);
	}
};

struct GPMaterial {
	xt_float4 baseColorAlphaLim;

	void reset() {
		baseColorAlphaLim.fill(0.0f);
	}
};

struct GPUVtx {
	xt_float3   pos;
	xt_float3   nrm;
	xt_texcoord tex;
	xt_rgba     clr;
	xt_float4   wgt;
	xt_int4     idx;
};

GLuint compile_shader_str(QOpenGLFunctions* pfn, const char* pSrc, size_t srcSize, GLenum kind) {
	GLuint sid = 0;
	if (pfn && pSrc && srcSize > 0) {
		sid = pfn->glCreateShader(kind);
		if (sid) {
			GLint len[1] = { (GLint)srcSize };
			pfn->glShaderSource(sid, 1, &pSrc, len);
			pfn->glCompileShader(sid);
			GLint status = 0;
			pfn->glGetShaderiv(sid, GL_COMPILE_STATUS, &status);
			if (!status) {
				GLint infoLen = 0;
				pfn->glGetShaderiv(sid, GL_INFO_LOG_LENGTH, &infoLen);
				if (infoLen > 0) {
					char* pInfo = (char*)nxCore::mem_alloc(infoLen);
					if (pInfo) {
						pfn->glGetShaderInfoLog(sid, infoLen, &infoLen, pInfo);
						nxCore::dbg_msg(pInfo);
						nxCore::mem_free(pInfo);
						pInfo = nullptr;
					}
				}
				pfn->glDeleteShader(sid);
				sid = 0;
			}
		}
	}
	return sid;
}

GLuint compile_prog_impl(QOpenGLFunctions* pfn, const char* pSrcVtx, size_t sizeVtx, const char* pSrcPix, size_t sizePix, GLuint* pSIdVtx, GLuint* pSIdPix) {
	GLuint pid = 0;
	GLuint sidVtx = 0;
	GLuint sidPix = 0;
	if (pfn && pSrcVtx && pSrcPix) {
		sidVtx = compile_shader_str(pfn, pSrcVtx, sizeVtx, GL_VERTEX_SHADER);
		if (sidVtx) {
			sidPix = compile_shader_str(pfn, pSrcPix, sizePix, GL_FRAGMENT_SHADER);
			if (sidPix) {
				pid = pfn->glCreateProgram();
				if (pid) {
					pfn->glAttachShader(pid, sidVtx);
					pfn->glAttachShader(pid, sidPix);
					pfn->glLinkProgram(pid);
					GLint status = 0;
					pfn->glGetProgramiv(pid, GL_LINK_STATUS, &status);
					if (!status) {
						GLint infoLen = 0;
						pfn->glGetProgramiv(pid, GL_INFO_LOG_LENGTH, &infoLen);
						if (infoLen > 0) {
							char* pInfo = (char*)nxCore::mem_alloc(infoLen);
							if (pInfo) {
								pfn->glGetProgramInfoLog(pid, infoLen, &infoLen, pInfo);
								nxCore::dbg_msg(pInfo);
								nxCore::mem_free(pInfo);
								pInfo = nullptr;
							}
						}
						pfn->glDeleteProgram(pid);
						pid = 0;
						pfn->glDeleteShader(sidVtx);
						sidVtx = 0;
						pfn->glDeleteShader(sidPix);
						sidPix = 0;
					}
				} else {
					pfn->glDeleteShader(sidVtx);
					sidVtx = 0;
					pfn->glDeleteShader(sidPix);
					sidPix = 0;
				}
			} else {
				pfn->glDeleteShader(sidVtx);
				sidVtx = 0;
			}
		}
	}
	if (pSIdVtx) {
		*pSIdVtx = sidVtx;
	}
	if (pSIdPix) {
		*pSIdPix = sidPix;
	}
	return pid;
}

GLuint compile_prog_strs(QOpenGLFunctions* pfn, const char* pSrcVtx, const char* pSrcPix, GLuint* pSIdVtx, GLuint* pSIdPix) {
	return compile_prog_impl(pfn, pSrcVtx, ::strlen(pSrcVtx), pSrcPix, ::strlen(pSrcPix), pSIdVtx, pSIdPix);
}

GLuint compile_prog_strs(QOpenGLFunctions* pfn, const char* pSrcVtx, const char* pSrcPix) {
	GLuint sidVtx = 0;
	GLuint sidPix = 0;
	GLuint pid = compile_prog_strs(pfn, pSrcVtx, pSrcPix, &sidVtx, &sidPix);
	if (pfn) {
		if (pid) {
			pfn->glDetachShader(pid, sidVtx);
			pfn->glDetachShader(pid, sidPix);
		}
		pfn->glDeleteShader(sidVtx);
		pfn->glDeleteShader(sidPix);
	}
	return pid;
}


static Qt::MouseButton s_rotBtn = Qt::LeftButton;
static Qt::MouseButton s_posBtn = Qt::MiddleButton;

class OGLW : public QOpenGLWidget {
private:
	cxView mView;

	sxModelData* mpMdl;
	bool mMdlUpdateFlg;

	sxTextureData** mppTexs;
	int mNumTexs;

	GLuint mProgId;
	GLint mAttrLocPos;
	GLint mAttrLocNrm;
	GLint mAttrLocTex;
	GLint mAttrLocClr;
	GLint mAttrLocWgt;
	GLint mAttrLocIdx;
	GLuint mGPIdxXform;
	GLuint mGPIdxLight;
	GLuint mGPIdxColor;
	GLuint mGPIdxMaterial;
	GLint mSmpBase;

	GLuint mVB;
	GLuint mIB16;
	GLuint mIB32;
	GLuint mXformUB;
	GLuint mLightUB;
	GLuint mColorUB;
	GLuint mMaterialUB;

	GPXform mGPXform;
	GPLight mGPLight;
	GPColor mGPColor;
	GPMaterial mGPMaterial;

	GLuint mWhiteTex;

	int mHemiMode;
	float mHemiUpperFactor;
	float mHemiLowerFactor;
	float mGamma;

	struct ViewCtrl {
		QWidget* mpWgt;
		cxQuat mSpin;
		cxQuat mQuat;
		float mRadius;
		float mRelOffs;
		QPointF mRotOrg;
		QPointF mRotNow;
		QPointF mRotOld;
		QPointF mPosOrg;
		QPointF mPosNow;
		QPointF mPosOld;
		cxVec mPosOffs;

		void init(QWidget* pWgt) {
			mpWgt = pWgt;
			mRadius = 0.5f;
			mRelOffs = 0.0f;
			mSpin.identity();
			mQuat.identity();
			mRotOrg.setX(0);
			mRotOrg.setY(0);
			mRotNow = mRotOrg;
			mRotOld = mRotNow;
			mPosOrg.setX(0);
			mPosOrg.setY(0);
			mPosNow = mPosOrg;
			mPosOld = mPosNow;
			mPosOffs.zero();
		}

		static cxVec tball_proj(float x, float y, float r) {
			float d = nxCalc::hypot(x, y);
			float t = r / ::sqrtf(2.0f);
			float z;
			if (d < t) {
				z = ::sqrtf(r*r - d*d);
			} else {
				z = (t*t) / d;
			}
			return cxVec(x, y, z);
		}

		void update_tball(const float x0, const float y0, const float x1, float const y1) {
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

		bool add_rel_offs(int d) {
			bool res = true;
			const float addVal = 0.025f;
			if (d < 0) {
				mRelOffs += addVal;
			} else if (d > 0) {
				mRelOffs -= addVal;
			} else {
				res = false;
			}
			mRelOffs = nxCalc::clamp(mRelOffs, -1.0f, 1.0f);
			return res;
		}

		QPointF get_rel_pos(QMouseEvent* pEvt) {
			QPointF pos = pEvt->localPos();
			if (mpWgt) {
				pos.setX(nxCalc::div0(pos.x(), qreal(mpWgt->width())));
				pos.setY(nxCalc::div0(pos.y(), qreal(mpWgt->height())));
			}
			return pos;
		}

		bool begin(QMouseEvent* pEvt) {
			bool res = false;
			if (pEvt->buttons() & s_rotBtn) {
				mRotOrg = get_rel_pos(pEvt);
				mRotNow = mRotOrg;
				mRotOld = mRotOrg;
				res = true;
			} else if (pEvt->buttons() & s_posBtn) {
				mPosOrg = get_rel_pos(pEvt);
				mPosNow = mPosOrg;
				mPosOld = mPosOrg;
				res = true;
			}
			return res;
		}

		bool update(QMouseEvent* pEvt) {
			bool res = false;
			if (pEvt->buttons() & s_rotBtn) {
				float dx = mRotNow.x() - mRotOld.x();
				float dy = mRotNow.y() - mRotOld.y();
				if (dx || dy) {
					float x0 = mRotOld.x() - mRotOrg.x();
					float y0 = mRotOrg.y() - mRotOld.y();
					float x1 = mRotNow.x() - mRotOrg.x();
					float y1 = mRotOrg.y() - mRotNow.y();
					update_tball(x0, y0, x1, y1);
				}
				mRotOld = mRotNow;
				mRotNow = get_rel_pos(pEvt);
				res = true;
			} else if (pEvt->buttons() & s_posBtn) {
				mPosOld = mPosNow;
				mPosNow = get_rel_pos(pEvt);
				float dx = mPosNow.x() - mPosOld.x();
				float dy = mPosNow.y() - mPosOld.y();
				float add = 0.01f;
				if (dx && qAbs(dx) > qAbs(dy)) {
					add += qAbs(dx);
					mPosOffs.x += dx < 0 ? add : -add;
				} else if (dy) {
					add += qAbs(dy);
					mPosOffs.y += dy > 0 ? add : -add;
				}
				res = true;
			}
			return res;
		}

		bool end(QMouseEvent* pEvt) {
			bool res = false;
			if (pEvt->buttons() & s_rotBtn) {
				mRotNow = get_rel_pos(pEvt);
				mRotOld = mRotNow;
				res = true;
			} else if (pEvt->buttons() & s_posBtn) {
				mPosNow = get_rel_pos(pEvt);
				mPosOld = mPosNow;
				res = true;
			}
			return res;
		}
	} mViewCtrl;

protected:
	void mousePressEvent(QMouseEvent* pEvt) {
		if (mViewCtrl.begin(pEvt)) {
			update();
		}
	}

	void mouseMoveEvent(QMouseEvent* pEvt) {
		if (mViewCtrl.update(pEvt)) {
			update();
		}
	}

	void mouseReleaseEvent(QMouseEvent* pEvt) {
		if (mViewCtrl.end(pEvt)) {
			update();
		}
	}

	void wheelEvent(QWheelEvent* pEvt) {
		QPoint d = pEvt->angleDelta();
		if (mViewCtrl.add_rel_offs(d.ry())) {
			update();
		}
	}

public:
	OGLW(QWidget* pParent)
	: QOpenGLWidget(pParent),
	mpMdl(nullptr), mMdlUpdateFlg(true), mppTexs(nullptr), mNumTexs(0),
	mHemiMode(0), mHemiUpperFactor(1.0f), mHemiLowerFactor(1.0f),
	mGamma(2.2f),
	mProgId(0), mAttrLocPos(-1), mAttrLocNrm(-1), mAttrLocTex(-1), mAttrLocClr(-1), mAttrLocWgt(-1), mAttrLocIdx(-1),
	mGPIdxXform((GLuint)-1), mGPIdxLight((GLuint)-1), mGPIdxColor((GLuint)-1), mGPIdxMaterial((GLuint)-1),
	mSmpBase(-1),
	mVB(0), mIB16(0), mIB32(0), mXformUB(0), mLightUB(0), mColorUB(0), mMaterialUB(0),
	mWhiteTex(0)
	{
		for (int i = 0; i < MAX_XFORMS; ++i) {
			mGPXform.xforms[i].identity();
		}
		mGPLight.reset();
		mGPColor.reset();
		mGPLight.reset();
		mViewCtrl.init(this);
		QSurfaceFormat fmt = format();
		fmt.setDepthBufferSize(24);
		setFormat(fmt);
	}

	~OGLW() {
		reset_ogl();
	}

	void reset_ogl() {
		QOpenGLContext* pCtx = QOpenGLContext::currentContext();
		if (pCtx) {
			QOpenGLFunctions* pfn = pCtx->functions();
			if (pfn) {
				if (mWhiteTex) {
					pfn->glDeleteTextures(1, &mWhiteTex);
					mWhiteTex = 0;
				}
			}
		}
	}

	void initializeGL() override {
		QOpenGLFunctions* pfn = QOpenGLContext::currentContext()->functions();
		if (!pfn) return;
		QOpenGLExtraFunctions* pxfn = QOpenGLContext::currentContext()->extraFunctions();
		mProgId = compile_prog_strs(pfn, s_vtx_vert, s_pix_frag);
		if (mProgId) {
			mAttrLocPos = pfn->glGetAttribLocation(mProgId, "vtxPos");
			mAttrLocNrm = pfn->glGetAttribLocation(mProgId, "vtxNrm");
			mAttrLocTex = pfn->glGetAttribLocation(mProgId, "vtxTex");
			mAttrLocClr = pfn->glGetAttribLocation(mProgId, "vtxClr");
			mAttrLocWgt = pfn->glGetAttribLocation(mProgId, "vtxWgt");
			mAttrLocIdx = pfn->glGetAttribLocation(mProgId, "vtxIdx");
			if (pxfn) {
				mGPIdxXform = pxfn->glGetUniformBlockIndex(mProgId, "GPXform");
				pxfn->glUniformBlockBinding(mProgId, mGPIdxXform, GP_BIND_XFORM);
				mGPIdxLight = pxfn->glGetUniformBlockIndex(mProgId, "GPLight");
				pxfn->glUniformBlockBinding(mProgId, mGPIdxLight, GP_BIND_LIGHT);
				mGPIdxColor = pxfn->glGetUniformBlockIndex(mProgId, "GPColor");
				pxfn->glUniformBlockBinding(mProgId, mGPIdxColor, GP_BIND_COLOR);
				mGPIdxMaterial = pxfn->glGetUniformBlockIndex(mProgId, "GPMaterial");
				pxfn->glUniformBlockBinding(mProgId, mGPIdxMaterial, GP_BIND_MATERIAL);
			}
			mSmpBase = pfn->glGetUniformLocation(mProgId, "smpBase");
		}
		pfn->glGenBuffers(1, &mXformUB);
		if (mXformUB) {
			pfn->glBindBuffer(GL_UNIFORM_BUFFER, mXformUB);
			pfn->glBufferData(GL_UNIFORM_BUFFER, sizeof(GPXform), nullptr, GL_DYNAMIC_DRAW);
			pfn->glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}
		pfn->glGenBuffers(1, &mLightUB);
		if (mLightUB) {
			pfn->glBindBuffer(GL_UNIFORM_BUFFER, mLightUB);
			pfn->glBufferData(GL_UNIFORM_BUFFER, sizeof(GPLight), nullptr, GL_DYNAMIC_DRAW);
			pfn->glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}
		pfn->glGenBuffers(1, &mColorUB);
		if (mColorUB) {
			pfn->glBindBuffer(GL_UNIFORM_BUFFER, mColorUB);
			pfn->glBufferData(GL_UNIFORM_BUFFER, sizeof(GPColor), nullptr, GL_DYNAMIC_DRAW);
			pfn->glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}
		pfn->glGenBuffers(1, &mMaterialUB);
		if (mMaterialUB) {
			pfn->glBindBuffer(GL_UNIFORM_BUFFER, mMaterialUB);
			pfn->glBufferData(GL_UNIFORM_BUFFER, sizeof(GPMaterial), nullptr, GL_DYNAMIC_DRAW);
			pfn->glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}
		pfn->glGenTextures(1, &mWhiteTex);
		if (mWhiteTex) {
			static uint32_t whitePix = 0xFFFFFFFFU;
			pfn->glBindTexture(GL_TEXTURE_2D, mWhiteTex);
			pfn->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			pfn->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &whitePix);
			pfn->glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

	void resizeGL(int w, int h) override {
	}

	void paintGL() override {
		QOpenGLExtraFunctions* pfn = QOpenGLContext::currentContext()->extraFunctions();
		if (!pfn) return;

		int w = width();
		int h = height();
		pfn->glViewport(0, 0, w, h);
		pfn->glScissor(0, 0, w, h);
		pfn->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		pfn->glDepthMask(GL_TRUE);
		pfn->glEnable(GL_DEPTH_TEST);
		pfn->glDepthFunc(GL_LEQUAL);

		pfn->glClearColor(0.33f, 0.44f, 0.55f, 1.0f);
		pfn->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		if (!mProgId) return;
		if (!mpMdl) return;

		prepare_mdl();
		if (!mVB) return;

		mView.set_window(w, h);
		float mdlMaxSize = mpMdl->mBBox.get_size_vec().max_elem();
		cxVec viewTgt = mpMdl->mBBox.get_center();
		cxVec viewOffs = cxVec(0.0f, 0.0f, 2.0f) * mdlMaxSize;
		viewOffs += cxVec(0.0f, 0.0f, mViewCtrl.mRelOffs * (mViewCtrl.mRelOffs > 0.0f ? 1.0f : 1.5f)) * mdlMaxSize;
		cxQuat viewRot = mViewCtrl.mQuat;
		cxVec viewPos = viewTgt + viewRot.apply(viewOffs);
		cxVec viewTns = viewRot.apply(mViewCtrl.mPosOffs);
		viewTgt += viewTns;
		viewPos += viewTns;
		cxVec viewUp = viewRot.apply(nxVec::get_axis(exAxis::PLUS_Y));
		mView.set_frame(viewPos, viewTgt, viewUp);
		mView.set_z_planes(0.1f, mdlMaxSize * 5.0f);
		mView.update();

		pfn->glUseProgram(mProgId);

		mGPXform.viewProj = mView.mViewProjMtx;
		size_t xformSize = sizeof(xt_mtx) + sizeof(xt_xmtx)*MAX_XFORMS;
		pfn->glBindBufferBase(GL_UNIFORM_BUFFER, GP_BIND_XFORM, mXformUB);
		pfn->glBindBuffer(GL_UNIFORM_BUFFER, mXformUB);
		void* pXformDst = pfn->glMapBufferRange(GL_UNIFORM_BUFFER, 0, xformSize, GL_MAP_WRITE_BIT);
		if (pXformDst) {
			::memcpy(pXformDst, &mGPXform, xformSize);
		}
		pfn->glUnmapBuffer(GL_UNIFORM_BUFFER);

		GPLight gpLightBuf = mGPLight;
		pfn->glBindBufferBase(GL_UNIFORM_BUFFER, GP_BIND_LIGHT, mLightUB);
		pfn->glBindBuffer(GL_UNIFORM_BUFFER, mLightUB);
		void* pLightDst = pfn->glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(GPLight), GL_MAP_WRITE_BIT);
		if (pLightDst) {
			switch (mHemiMode) {
				case 0:
				default:
					gpLightBuf.hemiUp.set(0.0f, 1.0f, 0.0f);
					break;
				case 1:
					gpLightBuf.hemiUp = (mView.mTgt - mView.mPos).get_normalized();
					break;
				case 2:
					gpLightBuf.hemiUp = (mView.mPos - mView.mTgt).get_normalized();
					break;
			}
			if (mGamma > 0.0f && mGamma != 1.0f) {
				for (int i = 0; i < 2; ++i) {
					float* pC = i ? gpLightBuf.hemiUpper : gpLightBuf.hemiLower;
					for (int j = 0; j < 3; ++j) {
						pC[j] = ::powf(pC[j], mGamma);
					}
				}
			}
			gpLightBuf.hemiUpper.scl(mHemiUpperFactor);
			gpLightBuf.hemiLower.scl(mHemiLowerFactor);
			::memcpy(pLightDst, &gpLightBuf, sizeof(GPLight));
		}
		pfn->glUnmapBuffer(GL_UNIFORM_BUFFER);

		pfn->glBindBufferBase(GL_UNIFORM_BUFFER, GP_BIND_COLOR, mColorUB);
		pfn->glBindBuffer(GL_UNIFORM_BUFFER, mColorUB);
		void* pColorDst = pfn->glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(GPColor), GL_MAP_WRITE_BIT);
		if (pColorDst) {
			for (int i = 0; i < 3; ++i) {
				mGPColor.invGamma.fill(nxCalc::max(0.01f, nxCalc::rcp0(mGamma)));
			}
			::memcpy(pColorDst, &mGPColor, sizeof(GPColor));
		}
		pfn->glUnmapBuffer(GL_UNIFORM_BUFFER);

		pfn->glBindBuffer(GL_UNIFORM_BUFFER, 0);

		pfn->glBindBuffer(GL_ARRAY_BUFFER, mVB);

		pfn->glBindBufferBase(GL_UNIFORM_BUFFER, GP_BIND_MATERIAL, mMaterialUB);

		GPMaterial gpMtlBuf;
		int nbat = mpMdl->mBatNum;
		for (int i = 0; i < nbat; ++i) {
			const sxModelData::Batch* pBat = mpMdl->get_batch_ptr(i);
			if (!pBat) continue;
			const sxModelData::Material* pMtl = mpMdl->get_material(pBat->mMtlId);
			if (pMtl) {
				float alphaLim = pMtl->is_alpha() ? 0.5f : 0.0f;
				gpMtlBuf.baseColorAlphaLim.set(pMtl->mBaseColor.x, pMtl->mBaseColor.y, pMtl->mBaseColor.z, alphaLim);
			} else {
				gpMtlBuf.baseColorAlphaLim.set(1.0f, 1.0f, 1.0f, 0.0f);
			}
			if (0 == i || ::memcmp(&gpMtlBuf, &mGPMaterial, sizeof(GPMaterial)) != 0) {
				mGPMaterial = gpMtlBuf;
				pfn->glBindBuffer(GL_UNIFORM_BUFFER, mMaterialUB);
				void* pMaterialDst = pfn->glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(GPMaterial), GL_MAP_WRITE_BIT);
				if (pMaterialDst) {
					::memcpy(pMaterialDst, &mGPMaterial, sizeof(GPMaterial));
					pfn->glUnmapBuffer(GL_UNIFORM_BUFFER);
				}
			}
			GLsizei stride = (GLsizei)sizeof(GPUVtx);
			size_t top = pBat->mMinIdx * stride;
			if (mAttrLocPos >= 0) {
				pfn->glEnableVertexAttribArray(mAttrLocPos);
				pfn->glVertexAttribPointer(mAttrLocPos, 3, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(GPUVtx, pos)));
			}
			if (mAttrLocNrm >= 0) {
				pfn->glEnableVertexAttribArray(mAttrLocNrm);
				pfn->glVertexAttribPointer(mAttrLocNrm, 3, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(GPUVtx, nrm)));
			}
			if (mAttrLocTex >= 0) {
				pfn->glEnableVertexAttribArray(mAttrLocTex);
				pfn->glVertexAttribPointer(mAttrLocTex, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(GPUVtx, tex)));
			}
			if (mAttrLocClr >= 0) {
				pfn->glEnableVertexAttribArray(mAttrLocClr);
				pfn->glVertexAttribPointer(mAttrLocClr, 4, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(GPUVtx, clr)));
			}
			if (mAttrLocWgt >= 0) {
				pfn->glEnableVertexAttribArray(mAttrLocWgt);
				pfn->glVertexAttribPointer(mAttrLocWgt, 4, GL_FLOAT, GL_FALSE, stride, (const void*)(top + offsetof(GPUVtx, wgt)));
			}
			if (mAttrLocIdx >= 0) {
				pfn->glEnableVertexAttribArray(mAttrLocIdx);
				pfn->glVertexAttribPointer(mAttrLocIdx, 4, GL_INT, GL_FALSE, stride, (const void*)(top + offsetof(GPUVtx, idx)));
			}

			if (mSmpBase >= 0) {
				pfn->glUniform1i(mSmpBase, TU_BASE);
				pfn->glActiveTexture(GL_TEXTURE0 + TU_BASE);
				GLuint baseTexHandle = mWhiteTex;
				if (pMtl) {
					baseTexHandle = prepare_tex(pMtl->mBaseTexId);
					if (!baseTexHandle) {
						baseTexHandle = mWhiteTex;
					}
				}
				pfn->glBindTexture(GL_TEXTURE_2D, baseTexHandle);
			}

			if (pMtl && pMtl->mFlags.dblSided) {
				pfn->glDisable(GL_CULL_FACE);
			} else {
				pfn->glEnable(GL_CULL_FACE);
				pfn->glFrontFace(GL_CW);
				pfn->glCullFace(GL_BACK);
			}

			intptr_t org = 0;
			GLenum typ = GL_NONE;
			if (pBat->is_idx16()) {
				pfn->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIB16);
				org = pBat->mIdxOrg * sizeof(uint16_t);
				typ = GL_UNSIGNED_SHORT;
			} else {
				pfn->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIB32);
				org = pBat->mIdxOrg * sizeof(uint32_t);
				typ = GL_UNSIGNED_INT;
			}
			pfn->glDrawElements(GL_TRIANGLES, pBat->mTriNum * 3, typ, (const void*)org);

			if (mAttrLocPos >= 0) {
				pfn->glDisableVertexAttribArray(mAttrLocPos);
			}
			if (mAttrLocNrm >= 0) {
				pfn->glDisableVertexAttribArray(mAttrLocNrm);
			}
			if (mAttrLocTex >= 0) {
				pfn->glDisableVertexAttribArray(mAttrLocTex);
			}
			if (mAttrLocClr >= 0) {
				pfn->glDisableVertexAttribArray(mAttrLocClr);
			}
			if (mAttrLocWgt >= 0) {
				pfn->glDisableVertexAttribArray(mAttrLocWgt);
			}
			if (mAttrLocIdx >= 0) {
				pfn->glDisableVertexAttribArray(mAttrLocIdx);
			}
		}
	}

	void prepare_mdl() {
		if (!mpMdl) return;
		if (!mMdlUpdateFlg) return;
		QOpenGLFunctions* pfn = QOpenGLContext::currentContext()->functions();
		if (!pfn) return;
		release_mdl();
		pfn->glGenBuffers(1, &mVB);
		if (mVB) {
			int npnt = mpMdl->mPntNum;
			GPUVtx* pVtx = nxCore::tMem<GPUVtx>::alloc(npnt, "VtxTemp");
			if (pVtx) {
				for (int i = 0; i < npnt; ++i) {
					pVtx[i].pos = mpMdl->get_pnt_pos(i);
					pVtx[i].nrm = mpMdl->get_pnt_nrm(i);
					pVtx[i].tex = mpMdl->get_pnt_tex(i);
					pVtx[i].clr = mpMdl->get_pnt_clr(i);
					pVtx[i].wgt.fill(0.0f);
					pVtx[i].idx.fill(0);
					sxModelData::PntSkin skn = mpMdl->get_pnt_skin(i);
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

				pfn->glBindBuffer(GL_ARRAY_BUFFER, mVB);
				pfn->glBufferData(GL_ARRAY_BUFFER, npnt * sizeof(GPUVtx), pVtx, GL_STATIC_DRAW);
				pfn->glBindBuffer(GL_ARRAY_BUFFER, 0);
				nxCore::tMem<GPUVtx>::free(pVtx);
			}
		}
		if (mpMdl->mIdx16Num > 0) {
			const uint16_t* pIdxData = mpMdl->get_idx16_top();
			if (pIdxData) {
				pfn->glGenBuffers(1, &mIB16);
				if (mIB16) {
					pfn->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIB16);
					pfn->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mpMdl->mIdx16Num * sizeof(uint16_t), pIdxData, GL_STATIC_DRAW);
					pfn->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
				}
			}
		}
		if (mpMdl->mIdx32Num > 0) {
			const uint32_t* pIdxData = mpMdl->get_idx32_top();
			if (pIdxData) {
				pfn->glGenBuffers(1, &mIB32);
				if (mIB32) {
					pfn->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIB32);
					pfn->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mpMdl->mIdx32Num * sizeof(uint32_t), pIdxData, GL_STATIC_DRAW);
					pfn->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
				}
			}
		}
		mMdlUpdateFlg = false;
	}

	void release_mdl() {
		QOpenGLFunctions* pfn = QOpenGLContext::currentContext()->functions();
		if (!pfn) return;
		if (mVB) {
			pfn->glDeleteBuffers(1, &mVB);
			mVB = 0;
		}
		if (mIB16) {
			pfn->glDeleteBuffers(1, &mIB16);
			mIB16 = 0;
		}
		if (mIB32) {
			pfn->glDeleteBuffers(1, &mIB32);
			mIB32 = 0;
		}
		mpMdl->clear_tex_wk();
	}

	GLuint prepare_tex(int tid) {
		GLuint thandle = 0;
		QOpenGLFunctions* pfn = QOpenGLContext::currentContext()->functions();
		if (pfn && mpMdl && mppTexs && mpMdl->ck_tex_id(tid)) {
			sxTextureData* pTex = mppTexs[tid];
			if (pTex) {
				GLuint* pHandle = pTex->get_gpu_wk<GLuint>();
				if (pHandle) {
					if (!*pHandle) {
						pfn->glGenTextures(1, pHandle);
						if (*pHandle) {
							pfn->glBindTexture(GL_TEXTURE_2D, *pHandle);
							pfn->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
							pfn->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pTex->mWidth, pTex->mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pTex->get_data_ptr());
							pfn->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
							pfn->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
							pfn->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
							pfn->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
						}
					}
					thandle = *pHandle;
				}
			}
		}
		return thandle;
	}

	void release_texs() {
		QOpenGLFunctions* pfn = QOpenGLContext::currentContext()->functions();
		if (pfn && mppTexs) {
			for (int i = 0; i < mNumTexs; ++i) {
				sxTextureData* pTex = mppTexs[i];
				if (pTex) {
					GLuint* pHandle = pTex->get_gpu_wk<GLuint>();
					if (pHandle) {
						if (*pHandle) {
							pfn->glDeleteTextures(1, pHandle);
						}
						*pHandle = 0;
					}
				}
			}
		}
	}

	void unload_texs() {
		release_texs();
		if (mppTexs) {
			for (int i = 0; i < mNumTexs; ++i) {
				nxData::unload(mppTexs[i]);
				mppTexs[i] = nullptr;
			}
			nxCore::mem_free(mppTexs);
			mppTexs = nullptr;
		}
		mNumTexs = 0;
	}

	void load_texs(QString dir) {
		unload_texs();
		if (mpMdl && QFileInfo(dir).exists()) {
			int ntex = mpMdl->mTexNum;
			mNumTexs = ntex;
			if (ntex > 0) {
				mppTexs = nxCore::tMem<sxTextureData*>::alloc(ntex, "TexList");
				for (int i = 0; i < ntex; ++i) {
					mppTexs[i] = nullptr;
					QString texPath = dir + "/" + mpMdl->get_tex_name(i) + ".xtex";
					if (QFileInfo(texPath).exists()) {
						sxData* pTexData = nxData::load(texPath.toLocal8Bit().data());
						if (pTexData) {
							if (pTexData->is<sxTextureData>()) {
								mppTexs[i] = pTexData->as<sxTextureData>();
							} else {
								nxData::unload(pTexData);
							}
						}
					}
				}
			}
		}
	}

	void set_mdl(sxModelData* pMdl, QString dir) {
		mpMdl = pMdl;
		load_texs(dir);
		mViewCtrl.mQuat = nxQuat::from_degrees(-20.0f, 40.0f, 0.0f);
		mViewCtrl.mSpin.identity();
		mViewCtrl.mRelOffs = 0.0f;
		mViewCtrl.mPosOffs.zero();
		mMdlUpdateFlg = true;
		update();
	}

	GPLight* get_gp_light() { return &mGPLight; }
	int* get_hemi_mode_ptr() { return &mHemiMode; }
	float* get_hemi_upper_factor_ptr() { return &mHemiUpperFactor; }
	float* get_hemi_lower_factor_ptr() { return &mHemiLowerFactor; }
	float* get_gamma_ptr() { return &mGamma; }
};


class ColorLabel : public QLabel {
protected:
	xt_float3* mpDst;
public:
	ColorLabel(QWidget* pParent = nullptr)
	: QLabel(pParent),
	mpDst(nullptr)
	{
		setText("  ");
	}

	void update_color() {
		if (mpDst) {
			char cc[8];
			::memset(cc, 0, sizeof(cc));
			cxColor cf(mpDst->x, mpDst->y, mpDst->z);
			uint32_t ci = cf.encode_bgra8();
			for (int i = 0; i < 6; ++i) {
				static const char* pHex = "0123456789ABCDEF";
				cc[i] = pHex[(ci >> ((5-i)*4)) & 0xF];
			}
			setText(QString(" #") + cc);
			setStyleSheet(QString("color:black;background-color:#") + cc);
		}
		update();
	}

	void set_dst(xt_float3* pDst) {
		mpDst = pDst;
	}
};

class ColorChSlider : public QSlider {
protected:
	QWidget* mpUpdateWgt;
	ColorLabel* mpUpdateClrLbl;
	float* mpDst;
public:
	ColorChSlider(QWidget* pParent = nullptr)
	: QSlider(Qt::Horizontal, pParent),
	mpUpdateWgt(nullptr), mpUpdateClrLbl(nullptr), mpDst(nullptr)
	{
		setMinimum(0);
		setMaximum(1024);
	}

	void sliderChange(QAbstractSlider::SliderChange chg) {
		if (chg == SliderValueChange) {
			float scl = nxCalc::rcp0(float(maximum()));
			float val = float(value()) * scl;
			if (mpDst) {
				*mpDst = val;
			}
			update();
			if (mpUpdateWgt) {
				mpUpdateWgt->update();
			}
			if (mpUpdateClrLbl) {
				mpUpdateClrLbl->update_color();
			}
		}
	}

	void set_dst(float* pDst) { mpDst = pDst; }
	void set_update_widget(QWidget* pWgt) { mpUpdateWgt = pWgt; }
	void set_update_label(ColorLabel* pLbl) { mpUpdateClrLbl = pLbl; }

	void set_val(const float val) {
		setValue(int(nxCalc::saturate(val) * float(maximum())));
	}

	void set_val_to_dst() {
		if (mpDst) {
			set_val(*mpDst);
		}
	}
};


class ValSlider : public QSlider {
protected:
	QWidget* mpUpdateWgt;
	float* mpDst;
	float mMinVal;
	float mMaxVal;

public:
	ValSlider(const float minVal, const float maxVal, QWidget* pParent = nullptr)
	: QSlider(Qt::Horizontal, pParent),
	mpUpdateWgt(nullptr), mpDst(nullptr),
	mMinVal(minVal), mMaxVal(maxVal)
	{
		setMinimum(0);
		setMaximum(1024);
		setStyleSheet("QSlider::handle:horizontal{background-color:gray;}");
	}

	void sliderChange(QAbstractSlider::SliderChange chg) {
		if (chg == SliderValueChange) {
			float val = nxCalc::fit(float(value()), float(minimum()), float(maximum()), mMinVal, mMaxVal);
			if (mpDst) {
				*mpDst = val;
			}
			update();
			if (mpUpdateWgt) {
				mpUpdateWgt->update();
			}
		}
	}

	void set_update_widget(QWidget* pWgt) { mpUpdateWgt = pWgt; }

	void set_dst(float* pDst) { mpDst = pDst; }

	void set_val(const float val) {
		float rval = nxCalc::fit(val, mMinVal, mMaxVal, float(minimum()), float(maximum()));
		setValue(int(rval));
	}

	void set_val_to_dst() {
		if (mpDst) {
			set_val(*mpDst);
		}
	}
};

class ModeSel : public QComboBox {
protected:
	QWidget* mpUpdateWgt;
	int* mpDst;

public:
	ModeSel(QWidget* pParent = nullptr)
	: QComboBox(pParent),
	mpUpdateWgt(nullptr), mpDst(nullptr)
	{
		connect(this, QOverload<int>::of(&QComboBox::activated),
			[=](int idx) {
				if (mpDst) {
					*mpDst = idx;
				}
				if (mpUpdateWgt) {
					mpUpdateWgt->update();
				}
			}
		);
	}

	void set_dst(int* pDst) { mpDst = pDst; }
	void set_update_widget(QWidget* pWgt) { mpUpdateWgt = pWgt; }
};

class FloatSpin : public QDoubleSpinBox {
protected:
	QWidget* mpUpdateWgt;
	float* mpDst;

public:
	FloatSpin(const float minVal, const float maxVal, const float step, QWidget* pParent = nullptr)
	: QDoubleSpinBox(pParent),
	mpUpdateWgt(nullptr), mpDst(nullptr)
	{
		setRange(minVal, maxVal);
		setSingleStep(step);

		connect(this, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			[=](double val) {
				if (mpDst) {
					*mpDst = float(val);
				}
				if (mpUpdateWgt) {
					mpUpdateWgt->update();
				}
			}
		);
	}

	void set_dst(float* pDst) { mpDst = pDst; }

	void set_update_widget(QWidget* pWgt) { mpUpdateWgt = pWgt; }

	void set_val(const float val) {
		double cval = nxCalc::clamp(double(val), minimum(), maximum());
		setValue(cval);
	}

	void set_val_to_dst() {
		if (mpDst) {
			set_val(*mpDst);
		}
	}
};

class LightCtrl : public QWidget {
protected:
	QGridLayout* mpGrid;
	ColorChSlider* mpHemiUpperSliders[3];
	ColorLabel* mpHemiUpperLabel;
	FloatSpin* mpHemiUpperFactor;
	ColorChSlider* mpHemiLowerSliders[3];
	ColorLabel* mpHemiLowerLabel;
	FloatSpin* mpHemiLowerFactor;
	ValSlider* mpHemiExpSld;
	ValSlider* mpHemiGainSld;
	ModeSel* mpHemiModeSel;

public:
	LightCtrl(QWidget* pParent)
	: QWidget(pParent),
	mpGrid(nullptr)
	{
		QWidget* pWgt = new QWidget(this);
		mpGrid = new QGridLayout(pWgt);
		mpGrid->setColumnStretch(0, 1);

		int lblStyle = 0;

		QLabel* pLbl = new QLabel();
		pLbl->setFrameStyle(lblStyle);
		pLbl->setText("Hemi Upper:");
		mpGrid->addWidget(pLbl, 0, 0);

		ColorLabel* pClrLbl = new ColorLabel();
		pClrLbl->setFrameStyle(lblStyle);
		pClrLbl->setStyleSheet("background-color:black");
		mpGrid->addWidget(pClrLbl, 0, 1);
		mpHemiUpperLabel = pClrLbl;

		pLbl = new QLabel();
		pLbl->setFrameStyle(lblStyle);
		pLbl->setText("Upper Factor:");
		mpGrid->addWidget(pLbl, 4, 0);
		mpHemiUpperFactor = new FloatSpin(0.0f, 5.0f, 0.1f);
		mpGrid->addWidget(mpHemiUpperFactor, 4, 1);

		pLbl = new QLabel();
		pLbl->setFrameStyle(lblStyle);
		pLbl->setText("Hemi Lower:");
		mpGrid->addWidget(pLbl, 5, 0);

		pClrLbl = new ColorLabel();
		pClrLbl->setFrameStyle(lblStyle);
		pClrLbl->setStyleSheet("background-color:black");
		mpGrid->addWidget(pClrLbl, 5, 1);
		mpHemiLowerLabel = pClrLbl;

		pLbl = new QLabel();
		pLbl->setFrameStyle(lblStyle);
		pLbl->setText("Lower Factor:");
		mpGrid->addWidget(pLbl, 9, 0);
		mpHemiLowerFactor = new FloatSpin(0.0f, 5.0f, 0.1f);
		mpGrid->addWidget(mpHemiLowerFactor, 9, 1);

		for (int j = 0; j < 2; ++j) {
			ColorChSlider** ppSldLst = nullptr;
			int topRow = 0;
			switch (j) {
				case 0:
					ppSldLst = mpHemiUpperSliders;
					topRow = 1;
					break;
				case 1:
					ppSldLst = mpHemiLowerSliders;
					topRow = 6;
					break;
			}
			if (ppSldLst) {
				for (int i = 0; i < 3; ++i) {
					static const char* pSldNames[] = { "R:", "G:", "B:" };
					static const char* pSldHandleClrs[] = { "#992222", "#119911", "#2222AA" };
					pLbl = new QLabel();
					pLbl->setFrameStyle(lblStyle);
					pLbl->setText(pSldNames[i]);
					pLbl->setAlignment(Qt::AlignRight);
					mpGrid->addWidget(pLbl, topRow + i, 0);
					ColorChSlider* pSld = new ColorChSlider();
					pSld->setStyleSheet(QString("QSlider::handle:horizontal{background-color:") + pSldHandleClrs[i] + ";}");
					ppSldLst[i] = pSld;
					mpGrid->addWidget(pSld, topRow + i, 1);
				}
			}
		}

		pLbl = new QLabel();
		pLbl->setFrameStyle(lblStyle);
		pLbl->setText("Hemi Exp:");
		mpGrid->addWidget(pLbl, 10, 0);
		mpHemiExpSld = new ValSlider(0.1f, 4.0f);
		mpGrid->addWidget(mpHemiExpSld, 10, 1);

		pLbl = new QLabel();
		pLbl->setFrameStyle(lblStyle);
		pLbl->setText("Hemi Gain:");
		mpGrid->addWidget(pLbl, 11, 0);
		mpHemiGainSld = new ValSlider(0.001f, 4.0f);
		mpGrid->addWidget(mpHemiGainSld, 11, 1);

		pLbl = new QLabel();
		pLbl->setFrameStyle(lblStyle);
		pLbl->setText("Hemi Mode:");
		mpGrid->addWidget(pLbl, 12, 0);
		mpHemiModeSel = new ModeSel();
		mpHemiModeSel->addItems(QStringList() << "Sky" << "Back" << "Front");
		mpGrid->addWidget(mpHemiModeSel, 12, 1);
	}

	ColorChSlider** get_hemi_upper_sliders() { return mpHemiUpperSliders; }
	ColorLabel* get_hemi_upper_label() { return mpHemiUpperLabel; }
	FloatSpin* get_hemi_upper_factor() { return mpHemiUpperFactor; }
	ColorChSlider** get_hemi_lower_sliders() { return mpHemiLowerSliders; }
	ColorLabel* get_hemi_lower_label() { return mpHemiLowerLabel; }
	FloatSpin* get_hemi_lower_factor() { return mpHemiLowerFactor; }
	ValSlider* get_hemi_exp_slider() { return mpHemiExpSld; }
	ValSlider* get_hemi_gain_slider() { return mpHemiGainSld; }
	ModeSel* get_hemi_mode_sel() { return mpHemiModeSel; }
};

class ColorCtrl : public QWidget {
private:

protected:
	QGridLayout* mpGrid;
	FloatSpin* mpGammaSpin;

public:
	ColorCtrl(QWidget* pParent)
	: QWidget(pParent),
	mpGrid(nullptr)
	{
		QWidget* pWgt = new QWidget(this);
		mpGrid = new QGridLayout(pWgt);
		mpGrid->setColumnStretch(0, 1);

		int lblStyle = 0;

		QLabel* pLbl = new QLabel();
		pLbl->setFrameStyle(lblStyle);
		pLbl->setText("Gamma:");
		mpGrid->addWidget(pLbl, 0, 0);

		mpGammaSpin = new FloatSpin(0.1f, 4.0f, 0.1f);
		mpGrid->addWidget(mpGammaSpin, 0, 1);
	}

	FloatSpin* get_gamma_spin() { return mpGammaSpin; }
};


class AppWnd : public QMainWindow {
private:
	Q_OBJECT

protected:
	QMenu* mpFileMenu;
	QAction* mpFileOpenAct;
	QAction* mpAppExitAct;
	OGLW* mpOGLW;
	QListWidget* mpMtlList;
	QListWidget* mpBatList;
	QListWidget* mpSklList;
	LightCtrl* mpLightCtrl;
	ColorCtrl* mpColorCtrl;

	sxModelData* mpMdlData;

	void wnd_init();

signals:

private slots:
	void file_open();
	void app_exit();

public:
	AppWnd(QWidget* pParent = nullptr)
	: QMainWindow(pParent),
	mpFileMenu(nullptr), mpFileOpenAct(nullptr), mpAppExitAct(nullptr),
	mpMtlList(nullptr), mpBatList(nullptr), mpSklList(nullptr),
	mpLightCtrl(nullptr), mpColorCtrl(nullptr),
	mpOGLW(nullptr),
	mpMdlData(nullptr)
	{
		wnd_init();
	}

	~AppWnd() {
		if (mpMdlData) {
			nxData::unload(mpMdlData);
		}
	}
};

#include "qt_meta.inc"

void AppWnd::file_open() {
	QString mdlPath = QFileDialog::getOpenFileName(this, "Load crosscore model", "", "crosscore models (*.xmdl)");
	QFileInfo fi = QFileInfo(mdlPath);
	if (fi.exists()) {
		QByteArray path = mdlPath.toLocal8Bit();
		char* pPath = path.data();
		if (pPath) {
			sxData* pData = nxData::load(pPath);
			if (pData && pData->is<sxModelData>()) {
				if (mpMdlData) {
					nxData::unload(mpMdlData);
					mpMdlData = nullptr;
				}
				mpMdlData = pData->as<sxModelData>();
				if (mpOGLW) {
					mpOGLW->set_mdl(mpMdlData, fi.absoluteDir().absolutePath());
				}
				if (mpMdlData) {
					if (mpMtlList) {
						mpMtlList->clear();
						int nmtl = mpMdlData->mMtlNum;
						QStringList mtls = QStringList();
						for (int i = 0; i < nmtl; ++i) {
							mtls << (QString::number(i) + ": " + mpMdlData->get_material_name(i));
						}
						mpMtlList->addItems(mtls);
					}
					if (mpBatList) {
						mpBatList->clear();
						int nbat = mpMdlData->mBatNum;
						QStringList bats = QStringList();
						for (int i = 0; i < nbat; ++i) {
							const sxModelData::Batch* pBat = mpMdlData->get_batch_ptr(i);
							bats << (QString::number(i) + ": " + mpMdlData->get_batch_name(i) + ", " + QString::number(pBat->mTriNum) + " tris");
						}
						mpBatList->addItems(bats);
					}
					if (mpSklList) {
						mpSklList->clear();
						int nskl = mpMdlData->mSklNum;
						QStringList skls = QStringList();
						if (nskl > 0) {
							for (int i = 0; i < nskl; ++i) {
								skls << (QString::number(i) + ": " + mpMdlData->get_skel_name(i));
							}
						} else {
							skls << "-- no skeleton --";
						}
						mpSklList->addItems(skls);
					}
				}
			}
		}
	}
}

void AppWnd::app_exit() {
	if (mpOGLW) {
		mpOGLW->reset_ogl();
	}
	close();
}

void AppWnd::wnd_init() {
	QPalette pal = palette();
	pal.setColor(QPalette::Background, QColor(60, 70, 80));
	pal.setColor(QPalette::Foreground, QColor(210, 210, 190));
	setAutoFillBackground(true);
	setPalette(pal);

	setWindowTitle("xmdl viewer");
	resize(1000, 600);

	mpFileOpenAct = new QAction("&Open", this);
	connect(mpFileOpenAct, &QAction::triggered, this, &AppWnd::file_open);

	mpAppExitAct = new QAction("Exit", this);
	connect(mpAppExitAct, &QAction::triggered, this, &AppWnd::app_exit);

	QMenuBar* pMenu = menuBar();
	if (pMenu) {
		mpFileMenu = pMenu->addMenu("&File");
		if (mpFileMenu) {
			mpFileMenu->addAction(mpFileOpenAct);
			mpFileMenu->addSeparator();
			mpFileMenu->addAction(mpAppExitAct);
		}
		pMenu->setStyleSheet(
			"QMenuBar{color:#909080;background-color:#333333;}QMenuBar::item{background:transparent;}QMenuBar::item:selected{background:white;}QMenuBar::item:selected{background:black;}"
			"QMenu{color:#909080;background-color:#333333;}QMenu::item{background:transparent;}QMenu::item:selected{background:white;}QMenu::item:selected{background:#222222;}"
		);
	}

	mpOGLW = new OGLW(this);
	if (mpOGLW) {
		setCentralWidget(mpOGLW);
	}

	Qt::DockWidgetArea dockPlacementInfo = Qt::LeftDockWidgetArea;
	Qt::DockWidgetArea dockPlacementCtrl = Qt::RightDockWidgetArea;
	QString listStyleSheet = "color:#88E8F0;background-color:#333333";
	QString ctrlStyleSheet = "color:#AAAAAA;background-color:#222222";

	QDockWidget* pDock = new QDockWidget(":Materials", this);
	mpMtlList = new QListWidget(pDock);
	mpMtlList->addItems(QStringList() << "-- no materials --");
	mpMtlList->setStyleSheet(listStyleSheet);
	pDock->setWidget(mpMtlList);
	addDockWidget(dockPlacementInfo, pDock);

	pDock = new QDockWidget(":Batches", this);
	mpBatList = new QListWidget(pDock);
	mpBatList->addItems(QStringList() << "-- no batches --");
	mpBatList->setStyleSheet(listStyleSheet);
	pDock->setWidget(mpBatList);
	addDockWidget(dockPlacementInfo, pDock);

	pDock = new QDockWidget(":Skeleton", this);
	mpSklList = new QListWidget(pDock);
	mpSklList->addItems(QStringList() << "-- no skeleton --");
	mpSklList->setStyleSheet(listStyleSheet);
	pDock->setWidget(mpSklList);
	addDockWidget(dockPlacementInfo, pDock);

	pDock = new QDockWidget("Light", this);
	mpLightCtrl = new LightCtrl(pDock);
	mpLightCtrl->setStyleSheet(ctrlStyleSheet);
	mpLightCtrl->setMinimumWidth(180);
	mpLightCtrl->setMinimumHeight(20);
	pDock->setWidget(mpLightCtrl);
	addDockWidget(dockPlacementCtrl, pDock);

	pDock = new QDockWidget("Color", this);
	mpColorCtrl = new ColorCtrl(pDock);
	mpColorCtrl->setStyleSheet(ctrlStyleSheet);
	pDock->setWidget(mpColorCtrl);
	addDockWidget(dockPlacementCtrl, pDock);

	if (mpOGLW) {
		for (int j = 0; j < 2; ++j) {
			xt_float3* pDstVal = nullptr;
			ColorLabel* pClrLbl = nullptr;
			ColorChSlider** ppSlds = nullptr;
			switch (j) {
				case 0:
					pDstVal = &mpOGLW->get_gp_light()->hemiUpper;
					pClrLbl = mpLightCtrl->get_hemi_upper_label();
					ppSlds = mpLightCtrl->get_hemi_upper_sliders();
					break;
				case 1:
					pDstVal = &mpOGLW->get_gp_light()->hemiLower;
					pClrLbl = mpLightCtrl->get_hemi_lower_label();
					ppSlds = mpLightCtrl->get_hemi_lower_sliders();
					break;
				default:
					break;
			}
			if (pDstVal && pClrLbl && ppSlds) {
				pClrLbl->set_dst(pDstVal);
				for (int i = 0; i < 3; ++i) {
					ColorChSlider* pClrSld = ppSlds[i];
					pClrSld->set_dst(&(*pDstVal)[i]);
					pClrSld->set_val_to_dst();
					pClrSld->set_update_widget(mpOGLW);
					pClrLbl->update_color();
					pClrSld->set_update_label(pClrLbl);
				}
			}
		}

		ValSlider* pHemiExpSld = mpLightCtrl->get_hemi_exp_slider();
		pHemiExpSld->set_update_widget(mpOGLW);
		pHemiExpSld->set_dst(&mpOGLW->get_gp_light()->hemiExp);
		pHemiExpSld->set_val_to_dst();
		pHemiExpSld->update();

		ValSlider* pHemiGainSld = mpLightCtrl->get_hemi_gain_slider();
		pHemiGainSld->set_update_widget(mpOGLW);
		pHemiGainSld->set_dst(&mpOGLW->get_gp_light()->hemiGain);
		pHemiGainSld->set_val_to_dst();
		pHemiGainSld->update();

		ModeSel* pHemiModeSel = mpLightCtrl->get_hemi_mode_sel();
		pHemiModeSel->set_dst(mpOGLW->get_hemi_mode_ptr());
		pHemiModeSel->set_update_widget(mpOGLW);

		FloatSpin* pHemiUpperFactorSpin = mpLightCtrl->get_hemi_upper_factor();
		pHemiUpperFactorSpin->set_dst(mpOGLW->get_hemi_upper_factor_ptr());
		pHemiUpperFactorSpin->set_val_to_dst();
		pHemiUpperFactorSpin->set_update_widget(mpOGLW);

		FloatSpin* pHemiLowerFactorSpin = mpLightCtrl->get_hemi_lower_factor();
		pHemiLowerFactorSpin->set_dst(mpOGLW->get_hemi_lower_factor_ptr());
		pHemiLowerFactorSpin->set_val_to_dst();
		pHemiLowerFactorSpin->set_update_widget(mpOGLW);

		FloatSpin* pGammaSpin = mpColorCtrl->get_gamma_spin();
		pGammaSpin->set_dst(mpOGLW->get_gamma_ptr());
		pGammaSpin->set_val_to_dst();
		pGammaSpin->set_update_widget(mpOGLW);
	}
}

static void dbgmsg(const char* pMsg) {
	::printf("%s", pMsg);
}

int main(int argc, char* argv[]) {
	sxSysIfc sysIfc;
	::memset(&sysIfc, 0, sizeof(sysIfc));
	sysIfc.fn_dbgmsg = dbgmsg;
	nxSys::init(&sysIfc);

	QApplication app(argc, argv);
	AppWnd wnd;
	wnd.show();
	int res = app.exec();
	nxCore::mem_dbg();
	return res;
}
