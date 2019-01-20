#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX
#define _WIN32_WINNT 0x0500
#include <windows.h>

#include "crossdata.hpp"

#include "keyctrl.hpp"

struct KEY_CODE {
	int32_t mCode;
	int32_t mId;
};

struct KEY_CTRL {
	uint32_t mOld;
	uint32_t mNow;
	uint32_t mChg;
	uint32_t mTrg;
	int mNum;
	KEY_CODE mKeys[1];
};

static bool validate_key_id(uint32_t id) { return id < 32U; }

static struct KEY_MAP {
	const char* pName;
	int32_t code;
} s_keymap[] = {
	{ "[LMB]", VK_LBUTTON },
	{ "[RMB]", VK_RBUTTON },
	{ "[MMB]", VK_MBUTTON },
	{ "[BACK]", VK_BACK },
	{ "[TAB]", VK_TAB },
	{ "[ENTER]", VK_RETURN },
	{ "[SPACE]",  VK_SPACE },
	{ "[LSHIFT]", VK_LSHIFT },
	{ "[LCTRL]", VK_LCONTROL },
	{ "[RSHIFT]", VK_RSHIFT },
	{ "[RCTRL]", VK_RCONTROL },
	{ "[UP]", VK_UP },
	{ "[DOWN]", VK_DOWN },
	{ "[LEFT]", VK_LEFT },
	{ "[RIGHT]", VK_RIGHT },
	{ "[INS]", VK_INSERT },
	{ "[DEL]", VK_DELETE },
	{ "[HOME]", VK_HOME },
	{ "[END]", VK_END },
	{ "[PGUP]", VK_PRIOR },
	{ "[PGDN]", VK_NEXT },
	{ "[ESC]", VK_ESCAPE },
	{ "[F1]", VK_F1 },
	{ "[F2]", VK_F2 },
	{ "[F3]", VK_F3 },
	{ "[F4]", VK_F4 },
	{ "[F5]", VK_F5 },
	{ "[F6]", VK_F6 },
	{ "[F7]", VK_F7 },
	{ "[F8]", VK_F8 },
	{ "[F9]", VK_F9 },
	{ "[F10]", VK_F10 },
	{ "[F11]", VK_F11 },
	{ "[F12]", VK_F12 },
	{ "#0", VK_NUMPAD0 },
	{ "#1", VK_NUMPAD1 },
	{ "#2", VK_NUMPAD2 },
	{ "#3", VK_NUMPAD3 },
	{ "#4", VK_NUMPAD4 },
	{ "#5", VK_NUMPAD5 },
	{ "#6", VK_NUMPAD6 },
	{ "#7", VK_NUMPAD7 },
	{ "#8", VK_NUMPAD8 },
	{ "#9", VK_NUMPAD9 },
	{ "#/", VK_DIVIDE },
	{ "#*", VK_MULTIPLY },
	{ "#-", VK_SUBTRACT },
	{ "#+", VK_ADD },
	{ "#.", VK_DECIMAL }
};

static bool key_name_ck(const char* pName, size_t len, int i) {
#if 0
	return ::_strcmpi(pName, s_keymap[i].pName) == 0;
#else
	const char* p0 = pName;
	const char* p1 = s_keymap[i].pName;
	if (len != ::strlen(p1)) return false;
	for (int i = 0; i < int(len); ++i) {
		char c0 = *p0++;
		char c1 = *p1++;
		c0 = ::toupper(c0);
		if (c0 != c1) {
			return false;
		}
	}
	return true;
#endif
}

static int32_t find_key_code(const char* pName) {
	size_t len = ::strlen(pName);
	if (1 == len) {
		int32_t chr = ::toupper(pName[0]);
		if ((chr >= '0' && chr <= '9') || (chr >= 'A' && chr <= 'Z')) {
			return chr;
		}
	} else {
		for (int i = 0; i < XD_ARY_LEN(s_keymap); ++i) {
			if (key_name_ck(pName, len, i)) {
				return s_keymap[i].code;
			}
		}
	}
	return -1;
}

KEY_CTRL* keyCreate(KEY_INFO* pKeys, int nkeys) {
	KEY_CTRL* pCtrl = nullptr;
	if (pKeys) {
		int n = 0;
		for (int i = 0; i < nkeys; ++i) {
			if (pKeys[i].mpName) {
				if (validate_key_id(pKeys[i].mId)) {
					if (find_key_code(pKeys[i].mpName) > 0) {
						++n;
					} else {
						nxCore::dbg_msg("Unknown key name: %s\n", pKeys[i].mpName);
					}
				} else {
					nxCore::dbg_msg("Invalid key id: %s, 0x%X\n", pKeys[i].mpName, pKeys[i].mId);
				}
			}
		}
		if (n > 0) {
			size_t memsize = sizeof(KEY_CTRL) + (n-1)*sizeof(KEY_CODE);
			pCtrl = (KEY_CTRL*)nxCore::mem_alloc(memsize, XD_FOURCC('K','E','Y','S'));
			if (pCtrl) {
				::ZeroMemory(pCtrl, memsize);
				pCtrl->mNum = n;
				int kidx = 0;
				for (int i = 0; i < nkeys; ++i) {
					if (pKeys[i].mpName) {
						if (validate_key_id(pKeys[i].mId)) {
							int code = find_key_code(pKeys[i].mpName);
							if (code > 0) {
								pCtrl->mKeys[kidx].mCode = code;
								pCtrl->mKeys[kidx].mId = pKeys[i].mId;
								++kidx;
							}
						}
					}
				}
			}
		}
	}
	return pCtrl;
}

void keyDestroy(KEY_CTRL* pCtrl) {
	if (pCtrl) {
		nxCore::mem_free(pCtrl);
	}
}

void keyUpdate(KEY_CTRL* pCtrl) {
	if (!pCtrl) return;
	int n = pCtrl->mNum;
	uint32_t state = 0;
	for (int i = 0; i < n; ++i) {
		if (!!(::GetAsyncKeyState(pCtrl->mKeys[i].mCode) & 0x8000)) {
			state |= 1U << pCtrl->mKeys[i].mId;
		}
	}
	pCtrl->mOld = pCtrl->mNow;
	pCtrl->mNow = state;
	pCtrl->mChg = state ^ pCtrl->mOld;
	pCtrl->mTrg = state & pCtrl->mChg;
}

bool keyCkNow(KEY_CTRL* pCtrl, uint32_t id) {
	bool res = false;
	if (pCtrl && validate_key_id(id)) {
		res = !!(pCtrl->mNow & (1U << id));
	}
	return res;
}

bool keyCkOld(KEY_CTRL* pCtrl, uint32_t id) {
	bool res = false;
	if (pCtrl && validate_key_id(id)) {
		res = !!(pCtrl->mOld & (1U << id));
	}
	return res;
}

bool keyCkChg(KEY_CTRL* pCtrl, uint32_t id) {
	bool res = false;
	if (pCtrl && validate_key_id(id)) {
		res = !!(pCtrl->mChg & (1U << id));
	}
	return res;
}

bool keyCkTrg(KEY_CTRL* pCtrl, uint32_t id) {
	bool res = false;
	if (pCtrl && validate_key_id(id)) {
		res = !!(pCtrl->mTrg & (1U << id));
	}
	return res;
}
