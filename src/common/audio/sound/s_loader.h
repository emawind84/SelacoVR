#pragma once

#include "printf.h"
#include "s_sound.h"
#include <thread>
#include <atomic>
#include <chrono>

class MThread {
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
{}


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

	bool joinable() { return mThread.joinable(); }
	void join() { mThread.join(); }
	bool active() const noexcept { return mActive.load(std::memory_order_acquire); }

	SoundHandle getSoundHandle() { if (active()) return { NULL }; else return loadSnd; }

private:
	std::atomic<bool> mActive{ true };
	std::thread mThread;
	SoundHandle loadSnd;

	AudioQItem qItem;

	void startLoading();
};



class AudioLoaderQueue
{
private:
	static const unsigned int MaxThreads = 4;		// TODO: Make this a CVAR

	TArray<AudioQItem> mQueue;
	TArray<AudioLoaderThread*> mRunning;


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

	static AudioLoaderQueue *Instance;
};