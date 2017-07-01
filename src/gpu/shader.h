#include "defs.h"

#define PI (3.141593)

#define TREG(_id) t##_id
#define VSCTX(_name) _name : register(TREG(VSCTX_##_name))
#define PSCTX(_name) _name : register(TREG(PSCTX_##_name))
#define PSTEX(_name) _name : register(TREG(PSTEX_##_name))
#define HSCTX(_name) _name : register(TREG(HSCTX_##_name))
#define DSCTX(_name) _name : register(TREG(DSCTX_##_name))

typedef int xt_int;
typedef int4 xt_int4;
typedef float xt_float;
typedef float2 xt_float2;
typedef float3 xt_float3;
typedef float4 xt_float4;
typedef float4x4 xt_mtx;
typedef row_major float3x4 xt_wmtx;


struct VTX_IN {
	float3 pos : POSITION;
	int    pid : PSIZE;
#if D_GEX_VTXENC == D_GEX_VTXENC_COMPACT
	half4  nto : NORMAL;
	half4  clr : COLOR;
	half4  tex : TEXCOORD;
#else
	float3 nrm : NORMAL;
	float3 tng : TANGENT;
	float4 clr : COLOR;
	float4 tex : TEXCOORD;
#endif
};

struct GEO_INFO {
	float3 pos;
	float3 nrm;
	float3 tng;
	float3 btg;
	float4 tex;
	float4 clr;
};

struct SHADOW_PIX_INFO {
	float4 cpos;
	float4 tex;
};


float3 decodeOcta(float2 oct) {
	float2 xy = oct;
	float2 a = abs(xy);
	float z = 1.0 - a.x - a.y;
	xy = lerp(xy, float2(1.0 - a.y, 1.0 - a.x) * (xy < 0 ? -1 : 1), z < 0);
	float3 n = float3(xy, z);
	return normalize(n);
}

#if D_GEX_VTXENC == D_GEX_VTXENC_COMPACT
float3 getVtxNrm(VTX_IN v) { return decodeOcta(v.nto.xy); }
float3 getVtxTng(VTX_IN v) { return decodeOcta(v.nto.zw); }
#else
float3 getVtxNrm(VTX_IN v) { return v.nrm; }
float3 getVtxTng(VTX_IN v) { return v.tng; }
#endif


static float4x4 c_bbezMtx = {
	{-1.0,  3.0, -3.0, 1.0},
	{ 3.0, -6.0,  3.0, 0.0},
	{-3.0,  3.0,  0.0, 0.0},
	{ 1.0,  0.0,  0.0, 0.0}
};

float curve01(float p1, float p2, float t) {
	float tt = t*t;
	float ttt = tt*t;
	float4 v = float4(0.0, p1, p2, 1.0);
	return dot(v, mul(float4(ttt, tt, t, 1.0), c_bbezMtx));
}

