float4 tgtColor(float4 clr) {
	GLB_CTX glb = g_glb[0];
	float3 c = clr.rgb;

	c = (c * (1.0 + c / glb.linWhite)) / (1.0 + c);
	c *= glb.linGain;
	c += glb.linBias;
	clr.rgb = c;

	float3 e = glb.exposure;
	if (all(e > 0)) {
		clr.rgb = 1.0 - exp(clr.rgb * -e);
	}
	clr = max(clr, 0);
	clr = pow(clr, glb.invGamma);

	return clr;
}
