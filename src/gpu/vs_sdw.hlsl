#include "shader.h"
#include "context.h"

StructuredBuffer<CAM_CTX>   VSCTX(g_cam);//
StructuredBuffer<OBJ_CTX>   VSCTX(g_obj);
StructuredBuffer<XFORM_CTX> VSCTX(g_xform);
StructuredBuffer<SKIN_CTX>  VSCTX(g_skin);
StructuredBuffer<SDW_CTX>   VSCTX(g_sdw);

#include "xform.h"

void main(VTX_IN vtx, out float4 cpos : SV_POSITION, out SHADOW_PIX_INFO pixOut : TEXCOORD) {
	int pid = vtx.pid;
	xt_wmtx wmtx = calcVtxWMtx(pid);
	float3 pos = mul(wmtx, float4(vtx.pos, 1.0));
	cpos = mul(float4(pos, 1.0), g_sdw[0].viewProj);
	pixOut.cpos = cpos;
	pixOut.tex = vtx.tex;
}
