#include "crossdata.hpp"
#include "gex.hpp"
#include "keyctrl.hpp"
#include "timer.hpp"
#include "remote.hpp"

#include "util.hpp"
#include "test.hpp"

#include "test3_chr.hpp"
#include "test3_stg.hpp"

#define D_KEY_F1 20
#define D_KEY_F2 21
#define D_KEY_F3 22
#define D_KEY_F4 24
#define D_KEY_CAM_SW 30
#define D_KEY_LIT_SW 31

static struct TEST3_WK {
	cCharacter3 chr;
	cStage3 stg;
	GEX_CAM* pCam;
	GEX_LIT* pLit;
	GEX_LIT* pConstLit;
	KEY_CTRL* pKeys;
	int colorMode;
	cStopWatch stopWatch;
	RMT_PIPE_IN* pPipeIn;
	union {
		TD_JOYSTICK_CHANNELS joych;
		float pipeInVals[16];
	};
} WK;

KEY_CTRL* get_ctrl_keys() {
	return WK.pKeys;
}

TD_JOYSTICK_CHANNELS* get_td_joy() {
	return &WK.joych;
}

float get_anim_speed() {
	return 0.5f;
}

bool stg_hit_ck(const cxVec& pos0, const cxVec& pos1, cxVec* pHitPos, cxVec* pHitNrm) {
	return WK.stg.hit_ck(pos0, pos1, pHitPos, pHitNrm);
}

sxGeometryData* get_stg_obst_geo() {
	return WK.stg.get_obst_geo();
}

void pipe_in_ctrl();

void test3_init() {
	::memset(WK.pipeInVals, 0, sizeof(WK.pipeInVals));
	rmtSocketInit();
	WK.pPipeIn = rmtPipeInCreate();
	for (int i = 0; i < 100; ++i) {
		pipe_in_ctrl();
	}

	WK.colorMode = get_test_mode() ? 2 : 1;

	WK.chr.create();
	WK.stg.create();
	WK.pCam = gexCamCreate("test3_cam");

	WK.pLit = gexLitCreate("test3_lit");
	gexFog(WK.pLit, cxColor(0.748f, 0.74f, 0.65f, 1.0f), 10.0f, 2000.0f, 0.1f, 0.1f);
	gexLitUpdate(WK.pLit);

	WK.pConstLit = make_const_lit("test3_const_lit", 0.75f);

	gexShadowColor(cxColor(0.0f, 0.0f, 0.0f, 1.5f)); // increase default shadow density
	gexShadowViewSize(8.0f);

	static struct KEY_INFO keys[] = {
		{ "[UP]", D_KEY_UP },
		{ "[DOWN]", D_KEY_DOWN },
		{ "[LEFT]", D_KEY_LEFT },
		{ "[RIGHT]", D_KEY_RIGHT },
		{ "[LSHIFT]", D_KEY_L1 },
		{ "[LCTRL]", D_KEY_L2 },
		{ "[RSHIFT]", D_KEY_R1 },
		{ "[RCTRL]", D_KEY_R2 },
		{ "Z", D_KEY_A },
		{ "X", D_KEY_B },

		{ "[F1]", D_KEY_F1 },
		{ "[F2]", D_KEY_F2 },
		{ "[F3]", D_KEY_F3 },
		{ "[F4]", D_KEY_F4 },

		{ "[PGDN]", D_KEY_CAM_SW },
		{ "[END]", D_KEY_LIT_SW }
	};
	WK.pKeys = keyCreate(keys, XD_ARY_LEN(keys));

	WK.stopWatch.alloc(120);
}

