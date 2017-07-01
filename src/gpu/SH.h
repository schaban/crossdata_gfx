void shGet3(out float3 shc[9], out float shw[3]) {
	SHL_CTX shl = g_shl[0];
	int i;
	[unroll(9)]
	for (i = 0; i < 9; ++i) {
		shc[i] = shl.data[i].rgb;
	}
	[unroll(3)]
	for (i = 0; i < 3; ++i) {
		shw[i] = shl.data[i+0].a;
	}
}

void shApply3(out float3 res0, float3 shc[9], float shw[3], float x, float y, float z) {
	float zz = z*z;
	float tmp, prev0, prev1, prev2, s0 = y, s1, c0 = x, c1;
	float sh[9];
	res0 = 0;
	sh[0] = 0.282094791774;
	res0 += shc[0] * sh[0] * shw[0];
	sh[2] = 0.488602511903 * z;
	res0 += shc[2] * sh[2] * shw[1];
	sh[6] = 0.946174695758 * zz + -0.315391565253;
	res0 += shc[6] * sh[6] * shw[2];
	tmp = -0.488602511903;
	sh[1] = tmp * s0;
	res0 += shc[1] * sh[1] * shw[1];
	sh[3] = tmp * c0;
	res0 += shc[3] * sh[3] * shw[1];
	prev1 = -1.09254843059 * z;
	sh[5] = prev1 * s0;
	res0 += shc[5] * sh[5] * shw[2];
	sh[7] = prev1 * c0;
	res0 += shc[7] * sh[7] * shw[2];
	s1 = x*s0 + y*c0;
	c1 = x*c0 - y*s0;
	tmp = 0.546274215296;
	sh[4] = tmp * s1;
	res0 += shc[4] * sh[4] * shw[2];
	sh[8] = tmp * c1;
	res0 += shc[8] * sh[8] * shw[2];
}

void shGet6(out float3 shc[36], out float2 shw[6]) {
	SHL_CTX shl = g_shl[0];
	int i;
	[unroll(36)]
	for (i = 0; i < 36; ++i) {
		shc[i] = shl.data[i].rgb;
	}
	[unroll(6)]
	for (i = 0; i < 6; ++i) {
		shw[i][0] = shl.data[i+0].a;
	}
	[unroll(6)]
	for (i = 0; i < 6; ++i) {
		shw[i][1] = shl.data[i+6].a;
	}
}

