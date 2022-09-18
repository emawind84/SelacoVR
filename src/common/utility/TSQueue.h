#pragma once

#include <thread>
#include <functional>
#include <mutex>
#include <atomic>
#include "tarray.h"

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
		mWake.notify_all();
	}

	int numQueued() {
		return mInputQ.size();
	}

	bool popFinished(OP &output) {
		return mOutputQ.dequeue(output);
	}

protected:
	// Replace this to actually load the resource in the background
	virtual bool loadResource(IP &input, OP &output) { return false; }
	virtual void prepareLoad() {}		// Before load
	virtual void completeLoad() {}		// After load
	virtual void cancelLoad() {}		// Load was cancelled

	std::atomic<bool> mActive{ true };
	std::thread mThread;
	std::mutex mWakeLock;
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
					prepareLoad();
				}

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
			}

			if (!processed) {
				mWake.wait_for(lock, std::chrono::milliseconds(5));
			}
		}
	}
};