// Author: Sergey Chaban <sergey.chaban@gmail.com>

#include "shader.h"
#include "context.h"

StructuredBuffer<CAM_CTX> PSCTX(g_cam);
StructuredBuffer<MTL_CTX> PSCTX(g_mtl);
StructuredBuffer<LIT_CTX> PSCTX(g_lit);
StructuredBuffer<GLB_CTX> PSCTX(g_glb);
StructuredBuffer<SHL_CTX> PSCTX(g_shl);
StructuredBuffer<FOG_CTX> PSCTX(g_fog);
StructuredBuffer<SDW_CTX> PSCTX(g_sdw);

Texture2D PSTEX(g_texBase);
Texture2D PSTEX(g_texSpec);
Texture2D PSTEX(g_texBump);
Texture2D PSTEX(g_texSMap);
Texture2D PSTEX(g_texRefl);

SamplerState g_smpLin : register(s0);
SamplerState g_smpPnt : register(s1);

#include "SH.h"
#include "pano.h"
#include "color.h"

struct MTL_GEOM {
	float3 pos;
	float3 nrm;
	float3 tng;
	float3 btg;
	float3 vnrm;
	float4 vclr;
};

struct MTL_VIEW {
	float3 pos;
	float3 dir;
	float3 refl;
};

struct MTL_UV {
	float2 base;
	float2 spec;
	float2 bump;
	float2 lmap;
};

struct MTL_TEX {
	float4 base;
	float4 spec;
	float3 refl;
};

struct MTL_LIT {
	float3 diff;
	float3 spec;
	float3 refl;
};

struct MTL_SDW {
	float sval;
	float val;
};

struct MTL_FOG {
	float3 add;
	float3 mul;
	float val;
};

struct MTL_WK {
	float4 scrPos;
	MTL_GEOM geom;
	MTL_VIEW view;
	MTL_UV uv;
	MTL_TEX tex;
	MTL_LIT lit;
	MTL_SDW sdw;
	MTL_FOG fog;
};

float3 approxFresnelSchlick(float tcos, float3 ior) {
	float3 r = (ior - 1) / (ior + 1);
	float3 f0 = r*r;
	return f0 + (1 - f0) * pow(1 - tcos, 5);
}

float3 calcSpecFresnel(float tcos, float3 ior) {
	return approxFresnelSchlick(tcos, ior);
}

float calcViewFresnel(float tcos, float gain, float bias) {
	return 1 - saturate((pow(1 - tcos, 5) * gain) + bias);
}

float3 calcDiffFresnel(float nl, float nv, float3 ior) {
	float3 r = (ior - 1) / (ior + 1);
	float3 f0 = r*r;
	return (21.0/20.0f) * (1 - f0) * pow(1 - nl, 5) * pow(1 - nv, 5);
}

bool lightRadiusCk(float3 pos, int lidx) {
	return distance(pos, g_lit[0].pos[lidx]) <= g_lit[0].rad[lidx];
}


