void set_scene_fog(GEX_LIT* pLit);

class cStage4 {
protected:
	TEXTURE_LIB mTexLib;
	GEX_OBJ* mpObj;
	GEX_OBJ* mpSkyObj;
	GEX_LIT* mpConstLit;

	bool mInitFlg;

public:
	cStage4()
	: mInitFlg(false),
	mpObj(nullptr),
	mpSkyObj(nullptr),
	mpConstLit(nullptr)
	{}

	void create();
	void destroy();

	void disp();
};

