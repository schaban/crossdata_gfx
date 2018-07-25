struct OBJ_CTX {
	xt_int xformMode;
	xt_int vtxNum;
};

struct CAM_CTX {
	xt_mtx view;
	xt_mtx proj;
	xt_mtx viewProj;
	xt_mtx invView;
	xt_mtx invProj;
	xt_mtx invViewProj;
	xt_float3 viewPos;
	xt_int param;
};

struct XFORM_CTX {
	xt_wmtx mtx;
};

struct SKIN_CTX {
	xt_int4   jnt;
	xt_float4 wgt;
};

struct LIT_CTX {
	xt_float3 hemiUp;
	xt_float3 hemiSky;
	xt_float3 hemiGround;
	xt_float3 pos[D_GEX_MAX_DYN_LIGHTS];
	xt_float3 dir[D_GEX_MAX_DYN_LIGHTS];
	xt_float3 clr[D_GEX_MAX_DYN_LIGHTS];
	xt_float2 scl[D_GEX_MAX_DYN_LIGHTS];
	xt_float4 atn[D_GEX_MAX_DYN_LIGHTS];
	xt_float rad[D_GEX_MAX_DYN_LIGHTS];
	xt_int mode[D_GEX_MAX_DYN_LIGHTS];
	xt_int maxDynIdx;
	xt_int shMode;
};

struct SHL_CTX {
	xt_float4 data[D_GEX_NUM_SH_COEFS];
};

struct SDW_CTX {
	xt_mtx viewProj;
	xt_mtx xform;
	xt_float4 color;
	xt_float3 dir;
	xt_float2 fade; // start, invRange
	xt_float size;
	xt_float invSize;
	xt_float specValSel; // 0:sval, 1:val
	xt_int litIdx;
};

struct FOG_CTX {
	xt_float4 color;
	xt_float2 curve;
	xt_float start;
	xt_float invRange;
};

struct MTL_CTX {
	xt_float4 vclrGain;
	xt_float4 vclrBias;
	xt_float3 baseColor;
	xt_float3 specColor;
	xt_float3 diffRoughness;
	xt_float diffRoughnessTexRate;
	xt_float diffWrapGain;
	xt_float diffWrapBias;
	xt_float diffExpGain;
	xt_float diffExpBias;
	xt_float3 specRoughness;
	xt_float specRoughnessTexRate;
	xt_float specRoughnessMin;
	xt_float3 IOR;
	xt_float3 reflColor;
	xt_float reflLvl;
	xt_float3 shDiffClr;
	xt_float shDiffDtl;
	xt_float3 shReflClr;
	xt_float shReflDtl;
	xt_float shReflFrRate;
	xt_float reflDownFadeRate;
	xt_float reflFrGain;
	xt_float reflFrBias;
	xt_float viewFrGain;
	xt_float viewFrBias;
	xt_int diffMode;
	xt_int specMode;
	xt_int bumpMode;
	xt_int tangentMode;
	xt_int diffFresnelMode;
	xt_int specFresnelMode;
	xt_int vclrMode; // != 0 -> lmap; else Cd
	xt_int enableAlpha;
	xt_int receiveShadows;
	xt_int cullShadows;
	xt_float bumpFactor;
	xt_int flipTangent;
	xt_int flipBitangent;
	xt_float selfShadowFactor;
	xt_float shadowDensity;
	xt_float shadowAlphaThreshold;
	xt_float tessFactor;
};

struct GLB_CTX {
	xt_float4 invGamma;
	xt_float3 exposure;
	xt_float3 linWhite;
	xt_float3 linGain;
	xt_float3 linBias;
	xt_float3 diffLightFactor;
	xt_float3 diffSHFactor;
	xt_float3 reflSHFactor;
};
