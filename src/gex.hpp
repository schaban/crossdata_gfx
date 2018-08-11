/*
 * Author: Sergey Chaban <sergey.chaban@gmail.com>
 */

#include "gpu/defs.h"

#define D_GEX_CAM_TAG XD_FOURCC('G', 'C', 'A', 'M')
#define D_GEX_LIT_TAG XD_FOURCC('G', 'L', 'I', 'T')
#define D_GEX_TEX_TAG XD_FOURCC('G', 'T', 'E', 'X')
#define D_GEX_OBJ_TAG XD_FOURCC('G', 'O', 'B', 'J')
#define D_GEX_POL_TAG XD_FOURCC('G', 'P', 'O', 'L')

#define D_GEX_SYS_CAM_NAME "<syscam>"
#define D_GEX_SYS_LIT_NAME "<syslit>"

struct GEX_WNDMSG {
	void* mHandle;
	uint32_t mCode;
	uint32_t mParams[4];
};

struct GEX_CONFIG {
	void* mSysHandle;
	void (*mpMsgFunc)(const GEX_WNDMSG&);
	int mWidth;
	int mHeight;
	int mShadowMapSize;
	int mMSAA;
};

enum class GEX_LIGHT_MODE {
	NONE = D_GEX_LIGHT_MODE_NONE,
	DIST = D_GEX_LIGHT_MODE_DIST,
	OMNI = D_GEX_LIGHT_MODE_OMNI,
	SPOT = D_GEX_LIGHT_MODE_SPOT
};

enum class GEX_SHL_MODE {
	NONE      = D_GEX_SHL_NONE,
	DIFF      = D_GEX_SHL_DIFF,
	DIFF_REFL = D_GEX_SHL_DIFF_REFL
};

enum class GEX_DIFF_MODE {
	LAMBERT    = D_GEX_DIFF_LAMBERT,
	WRAP       = D_GEX_DIFF_WRAP,
	OREN_NAYAR = D_GEX_DIFF_OREN_NAYAR
};

enum class GEX_SPEC_MODE {
	PHONG = D_GEX_SPEC_PHONG,
	BLINN = D_GEX_SPEC_BLINN,
	GGX   = D_GEX_SPEC_GGX
};

enum class GEX_BUMP_MODE {
	NONE = D_GEX_BUMP_NONE,
	NMAP = D_GEX_BUMP_NMAP,
	HMAP = D_GEX_BUMP_HMAP
};

enum class GEX_TANGENT_MODE {
	GEOM = D_GEX_TNG_GEOM,
	AUTO = D_GEX_TNG_AUTO
};

enum class GEX_TESS_MODE : uint8_t {
	NONE  = 0,
	PNTRI = 1
};

enum class GEX_UV_MODE : uint8_t {
	WRAP  = 0,
	CLAMP = 1
};

enum class GEX_POL_TYPE {
	TRILIST  = 0,
	TRISTRIP = 1,
	SEGLIST  = 2,
	SEGSTRIP = 3
};

enum class GEX_SORT_MODE {
	NEAR_POS = 0,
	CENTER   = 1,
	FAR_POS  = 2
};

enum class GEX_SHADOW_PROJ {
	UNIFORM = 0,
	PERSPECTIVE = 1
};

enum class GEX_BG_MODE {
	NONE = 0,
	PANO = 1
};

struct GEX_CAM;
struct GEX_LIT;
struct GEX_OBJ;
struct GEX_POL;
struct GEX_TEX;
struct GEX_MTL;

void gexInit(const GEX_CONFIG& cfg);
void gexReset();
void gexLoop(void (*pLoop)());
int gexGetDispWidth();
int gexGetDispHeight();
int gexGetDispFreq();

void gexClearTarget(const cxColor& clr = cxColor(0.0f));
void gexClearDepth();
void gexDiffLightFactor(const cxColor& clr);
void gexDiffSHFactor(const cxColor& clr);
void gexReflSHFactor(const cxColor& clr);
void gexGamma(float y);
void gexGammaRGBA(float yr, float yg, float yb, float ya);
void gexExposure(float e);
void gexExposureRGB(float er, float eg, float eb);
void gexLinearWhite(float w);
void gexLinearWhiteRGB(float wr, float wg, float wb);
void gexLinearGain(float gain);
void gexLinearGainRGB(float r, float g, float b);
void gexLinearBias(float bias);
void gexLinearBiasRGB(float r, float g, float b);
void gexUpdateGlobals();

void gexBgMode(GEX_BG_MODE mode);
void gexBgPanoramaTex(GEX_TEX* pTex);

float gexCalcFOVY(float focal, float aperture);
void gexMtxMul(cxMtx* pDst, const cxMtx* pSrcA, const cxMtx* pSrcB);

void gexBeginScene(GEX_CAM* pCam);
void gexEndScene();
void gexSwap(uint32_t syncInterval = 1);