MTL_WK calcLight(MTL_WK wk) {
	MTL_CTX mtl = g_mtl[0];
	GLB_CTX glb = g_glb[0];
	LIT_CTX lctx = g_lit[0];
	MTL_LIT lit = (MTL_LIT)0;
	float3 P = wk.geom.pos;
	float3 N = wk.geom.nrm;
	float3 V = -wk.view.dir;
	float3 R = wk.view.refl;
	float NV = dot(N, V);
	float nv = max(0.0, NV);

	float3 drough = clamp(mtl.diffRoughness * lerp(1.0, wk.tex.spec.a, mtl.diffRoughnessTexRate), 1e-6, 1.0);
	float3 srough = clamp(mtl.specRoughness * lerp(1.0, wk.tex.spec.a, mtl.specRoughnessTexRate), mtl.specRoughnessMin, 1.0);

	if (lctx.maxDynIdx >= 0) {
		[unroll(D_GEX_MAX_DYN_LIGHTS)]
		for (int i = 0; i < D_GEX_MAX_DYN_LIGHTS; ++i) {
			if (i > lctx.maxDynIdx) break;
			int lmode = lctx.mode[i];
			if (lmode > 0 && !(lmode == D_GEX_LIGHT_MODE_OMNI && !lightRadiusCk(P, i))) {
				float3 lpos = g_lit[0].pos[i];
				float3 ldir = g_lit[0].dir[i];
				float3 lclr = g_lit[0].clr[i];
				float2 lscl = g_lit[0].scl[i];
				float4 latn = g_lit[0].atn[i];
				if (lmode == D_GEX_LIGHT_MODE_OMNI) {
					ldir = normalize(P - lpos);
				}
				float ldiffScl = lscl.x;
				float lspecScl = lscl.y;
				float3 ldiff = lclr*ldiffScl;
				float3 lspec = lclr*lspecScl;
				float3 L = -ldir;
				float3 H = normalize(L+V);
				float NL = dot(N, L);
				float NH = dot(N, H);
				float LH = dot(L, H);
				float nl = max(0.0, NL);
				float nh = max(0.0, NH);
				float lh = max(0.0, LH);

				float distAttn = 1.0;
				float attnStart = latn.x;
				float attnFalloff = latn.y;
				if (attnStart >= 0) {
					float dist = length(P - lpos);
					distAttn = 1.0 - min(max((dist-attnStart)*attnFalloff, 0.0), 1.0);
					distAttn *= distAttn;
				}

				float angAttn = 1.0;
				if (lmode == D_GEX_LIGHT_MODE_SPOT) {
					float coneStart = latn.z;
					float coneFalloff = latn.w;
					angAttn = saturate((dot(normalize(P - lpos), ldir) - coneStart) * coneFalloff);
				}

				float3 diff = 0;
				if (mtl.diffMode == D_GEX_DIFF_WRAP) {
					float dval = abs(NL*mtl.diffWrapGain + mtl.diffWrapBias);
					float3 dexp = (1 - drough)*mtl.diffExpGain + mtl.diffExpBias;
					diff = pow((float3)dval, dexp) * ldiff;
				} else if (mtl.diffMode == D_GEX_DIFF_OREN_NAYAR) {
					float3 s = drough;
					float3 ss = s*s;
					float3 a = 1.0 - 0.5*(ss/(ss+0.57));
					float3 b = 0.45*(ss/(ss+0.09));
					float3 al = normalize(L - NL*N);
					float3 av = normalize(V - NV*N);
					float c = max(0, dot(al, av));
					float ci = clamp(NL, -1.0, 1.0);
					float cr = clamp(NV, -1.0, 1.0);
					float ti = acos(ci);
					float tr = acos(cr);
					float aa = max(ti, tr);
					float bb = min(ti, tr);
					float d = sin(aa)*tan(bb);
					diff = saturate(NL)*(a + b*c*d) * ldiff;
				} else {
					/* Lambert */
					diff = saturate(NL) * ldiff;
				}
				if (mtl.diffFresnelMode) {
					diff *= calcDiffFresnel(nl, nv, mtl.IOR);
				}

				float3 spec = 0;
				float3 sval = 0;
				float3 srr = srough*srough;
				if (mtl.specMode == D_GEX_SPEC_BLINN) {
					sval = pow((float3)nh, max(0.001, 2.0/(srr*srr) - 2.0));
					sval *= mtl.specFresnelMode ? 12 : 1; //TODO
				} else if (mtl.specMode == D_GEX_SPEC_GGX) {
					float3 srr2 = srr*srr;
					float3 dv = nh*nh*(srr2-1.0) + 1.0;
					float3 dstr = srr2 / (dv*dv*PI);
					float3 hsrr = srr / 2;
					float3 tnl = nl*(1.0-hsrr) + hsrr;
					float3 tnv = nv*(1.0-hsrr) + hsrr;
					sval = (nl*dstr) / (tnl*tnv);
				} else {
					/* Phong/default */
					float vr = max(0.0, dot(V, reflect(-L, N)));
					sval = pow((float3)vr, max(0.001, 2.0/srr - 2.0)) / 2.0;
					sval *= mtl.specFresnelMode ? 16 : 1; //TODO
				}
				spec = sval * lspec;
				if (mtl.specFresnelMode) {
					spec *= calcSpecFresnel(lh, mtl.IOR);
				}

				spec *= 1.0 - lerp(wk.sdw.sval, wk.sdw.val, g_sdw[0].specValSel)*(i == g_sdw[0].litIdx);

				diff *= distAttn;
				diff *= angAttn;
				spec *= distAttn;
				spec *= angAttn;
				lit.diff += diff;
				lit.spec += spec;
			}
		}
		lit.diff *= glb.diffLightFactor;
	}

	float3 hemi = lctx.hemiGround == lctx.hemiSky ? lctx.hemiSky : lerp(lctx.hemiGround, lctx.hemiSky, (dot(N, lctx.hemiUp) + 1.0) * 0.5);
	lit.diff += hemi;

	if (lctx.shMode != D_GEX_SHL_NONE) {
		float3 shDiff = 0;
		if (lctx.shMode == D_GEX_SHL_DIFF) {
			float3 shc[3*3];
			float shw[3];
			shGet3(shc, shw);
			shw[0] = 1.0;
			shw[1] = 1.0 / 1.5;
			shw[2] = 1.0 / 4.0;
			shApply3(shDiff, shc, shw, N.x, N.y, N.z);
		} else {
			// D_GEX_SHL_DIFF_REFL
			float3 shc[6*6];
			float2 shw[6];
			shGet6(shc, shw);
			float shDiffDtl = g_mtl[0].shDiffDtl;
			float shReflDtl = g_mtl[0].shReflDtl;
#if 1
			shReflDtl += lerp(24, 1, saturate((srough.r + srough.g + srough.b) / 3.0));
#endif
			[unroll(6)] for (int i = 0; i < 6; ++i) {
				float ii = (float)(-i*i);
				float2 wdr = ii / (2.0 * float2(shDiffDtl, shReflDtl));
				shw[i] = exp(wdr);
			}
#if 1
			float t = NV;
			float r = max(0.1, (drough.r + drough.g + drough.b) / 3.0);
			float s = 1.0 / (r*r / 2.0);
			float f = 1.0 - 1.0/(2.0 + s*0.65);
			float g = 1.0 / (2.22222 + s*0.1);
			float adjDC = f + g*0.5*(1.0 + 2.0*acos(t)/PI - t);
			float adjLin = f + (f - 1.0) * (1.0 - t);
			shw[0].x *= adjDC;
			shw[1].x *= adjLin;
#endif
			float3 shRefl;
			shApply6(shDiff, shRefl, shc, shw, float2(N.x, R.x), float2(N.y, R.y), float2(N.z, R.z));
			shRefl = lerp(shRefl, shRefl * calcSpecFresnel(nv, g_mtl[0].IOR), g_mtl[0].shReflFrRate);
			shRefl *= g_mtl[0].shReflClr;
			shRefl *= g_glb[0].reflSHFactor;
			shRefl = max(shRefl, 0);
			lit.refl += shRefl;
		}
		shDiff *= g_mtl[0].shDiffClr;
		shDiff *= g_glb[0].diffSHFactor;
		lit.diff += shDiff;
	}

	wk.lit = lit;
	return wk;
}

