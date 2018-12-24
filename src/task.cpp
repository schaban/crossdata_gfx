// Author: Sergey Chaban <sergey.chaban@gmail.com>

#include "crossdata.hpp"
#include "task.hpp"

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <new>

using namespace std;


struct TSK_LOCK {
	mutex mMutex;
};

TSK_LOCK* tskLockCreate() {
	TSK_LOCK* pLock = (TSK_LOCK*)nxCore::mem_alloc(sizeof(TSK_LOCK), XD_FOURCC('l', 'o', 'c', 'k'));
	if (pLock) {
		::new ((void*)pLock) TSK_LOCK;
	}
	return pLock;
}

void tskLockDestroy(TSK_LOCK* pLock) {
	if (pLock) {
		pLock->~TSK_LOCK();
		nxCore::mem_free(pLock);
	}
}

bool tskLockAcquire(TSK_LOCK* pLock) {
	bool res = false;
	if (pLock) {
		try {
			pLock->mMutex.lock();
			res = true;
		} catch (...) {
			res = false;
		}
	}
	return res;
}

bool tskLockRelease(TSK_LOCK* pLock) {
	bool res = false;
	if (pLock) {
		pLock->mMutex.unlock();
		res = true;
	}
	return res;
}


struct TSK_SIGNAL {
	mutex mMutex;
	condition_variable mCndVar;
	bool mState;
};

TSK_SIGNAL* tskSignalCreate() {
	TSK_SIGNAL* pSig = (TSK_SIGNAL*)nxCore::mem_alloc(sizeof(TSK_SIGNAL), XD_FOURCC('s', 'g', 'n', 'l'));
	if (pSig) {
		::new ((void*)pSig) TSK_SIGNAL;
		pSig->mState = false;
	}
	return pSig;
}

void tskSignalDestroy(TSK_SIGNAL* pSig) {
	if (pSig) {
		pSig->~TSK_SIGNAL();
		nxCore::mem_free(pSig);
	}
}

bool tskSignalWait(TSK_SIGNAL* pSig) {
	bool res = false;
	if (pSig) {
		unique_lock<mutex> ulk(pSig->mMutex);
		if (!pSig->mState) {
			while (true) {
				pSig->mCndVar.wait(ulk);
				res = true;
				if (pSig->mState) break;
			}
			if (res) {
				pSig->mState = false;
			}
		} else {
			res = true;
			pSig->mState = false;
		}
	}
	return res;
}

bool tskSignalSet(TSK_SIGNAL* pSig) {
	if (!pSig) return false;
	pSig->mMutex.lock();
	pSig->mState = true;
	if (pSig->mState) {
		pSig->mMutex.unlock();
		pSig->mCndVar.notify_one();
	}
	return true;
}

bool tskSignalReset(TSK_SIGNAL* pSig) {
	if (!pSig) return false;
	pSig->mMutex.lock();
	pSig->mState = false;
	pSig->mMutex.unlock();
	return true;
}

struct TSK_WORKER {
	TSK_SIGNAL* mpSigExec;
	TSK_SIGNAL* mpSigDone;
	TSK_WRK_FUNC mFunc;
	void* mpData;
	thread mThread;
	bool mEndFlg;
};

static void workerFunc(TSK_WORKER* pWrk) {
	if (!pWrk) return;
	tskSignalSet(pWrk->mpSigDone);
	while (!pWrk->mEndFlg) {
		if (tskSignalWait(pWrk->mpSigExec)) {
			//tskSignalReset(pWrk->mpSigExec);
			if (!pWrk->mEndFlg && pWrk->mFunc) {
				pWrk->mFunc(pWrk->mpData);
			}
			tskSignalSet(pWrk->mpSigDone);
		}
	}
}

TSK_WORKER* tskWorkerCreate(TSK_WRK_FUNC func, void* pData) {
	TSK_WORKER* pWrk = (TSK_WORKER*)nxCore::mem_alloc(sizeof(TSK_WORKER), XD_FOURCC('w', 'r', 'k', 'r'));
	if (pWrk) {
		::new ((void*)pWrk) TSK_WORKER;
		pWrk->mFunc = func;
		pWrk->mpData = pData;
		pWrk->mpSigExec = tskSignalCreate();
		pWrk->mpSigDone = tskSignalCreate();
		pWrk->mEndFlg = false;
		::new ((void*)&pWrk->mThread) thread(workerFunc, pWrk);
	}
	return pWrk;
}

void tskWorkerDestroy(TSK_WORKER* pWrk) {
	if (pWrk) {
		tskWorkerStop(pWrk);
		tskSignalDestroy(pWrk->mpSigDone);
		tskSignalDestroy(pWrk->mpSigExec);
		pWrk->~TSK_WORKER();
		nxCore::mem_free(pWrk);
	}
}

void tskWorkerExec(TSK_WORKER* pWrk) {
	if (pWrk) {
		tskSignalReset(pWrk->mpSigDone);
		tskSignalSet(pWrk->mpSigExec);
	}
}