void shApply6(out float3 res0, out float3 res1, float3 shc[36], float2 shw[6], float2 x, float2 y, float2 z) {
	float2 zz = z*z;
	float2 tmp, prev0, prev1, prev2, s0 = y, s1, c0 = x, c1;
	float2 sh[36];
	res0 = 0;
	res1 = 0;
	sh[0] = 0.282094791774;
	res0 += shc[0] * sh[0][0] * shw[0][0];
	res1 += shc[0] * sh[0][1] * shw[0][1];
	sh[2] = 0.488602511903 * z;
	res0 += shc[2] * sh[2][0] * shw[1][0];
	res1 += shc[2] * sh[2][1] * shw[1][1];
	sh[6] = 0.946174695758 * zz + -0.315391565253;
	res0 += shc[6] * sh[6][0] * shw[2][0];
	res1 += shc[6] * sh[6][1] * shw[2][1];
	sh[12] =  ( 1.86588166295 * zz + -1.11952899777 )  * z;
	res0 += shc[12] * sh[12][0] * shw[3][0];
	res1 += shc[12] * sh[12][1] * shw[3][1];
	sh[20] = 1.9843134833 * z * sh[12] + -1.00623058987 * sh[6];
	res0 += shc[20] * sh[20][0] * shw[4][0];
	res1 += shc[20] * sh[20][1] * shw[4][1];
	sh[30] = 1.98997487421 * z * sh[20] + -1.00285307284 * sh[12];
	res0 += shc[30] * sh[30][0] * shw[5][0];
	res1 += shc[30] * sh[30][1] * shw[5][1];
	tmp = -0.488602511903;
	sh[1] = tmp * s0;
	res0 += shc[1] * sh[1][0] * shw[1][0];
	res1 += shc[1] * sh[1][1] * shw[1][1];
	sh[3] = tmp * c0;
	res0 += shc[3] * sh[3][0] * shw[1][0];
	res1 += shc[3] * sh[3][1] * shw[1][1];
	prev1 = -1.09254843059 * z;
	sh[5] = prev1 * s0;
	res0 += shc[5] * sh[5][0] * shw[2][0];
	res1 += shc[5] * sh[5][1] * shw[2][1];
	sh[7] = prev1 * c0;
	res0 += shc[7] * sh[7][0] * shw[2][0];
	res1 += shc[7] * sh[7][1] * shw[2][1];
	prev2 = -2.28522899732 * zz + 0.457045799464;
	sh[11] = prev2 * s0;
	res0 += shc[11] * sh[11][0] * shw[3][0];
	res1 += shc[11] * sh[11][1] * shw[3][1];
	sh[13] = prev2 * c0;
	res0 += shc[13] * sh[13][0] * shw[3][0];
	res1 += shc[13] * sh[13][1] * shw[3][1];
	tmp = -4.6833258049 * zz + 2.00713963067;
	prev0 = tmp * z;
	sh[19] = prev0 * s0;
	res0 += shc[19] * sh[19][0] * shw[4][0];
	res1 += shc[19] * sh[19][1] * shw[4][1];
	sh[21] = prev0 * c0;
	res0 += shc[21] * sh[21][0] * shw[4][0];
	res1 += shc[21] * sh[21][1] * shw[4][1];
	tmp = 2.03100960116 * z;
	prev1 = tmp * prev0;
	tmp = -0.991031208965 * prev2;
	prev1 += tmp;
	sh[29] = prev1 * s0;
	res0 += shc[29] * sh[29][0] * shw[5][0];
	res1 += shc[29] * sh[29][1] * shw[5][1];
	sh[31] = prev1 * c0;
	res0 += shc[31] * sh[31][0] * shw[5][0];
	res1 += shc[31] * sh[31][1] * shw[5][1];
	s1 = x*s0 + y*c0;
	c1 = x*c0 - y*s0;
	tmp = 0.546274215296;
	sh[4] = tmp * s1;
	res0 += shc[4] * sh[4][0] * shw[2][0];
	res1 += shc[4] * sh[4][1] * shw[2][1];
	sh[8] = tmp * c1;
	res0 += shc[8] * sh[8][0] * shw[2][0];
	res1 += shc[8] * sh[8][1] * shw[2][1];
	prev1 = 1.44530572132 * z;
	sh[10] = prev1 * s1;
	res0 += shc[10] * sh[10][0] * shw[3][0];
	res1 += shc[10] * sh[10][1] * shw[3][1];
	sh[14] = prev1 * c1;
	res0 += shc[14] * sh[14][0] * shw[3][0];
	res1 += shc[14] * sh[14][1] * shw[3][1];
	prev2 = 3.31161143515 * zz + -0.473087347879;
	sh[18] = prev2 * s1;
	res0 += shc[18] * sh[18][0] * shw[4][0];
	res1 += shc[18] * sh[18][1] * shw[4][1];
	sh[22] = prev2 * c1;
	res0 += shc[22] * sh[22][0] * shw[4][0];
	res1 += shc[22] * sh[22][1] * shw[4][1];
	tmp = 7.19030517746 * zz + -2.39676839249;
	prev0 = tmp * z;
	sh[28] = prev0 * s1;
	res0 += shc[28] * sh[28][0] * shw[5][0];
	res1 += shc[28] * sh[28][1] * shw[5][1];
	sh[32] = prev0 * c1;
	res0 += shc[32] * sh[32][0] * shw[5][0];
	res1 += shc[32] * sh[32][1] * shw[5][1];
	s0 = x*s1 + y*c1;
	c0 = x*c1 - y*s1;
	tmp = -0.590043589927;
	sh[9] = tmp * s0;
	res0 += shc[9] * sh[9][0] * shw[3][0];
	res1 += shc[9] * sh[9][1] * shw[3][1];
	sh[15] = tmp * c0;
	res0 += shc[15] * sh[15][0] * shw[3][0];
	res1 += shc[15] * sh[15][1] * shw[3][1];
	prev1 = -1.77013076978 * z;
	sh[17] = prev1 * s0;
	res0 += shc[17] * sh[17][0] * shw[4][0];
	res1 += shc[17] * sh[17][1] * shw[4][1];
	sh[23] = prev1 * c0;
	res0 += shc[23] * sh[23][0] * shw[4][0];
	res1 += shc[23] * sh[23][1] * shw[4][1];
	prev2 = -4.40314469492 * zz + 0.489238299435;
	sh[27] = prev2 * s0;
	res0 += shc[27] * sh[27][0] * shw[5][0];
	res1 += shc[27] * sh[27][1] * shw[5][1];
	sh[33] = prev2 * c0;
	res0 += shc[33] * sh[33][0] * shw[5][0];
	res1 += shc[33] * sh[33][1] * shw[5][1];
	s1 = x*s0 + y*c0;
	c1 = x*c0 - y*s0;
	tmp = 0.625835735449;
	sh[16] = tmp * s1;
	res0 += shc[16] * sh[16][0] * shw[4][0];
	res1 += shc[16] * sh[16][1] * shw[4][1];
	sh[24] = tmp * c1;
	res0 += shc[24] * sh[24][0] * shw[4][0];
	res1 += shc[24] * sh[24][1] * shw[4][1];
	prev1 = 2.07566231488 * z;
	sh[26] = prev1 * s1;
	res0 += shc[26] * sh[26][0] * shw[5][0];
	res1 += shc[26] * sh[26][1] * shw[5][1];
	sh[34] = prev1 * c1;
	res0 += shc[34] * sh[34][0] * shw[5][0];
	res1 += shc[34] * sh[34][1] * shw[5][1];
	s0 = x*s1 + y*c1;
	c0 = x*c1 - y*s1;
	tmp = -0.65638205684;
	sh[25] = tmp * s0;
	res0 += shc[25] * sh[25][0] * shw[5][0];
	res1 += shc[25] * sh[25][1] * shw[5][1];
	sh[35] = tmp * c0;
	res0 += shc[35] * sh[35][0] * shw[5][0];
	res1 += shc[35] * sh[35][1] * shw[5][1];
}
