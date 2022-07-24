#pragma once

#include "printf.h"
#include "s_sound.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include "stats.h"

// @Cockatrice: Queue wrapper
// Funcs added as are necessary
template <typename T>
class TSQueue {
	TSQueue() {}
	~TSQueue() {
		clear();
	}

	bool dequeue(T &item) {
		std::lock_guard lock(mQLock);
		return mQueue.pop(item);
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
	int deleteSearch(bool(*func)(T&)) {
		std::lock_guard lock(mQLock);
		for (int x = 0; x < mQueue.Size(); x++) {
			if (func(mQueue[x])) {
				mQueue.Delete(x);
				x--;
			}
		}
	}

	// Run this func for all elements in the queue
	void foreach(void(*func)(T&)) { 
		std::lock_guard lock(mQLock);  
		for (int x = 0; x < mQueue.Size(); x++) { func(mQueue[x]); } 
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
	ResourceLoader() {}

	void start() {
		if (!mThread) {
			mThread = thread(&ResourceLoader<IP, OP>::bgproc, this);
		}
	}

	void stop() {
		// Kill and finish the thread
		if (mThread.joinable()) {
			mActive.store(false);
			mWake.notify_all();
			mThread.join();
		}
	}


	void queue(IP &input) {
		mInputQ.queue(input);
		mWake.notify_all();
	}

	bool getFinished(OP &output) {
		return mOutputQ.dequeue(output);
	}

protected:
	// Replace this to actually load the resource in the background
	virtual bool loadResource(IP &input, OP &output) { return false; }

private:
	std::atomic<bool> mActive{ true };
	std::thread mThread;
	std::mutex mWakeLock;
	std::condition_variable mWake;

	TSQueue<IP> mInputQ;
	TSQueue<OP> mOutputQ;

	void bgproc() {
		std::unique_lock<std::mutex> lock(mWakeLock);

		while (mActive.load()) {
			bool processed;
			lock.try_lock();

			// Process the queue
			while (true) {
				IP input;
				if (!mInputQ.dequeue(&input)) {
					break;
				}

				OP output;
				if (loadResource(input, &output)) {
					mOutputQ.queue(output);
				}
				processed = true;
			}

			if (!processed) {
				mWake.wait_for(lock, std::chrono::milliseconds(5));
			} else {
				lock.unlock();
			}
		}
	}
};


/*class MThread {
public:
	template< class Function, class... Args> explicit MThread(Function&& f, Args&&... args);
	void join() { m_thread.join(); }
	bool active() const noexcept { return m_active.load(std::memory_order_acquire); }

private:
	std::atomic<bool> m_active{ true };
	std::thread m_thread;
};


template < class Function, class... Args >
MThread::MThread(Function&& fn, Args&&... args) :
	m_active(false),
	m_thread(
		[this](auto&& fn2, auto&&... args2) -> void {
			m_active = true;
			fn2(std::forward<Args>(args2)...);
			m_active = false;
		},
		std::forward<Function>(fn), std::forward<Args>(args)...
	)
{}*/


// This should encapsulate pre-calculated data on how the sound will be played
// After it finishes loading via the queue
struct AudioQueuePlayInfo {
	FSoundID orgSoundID;
	FVector3 pos, vel;
	int channel, type;
	float pitch, volume, attenuation, startTime;
	EChanFlags flags;
	FRolloffInfo rolloff;
	const void *source = NULL;
};


struct AudioQItem {
	FSoundID soundID;
	sfxinfo_t *sfx;
	TArray<AudioQueuePlayInfo> playInfo;
	// Using an array because if more than one attempt to play the same unloaded sound is made, it can be queued
	// to play when loading is complete. If playInfo is empty, the sound was just meant to be loaded and not
	// meant to be played.

	bool operator ==(const AudioQItem& b) {
		return sfx == b.sfx || soundID == b.soundID;
	}
};



// A single thread used to read and convert audio contents
// Does not modify any data but stores loaded audio for extraction later in the main thread
class AudioLoaderThread {
	friend class AudioLoaderQueue;	 // Should be temporary, eventually manage access to qItem
public:
	AudioLoaderThread(AudioQItem &item);
	~AudioLoaderThread();

	bool joinable() { return mThread.joinable(); }
	void join() { mThread.join(); }
	bool active() const noexcept { return mActive.load(std::memory_order_acquire); }

	SoundHandle getSoundHandle() { if (active()) return { NULL }; else return loadSnd; }

private:
	std::atomic<bool> mActive{ true };
	std::thread mThread;
	SoundHandle loadSnd;

	// Stat handling
	cycle_t totalTime, threadTime;

	AudioQItem qItem;
	char *data = nullptr;
	FileReader *readerCopy = nullptr;
	bool createdNewData = false;

	void startLoading();
};



class AudioLoaderQueue
{
private:
	//static const unsigned int MaxThreads = 4;		// TODO: Make this a CVAR

	struct QStat {
		double totalTime, threadTime, integrationTime;
	};

	TArray<AudioQItem> mQueue;
	TArray<AudioLoaderThread*> mRunning;
	TArray<QStat> mStats;

	cycle_t updateCycles;
	int totalLoaded = 0, totalFailed = 0;

	void start(AudioQItem &item);
	void relinkSound(AudioQItem &item, int sourcetype, const void *from, const void *to, const FVector3 *optpos);

public:
	AudioLoaderQueue();
	~AudioLoaderQueue();

	
	void startup();
	void shutdown();

	// Call this during the game loop when queued sounds are allowed to be finished and potentially played
	void update();

	// Empty the queue, blocks until current load ops are complete
	void clear();

	// Queue a sound to load. The sound will be played on load if there is valid playInfo data
	void queue(sfxinfo_t *sfx, FSoundID soundID, const AudioQueuePlayInfo *playInfo = NULL);
	void relinkSound(int sourcetype, const void *from, const void *to, const FVector3 *optpos);
	void stopSound(FSoundID soundID);
	void stopSound(int channel, FSoundID soundID);
	void stopSound(int sourcetype, const void* actor, int channel, int soundID);
	void stopActorSounds(int sourcetype, const void* actor, int chanmin, int chanmax);
	void stopAllSounds();
	int getSoundPlayingInfo(int sourcetype, const void *source, int sound_id, int chann);

	double updateTimeLast() { return updateCycles.TimeMS(); }
	int queueSize() { return mQueue.Size(); }
	int numActive() { return mRunning.Size(); }
	double avgLoad() { return calcLoadAvg(); }
	double maxLoad() { return calcMaxLoad(); }
	double minLoad() { return calcMinLoad(); }

	double calcLoadAvg() {
		double totalLoad = 0;
		for (QStat &s : mStats) { totalLoad += s.threadTime; }
		return mStats.Size() > 0 ? totalLoad / (double)mStats.Size()  : 0;
	}

	double calcMaxLoad() {
		double maxLoad = 0;
		for (QStat &s : mStats) { if (s.threadTime > maxLoad) maxLoad = s.threadTime; }
		return maxLoad;
	}

	double calcMinLoad() {
		double minLoad = mStats.Size() > 0 ? mStats[0].totalTime : -1;
		for (QStat &s : mStats) { if (s.threadTime < minLoad) minLoad = s.threadTime; }
		return minLoad >= 0 ? minLoad : 0;
	}

	double calcMinIntegration() {
		double mintegration = mStats.Size() > 0 ? mStats[0].integrationTime : -1;
		for (QStat &s : mStats) { if (s.integrationTime < mintegration) mintegration = s.integrationTime; }
		return mintegration >= 0 ? mintegration : 0;
	}

	double calcMaxIntegration() {
		double maxIntegration = 0;
		for (QStat &s : mStats) { if (s.integrationTime > maxIntegration) maxIntegration = s.integrationTime; }
		return maxIntegration;
	}

	double calcAvgIntegration() {
		double totalInt = 0;
		for (QStat &s : mStats) { totalInt += s.integrationTime; }
		return mStats.Size() > 0 ? totalInt / (double)mStats.Size() : 0;
	}

	int getTotalLoaded() { return totalLoaded; }
	int getTotalFailed() { return totalFailed; }

	static AudioLoaderQueue *Instance;
};