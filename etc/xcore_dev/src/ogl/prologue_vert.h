#version 120

#ifdef GL_ES
precision highp float;
#	define HALF mediump
#	define FULL highp
#else
#	define HALF
#	define FULL
#endif

