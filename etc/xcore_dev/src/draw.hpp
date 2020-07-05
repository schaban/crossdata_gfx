#ifndef DRW_NO_VULKAN
#	define DRW_NO_VULKAN 0
#endif

#define DRW_IMPL_BEGIN namespace {
#define DRW_IMPL_END }


namespace Draw {

	enum Mode {
		DRWMODE_STD,
		DRWMODE_DISCARD,
		DRWMODE_SHADOW_CAST
	};

	enum TexUnit {
		TEXUNIT_Base = 0,
		TEXUNIT_Bump,
		TEXUNIT_Spec,
		TEXUNIT_Surf,
		TEXUNIT_Shadow,
		TEXUNIT_COUNT
	};

	inline float clip_gamma(const float gamma) { return nxCalc::max(gamma, 0.01f); }

	struct MdlParam {
		xt_float3 baseColorScl;
		float shadowOffsBias;
		float shadowWeightBias;
		float shadowDensScl;

		void reset() {
			baseColorScl.fill(1.0f);
			shadowOffsBias = 0.0f;
			shadowWeightBias = 0.0f;
			shadowDensScl = 1.0f;
		}
	};

	struct Context {
		struct View {
			cxFrustum frustum;
			cxMtx viewMtx;
			cxMtx projMtx;
			cxMtx viewProjMtx;
			cxMtx invViewMtx;
			cxMtx invProjMtx;
			cxMtx invViewProjMtx;
			cxVec pos;
			cxVec tgt;
			cxVec up;
			float znear;
			float zfar;
			float degFOVY;

			void update(const int width, const int height) {
				viewMtx.mk_view(pos, tgt, up, &invViewMtx);
				float aspect = nxCalc::div0(float(width), float(height));
				float fovy = XD_DEG2RAD(degFOVY);
				projMtx.mk_proj(fovy, aspect, znear, zfar);
				viewProjMtx = viewMtx * projMtx;
				invProjMtx = projMtx.get_inverted();
				invViewProjMtx = viewProjMtx.get_inverted();
				frustum.init(invViewMtx, fovy, aspect, znear, zfar);
			}

			void set_view(const cxVec& pos, const cxVec& tgt, const cxVec& up = cxVec(0.0f, 1.0f, 0.0f)) {
				this->pos = pos;
				this->tgt = tgt;
				this->up = up;
			}

			cxVec get_dir() const { return (tgt - pos).get_normalized(); }

			void reset() {
				set_view(cxVec(0.75f, 1.3f, 3.5f), cxVec(0.0f, 0.95f, 0.0f));
				znear = 0.1f;
				zfar = 1000.0f;
				degFOVY = 30.0f;
				update(640, 480);
			}
		} view;

		struct Hemi {
			xt_float3 upper;
			xt_float3 lower;
			xt_float3 up;
			float exp;
			float gain;

			void set_upvec(const cxVec& v) {
				cxVec n = v.get_normalized();
				up.set(n.x, n.y, n.z);
			}

			void reset() {
				upper.set(1.1f, 1.09f, 1.12f);
				lower.set(0.12f, 0.08f, 0.06f);
				up.set(0.0f, 1.0f, 0.0f);
				exp = 1.0f;
				gain = 1.0f;
			}
		} hemi;

		struct Spec {
			cxVec dir;
			xt_float3 clr;
			float shadowing;
			bool enabled;

			void set_dir(const cxVec& v) {
				dir = v.get_normalized();
			}

			void set_rgb(const float r, const float g, const float b) {
				clr.x = r;
				clr.y = g;
				clr.z = b;
			}

			void reset() {
				dir.set(0.0f, 0.0f, -1.0f);
				clr.fill(1.0f);
				shadowing = 1.0f;
				enabled = true;
			}
		} spec;

		struct Fog {
			xt_float4 color;
			xt_float4 param;

			void set_rgb(const float r, const float g, const float b) {
				color.x = r;
				color.y = g;
				color.z = b;
			}

			void set_density(const float a) {
				color.w = nxCalc::max(a, 0.0f);
			}

			void set_range(const float start, const float end) {
				param.x = start;
				param.y = nxCalc::rcp0(end - start);
			}

			void set_curve(const float cp1, const float cp2) {
				param.z = cp1;
				param.w = cp2;
			}

			void set_linear() {
				set_curve(1.0f / 3.0f, 2.0f / 3.0f);
			}

			void reset() {
				set_rgb(1.0f, 1.0f, 1.0f);
				set_density(0.0f);
				set_range(10.0f, 1000.0f);
				set_linear();
			}
		} fog;