void tskWorkerWait(TSK_WORKER* pWrk) {
	if (pWrk) {
		tskSignalWait(pWrk->mpSigDone);
	}
}

void tskWorkerStop(TSK_WORKER* pWrk) {
	if (pWrk && !pWrk->mEndFlg) {
		int exitRes = -1;
		pWrk->mEndFlg = true;
		tskWorkerExec(pWrk);
		tskWorkerWait(pWrk);
		pWrk->mThread.join();
	}
}


struct TSK_QUEUE {
	TSK_JOB** mpJobSlots;
	int mSlotsNum;
	int mPutIdx;
	atomic<int> mAccessIdx;

	TSK_JOB* get_next_job() {
		TSK_JOB* pJob = nullptr;
		int count = mPutIdx;
		if (count > 0) {
			int idx = mAccessIdx.fetch_add(1);
			if (idx < count) {
				pJob = mpJobSlots[idx];
			}
		}
		return pJob;
	}

	void resetCursor() {
		mAccessIdx.store(0);
	}
};

struct TSK_BRIGADE {
	TSK_QUEUE* mpQue;
	TSK_WORKER** mpWrkPtrs;
	TSK_CONTEXT* mpCtx;
	int mWrkNum;
	int mActiveWrkNum;
};

static void brigadeWrkFunc(void* pMem) {
	TSK_CONTEXT* pCtx = (TSK_CONTEXT*)pMem;
	if (!pCtx) return;
	TSK_BRIGADE* pBgd = pCtx->mpBrigade;
	if (!pBgd) return;
	TSK_QUEUE* pQue = pBgd->mpQue;
	if (!pQue) return;
	while (true) {
		TSK_JOB* pJob = pQue->get_next_job();
		if (!pJob) break;
		pCtx->mpJob = pJob;
		if (pJob->mFunc) {
			pJob->mFunc(pCtx);
		}
		++pCtx->mJobsDone;
	}
}

TSK_BRIGADE* tskBrigadeCreate(int wrkNum) {
	TSK_BRIGADE* pBgd = nullptr;
	if (wrkNum < 1) wrkNum = 1;
	size_t memSize = sizeof(TSK_BRIGADE) + wrkNum * sizeof(TSK_WORKER*) + wrkNum * sizeof(TSK_CONTEXT);
	pBgd = (TSK_BRIGADE*)nxCore::mem_alloc(memSize, XD_FOURCC('c', 'r', 'e', 'w'));
	if (pBgd) {
		::memset(pBgd, 0, memSize);
		pBgd->mWrkNum = wrkNum;
		pBgd->mActiveWrkNum = wrkNum;
		pBgd->mpWrkPtrs = (TSK_WORKER**)(pBgd + 1);
		pBgd->mpCtx = (TSK_CONTEXT*)(pBgd->mpWrkPtrs + wrkNum);
		for (int i = 0; i < wrkNum; ++i) {
			pBgd->mpCtx[i].mpBrigade = pBgd;
			pBgd->mpCtx[i].mWrkId = i;
			pBgd->mpWrkPtrs[i] = tskWorkerCreate(brigadeWrkFunc, &pBgd->mpCtx[i]);
		}
	}
	return pBgd;
}

void tskBrigadeDestroy(TSK_BRIGADE* pBgd) {
	if (!pBgd) return;
	int wrkNum = pBgd->mWrkNum;
	for (int i = 0; i < wrkNum; ++i) {
		tskWorkerStop(pBgd->mpWrkPtrs[i]);
	}
	for (int i = 0; i < wrkNum; ++i) {
		tskWorkerDestroy(pBgd->mpWrkPtrs[i]);
	}
	nxCore::mem_free(pBgd);
}

void tskBrigadeExec(TSK_BRIGADE* pBgd, TSK_QUEUE* pQue) {
	if (!pBgd) return;
	if (!pQue) return;
	pQue->resetCursor();
	pBgd->mpQue = pQue;
	int wrkNum = pBgd->mActiveWrkNum;
	for (int i = 0; i < wrkNum; ++i) {
		pBgd->mpCtx[i].mJobsDone = 0;
	}
	for (int i = 0; i < wrkNum; ++i) {
		tskWorkerExec(pBgd->mpWrkPtrs[i]);
	}
}

void tskBrigadeWait(TSK_BRIGADE* pBgd) {
	if (!pBgd) return;
	if (!pBgd->mpQue) return;
	int wrkNum = pBgd->mActiveWrkNum;
	for (int i = 0; i < wrkNum; ++i) {
		tskWorkerWait(pBgd->mpWrkPtrs[i]);
	}
	pBgd->mpQue = nullptr;
}

bool tskBrigadeCkWrkId(TSK_BRIGADE* pBgd, int wrkId) {
	return pBgd && ((unsigned)wrkId < (unsigned)pBgd->mWrkNum);
}

