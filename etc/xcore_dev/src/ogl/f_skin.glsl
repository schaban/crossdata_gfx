#ifdef WEBGL

int jntIdx(float gidx) {
	int si = int(gidx / 4.0);
	vec4 sv = vec4(0.0);;
	for (int i = 0; i < JMAP_SIZE; ++i) {
		if (i == si) { sv = gpSkinMap[i]; break; }
	}
	int ei = int(mod(gidx, 4.0));
	int ji = 0;
	for (int i = 0; i < 4; ++i) {
		if (i == ei) { ji = int(sv[i]); break; }
	}
	return ji;
}

vec4 skinMtxR(int idx) {
	vec4 rv = vec4(0.0);
	for (int i = 0; i < JMTX_SIZE; ++i) {
		if (i == idx) { rv = gpSkinMtx[i]; break; }
	}
	return rv;
}

void skinWM(vec4 jnt, vec4 wgt, out vec4 wm[3]) {
	int j0 = jntIdx(jnt[0]);
	wm[0] = skinMtxR(j0 + 0) * wgt[0];
	wm[1] = skinMtxR(j0 + 1) * wgt[0];
	wm[2] = skinMtxR(j0 + 2) * wgt[0];
	int j1 = jntIdx(jnt[1]);
	wm[0] += skinMtxR(j1 + 0) * wgt[1];
	wm[1] += skinMtxR(j1 + 1) * wgt[1];
	wm[2] += skinMtxR(j1 + 2) * wgt[1];
	int j2 = jntIdx(jnt[2]);
	wm[0] += skinMtxR(j2 + 0) * wgt[2];
	wm[1] += skinMtxR(j2 + 1) * wgt[2];
	wm[2] += skinMtxR(j2 + 2) * wgt[2];
	int j3 = jntIdx(jnt[3]);
	wm[0] += skinMtxR(j3 + 0) * wgt[3];
	wm[1] += skinMtxR(j3 + 1) * wgt[3];
	wm[2] += skinMtxR(j3 + 2) * wgt[3];
}

#else

int jntIdx(float gidx) {
	return int(gpSkinMap[int(gidx / 4.0)][int(mod(gidx, 4.0))]);
}

void skinWM(vec4 jnt, vec4 wgt, out vec4 wm[3]) {
	int j0 = jntIdx(jnt[0]);
	wm[0] = gpSkinMtx[j0 + 0] * wgt[0];
	wm[1] = gpSkinMtx[j0 + 1] * wgt[0];
	wm[2] = gpSkinMtx[j0 + 2] * wgt[0];
	int j1 = jntIdx(jnt[1]);
	wm[0] += gpSkinMtx[j1 + 0] * wgt[1];
	wm[1] += gpSkinMtx[j1 + 1] * wgt[1];
	wm[2] += gpSkinMtx[j1 + 2] * wgt[1];
	int j2 = jntIdx(jnt[2]);
	wm[0] += gpSkinMtx[j2 + 0] * wgt[2];
	wm[1] += gpSkinMtx[j2 + 1] * wgt[2];
	wm[2] += gpSkinMtx[j2 + 2] * wgt[2];
	int j3 = jntIdx(jnt[3]);
	wm[0] += gpSkinMtx[j3 + 0] * wgt[3];
	wm[1] += gpSkinMtx[j3 + 1] * wgt[3];
	wm[2] += gpSkinMtx[j3 + 2] * wgt[3];
}

#endif