		struct CC {
			xt_float3 gamma;
			xt_float3 exposure;
			xt_float3 linWhite;
			xt_float3 linGain;
			xt_float3 linBias;

			void set_gamma(const float gval) {
				gamma.fill(clip_gamma(gval));
			}

			void set_gamma_rgb(const float r, const float g, const float b) {
				gamma.set(clip_gamma(r), clip_gamma(g), clip_gamma(b));
			}

			void set_exposure(const float e) {
				exposure.fill(e);
			}

			void set_exposure_rgb(const float r, const float g, const float b) {
				exposure.set(r, g, b);
			}

			void set_linear_white(const float val) {
				linWhite.fill(val);
			}

			void set_linear_white_rgb(const float r, const float g, const float b) {
				linWhite.set(r, g, b);
			}

			void set_linear_gain(const float val) {
				linGain.fill(val);
			}

			void set_linear_gain_rgb(const float r, const float g, const float b) {
				linGain.set(r, g, b);
			}

			void set_linear_bias(const float val) {
				linBias.fill(val);
			}

			void set_linear_bias_rgb(const float r, const float g, const float b) {
				linBias.set(r, g, b);
			}

			void reset() {
				set_linear_white(1.0f);
				set_linear_gain(1.0f);
				set_linear_bias(0.0f);
				set_exposure(-1.0f);
				set_gamma(2.2f);
			}
		} cc;

		struct Shadow {
			cxMtx viewProjMtx;
			cxMtx mtx;
			cxVec dir;
			float dens;
			float densBias;
			float offsBias;
			float wghtBias;
			float objWghtBias;
			float fadeStart;
			float fadeEnd;

			void set_dir(const cxVec& v) {
				dir = v.get_normalized();
			}

			void set_dir_degrees(const float dx, const float dy) {
				set_dir(nxQuat::from_degrees(dx, dy, 0.0f).apply(nxVec::get_axis(exAxis::PLUS_Z)));
			}

			float get_density() const {
				return nxCalc::max(dens + densBias, 0.0f);
			}

			void reset() {
				set_dir_degrees(70, 140);
				dens = 1.0f;
				densBias = 0.0f;
				offsBias = 0.0f;
				wghtBias = 0.0f;
				fadeStart = 0.0f;
				fadeEnd = 0.0f;
				viewProjMtx.identity();
				mtx.identity();
			}
		} shadow;

		struct Glb {
			bool useSpec;
			bool useBump;

			void reset() {
				useSpec = true;
				useBump = true;
			}
		} glb;

		void reset() {
			view.reset();
			hemi.reset();
			spec.reset();
			fog.reset();
			cc.reset();
			shadow.reset();
			glb.reset();
		}

		bool ck_bbox_shadow_receive(const cxAABB& bbox) const {
			float sfadeStart = shadow.fadeStart;
			float sfadeEnd = shadow.fadeEnd;
			bool recvFlg = true;
			if (sfadeEnd > 0.0f && sfadeEnd > sfadeStart) {
				cxVec vpos = view.pos;
				cxVec npos = bbox.closest_pnt(vpos);
				if (nxVec::dist(vpos, npos) > sfadeEnd) {
					recvFlg = false;
				}
			}
			return recvFlg;
		}

	};

	struct Sprite {
		xt_float2 pos[4];
		xt_float2 tex[4];
		cxColor color;
		xt_float3 gamma;
		float refWidth;
		float refHeight;
		sxTextureData* pTex;
		cxColor* pClrs;

		void set_gamma(const float gval) {
			gamma.fill(clip_gamma(gval));
		}

		void set_gamma_rgb(const float r, const float g, const float b) {
			gamma.set(clip_gamma(r), clip_gamma(g), clip_gamma(b));
		}
	};

	struct Ifc {
		struct Info {
			const char* pName;
			bool needOGLContext;
		} info;

		void (*init)(int shadowSize, cxResourceManager* pRsrcMgr);
		void (*reset)();

		int (*get_screen_width)();
		int (*get_screen_height)();

		cxMtx (*get_shadow_bias_mtx)();

		void (*begin)(const cxColor& clearColor);
		void (*end)();

		void (*batch)(cxModelWork* pWk, const int ibat, const Mode mode, const Context* pCtx);

		void (*sprite)(Sprite* pSpr);
	};

	int32_t register_ifc_impl(Ifc* pIfc);
	Ifc* find_ifc_impl(const char* pName);
	Ifc* get_ifc_impl();

} // Draw
