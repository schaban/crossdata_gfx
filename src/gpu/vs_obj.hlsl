#include "shader.h"
#include "context.h"

StructuredBuffer<CAM_CTX>   VSCTX(g_cam);
StructuredBuffer<OBJ_CTX>   VSCTX(g_obj);
StructuredBuffer<XFORM_CTX> VSCTX(g_xform);
StructuredBuffer<SKIN_CTX>  VSCTX(g_skin);

#include "xform.h"

void main(VTX_IN vtx, out float4 cpos : SV_POSITION, out GEO_INFO geo : TEXCOORD) {
	int pid = vtx.pid;

	geo = (GEO_INFO)0;

	xt_wmtx wmtx = calcVtxWMtx(pid);

	geo.pos = mul(wmtx, float4(vtx.pos, 1.0));
	geo.nrm = normalize(mul(wmtx, float4(getVtxNrm(vtx), 0.0)).xyz);
	geo.tng = normalize(mul(wmtx, float4(getVtxTng(vtx), 0.0)).xyz);
	geo.btg = calcBitangent(geo);
	geo.tex = vtx.tex;
	geo.clr = vtx.clr;

	cpos = mul(float4(geo.pos, 1.0), g_cam[0].viewProj);
}

