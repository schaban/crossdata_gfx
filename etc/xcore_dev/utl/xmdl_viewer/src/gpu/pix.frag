#version 300 es
precision mediump float;

layout(std140) uniform GPLight {
	vec3 gpHemiUpper;
	vec3 gpHemiLower;
	vec3 gpHemiUp;
	float gpHemiExp;
	float gpHemiGain;
};

layout(std140) uniform GPColor {
	vec3 gpInvGamma;
};

layout(std140) uniform GPMaterial {
	vec4 gpBaseColorAlphaLim;
};

uniform sampler2D smpBase;

in vec3 pixPos;
in vec3 pixNrm;
in vec2 pixTex;
in vec4 pixClr;

out vec4 outClr;

vec3 calcHemi(vec3 nrm) {
	float hemiVal = 0.5 * (dot(nrm, gpHemiUp) + 1.0);
	hemiVal = clamp(pow(hemiVal, gpHemiExp) * gpHemiGain, 0.0, 1.0);
	vec3 hemi = mix(gpHemiLower, gpHemiUpper, hemiVal);
	return hemi;
}

vec4 sampleBaseMap(vec2 uv) {
	vec4 tex = texture(smpBase, uv);
	tex.rgb *= tex.rgb; // gamma 2
	return tex;
}

vec4 getBaseMap(vec2 uv) {
	vec4 tex = sampleBaseMap(uv);
	tex.rgb *= gpBaseColorAlphaLim.rgb;
	return tex;
}

void main() {
	vec3 nrm = normalize(pixNrm);
	vec4 tex = getBaseMap(pixTex);
	vec4 clr = pixClr;
	clr *= tex;
	if (clr.a < gpBaseColorAlphaLim.a) discard;
	clr.rgb *= calcHemi(nrm);
	clr = max(clr, 0.0);
	clr.rgb = pow(clr.rgb, gpInvGamma);
	outClr = clr;
}