static void cam_ctrl() {
	static int camMode = 0;
	if (keyCkTrg(WK.pKeys, D_KEY_CAM_SW)) {
		camMode ^= 1;
	}
	GEX_CAM* pCam = WK.pCam;
	if (0) {
		//gexCamUpdate(pCam, cxVec(1.5f, 1.2f, 3.0f), cxVec(0.0f, 0.9f, 0.0f), nxVec::get_axis(exAxis::PLUS_Y), XD_DEG2RAD(40.0f), 0.1f, 1000.0f);
		gexCamUpdate(pCam, cxVec(1.5f, 1.2f, 3.0f), cxVec(0.6f, 0.9f, 0.0f), nxVec::get_axis(exAxis::PLUS_Y), XD_DEG2RAD(40.0f), 0.1f, 1000.0f);
	} else if (get_test_mode()) {
		cxMtx nowMtx = WK.chr.get_movement_mtx();
		cxQuat q;
		cxVec pos0(1.5f, 1.2f, 3.0f);
		cxVec pos1(-1.5f, 0.75f, 4.0f);
//cxVec pos0(1.5f, 1.2f-0.8f, 5.0f);
//cxVec pos1(-1.5f, 0.75f + 0.5f, 4.0f);
//cxVec pos0(2.5f, 1.2f - 0.8f, 5.0f);
//cxVec pos1(-1.5f, 0.75f + 0.6f, 4.0f);
//cxVec pos0(19.5f, 10.2f - 0.8f, 2.0f);
//cxVec pos1(28.5f, 9.75f + 0.6f, -18.0f);
//cxVec pos0(8.5f, 1.2f, 3.0f);
//cxVec pos1(-8.5f, 0.75f, 4.0f);
		q.from_vecs(pos0.get_normalized(), pos1.get_normalized());
		cxQuat q0;
		q0.identity();
		static float t = 0.0f;
		static float tadd = 0.001f;
		float tt = nxCalc::ease(t, 1.0f);
		cxVec pos = nxQuat::slerp(q0, q, tt).apply(pos0);
		t += tadd;
		if (t >= 1.0f) {
			t = 1.0f;
			tadd = -tadd;
		}
		if (t <= 0.0f) {
			t = 0.0f;
			tadd = -tadd;
		}
		float fov = XD_DEG2RAD(40.0f);
		gexCamUpdate(pCam, pos, cxVec(0.0f, 0.9f, 0.0f) + nowMtx.get_translation()*0.2f, nxVec::get_axis(exAxis::PLUS_Y), fov, 0.1f, 1000.0f);
	} else {
		static bool camFlg = false;
		static cxVec camPrev;
		cxVec tgt = WK.chr.get_world_pos();
		tgt.y += 1.5f;
		float tx = nxCalc::fit(tgt.x, -6.0f, 6.0f, 0.0f, 1.0f);
		tx = nxCalc::ease(tx);
		if (camMode == 0) {
			tx = nxCalc::fit(tx, 0.0f, 1.0f, -2.0f, 7.0f);
		} else {
			tx = nxCalc::fit(tx, 0.0f, 1.0f, -6.0f, 3.0f);
		}
		float tz = nxCalc::fit(nxCalc::max(tgt.z, -2.0f), -2.0f, 6.0f, 0.0f, 1.0f);
		tz = nxCalc::ease(tz, 0.75f);
		float ty = nxCalc::fit(tz, 0.0f, 1.0f, 1.1f, 3.0f);
		tz = nxCalc::fit(tz, 0.0f, 1.0f, 2.0f, 9.0f);
		cxVec pos(tx, ty, tz);
		if (camFlg) {
			pos = approach(camPrev, pos, 30);
		} else {
			camFlg = true;
		}
		camPrev = pos;
		gexCamUpdate(pCam, pos, tgt, nxVec::get_axis(exAxis::PLUS_Y), XD_DEG2RAD(40.0f), 0.1f, 1000.0f);
	}
}

