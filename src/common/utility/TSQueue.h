#pragma once

#include <thread>
#include <functional>
#include <mutex>
#include <atomic>
#ifdef __linux__
#include <condition_variable>
#endif
#include <chrono>
#include "stats.h"
#include "tarray.h"


// @Cockatrice - Simple ring buffer with averaging
template <typename T, int IN_NUM>
struct RingBuffer {
	const int length = IN_NUM;
	T input[IN_NUM];
	long pos = -1;

	void add(T v) {
		pos++;
		if (pos < 0) pos = 0;
		(*this)[0] = v;
	}

	T& operator[] (int index) {
		assert(index >= 0 && index <= pos);
		return input[(pos - index) % IN_NUM];
	}

	T getAverage(int numInputs) {
		assert(pos >= 0);

		T avg = (*this)[0];
		numInputs = std::min(numInputs, IN_NUM);
		numInputs = (int)std::min((long)numInputs, pos);

		for (int x = 1; x < numInputs; x++) {
			avg += (*this)[x];
		}

		return avg / (float)numInputs;
	}

	T getScaledAverage(int numInputs, float amount) {
		assert(pos >= 0);

		amount = std::clamp(1.0f - amount, 0.0f, 1.0f);

		T avg = getAverage(numInputs);
		return avg + (amount * ((*this)[0] - avg));
	}

	void reset() {
		pos = -1;
	}
};



// @Cockatrice: Queue wrapper
// Funcs added as are necessary
template <typename T>
class TSQueue {
public:
	TSQueue() {}
	~TSQueue() {
		clear();
	}

	bool dequeue(T &item) {
		std::lock_guard lock(mQLock);
		return mQueue.Pop(item);
	}

	void queue(T &item) {
		std::lock_guard lock(mQLock);
		mQueue.Insert(0, item);
	}

	void clear() {
		std::lock_guard lock(mQLock);
		mQueue.Clear();
	}

	// Delete all items from the queue that match
	// based on search function
	int deleteSearch(const std::function <bool(T&)>func) {
		std::lock_guard lock(mQLock);
		int size = mQueue.Size();
		for (unsigned int x = 0; x < mQueue.Size(); x++) {
			if (func(mQueue[x])) {
				mQueue.Delete(x);
				x--;
			}
		}

		return size - mQueue.Size();
	}

	// Run this func for all elements in the queue
	void foreach(const std::function <void(T&)>func) {
		std::lock_guard lock(mQLock);
		for (unsigned int x = 0; x < mQueue.Size(); x++) { func(mQueue[x]); }
	}

	int size() {
		std::lock_guard lock(mQLock);
		return mQueue.Size();
	}

protected:
	TArray<T> mQueue;
	std::mutex mQLock;
};



// ResourceLoader<InputType, OutputType>
template <typename IP, typename OP>
class ResourceLoader {
public:
	ResourceLoader() {}
	virtual ~ResourceLoader() { stop(); }

	void start() {
		if (mThread.get_id() == std::thread::id()) {
			mThread = std::thread(&ResourceLoader<IP, OP>::bgproc, this);
		}
	}

	void clearInputQueue() {
		mInputQ.clear();
	}

	void stop() {
		// Kill and finish the thread
		if (mThread.joinable()) {
			mActive.store(false);
			mWake.notify_all();
			mThread.join();
		}
	}


	void queue(IP input) {
		mInputQ.queue(input);
		mMaxQueue = std::max(mMaxQueue.load(), mInputQ.size());
		mWake.notify_all();
	}

	int numQueued() {
		return mInputQ.size();
	}

	bool isActive() {
		return mActive.load() && (mRunning.load() || mInputQ.size() > 0);
	}

	bool popFinished(OP &output) {
		return mOutputQ.dequeue(output);
	}

	void resetStats() {
		// TODO: Block stat updates
		mMaxQueue = 0;
		mStatLoadTime = 0;
		mStatLoadCount = 0;
		mStatTotalLoaded = 0;
		mStatMinTime = 99999.0;
		mStatMaxTime = 0;
	}

	int statMaxQueued() {
		return mMaxQueue.load();
	}

	double statAvgLoadTime() {
		return mStatAvgTime;
	}

	double statMinLoadTime() {
		return std::min(99999.0, mStatMinTime.load());
	}

	double statMaxLoadTime() {
		return mStatMaxTime.load();
	}

	int statTotalLoaded() {
		return mStatTotalLoaded.load();
	}

protected:
	// Replace this to actually load the resource in the background
	virtual bool loadResource(IP &input, OP &output) { return false; }
	virtual void prepareLoad() {}		// Before load
	virtual void completeLoad() {}		// After load
	virtual void cancelLoad() {}		// Load was cancelled

	std::atomic<bool> mActive{ true };
	std::atomic<bool> mRunning{ false };
	std::atomic<int> mMaxQueue{ 0 }, mStatTotalLoaded{ 0 };
	std::atomic<double> mStatAvgTime{ 0 }, mStatMinTime{ 0 }, mStatMaxTime{ 0 };

	double mStatLoadTime = 0, mStatLoadCount = 0;

	std::thread mThread;
	std::mutex mWakeLock, mStatsLock;
	std::condition_variable mWake;

	TSQueue<IP> mInputQ;
	TSQueue<OP> mOutputQ;


private:
	void bgproc() {
		std::unique_lock<std::mutex> lock(mWakeLock);

		while (mActive.load()) {
			bool processed = false;

			// Process the queue
			while (true) {
				if (mInputQ.size() > 0) {
					mRunning.store(true);

					cycle_t lTime;
					lTime.Reset();
					lTime.Clock();

					prepareLoad();

					IP input;
					if (!mInputQ.dequeue(input)) {
						cancelLoad();
						break;
					}

					OP output;
					if (loadResource(input, output)) {
						mOutputQ.queue(output);
					}
					processed = true;

					completeLoad();

					// Update load stats
					lTime.Unclock();
					mStatLoadTime += lTime.TimeMS();
					mStatLoadCount += 1;
					mStatAvgTime = mStatLoadTime / mStatLoadCount;
					mStatMinTime = std::min(mStatMinTime.load(), lTime.TimeMS());
					mStatMaxTime = std::max(mStatMaxTime.load(), lTime.TimeMS());
					mStatTotalLoaded++;
				}
				else {
					break;
				}
			}

			mRunning.store(false);

			if (!processed) {
				mWake.wait_for(lock, std::chrono::milliseconds(5));
			}
		}
	}
};