void tskBrigadeSetActiveWorkers(TSK_BRIGADE* pBgd, int num) {
	if (pBgd) {
		pBgd->mActiveWrkNum = nxCalc::clamp(num, 1, pBgd->mWrkNum);
	}
}

void tskBrigadeResetActiveWorkers(TSK_BRIGADE* pBgd) {
	if (pBgd) {
		pBgd->mActiveWrkNum = pBgd->mWrkNum;
		for (int i = 0; i < pBgd->mWrkNum; ++i) {
			pBgd->mpCtx[i].mJobsDone = 0;
		}
	}
}

int tskBrigadeGetNumActiveWorkers(TSK_BRIGADE* pBgd) {
	return pBgd ? pBgd->mActiveWrkNum : 0;
}

int tskBrigadeGetNumWorkers(TSK_BRIGADE* pBgd) {
	return pBgd ? pBgd->mWrkNum : 0;
}

int tskBrigadeGetNumJobsDone(TSK_BRIGADE* pBgd, int wrkId) {
	int n = 0;
	if (pBgd && tskBrigadeCkWrkId(pBgd, wrkId)) {
		n = pBgd->mpCtx[wrkId].mJobsDone;
	}
	return n;
}

TSK_CONTEXT* tskBrigadeGetContext(TSK_BRIGADE* pBgd, int wrkId) {
	TSK_CONTEXT* pCtx = nullptr;
	if (tskBrigadeCkWrkId(pBgd, wrkId)) {
		pCtx = &pBgd->mpCtx[wrkId];
	}
	return pCtx;
}


TSK_QUEUE* tskQueueCreate(int nslots) {
	TSK_QUEUE* pQue = nullptr;
	if (nslots > 0) {
		pQue = (TSK_QUEUE*)nxCore::mem_alloc(sizeof(TSK_QUEUE), XD_FOURCC('t', 'q', 'u', 'e'));
		if (pQue) {
			size_t memSize = nslots * sizeof(TSK_JOB*);
			TSK_JOB** pSlots = (TSK_JOB**)nxCore::mem_alloc(memSize, XD_FOURCC('s', 'l', 'o', 't'));
			if (pSlots) {
				::memset(pSlots, 0, memSize);
				pQue->mSlotsNum = nslots;
				pQue->mPutIdx = 0;
				pQue->resetCursor();
				pQue->mpJobSlots = pSlots;
			} else {
				nxCore::mem_free(pQue);
				pQue = nullptr;
			}
		}
	}
	return pQue;
}

void tskQueueDestroy(TSK_QUEUE* pQue) {
	if (pQue) {
		nxCore::mem_free(pQue->mpJobSlots);
		nxCore::mem_free(pQue);
	}
}

void tskQueueAdd(TSK_QUEUE* pQue, TSK_JOB* pJob) {
	if (!pQue) return;
	if (!pJob) return;
	if (pQue->mpJobSlots) {
		if (pQue->mPutIdx < pQue->mSlotsNum) {
			pQue->mpJobSlots[pQue->mPutIdx] = pJob;
			pJob->mId = pQue->mPutIdx;
			++pQue->mPutIdx;
		}
	}
}

void tskQueueAdjust(TSK_QUEUE* pQue, int count) {
	if (pQue && count <= pQue->mSlotsNum) {
		pQue->mPutIdx = count;
	}
}

void tskQueuePurge(TSK_QUEUE* pQue) {
	if (pQue) {
		pQue->mPutIdx = 0;
	}
}

void tskQueueExec(TSK_QUEUE* pQue, TSK_BRIGADE* pBgd) {
	if (!pQue) return;
	if (pBgd) {
		tskBrigadeExec(pBgd, pQue);
		tskBrigadeWait(pBgd);
	} else {
		TSK_CONTEXT ctx;
		ctx.mWrkId = -1;
		ctx.mpBrigade = nullptr;
		ctx.mJobsDone = 0;
		int n = tskQueueJobsCount(pQue);
		for (int i = 0; i < n; ++i) {
			TSK_JOB* pJob = pQue->mpJobSlots[i];
			if (pJob->mFunc) {
				ctx.mpJob = pJob;
				pJob->mFunc(&ctx);
			}
		}
	}
}

int tskQueueJobsCount(TSK_QUEUE* pQue) {
	int n = 0;
	if (pQue) {
		n = pQue->mPutIdx;
	}
	return n;
}

TSK_JOB* tskJobsAlloc(int n) {
	TSK_JOB* pJobs = nullptr;
	if (n > 0) {
		size_t memSize = n * sizeof(TSK_JOB);
		pJobs = (TSK_JOB*)nxCore::mem_alloc(memSize, XD_FOURCC('j', 'o', 'b', 's'));
		if (pJobs) {
			::memset(pJobs, 0, memSize);
		}
	}
	return pJobs;
}

void tskJobsFree(TSK_JOB* pJobs) {
	if (pJobs) {
		nxCore::mem_free(pJobs);
	}
}

