double time_micros();


class cStopWatch {
protected:
	double* mpSmps;
	double mT;
	int mSmpsNum;
	int mSmpIdx;

public:
	cStopWatch() : mpSmps(nullptr), mSmpsNum(0), mSmpIdx(0) {}
	~cStopWatch() { free(); }

	void alloc(int nsmps);
	void free();

	void begin();
	bool end();
	void reset();
	double median();
};