MTL_WK calcFog(MTL_WK wk) {
	MTL_FOG fog = (MTL_FOG)0;
	float density = g_fog[0].color.a;
	if (density > 0) {
		float dist = distance(wk.geom.pos, wk.view.pos);
		float t = saturate((dist - g_fog[0].start) * g_fog[0].invRange) * density;
		fog.val = curve01(g_fog[0].curve.x, g_fog[0].curve.y, t);
		fog.add = g_fog[0].color.rgb * fog.val;
	}
	fog.mul = 1 - fog.val;
	wk.fog = fog;
	return wk;
}

float smpShadowMap(float2 uv) {
	return g_texSMap.SampleLevel(g_smpPnt, uv, 0, 0).r;
}

float shadowVal1(float4 spos) {
	float3 tpos = spos.xyz / spos.w;
	float2 uv = tpos.xy;
	float z = tpos.z;
	float d = smpShadowMap(uv);
	return d < z ? 1 : 0;
}

float shadowVal2(float4 spos) {
	spos.xyz /= spos.w;
	float2 uv00 = spos.xy;
	float2 uv11 = uv00 + g_sdw[0].invSize;

	float s00 = smpShadowMap(float2(uv00.x, uv00.y));
	float s01 = smpShadowMap(float2(uv00.x, uv11.y));
	float s10 = smpShadowMap(float2(uv11.x, uv00.y));
	float s11 = smpShadowMap(float2(uv11.x, uv11.y));

	half4 z = (half4)step(half4(s00, s01, s10, s11), spos.zzzz);
	half2 t = (half2)frac(uv00 * g_sdw[0].size);
	half2 v = (half2)lerp(z.xy, z.zw, t.x);
	return (half)lerp(v.x, v.y, t.y);
}

