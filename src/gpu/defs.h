#define D_GEX_MAX_DYN_LIGHTS 8
#define D_GEX_MAX_SH_ORDER 6
#define D_GEX_NUM_SH_COEFS ((D_GEX_MAX_SH_ORDER) * (D_GEX_MAX_SH_ORDER))

#define D_GEX_LIGHT_MODE_NONE 0
#define D_GEX_LIGHT_MODE_DIST 1
#define D_GEX_LIGHT_MODE_OMNI 2
#define D_GEX_LIGHT_MODE_SPOT 3

#define D_GEX_DIFF_LAMBERT 0
#define D_GEX_DIFF_WRAP 1
#define D_GEX_DIFF_OREN_NAYAR 2

#define D_GEX_SPEC_PHONG 0
#define D_GEX_SPEC_BLINN 1
#define D_GEX_SPEC_GGX 2

#define D_GEX_BUMP_NONE 0
#define D_GEX_BUMP_NMAP 1
#define D_GEX_BUMP_HMAP 2

#define D_GEX_TNG_GEOM 0
#define D_GEX_TNG_AUTO 1

#define D_GEX_SHL_NONE 0
#define D_GEX_SHL_DIFF 1
#define D_GEX_SHL_DIFF_REFL 2

#define D_GEX_VTXENC_FULL 0
#define D_GEX_VTXENC_COMPACT 1
#define D_GEX_VTXENC D_GEX_VTXENC_COMPACT


#define VSCTX_g_cam   0
#define VSCTX_g_obj   1
#define VSCTX_g_xform 2
#define VSCTX_g_skin  3
#define VSCTX_g_sdw   4

#define PSCTX_g_cam 0
#define PSCTX_g_mtl 1
#define PSCTX_g_lit 2
#define PSCTX_g_glb 3
#define PSCTX_g_shl 4
#define PSCTX_g_fog 5
#define PSCTX_g_sdw 6

#define PSTEX_g_texBase 7
#define PSTEX_g_texSpec 8
#define PSTEX_g_texBump 9
#define PSTEX_g_texSMap 10
#define PSTEX_g_texRefl 11
#define PSTEX_g_texPano 11

#define HSCTX_g_cam 0
#define HSCTX_g_mtl 1

#define DSCTX_g_cam 0
