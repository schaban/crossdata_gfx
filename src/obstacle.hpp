struct sObstGeoWk {
	uint32_t* mpStamps;
	int* mpPolLst;
	int mPolNum;

	sObstGeoWk() : mpStamps(nullptr), mpPolLst(nullptr), mPolNum(0) {}

	void alloc(const sxGeometryData& geo);
	void free();
};

const float c_obstSepMargin = 1e-2f;

inline int obstCalcStampBytes(int npol) { return XD_BIT_ARY_SIZE(uint32_t, npol) * sizeof(uint32_t); }

bool obstBallToWalls(const cxVec& newPos, const cxVec& oldPos, float radius, const sxGeometryData& geo, cxVec* pAdjPos, sObstGeoWk* pGeoWk = nullptr, float wallSlopeLim = 0.7f);
bool obstBallToBall(const cxVec& newPos, const cxVec& oldPos, float radius, const cxVec& staticPos, float staticRadius, cxVec* pAdjPos, float reflectFactor = 0.5f, float margin = c_obstSepMargin);
bool obstBallToCap(const cxVec& newPos, const cxVec& oldPos, float radius, const cxVec& staticPos0, const cxVec& staticPos1, float staticRadius, cxVec* pAdjPos, float reflectFactor = 0.5f, float margin = c_obstSepMargin);
bool obstPillarToPillar(const cxVec& newPos, const cxVec& oldPos, float radius, float height, const cxVec& staticPos, float staticRadius, float staticHeight, cxVec* pAdjPos, float reflectFactor = 0.5f, float margin = c_obstSepMargin);
bool obstSeparateSpheres(const cxSphere& movSph, const cxVec& vel, const cxSphere& staticSph, cxVec* pSepVec, float margin = c_obstSepMargin);
bool obstSeparateSphereCapsule(const cxSphere& movSph, const cxVec& vel, const cxCapsule& staticCap, cxVec* pSepVec, cxVec* pAxisPnt, float margin = c_obstSepMargin);
int obstGround(const cxVec& pos, const sxGeometryData& geo, cxVec* pPos, cxVec* pNrm, float offsUp = 0.5, float offsDown = 1.5f, float slopeLim = 0.5f);
