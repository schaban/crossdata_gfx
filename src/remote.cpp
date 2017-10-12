#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX
#define _WIN32_WINNT 0x0500
#include <Windows.h>
#define INCL_WINSOCK_API_TYPEDEFS 1
#include <WinSock2.h>

#include "crossdata.hpp"

#include "remote.hpp"

#define D_SOCK_ERR_CK(sock_errno, sock_errcode) ((sock_errno) == WSA##sock_errcode)

static struct SOCKET_SYS {
	HMODULE mHandle;
	bool mInitFlg;
	struct IFC {
		LPFN_WSASTARTUP startup;
		LPFN_WSAGETLASTERROR get_last_err;
		LPFN_GETHOSTNAME get_host_name;
		LPFN_GETHOSTBYNAME get_host_by_name;
		LPFN_HTONS host_to_net_u16;
		LPFN_SOCKET socket_open;
		LPFN_CLOSESOCKET socket_close;
		LPFN_SHUTDOWN socket_shutdown;
		LPFN_SETSOCKOPT socket_opt;
		LPFN_CONNECT socket_connect;
		LPFN_BIND socket_bind;
		LPFN_IOCTLSOCKET socket_cmd;
		LPFN_RECV socket_recv;
	} mIfc;

	SOCKET_SYS() : mHandle(nullptr), mInitFlg(false) {
		::memset(&mIfc, 0, sizeof(mIfc));
	}

	bool init() {
		if (!mHandle) {
			mHandle = ::LoadLibraryW(L"ws2_32.dll");
			if (mHandle) {
				WSADATA wsdat;
				WORD wsverMaj = 2;
				WORD wsverMin = 0;
				WORD wsver = MAKEWORD(wsverMaj, wsverMin);
				*(FARPROC*)&mIfc.startup = ::GetProcAddress(mHandle, "WSAStartup");
				if (mIfc.startup && mIfc.startup(wsver, &wsdat) == 0) {
					char hostName[512];
					hostent* pHostInfo = nullptr;
					nxCore::dbg_msg("%s: %s\n", wsdat.szDescription, wsdat.szSystemStatus);
					*(FARPROC*)&mIfc.get_host_name = ::GetProcAddress(mHandle, "gethostname");
					if (mIfc.get_host_name && mIfc.get_host_name(hostName, sizeof(hostName)) == 0) {
						nxCore::dbg_msg("host: %s\n", hostName);
						*(FARPROC*)&mIfc.get_host_by_name = ::GetProcAddress(mHandle, "gethostbyname");
						if (mIfc.get_host_by_name && (pHostInfo = mIfc.get_host_by_name(hostName))) {
							*(FARPROC*)&mIfc.get_last_err = ::GetProcAddress(mHandle, "WSAGetLastError");
							*(FARPROC*)&mIfc.host_to_net_u16 = ::GetProcAddress(mHandle, "htons");
							*(FARPROC*)&mIfc.socket_open = ::GetProcAddress(mHandle, "socket");
							*(FARPROC*)&mIfc.socket_close = ::GetProcAddress(mHandle, "closesocket");
							*(FARPROC*)&mIfc.socket_shutdown = ::GetProcAddress(mHandle, "shutdown");
							*(FARPROC*)&mIfc.socket_opt = ::GetProcAddress(mHandle, "setsockopt");
							*(FARPROC*)&mIfc.socket_connect = ::GetProcAddress(mHandle, "connect");
							*(FARPROC*)&mIfc.socket_bind = ::GetProcAddress(mHandle, "bind");
							*(FARPROC*)&mIfc.socket_cmd = ::GetProcAddress(mHandle, "ioctlsocket");
							*(FARPROC*)&mIfc.socket_recv = ::GetProcAddress(mHandle, "recv");
							mInitFlg = true;
						}
					}
				}
			}
		}
		return mInitFlg;
	}

	void reset() {
		if (mHandle) {
			::FreeLibrary(mHandle);
		}
		mHandle = nullptr;
		::memset(&mIfc, 0, sizeof(mIfc));
	}

	int get_last_err() {
		if (mInitFlg && mIfc.get_last_err) {
			return mIfc.get_last_err();
		}
		return 0;
	}

	int get_host_name(char* pNameBuf, size_t nameBufLen) {
		if (mInitFlg && mIfc.get_host_name) {
			return mIfc.get_host_name(pNameBuf, (int)nameBufLen);
		}
		return SOCKET_ERROR;
	}

	hostent* get_host_by_name(const char* pName) {
		if (mInitFlg && pName && mIfc.get_host_by_name) {
			return mIfc.get_host_by_name(pName);
		}
		return nullptr;
	}

	u_short host_to_net_u16(u_short val) {
		if (mInitFlg && mIfc.host_to_net_u16) {
			return mIfc.host_to_net_u16(val);
		}
		return 0;
	}

	SOCKET socket_open(int addrFamily, int socketType, int protocol) {
		if (mInitFlg && mIfc.socket_open) {
			return mIfc.socket_open(addrFamily, socketType, protocol);
		}
		return INVALID_SOCKET;
	}

	int socket_close(SOCKET s) {
		if (mInitFlg && mIfc.socket_close) {
			return mIfc.socket_close(s);
		}
		return SOCKET_ERROR;
	}

	int socket_shutdown(SOCKET s, int how) {
		if (mInitFlg && mIfc.socket_shutdown) {
			return mIfc.socket_shutdown(s, how);
		}
		return SOCKET_ERROR;
	}

	int socket_opt(SOCKET s, int lvl, int opt, const char* pOptVal, size_t optValLen) {
		if (mInitFlg && mIfc.socket_opt) {
			return mIfc.socket_opt(s, lvl, opt, pOptVal, (int)optValLen);
		}
		return SOCKET_ERROR;
	}

