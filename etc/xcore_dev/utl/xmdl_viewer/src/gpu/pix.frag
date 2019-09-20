#version 310 es
precision mediump float;

layout(std140) uniform GPLight {
	vec3 gpHemiUpper;
	vec3 gpHemiLower;
	vec3 gpHemiUp;
	float gpHemiExp;
	float gpHemiGain;
};

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

void main() {
	vec3 nrm = normalize(pixNrm);
	vec4 clr = pixClr;
	clr.rgb *= calcHemi(nrm);
	clr = max(clr, 0.0);
	//clr.rgb = pow(clr.rgb, vec3(1.0 / 2.2));
	outClr = clr;
}