void gexShadowDir(const cxVec& dir);
void gexShadowDensity(float dens);
void gexShadowViewSize(float vsize);
void gexShadowColor(const cxColor& clr);
void gexShadowProjection(GEX_SHADOW_PROJ prj);
void gexShadowFade(float start, float end);
void gexShadowSpecCtrl(int lightIdx, float valSelector = 0.0f);

GEX_CAM* gexCamCreate(const char* pName = nullptr);
void gexCamDestroy(GEX_CAM* pCam);
void gexCamUpdate(GEX_CAM* pCam, const cxVec& pos, const cxVec& tgt, const cxVec& up, float fovy, float znear, float zfar);
float gexCalcViewDist(const GEX_CAM* pCam, const cxVec& pos);
const char* gexCamName(const GEX_CAM* pCam);
GEX_CAM* gexCamFind(const char* pName);
bool gexSphereVisibility(const GEX_CAM* pCam, const cxSphere& sph);
bool gexBoxVisibility(const GEX_CAM* pCam, const cxAABB& box);

GEX_LIT* gexLitCreate(const char* pName = nullptr);
void gexLitDestroy(GEX_LIT* pLit);
void gexLitUpdate(GEX_LIT* pLit);
void gexAmbient(GEX_LIT* pLit, const cxColor& clr);
void gexHemi(GEX_LIT* pLit, const cxColor& skyClr, const cxColor& groundClr, const cxVec& upVec = cxVec(0.0f, 1.0f, 0.0f));
bool gexCkLightIdx(int idx);
void gexLightMode(GEX_LIT* pLit, int idx, GEX_LIGHT_MODE mode);
void gexLightPos(GEX_LIT* pLit, int idx, const cxVec& pos);
void gexLightDir(GEX_LIT* pLit, int idx, const cxVec& dir);
void gexLightDirDegrees(GEX_LIT* pLit, int idx, float dx, float dy);
void gexLightColor(GEX_LIT* pLit, int idx, const cxColor& clr, float diff = 1.0f, float spec = 1.0f);
void gexLightRange(GEX_LIT* pLit, int idx, float attnStart, float rangeEnd);
void gexLightCone(GEX_LIT* pLit, int idx, float angle, float range);
void gexFog(GEX_LIT* pLit, const cxColor& clr, float start, float end, float curveP1 = 1.0f/3, float curveP2 = 2.0f/3);
void gexSHLW(GEX_LIT* pLit, GEX_SHL_MODE mode, int order, float* pR, float* pG, float* pB, float* pWgtDiff, float* pWgtRefl);
void gexSHL(GEX_LIT* pLit, GEX_SHL_MODE mode, int order, float* pR, float* pG, float* pB);
const char* gexLitName(const GEX_LIT* pLit);
GEX_LIT* gexLitFind(const char* pName);

GEX_TEX* gexTexCreate(const sxTextureData& tex, bool compact = true);
GEX_TEX* gexTexCreateConst(const cxColor& clr, const char* pName);
void gexTexDestroy(GEX_TEX* pTex);
const char* gexTexName(const GEX_TEX* pTex);
GEX_TEX* gexTexFind(const char* pName, const char* pPath = nullptr);

GEX_OBJ* gexObjCreate(const sxGeometryData& geo, const char* pBatGrpPrefix = nullptr, const char* pSortMtlsAttr = nullptr);
void gexObjDestroy(GEX_OBJ* pObj);
void gexObjSkinInit(GEX_OBJ* pObj, const cxMtx* pRestIW);
void gexObjTransform(GEX_OBJ* pObj, const cxMtx* pMtx);
void gexObjDisp(GEX_OBJ* pObj, GEX_LIT* pLit = nullptr);
int gexObjMtlNum(const GEX_OBJ* pObj);
cxAABB gexObjBBox(const GEX_OBJ* pObj);
const char* gexObjName(const GEX_OBJ* pObj);
GEX_OBJ* gexObjFind(const char* pName, const char* pPath = nullptr);
GEX_MTL* gexObjMaterial(const GEX_OBJ* pObj, int idx);
GEX_MTL* gexFindMaterial(const GEX_OBJ* pObj, const char* pName);
void gexUpdateMaterials(GEX_OBJ* pObj);

GEX_POL* gexPolCreate(int maxVtxNum, const char* pName = nullptr);
void gexPolDestroy(GEX_POL* pPol);
void gexPolEditBegin(GEX_POL* pPol);
void gexPolEditEnd(GEX_POL* pPol);
void gexPolAddVertex(GEX_POL* pPol, const cxVec& pos, const cxVec& nrm, const cxVec& tng, const cxColor& clr, const xt_texcoord& uv, const xt_texcoord& uv2);
void gexPolTransform(GEX_POL* pPol, const cxMtx& mtx);
void gexPolDisp(GEX_POL* pPol, GEX_POL_TYPE type, GEX_LIT* pLit = nullptr);
void gexPolSortMode(GEX_POL* pPol, GEX_SORT_MODE mode);
void gexPolSortBias(GEX_POL* pPol, float absBias, float relBias);
GEX_MTL* gexPolMaterial(const GEX_POL* pPol);
const char* gexPolName(const GEX_POL* pPol);
GEX_POL* gexPolFind(const char* pName);

