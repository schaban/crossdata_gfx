struct PNTRI_HS_CONST {
	float tessFactor[3] : SV_TessFactor;
	float insideTessFactor : SV_InsideTessFactor;
	float3 posCP[6] : POSITION3;
	float3 cpos : CENTER;
	float3 nrmCP[3] : NORMAL3;
};

struct PNTRI_HS_CP_OUT {
	GEO_INFO geo : TEXCOORD;
};

