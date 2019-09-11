void calcVtxOut(
	vec4 wm[3],
	vec3 vpos, vec3 vnrm, vec2 vtex, vec4 vclr, float tscl, float cscl,
	out vec3 fpos, out vec3 fnrm, out vec2 ftex, out vec4 fclr
) {
	vec4 hpos = vec4(vpos, 1.0);
	fpos = vec3(dot(hpos, wm[0]), dot(hpos, wm[1]), dot(hpos, wm[2]));
	fnrm = normalize(vec3(dot(vnrm, wm[0].xyz), dot(vnrm, wm[1].xyz), dot(vnrm, wm[2].xyz)));
	ftex = vtex * tscl;
	fclr = vclr * cscl;
}

void calcGLPos(vec3 pos) {
	pixCPos = gpViewProj * vec4(pos, 1.0);
	gl_Position = pixCPos;
}