	int socket_connect(SOCKET s, const sockaddr* pAddr, size_t addrLen) {
		if (mInitFlg && pAddr && mIfc.socket_connect) {
			return mIfc.socket_connect(s, pAddr, (int)addrLen);
		}
		return SOCKET_ERROR;
	}

	int socket_cmd(SOCKET s, long cmd, u_long* pArgs) {
		if (mInitFlg && mIfc.socket_cmd) {
			return mIfc.socket_cmd(s, cmd, pArgs);
		}
		return SOCKET_ERROR;
	}

	int socket_recv(SOCKET s, char* pBuf, int len, int flags = 0) {
		if (mInitFlg && mIfc.socket_recv && pBuf) {
			return mIfc.socket_recv(s, pBuf, len, flags);
		}
		return SOCKET_ERROR;
	}
} s_sock;

void rmtSocketInit() {
	s_sock.init();
}

void rmtSocketReset() {
	s_sock.reset();
}


struct RMT_PIPE_IN {
	SOCKET mSocket;
	char* mpHostName;
	int mHostPort;
	char mBuf[1024 * 16];
};

static SOCKET createPipeInSocket(const char* pHostName, int port) {
	SOCKET s = INVALID_SOCKET;
	if (pHostName) {
		SOCKET sock = s_sock.socket_open(PF_INET, SOCK_STREAM, 0);
		if (sock != INVALID_SOCKET) {
			int optTrue = 1;
			if (s_sock.socket_opt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&optTrue, sizeof(optTrue)) != SOCKET_ERROR) {
				u_long nonBlk = 1;
				if (s_sock.socket_cmd(sock, FIONBIO, &nonBlk) != SOCKET_ERROR) {
					hostent* pHostAddr = s_sock.get_host_by_name(pHostName);
					if (pHostAddr) {
						sockaddr_in addr;
						::memset(&addr, 0, sizeof(addr));
						addr.sin_family = AF_INET;
						addr.sin_port = s_sock.host_to_net_u16((u_short)port);
						::memcpy(&addr.sin_addr.s_addr, pHostAddr->h_addr_list[0], pHostAddr->h_length);
						if (s_sock.socket_connect(sock, (const sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR) {
							s = sock;
						} else {
							int lerr = s_sock.get_last_err();
							if (D_SOCK_ERR_CK(lerr, EWOULDBLOCK)) {
								::Sleep(100);
								s = sock;
							} else if (D_SOCK_ERR_CK(lerr, EALREADY) || D_SOCK_ERR_CK(lerr, EINPROGRESS) || D_SOCK_ERR_CK(lerr, EISCONN)) {
								s = sock;
							} else {
								s_sock.socket_close(sock);
							}
						}
					} else {
						s_sock.socket_close(sock);
					}
				} else {
					s_sock.socket_close(sock);
				}
			} else {
				s_sock.socket_close(sock);
			}
		}
	}
	return s;
}

RMT_PIPE_IN* rmtPipeInCreate(const char* pHostName, int port) {
	RMT_PIPE_IN* pPipe = nullptr;
	if (pHostName) {
		SOCKET sock = createPipeInSocket(pHostName, port);
		if (sock != INVALID_SOCKET) {
			size_t memsize = sizeof(RMT_PIPE_IN);
			pPipe = (RMT_PIPE_IN*)nxCore::mem_alloc(memsize, XD_FOURCC('p', 'i', 'p', 'I'));
			if (pPipe) {
				::memset(pPipe, 0, memsize);
				pPipe->mSocket = sock;
				pPipe->mpHostName = nxCore::str_dup(pHostName);
				pPipe->mHostPort = port;
			} else {
				s_sock.socket_close(sock);
			}
		}
	}
	return pPipe;
}

void rmtPipeInDestroy(RMT_PIPE_IN* pPipe) {
	if (pPipe) {
		if (pPipe->mSocket != INVALID_SOCKET) {
			u_long blk = 0;
			s_sock.mIfc.socket_shutdown(pPipe->mSocket, SD_BOTH);
			s_sock.socket_cmd(pPipe->mSocket, FIONBIO, &blk);
			s_sock.mIfc.socket_close(pPipe->mSocket);
		}
		nxCore::mem_free(pPipe->mpHostName);
		nxCore::mem_free(pPipe);
	}
}

int rmtPipeInRead(RMT_PIPE_IN* pPipe, char* pBuf, int num) {
	int nread = 0;
	if (pBuf && num > 0 && pPipe && pPipe->mSocket != INVALID_SOCKET) {
		while (nread < num) {
			int n = s_sock.socket_recv(pPipe->mSocket, &pBuf[nread], num - nread);
			if (n > 0) {
				nread += n;
			} else {
				break;
			}
		}
	}
	return nread;
}

char* rmtPipeInReadStr(RMT_PIPE_IN* pPipe, char* pBuf, size_t bufSize) {
	char* pStrBuf = pPipe->mBuf;
	size_t nmax = sizeof(pPipe->mBuf) - 1;
	if (pBuf && bufSize > 0) {
		pStrBuf = pBuf;
		nmax = bufSize - 1;
	}
	int ntry = 10;
	size_t offs = 0;
	while (ntry > 0) {
		int nread = rmtPipeInRead(pPipe, &pStrBuf[offs], 1);
		if (nread) {
			if (pStrBuf[offs] == 0) {
				break;
			}
			offs += nread;
			if (offs >= nmax) {
				pStrBuf[nmax] = 0;
				break;
			}
		} else {
			--ntry;
		}
	}
	return pStrBuf;
}
