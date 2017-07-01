struct KEY_CTRL;

struct KEY_INFO {
	const char* mpName;
	uint32_t mId;
};


KEY_CTRL* keyCreate(KEY_INFO* pKeys, int nkeys);
void keyDestroy(KEY_CTRL* pCtrl);
void keyUpdate(KEY_CTRL* pCtrl);
bool keyCkNow(KEY_CTRL* pCtrl, uint32_t id);
bool keyCkOld(KEY_CTRL* pCtrl, uint32_t id);
bool keyCkChg(KEY_CTRL* pCtrl, uint32_t id);
bool keyCkTrg(KEY_CTRL* pCtrl, uint32_t id);
