float4 smpPanorama(Texture2D tex, float3 dir, float lvl) {
	float2 uv = (float2)0.0;
	float lxz = sqrt(dir.x*dir.x + dir.z*dir.z);
	if (lxz > 1.0e-5) uv.x = -dir.x / lxz;
	uv.y = dir.y;
	uv = clamp(uv, -1.0, 1.0);
	uv = acos(uv) / PI;
	uv.x *= 0.5;
	if (dir.z >= 0.0) uv.x = 1.0 - uv.x;
	return tex.SampleLevel(g_smpLin, uv, lvl, 0);
}