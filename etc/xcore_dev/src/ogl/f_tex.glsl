HALF vec4 sampleBaseMap(HALF vec2 uv) {
	HALF vec4 tex = texture2D(smpBase, uv);
	tex.rgb *= tex.rgb; // gamma 2
	return tex;
}

HALF vec4 getBaseMap(HALF vec2 uv) {
	HALF vec4 tex = sampleBaseMap(uv);
	tex.rgb *= gpBaseColor;
	return tex;
}

HALF vec2 sampleNormMap(HALF vec2 uv) {
	return texture2D(smpBump, uv).xy;
}

HALF vec3 getNormMap(HALF vec2 uv) {
	HALF vec2 ntex = sampleNormMap(uv);
	HALF vec2 nxy = (ntex - 0.5) * 2.0;
	HALF float bumpScale = gpBumpParam.x;
	nxy *= bumpScale;
	HALF vec2 sqxy = nxy * nxy;
	HALF float nz = sqrt(max(0.0, 1.0 - sqxy.x - sqxy.y));
	return vec3(nxy.x, nxy.y, nz);
}
