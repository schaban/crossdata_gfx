#version 420
precision highp float;

layout (location = 0) in vec3 pixPos;
layout (location = 1) in vec3 pixNrm;
layout (location = 2) in vec2 pixTex;
layout (location = 3) in vec4 pixClr;

layout (location = 0) out vec4 outClr;

void main() {
	outClr = pixClr;
	outClr.rgb *= 0.5;
}