static void color_ctrl() {
	int colorMode = WK.colorMode;
	if (keyCkTrg(WK.pKeys, D_KEY_F1)) {
		colorMode = 0;
	} else if (keyCkTrg(WK.pKeys, D_KEY_F2)) {
		colorMode = 1;
	} else if (keyCkTrg(WK.pKeys, D_KEY_F3)) {
		colorMode = 2;
	} else if (keyCkTrg(WK.pKeys, D_KEY_F4)) {
		colorMode = 3;
	}
	WK.colorMode = colorMode;
	switch (colorMode) {
	case 0:
	default:
		// no correction
		gexLinearWhite(1.0f);
		gexLinearGain(1.0f);
		gexLinearBias(0.0f);
		WK.stg.sky_fog_enable(true);
		break;
	case 1:
		gexLinearWhiteRGB(0.25f, 0.25f, 0.3025f);
		gexLinearGainRGB(0.75f, 0.75f, 0.67f);
		gexLinearBias(0.0f);
		WK.stg.sky_fog_enable(true);
		break;
	case 2:
		gexLinearWhiteRGB(0.3f, 0.225f, 0.5f);
		gexLinearGain(1.2f);
		gexLinearBias(0.0f);
		WK.stg.sky_fog_enable(false);
		break;
	case 3:
		gexLinearWhiteRGB(0.25f, 0.25f, 0.3025f);
		gexLinearGainRGB(1.25f, 1.25f, 1.017f);
		gexLinearBias(-0.015f);
		WK.stg.sky_fog_enable(false);
		break;
	}
	gexUpdateGlobals();
}

const void lamp_ctrl() {
	GEX_LIT* pLit = WK.pLit;
	if (!pLit) return;
	GEX_CAM* pCam = WK.pCam;

	static float t = 0.0f;
	static float tadd = 0.02f;
	float tt = nxCalc::ease(t, 2.0f);
	t += tadd;
	if (t >= 1.0f) {
		t = 1.0f;
		tadd = -tadd;
	}
	if (t <= 0.0f) {
		t = 0.0f;
		tadd = -tadd;
	}

	float val = nxCalc::fit(tt, 0.0f, 1.0f, 0.25f, 4.0f);
	cxColor clr = cxColor(0.95f, 0.24f, 0.1f);
	clr.scl_rgb(val);

	float attnStart = 0.018f;
	float attnEnd = 2.0f;

	float diffVal = 0.75f;
	float specVal = 4.0f;

	cxVec posList[] = {
		cxVec(-5.9f, 1.9f, -4.0f),
		cxVec( 5.9f, 1.9f, -4.0f),
		cxVec(-5.9f, 1.9f,  4.0f),
		cxVec( 5.9f, 1.9f,  4.0f)
	};

	int n = XD_ARY_LEN(posList);
	int lidx = 1;
	for (int i = 0; i < n; ++i) {
		cxVec pos = posList[i];
		bool vis = true;
		if (pCam) {
			cxSphere sph(pos, attnEnd);
			vis = gexSphereVisibility(pCam, sph);
		}
		if (vis) {
			gexLightMode(pLit, lidx, GEX_LIGHT_MODE::OMNI);
			gexLightPos(pLit, lidx, pos);
			gexLightRange(pLit, lidx, attnStart, attnEnd);
			gexLightColor(pLit, lidx, clr, diffVal, specVal);
			++lidx;
		}
	}

	//::printf("#Lamps visible: %d\n", lidx - 1);

	gexLitUpdate(pLit);
}