float shadowVal4W(float4 spos) {
	float invW = 1.0 / spos.w;
	float2 uv00 = spos.xy * invW;
	float r = g_sdw[0].invSize;
	float2 uv_1_1 = uv00 - r;
	float2 uv11 = uv00 + r;
	float2 uv_2_2 = uv00 - r*2;

	float s_2_2 = smpShadowMap(float2(uv_2_2.x, uv_2_2.y));
	float s_2_1 = smpShadowMap(float2(uv_2_2.x, uv_1_1.y));
	float s_20 = smpShadowMap(float2(uv_2_2.x, uv00.y));
	float s_21 = smpShadowMap(float2(uv_2_2.x, uv11.y));

	float s_1_2 = smpShadowMap(float2(uv_1_1.x, uv_2_2.y));
	float s_1_1 = smpShadowMap(float2(uv_1_1.x, uv_1_1.y));
	float s_10 = smpShadowMap(float2(uv_1_1.x, uv00.y));
	float s_11 = smpShadowMap(float2(uv_1_1.x, uv11.y));

	float s0_2 = smpShadowMap(float2(uv00.x, uv_2_2.y));
	float s0_1 = smpShadowMap(float2(uv00.x, uv_1_1.y));
	float s00 = smpShadowMap(float2(uv00.x, uv00.y));
	float s01 = smpShadowMap(float2(uv00.x, uv11.y));

	float s1_2 = smpShadowMap(float2(uv11.x, uv_2_2.y));
	float s1_1 = smpShadowMap(float2(uv11.x, uv_1_1.y));
	float s10 = smpShadowMap(float2(uv11.x, uv00.y));
	float s11 = smpShadowMap(float2(uv11.x, uv11.y));
	
	float pixR =  g_sdw[0].size;
	float2 t = frac(uv00 * pixR);
	float z = spos.z * invW;
	float wgt = g_mtl[0].selfShadowFactor;
	
	float4 svec;
	float4 zvec;
	float4 zlim;
	
	svec = float4(s_2_2, s_1_2, s0_2, s1_2);
	zvec = z - svec;
	zlim = zvec >= 0 ? 1 : 0;
	float4 zvec_2 = wgt <= 0 ? zlim : saturate(zvec * wgt);
	
	svec = float4(s_2_1, s_1_1, s0_1, s1_1);
	zvec = z - svec;
	zlim = zvec >= 0 ? 1 : 0;
	float4 zvec_1 = wgt <= 0 ? zlim : saturate(zvec * wgt);
	
	svec = float4(s_20, s_10, s00, s10);
	zvec = z - svec;
	zlim = zvec >= 0 ? 1 : 0;
	float4 zvec0 = wgt <= 0 ? zlim : saturate(zvec * wgt);
	
	svec = float4(s_21, s_11, s01, s11);
	zvec = z - svec;
	zlim = zvec >= 0 ? 1 : 0;
	float4 zvec1 = wgt <= 0 ? zlim : saturate(zvec * wgt);
	
	zvec = zvec0 + zvec_1 + zvec_2*(1-t.y) + zvec1*t.y;
	return ((zvec.x + zvec.y + zvec.z) + (zvec.w - zvec.x)*t.x) / 9;
}

