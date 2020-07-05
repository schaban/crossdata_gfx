void main() {
	int i0 = int(vtxId / 2.0);
	int i1 = int(mod(vtxId, 2.0)) * 2;
	vec2 pos = vec2(gpSprVtxPos[i0][i1], gpSprVtxPos[i0][i1 + 1]);
	vec2 tex = vec2(gpSprVtxTex[i0][i1], gpSprVtxTex[i0][i1 + 1]);
	vec4 clr = gpSprVtxClr[int(vtxId)];
	pixTex = tex;
	pixClr = clr;
	gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
}
