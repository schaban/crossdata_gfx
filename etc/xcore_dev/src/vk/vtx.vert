#version 420
precision highp float;

#define MAX_XFORMS 128

layout(set = 0, binding = 0) uniform GPXform {
	mat4 gpViewProj;
	vec4 gpXforms[MAX_XFORMS * 3];
};

layout (location = 0) in vec3 vtxPos;
layout (location = 1) in vec3 vtxNrm;
layout (location = 2) in vec2 vtxTex;
layout (location = 3) in vec4 vtxClr;
layout (location = 4) in vec4 vtxWgt;
layout (location = 5) in ivec4 vtxIdx;

layout (location = 0) out vec3 pixPos;
layout (location = 1) out vec3 pixNrm;
layout (location = 2) out vec2 pixTex;
layout (location = 3) out vec4 pixClr;


void main() {
	float w0 = vtxWgt[0];
	float w1 = vtxWgt[1];
	float w2 = vtxWgt[2];
	float w3 = vtxWgt[3];
	int i0 = int(vtxIdx[0]);
	int i1 = int(vtxIdx[1]);
	int i2 = int(vtxIdx[2]);
	int i3 = int(vtxIdx[3]);

	vec4 wm[3];

	wm[0] = gpXforms[i0 + 0] * w0;
	wm[1] = gpXforms[i0 + 1] * w0;
	wm[2] = gpXforms[i0 + 2] * w0;

	wm[0] += gpXforms[i1 + 0] * w1;
	wm[1] += gpXforms[i1 + 1] * w1;
	wm[2] += gpXforms[i1 + 2] * w1;

	wm[0] += gpXforms[i2 + 0] * w2;
	wm[1] += gpXforms[i2 + 1] * w2;
	wm[2] += gpXforms[i2 + 2] * w2;

	wm[0] += gpXforms[i3 + 0] * w3;
	wm[1] += gpXforms[i3 + 1] * w3;
	wm[2] += gpXforms[i3 + 2] * w3;

	vec4 hpos = vec4(vtxPos, 1.0);
	pixPos = vec3(dot(hpos, wm[0]), dot(hpos, wm[1]), dot(hpos, wm[2]));

	vec3 vnrm = vtxNrm;
	pixNrm = normalize(vec3(dot(vnrm, wm[0].xyz), dot(vnrm, wm[1].xyz), dot(vnrm, wm[2].xyz)));

	pixTex = vtxTex;
	pixClr = vtxClr;

	gl_Position = gpViewProj * vec4(pixPos, 1.0);
}

