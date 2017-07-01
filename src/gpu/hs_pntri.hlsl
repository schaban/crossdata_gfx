#include "shader.h"
#include "context.h"

StructuredBuffer<CAM_CTX> HSCTX(g_cam);
StructuredBuffer<MTL_CTX> HSCTX(g_mtl);

#include "tess.h"

struct HS_IN {
	float4 scrPos : SV_POSITION;
	GEO_INFO geo : TEXCOORD;
};


float calcTessFactor(float3 p0, float3 n0, float3 p1, float3 n1) {
	float3 viewPos = g_cam[0].viewPos;
	float tessFactor = g_mtl[0].tessFactor;
	float pdist = length(p0 - p1);
	float3 v0 = viewPos - p0;
	float vdist0 = length(v0);
	v0 = normalize(v0);
	float3 v1 = viewPos - p1;
	float vdist1 = length(v1);
	v1 = normalize(v1);
	float pf0 = max(pdist / vdist0, 0.5);
	float nf0 = 1 - abs(dot(n0, v0));
	float f0 = tessFactor * pf0 * nf0;
	float pf1 = max(pdist / vdist1, 0.5);
	float nf1 = 1 - abs(dot(n1, v1));
	float f1 = tessFactor * pf1 * nf1;
	return max(f0, f1);
}

float calcTessFactorAng(float3 p0, float3 n0, float3 p1, float3 n1) {
	float3 viewPos = g_cam[0].viewPos;
	float tessFactor = g_mtl[0].tessFactor;
	float3 v0 = normalize(viewPos - p0);
	float3 v1 = normalize(viewPos - p1);
	float nf0 = 1 - abs(dot(n0, v0));
	float f0 = tessFactor * nf0;
	float nf1 = 1 - abs(dot(n1, v1));
	float f1 = tessFactor * nf1;
	return max(f0, f1);
}

float calcEdgeTessFactor(InputPatch<HS_IN, 3> tri, int i0, int i1) {
	return calcTessFactor(tri[i0].geo.pos, tri[i0].geo.nrm, tri[i1].geo.pos, tri[i1].geo.nrm);
	//return calcTessFactorAng(tri[i0].geo.pos, tri[i0].geo.nrm, tri[i1].geo.pos, tri[i1].geo.nrm);
}

float3 calcPosCP(InputPatch<HS_IN, 3> tri, int i0, int i1) {
	float3 n = tri[i0].geo.nrm;
	return ((2 * tri[i0].geo.pos) + tri[i1].geo.pos - (dot((tri[i1].geo.pos - tri[i0].geo.pos), n) * n)) / 3;
}

float3 triCentroid(InputPatch<HS_IN, 3> tri) {
	return (tri[0].geo.pos + tri[1].geo.pos + tri[2].geo.pos) / 3;
}

float3 calcNrmCP(InputPatch<HS_IN, 3> tri, int i0, int i1) {
	float3 p0 = tri[i0].geo.pos;
	float3 p1 = tri[i1].geo.pos;
	float3 n0 = tri[i0].geo.nrm;
	float3 n1 = tri[i1].geo.nrm;
	float3 e = p1 - p0;
	float3 h = n0 + n1;
	float d = 2 * dot(e, n0 + n1) / dot(e, e);
	return normalize(h - d*e);
}

PNTRI_HS_CONST pntri_const(InputPatch<HS_IN, 3> tri) {
	PNTRI_HS_CONST res = (PNTRI_HS_CONST)0;
#if 1
	res.tessFactor[0] = calcEdgeTessFactor(tri, 2, 0);
	res.tessFactor[1] = calcEdgeTessFactor(tri, 0, 1);
	res.tessFactor[2] = calcEdgeTessFactor(tri, 1, 2);
#else
	res.tessFactor[0] = g_mtl[0].tessFactor;
	res.tessFactor[1] = g_mtl[0].tessFactor;
	res.tessFactor[2] = g_mtl[0].tessFactor;
#endif
	res.insideTessFactor = (res.tessFactor[0] + res.tessFactor[1] + res.tessFactor[2]) / 3;
	res.posCP[0] = calcPosCP(tri, 0, 1);
	res.posCP[1] = calcPosCP(tri, 1, 0);
	res.posCP[2] = calcPosCP(tri, 1, 2);
	res.posCP[3] = calcPosCP(tri, 2, 1);
	res.posCP[4] = calcPosCP(tri, 2, 0);
	res.posCP[5] = calcPosCP(tri, 0, 2);
	res.cpos = 0;
	[unroll] for (int i = 0; i < 6; ++i) res.cpos += res.posCP[i];
	res.cpos /= 6;
	res.cpos += (res.cpos - triCentroid(tri)) / 2;
	res.nrmCP[0] = calcNrmCP(tri, 0, 1);
	res.nrmCP[1] = calcNrmCP(tri, 1, 2);
	res.nrmCP[2] = calcNrmCP(tri, 2, 0);
	return res;
}


[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[patchconstantfunc("pntri_const")]
[outputcontrolpoints(3)]
//[maxtessfactor(9.0)]
PNTRI_HS_CP_OUT main(InputPatch<HS_IN, 3> tri, uint cpid : SV_OutputControlPointID) {
	PNTRI_HS_CP_OUT cp = (PNTRI_HS_CP_OUT)0;
	cp.geo = tri[cpid].geo;
	return cp;
}
