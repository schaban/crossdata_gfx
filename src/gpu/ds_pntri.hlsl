#include "shader.h"
#include "context.h"

StructuredBuffer<CAM_CTX> DSCTX(g_cam);

#include "tess.h"

[domain("tri")]
void main(PNTRI_HS_CONST hsc, const OutputPatch<PNTRI_HS_CP_OUT, 3> tri, float3 bc : SV_DomainLocation, out float4 cpos : SV_POSITION, out GEO_INFO geo : TEXCOORD) {
	float u = bc.x;
	float v = bc.y;
	float w = bc.z;
	float uu = u * u;
	float vv = v * v;
	float ww = w * w;
	float uu3 = uu * 3;
	float vv3 = vv * 3;
	float ww3 = ww * 3;

	float3 pos = tri[0].geo.pos*ww*w + tri[1].geo.pos*uu*u + tri[2].geo.pos*vv*v;
	pos += hsc.posCP[0] * ww3 * u;
	pos += hsc.posCP[1] * uu3 * w;
	pos += hsc.posCP[2] * uu3 * v;
	pos += hsc.posCP[3] * vv3 * u;
	pos += hsc.posCP[4] * vv3 * w;
	pos += hsc.posCP[5] * ww3 * v;
	pos += hsc.cpos * u * v * w * 6;

	float3 nrm = tri[0].geo.nrm*ww + tri[1].geo.nrm*uu + tri[2].geo.nrm*vv;
	nrm += hsc.nrmCP[0] * u * w;
	nrm += hsc.nrmCP[1] * u * v;
	nrm += hsc.nrmCP[2] * v * w;
	nrm = normalize(nrm);

	float3 tng = tri[0].geo.tng*ww + tri[1].geo.tng*uu + tri[2].geo.tng*vv;
	tng += hsc.nrmCP[0] * u * w;
	tng += hsc.nrmCP[1] * u * v;
	tng += hsc.nrmCP[2] * v * w;
	tng = normalize(tng);

	float4 tex = tri[0].geo.tex*w + tri[1].geo.tex*u + tri[2].geo.tex*v;

	float4 clr = tri[0].geo.clr*w + tri[1].geo.clr*u + tri[2].geo.clr*v;

	geo.pos = pos;
	geo.nrm = nrm;
	geo.tng = tng;
	geo.btg = normalize(cross(geo.tng, geo.nrm));
	geo.tex = tex;
	geo.clr = clr;

	cpos = mul(float4(geo.pos, 1.0), g_cam[0].viewProj);;
}
