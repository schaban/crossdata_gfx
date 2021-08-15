HALF vec3 calcHemi(HALF vec3 nrm) {
	HALF vec3 hemiUp = gpHemiUp;
	HALF vec3 hemiLower = gpHemiLower;
	HALF vec3 hemiUpper = gpHemiUpper;
	HALF float hemiVal = 0.5 * (dot(nrm, hemiUp) + 1.0);
	HALF float hemiExp = gpHemiParam.x;
	HALF float hemiGain = gpHemiParam.y;
	hemiVal = clamp(pow(hemiVal, hemiExp) * hemiGain, 0.0, 1.0);
	HALF vec3 hemi = mix(hemiLower, hemiUpper, hemiVal);
	return hemi;
}



