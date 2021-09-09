void main() {
	vec2 pos = vtxPos;
	vec2 offs = gpFontXform.xy;
	vec2 scl = gpFontXform.zw;
	pos *= scl;
	pos += offs;
	gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
}
