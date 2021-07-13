void getSprAttrs(float ivtx, out vec2 pos, out vec2 tex, out vec4 clr) {
	int i0 = int(vtxId / 2.0);
	int i1 = int(mod(vtxId, 2.0)) * 2;
#ifdef WEBGL
	vec4 v = vec4(0.0);
	for (int i = 0; i < 2; ++i) { if (i == i0) { v = gpSprVtxPos[i]; break; } }
	float x = 0.0;
	float y = 0.0;
	for (int i = 0; i < 4; ++i) { if (i == i1) { x = v[i]; break; } }
	for (int i = 0; i < 4; ++i) { if (i == i1 + 1) { y = v[i]; break; } }
	for (int i = 0; i < 2; ++i) { if (i == i0) { v = gpSprVtxTex[i]; break; } }
	pos = vec2(x, y);
	for (int i = 0; i < 4; ++i) { if (i == i1) { x = v[i]; break; } }
	for (int i = 0; i < 4; ++i) { if (i == i1 + 1) { y = v[i]; break; } }
	tex = vec2(x, y);
#else
	pos = vec2(gpSprVtxPos[i0][i1], gpSprVtxPos[i0][i1 + 1]);
	tex = vec2(gpSprVtxTex[i0][i1], gpSprVtxTex[i0][i1 + 1]);
#endif
	clr = gpSprVtxClr[int(vtxId)];
}

void main() {
	vec2 pos;
	vec2 tex;
	vec4 clr;
	getSprAttrs(vtxId, pos, tex, clr);
	pixTex = tex;
	pixClr = clr;
	gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
}