static void pipe_in_ctrl() {
	RMT_PIPE_IN* pPipe = WK.pPipeIn;
	if (!pPipe) return;

	int ntry = 100;
	int mode = 0;
	int nvals = XD_ARY_LEN(WK.pipeInVals);
	for (int i = 0; i < nvals; ++i) {
		//WK.pipeInVals[i] = 0.0f;
	}
	while (ntry > 0) {
		char* pCmd = rmtPipeInReadStr(pPipe);
		if (::strlen(pCmd)) {
			bool isStart = nxCore::str_starts_with(pCmd, "start");
			bool isSlice = nxCore::str_starts_with(pCmd, "slice");
			if (mode == 0) {
				if (isStart) {
					++mode;
				} else {
					if (!isStart && !isSlice) {
						::printf("%s\n", pCmd);
					}
				}
			}
			if (mode == 1 && isSlice) {
				char* pNext = nullptr;
				char* pTok = pCmd;
				int idx = -1;
				for (pTok = ::strtok_s(pTok, " ", &pNext); pTok; pTok = ::strtok_s(nullptr, " ", &pNext)) {
					if (idx >= 0) {
						WK.pipeInVals[idx] = (float)::atof(pTok);
					}
					++idx;
					if (idx >= nvals) {
						break;
					}
				}
				for (int i = idx; i < nvals; ++i) {
					if (i >= 0) {
						WK.pipeInVals[i] = 0.0f;
					}
				}
			} else {
				if (!isStart && !isSlice) {
					::printf("%s\n", pCmd);
				}
			}
		}
		--ntry;
	}

	if (1) {
		TD_JOYSTICK_CHANNELS* pJoy = get_td_joy();
		if (pJoy) {
			static struct {
				const char* pLabel;
				float TD_JOYSTICK_CHANNELS::* pVal;
			} joyTbl[] = {
				{ "xaxis", &TD_JOYSTICK_CHANNELS::xaxis },
				{ "yaxis", &TD_JOYSTICK_CHANNELS::yaxis },
				{ "zaxis", &TD_JOYSTICK_CHANNELS::zaxis },
				{ " xrot", &TD_JOYSTICK_CHANNELS::xrot },
				{ " yrot", &TD_JOYSTICK_CHANNELS::yrot },
				{ "   b1", &TD_JOYSTICK_CHANNELS::b1 },
				{ "   b2", &TD_JOYSTICK_CHANNELS::b2 },
				{ "   b3", &TD_JOYSTICK_CHANNELS::b3 },
				{ "   b4", &TD_JOYSTICK_CHANNELS::b4 },
				{ "   b5", &TD_JOYSTICK_CHANNELS::b5 },
				{ "   b6", &TD_JOYSTICK_CHANNELS::b6 }
			};
			con_locate(0, 1);
			cxColor nameClr(2.0f, 0.0f, 0.0f);
			cxColor valClr0(0.0f, 0.75f, 0.0f);
			cxColor valClr1(0.0f, 1.0f, 0.0f);
			for (int i = 0; i < XD_ARY_LEN(joyTbl); ++i) {
				con_text_color(nameClr);
				::printf("%s: ", joyTbl[i].pLabel);
				float val = pJoy->*joyTbl[i].pVal;
				con_text_color(::fabsf(val) > 0.001f ? valClr1 : valClr0);
				::printf("%f     \n", val);
			}
			con_text_color();
		}
	}
}

void test3_loop() {
	gexClearTarget(cxColor(0.44f, 0.55f, 0.66f));
	gexClearDepth();

	GEX_CAM* pCam = WK.pCam;
	GEX_LIT* pLit = WK.pLit;

	pipe_in_ctrl();

	WK.stopWatch.begin();

	keyUpdate(WK.pKeys);
	lamp_ctrl();
	color_ctrl();
	static bool litOff = false;
	if (keyCkTrg(WK.pKeys, D_KEY_LIT_SW)) {
		litOff ^= true;
	}
	if (litOff) {
		pLit = WK.pConstLit;
	}

	WK.chr.update();
	cam_ctrl();
	WK.stg.update();

	gexBeginScene(pCam);
	WK.chr.disp(pLit);
	WK.stg.disp(pLit);
	gexEndScene();

	bool timeSmpFlg = WK.stopWatch.end();
	if (timeSmpFlg) {
		double dt = WK.stopWatch.median();
		WK.stopWatch.reset();
		//::printf("dt = %.0f\n", dt);
	}

	gexSwap();
}

void test3_end() {
	WK.chr.destroy();
	WK.stg.destroy();
	gexLitDestroy(WK.pConstLit);
	gexLitDestroy(WK.pLit);
	gexCamDestroy(WK.pCam);
	keyDestroy(WK.pKeys);
	WK.stopWatch.free();
	rmtPipeInDestroy(WK.pPipeIn);
	rmtSocketReset();
}