float shadowVal(float4 spos) {
	return shadowVal4W(spos);
}

MTL_WK calcShadow(MTL_WK wk) {
	MTL_SDW sdw = (MTL_SDW)0;
	if (g_mtl[0].receiveShadows) {
		float3 wpos = wk.geom.pos;
		float4 spos = mul(float4(wpos, 1), g_sdw[0].xform);
		float4 scp = spos.xyzz/spos.w;
		if (all(scp.xyz>=0) && all(scp.xyz<=1)) {
			bool scull = g_mtl[0].cullShadows ? dot(wk.geom.nrm, -g_sdw[0].dir) < 0 : false;
			if (!scull) {
				float sval = shadowVal(spos);
				sdw.sval = sval;
				float sdens = g_sdw[0].color.a * g_mtl[0].shadowDensity;
				sval *= sdens;
				float2 sfade = g_sdw[0].fade;
				float fadeInvRange = sfade.y;
				if (fadeInvRange > 0) {
					float sdist = -mul(float4(wpos, 1), g_cam[0].view).z;
					float fadeStart = sfade.x;
					float fadeVal = 1.0 - saturate((sdist - fadeStart) * fadeInvRange);
					sval *= fadeVal;
				}
				sdw.val = sval;
			}
		}
	}
	wk.sdw = sdw;
	return wk;
}


