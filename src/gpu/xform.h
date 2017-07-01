xt_wmtx calcVtxWMtx(int pid) {
	xt_wmtx wmtx;
	if (g_obj[0].xformMode > 0) {
		SKIN_CTX skin = g_skin[pid];
		wmtx = g_xform[skin.jnt[0]].mtx * skin.wgt[0]
		     + g_xform[skin.jnt[1]].mtx * skin.wgt[1]
		     + g_xform[skin.jnt[2]].mtx * skin.wgt[2]
		     + g_xform[skin.jnt[3]].mtx * skin.wgt[3];
		if (g_obj[0].xformMode > 4) {
			skin = g_skin[g_obj[0].vtxNum + pid];
			wmtx += g_xform[skin.jnt[0]].mtx * skin.wgt[0]
			      + g_xform[skin.jnt[1]].mtx * skin.wgt[1]
			      + g_xform[skin.jnt[2]].mtx * skin.wgt[2]
			      + g_xform[skin.jnt[3]].mtx * skin.wgt[3];
		}
	} else {
		wmtx = g_xform[0].mtx;
	}
	return wmtx;
}

float3 calcBitangent(GEO_INFO geo) {
	return normalize(cross(geo.tng, geo.nrm));
}
