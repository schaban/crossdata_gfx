HALF float ease(HALF vec2 cp, HALF float t) {
	HALF float tt = t*t;
	HALF float ttt = tt*t;
	HALF float b1 = dot(vec3(ttt, tt, t), vec3(3.0, -6.0, 3.0));
	HALF float b2 = dot(vec2(ttt, tt), vec2(-3.0, 3.0));
	return dot(vec3(cp, 1.0), vec3(b1, b2, ttt));
}

HALF float fog() {
	float d = distance(pixPos, gpViewPos);
	float start = gpFogParam.x;
	float falloff = gpFogParam.y;
	HALF float t = max(0.0, min((d - start) * falloff, 1.0));
	return ease(gpFogParam.zw, t);
}

HALF vec4 exposure(HALF vec4 c) {
	HALF vec3 e = gpExposure;
	if (all(greaterThan(e, vec3(0.0)))) {
		c.rgb = 1.0 - exp(c.rgb * -e);
	}
	return c;
}

HALF vec3 toneMap(HALF vec3 c) {
	c = (c * (1.0 + c*gpInvWhite)) / (1.0 + c);
	c *= gpLClrGain;
	c += gpLClrBias;
	c = max(c, 0.0);
	return c;
}

HALF vec4 applyCC(HALF vec4 clr) {
	HALF vec4 c = clr;

	HALF vec4 cfog = gpFogColor;
	c.rgb = mix(c.rgb, cfog.rgb, fog() * cfog.a);

	c.rgb = toneMap(c.rgb);

	c = exposure(c);
	c = max(c, 0.0);
	c.rgb = pow(c.rgb, gpInvGamma);
	return c;
}

vec4 calcFinalColorOpaq(vec4 c) {
	c = applyCC(c);
	c.a = 1.0;
	return c;
}

vec4 calcFinalColorSemi(vec4 c) {
	c = applyCC(c);
	c.a *= pixClr.a;
	return c;
}

vec4 calcFinalColorLimit(vec4 c) {
	c = applyCC(c);
	c.a *= pixClr.a;
	float lim = gpAlphaCtrl.x;
	c.a = lim < 0.0 ? c.a : c.a < lim ? 0.0 : 1.0;
	return c;
}

vec4 calcFinalColorDiscard(vec4 c) {
	c = applyCC(c);
	c.a *= pixClr.a;
	float lim = gpAlphaCtrl.x;
	if (c.a < lim) discard;
	return c;
}

