#include "shader.h"
#include "context.h"

StructuredBuffer<GLB_CTX> PSCTX(g_glb);

Texture2D PSTEX(g_texPano);

SamplerState g_smpLin : register(s0);

#include "pano.h"
#include "color.h"

float4 main(float4 scrPos : SV_POSITION, float3 wpos : TEXCOORD, bool frontFaceFlg : SV_IsFrontFace) : SV_TARGET {
	float3 dir = normalize(wpos);
	float4 clr = smpPanorama(g_texPano, dir, 0);
	clr = tgtColor(clr);
	return clr;
}
