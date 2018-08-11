#include "shader.h"
#include "context.h"

StructuredBuffer<CAM_CTX>   VSCTX(g_cam);
StructuredBuffer<OBJ_CTX>   VSCTX(g_obj);
StructuredBuffer<XFORM_CTX> VSCTX(g_xform);


void main(float3 pos : POSITION, out float4 cpos : SV_POSITION, out float3 wpos : TEXCOORD) {
	CAM_CTX cam = g_cam[0];
	wpos = mul(g_xform[0].mtx, float4(pos, 1.0)).xyz;
	cpos = mul(float4(wpos, 1.0), g_cam[0].viewProj);
}