float4 main(float4 scrPos : SV_POSITION, GEO_INFO geo : TEXCOORD, bool frontFaceFlg : SV_IsFrontFace) : SV_TARGET {
	CAM_CTX cam = g_cam[0];
	MTL_CTX mtl = g_mtl[0];

	MTL_WK wk = (MTL_WK)0;
	wk.scrPos = scrPos;

	wk.geom.pos = geo.pos;
	wk.geom.nrm = normalize(geo.nrm);
	wk.geom.tng = normalize(geo.tng);
	wk.geom.btg = normalize(geo.btg);
	wk.geom.vnrm = wk.geom.nrm;
	wk.geom.vclr = max(geo.clr*mtl.vclrGain + mtl.vclrBias, 0);
	if (!frontFaceFlg) wk.geom.nrm = -wk.geom.nrm;

	float2 priUV = geo.tex.xy;
	float2 secUV = geo.tex.zw;
	wk.uv.base = priUV;
	wk.uv.spec = priUV;
	wk.uv.bump = priUV;
	wk.uv.lmap = secUV;

	if (mtl.bumpMode != D_GEX_BUMP_NONE) {
		float3 nrm = wk.geom.nrm;
		float3 tng = wk.geom.tng;
		float3 btg = wk.geom.btg;
		if (mtl.tangentMode == D_GEX_TNG_AUTO) {
			float3 npos = normalize(wk.geom.pos);
			float3 s = ddx_fine(npos);
			float3 t = ddy_fine(npos);
			float3 v1 = cross(t, nrm);
			float3 v2 = cross(nrm, s);
			float2 uvdx = ddx_fine(wk.uv.bump);
			float2 uvdy = ddy_fine(wk.uv.bump);
			tng = v1*uvdx.x + v2*uvdy.x;
			btg = v1*uvdx.y + v2*uvdy.y;
			float tscl = 1.0f / sqrt(max(dot(tng, tng), dot(btg, btg)));
			tng *= tscl;
			btg *= tscl;
		}
		float4 bumpTex = g_texBump.Sample(g_smpLin, wk.uv.bump);
		if (mtl.bumpMode == D_GEX_BUMP_HMAP) {
			float2 uvdx = ddx_fine(wk.uv.bump);
			float2 uvdy = ddy_fine(wk.uv.bump);
			float bscl = 4;
			float xbump = g_texBump.Sample(g_smpLin, wk.uv.bump + uvdx).x;
			xbump -= bumpTex.x;
			xbump *= bscl;
			float ybump = g_texBump.Sample(g_smpLin, wk.uv.bump + uvdy).x;
			ybump -= bumpTex.x;
			ybump *= bscl;
			float3 nnrm = normalize(xbump*tng + ybump*btg + nrm);
			wk.geom.nrm = nnrm;
		} else {
			float2 nxy = (bumpTex.xy - 0.5) * 2.0;
			nxy *= g_mtl[0].bumpFactor;
			float2 sqxy = nxy * nxy;
			float nz = sqrt(max(0.0, 1.0 - sqxy.x - sqxy.y));
			float uflip = g_mtl[0].flipTangent != 0.0 ? -1.0 : 1.0;
			float vflip = g_mtl[0].flipBitangent != 0.0 ? -1.0 : 1.0;
			float3 nnrm = normalize(nxy.x*tng*uflip + nxy.y*btg*vflip + nz*nrm);
			wk.geom.nrm = nnrm;
		}
	}

	wk.view.pos = cam.viewPos;
	wk.view.dir = normalize(wk.geom.pos - wk.view.pos);
	wk.view.refl = reflect(wk.view.dir, wk.geom.nrm);

	wk.tex.base = g_texBase.Sample(g_smpLin, wk.uv.base);
	wk.tex.base.rgb *= mtl.baseColor;
	wk.tex.spec = g_texSpec.Sample(g_smpLin, wk.uv.spec);
	wk.tex.spec.rgb *= mtl.specColor;

	wk = calcShadow(wk);
	wk = calcLight(wk);
	if (mtl.vclrMode) {
		wk.lit.diff += wk.geom.vclr.rgb;
	} else {
		wk.tex.base.rgb *= wk.geom.vclr.rgb;
	}
	float reflLvl = g_mtl[0].reflLvl;
	if (reflLvl >= 0) {
		float4 refl = smpPanorama(g_texRefl, wk.view.refl, reflLvl);
		wk.lit.refl += refl.rgb * g_mtl[0].reflColor;
	}
	wk.lit.refl *= calcViewFresnel(dot(wk.view.dir, wk.geom.nrm), g_mtl[0].reflFrGain, g_mtl[0].reflFrBias);
	wk.lit.refl = lerp(wk.lit.refl, wk.lit.refl * (1.0 + min(wk.geom.nrm.y, 0.0)), g_mtl[0].reflDownFadeRate);
	float3 diff = wk.lit.diff * wk.tex.base.rgb;
	float3 spec = (wk.lit.spec + wk.lit.refl) * wk.tex.spec.rgb;
	spec *= calcViewFresnel(dot(wk.view.dir, wk.geom.nrm), g_mtl[0].viewFrGain, g_mtl[0].viewFrBias);

	float alpha = mtl.enableAlpha ? wk.tex.base.a*wk.geom.vclr.a : 1.0f;

	float4 tgtClr;
	tgtClr.rgb = diff + spec;
	wk = calcFog(wk);
	tgtClr.rgb = tgtClr.rgb*wk.fog.mul + wk.fog.add;
	tgtClr.rgb = lerp(tgtClr.rgb, g_sdw[0].color.rgb, wk.sdw.val);

	tgtClr.a = alpha;
	tgtClr = tgtColor(tgtClr);

	return tgtClr;
}