const char* gexMtlName(GEX_MTL* pMtl);
void gexMtlUpdate(GEX_MTL* pMtl);
void gexMtlDoubleSided(GEX_MTL* pMtl, bool dsidedFlg);
void gexDoubleSidedAll(GEX_OBJ* pObj, bool dsidedFlg);
void gexMtlHide(GEX_MTL* pMtl, bool hideFlg);
void gexMtlBaseColor(GEX_MTL* pMtl, const cxColor& clr);
void gexMtlBaseTexture(GEX_MTL* pMtl, GEX_TEX* pTex);
void gexMtlSpecularColor(GEX_MTL* pMtl, const cxColor& clr);
void gexMtlSpecularTexture(GEX_MTL* pMtl, GEX_TEX* pTex);
void gexMtlReflectionLevel(GEX_MTL* pMtl, float lvl);
void gexMtlReflectionColor(GEX_MTL* pMtl, const cxColor& clr);
void gexMtlReflectionTexture(GEX_MTL* pMtl, GEX_TEX* pTex);
void gexMtlBumpFactor(GEX_MTL* pMtl, float val);
void gexMtlBumpTexture(GEX_MTL* pMtl, GEX_TEX* pTex);
void gexMtlTangentMode(GEX_MTL* pMtl, GEX_TANGENT_MODE mode);
void gexMtlTangentOptions(GEX_MTL* pMtl, bool flipTangent, bool flipBitangent);
void gexMtlTesselationMode(GEX_MTL* pMtl, GEX_TESS_MODE mode);
void gexMtlTesselationFactor(GEX_MTL* pMtl, float factor);
void gexMtlAlpha(GEX_MTL* pMtl, bool enable);
void gexMtlAlphaBlend(GEX_MTL* pMtl, bool enable);
void gexMtlAlphaToCoverage(GEX_MTL* pMtl, bool enable);
void gexMtlRoughness(GEX_MTL* pMtl, float rough);
void gexMtlDiffuseRoughness(GEX_MTL* pMtl, const cxColor& rgb);
void gexMtlDiffuseRoughnessTexRate(GEX_MTL* pMtl, float rate);
void gexMtlSpecularRoughness(GEX_MTL* pMtl, const cxColor& rgb);
void gexMtlSpecularRoughnessTexRate(GEX_MTL* pMtl, float rate);
void gexMtlSpecularRoughnessMin(GEX_MTL* pMtl, float min);
void gexMtlIOR(GEX_MTL* pMtl, const cxVec& ior);
void gexMtlViewFresnel(GEX_MTL* pMtl, float gain, float bias);
void gexMtlReflFresnel(GEX_MTL* pMtl, float gain, float bias);
void gexMtlReflDownFadeRate(GEX_MTL* pMtl, float rate);
void gexMtlDiffMode(GEX_MTL* pMtl, GEX_DIFF_MODE mode);
void gexMtlSpecMode(GEX_MTL* pMtl, GEX_SPEC_MODE mode);
void gexMtlBumpMode(GEX_MTL* pMtl, GEX_BUMP_MODE mode);
void gexMtlSpecFresnelMode(GEX_MTL* pMtl, int mode);
void gexMtlDiffFresnelMode(GEX_MTL* pMtl, int mode);
void gexMtlUVMode(GEX_MTL* pMtl, GEX_UV_MODE mode);
void gexMtlSortMode(GEX_MTL* pMtl, GEX_SORT_MODE mode);
void gexMtlSortBias(GEX_MTL* pMtl, float absBias, float relBias);
void gexMtlShadowMode(GEX_MTL* pMtl, bool castShadows, bool receiveShadows);
void gexMtlShadowDensity(GEX_MTL* pMtl, float density);
void gexMtlSelfShadowFactor(GEX_MTL* pMtl, float factor);
void gexMtlShadowCulling(GEX_MTL* pMtl, bool enable);
void gexMtlShadowAlphaThreshold(GEX_MTL* pMtl, float val);
void gexMtlSHDiffuseColor(GEX_MTL* pMtl, const cxColor& clr);
void gexMtlSHDiffuseDetail(GEX_MTL* pMtl, float dtl);
void gexMtlSHReflectionColor(GEX_MTL* pMtl, const cxColor& clr);
void gexMtlSHReflectionDetail(GEX_MTL* pMtl, float dtl);
void gexMtlSHReflectionFresnelRate(GEX_MTL* pMtl, float rate);
