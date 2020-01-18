#version 300 es
precision mediump float;

in vec3 pixPos;
in vec3 pixNrm;
in vec2 pixTex;
in vec4 pixClr;

out vec4 outClr;

void main() {
	outClr = pixClr;
	outClr.rgb *= 0.75;
}
