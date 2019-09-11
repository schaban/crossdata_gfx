void main() {
	vec3 hpos = vec3(vtxPos, 1.0);
	float x = dot(hpos, gpScrXform[0]);
	float y = dot(hpos, gpScrXform[1]);
	pixTex = vtxTex;
	pixClr = vtxClr;
	gl_Position = vec4(x, y, 0.0, 1.0);
}
