class cStage3 {
protected:
	TEXTURE_LIB mTexLib;
	GEX_OBJ* mpObj;
	GEX_OBJ* mpFenceObj;
	GEX_OBJ* mpSkyObj;
	GEX_LIT* mpSkyLit;
	sxGeometryData* mpObstGeo;

	bool mInitFlg;

public:
	cStage3()
	: mInitFlg(false),
	mpObj(nullptr), mpFenceObj(nullptr),
	mpSkyObj(nullptr), mpSkyLit(nullptr),
	mpObstGeo(nullptr)
	{}

	void create();
	void destroy();

	void update();
	void disp(GEX_LIT* pLit = nullptr);

	GEX_LIT* get_sky_light() const { return mpSkyLit; }
	void sky_fog_enable(bool enable);

	sxGeometryData* get_obst_geo() { return mpObstGeo; }
	bool hit_ck(const cxVec& pos0, const cxVec& pos1, cxVec* pHitPos, cxVec* pHitNrm);
};

