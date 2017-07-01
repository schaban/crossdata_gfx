#include "shader.h"
#include "context.h"

StructuredBuffer<MTL_CTX> PSCTX(g_mtl);

Texture2D PSTEX(g_texBase);

SamplerState g_smpLin : register(s0);

float4 main(float4 scrPos : SV_POSITION, SHADOW_PIX_INFO info : TEXCOORD, bool frontFaceFlg : SV_IsFrontFace) : SV_TARGET {
	if (g_mtl[0].enableAlpha) {
		float texa = g_texBase.Sample(g_smpLin, info.tex.xy).a;
		float a = texa - g_mtl[0].shadowAlphaThreshold;
		clip(a);
	}
	float4 spos = info.cpos;
	return float4(spos.z / spos.w, 0, 0, 1);
}

