vec4 hemiBasic() {
	vec4 clr = getBaseMap(pixTex);
	clr.rgb *= pixClr.rgb;
	clr.rgb *= calcHemi(pixNrm);
	return clr;
}

