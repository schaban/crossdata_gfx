struct RMT_PIPE_IN;

struct TD_JOYSTICK_CHANNELS {
	float xaxis;
	float yaxis;
	float zaxis;
	float xrot;
	float yrot;
	float zrot;
	float slider1;
	float slider2;
	float b1;
	float b2;
	float b3;
	float b4;
	float b5;
	float b6;
	float p1_X;
	float p1_Y;
};

void rmtSocketInit();
void rmtSocketReset();

RMT_PIPE_IN* rmtPipeInCreate(const char* pHostName = "localhost", int port = 7000);
void rmtPipeInDestroy(RMT_PIPE_IN* pPipe);
int rmtPipeInRead(RMT_PIPE_IN* pPipe, char* pBuf, int num);
char* rmtPipeInReadStr(RMT_PIPE_IN* pPipe, char* pBuf = nullptr, size_t bufSize = 0);

