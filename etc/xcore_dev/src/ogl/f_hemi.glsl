HALF vec3 calcHemi(HALF vec3 nrm) {
	HALF float hemiVal = 0.5 * (dot(nrm, gpHemiUp) + 1.0);
	HALF float hemiExp = gpHemiParam.x;
	HALF float hemiGain = gpHemiParam.y;
	hemiVal = clamp(pow(hemiVal, hemiExp) * hemiGain, 0.0, 1.0);
	HALF vec3 hemi = mix(gpHemiLower, gpHemiUpper, hemiVal);
	return hemi;
}



