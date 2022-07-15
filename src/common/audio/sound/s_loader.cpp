#include "s_loader.h"
#include "filesystem.h"
#include "s_sound.h"
#include "actor.h"
#include "stats.h"
#include "i_time.h"
#include "file_directory.h"

AudioLoaderQueue *AudioLoaderQueue::Instance = new AudioLoaderQueue();

CVAR(Int, audio_max_threads, 4, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

static void AppendAudioThreadStats(int q, int l, double tt, FString &out)
{
	out.AppendFormat(
		"Queued: %d\n"
		"Loading: %d  Total: %d  Failed: %d\n"
		"Avg Load Time: %2.3f +(%2.3f)\n"
		"Min Load Time: %2.3f +(%2.3f)\n"
		"Max Load Time: %2.3f +(%2.3f)\n"
		"Update Time: %2.3f\n",
		q, 
		l, AudioLoaderQueue::Instance->getTotalLoaded(), AudioLoaderQueue::Instance->getTotalFailed(),
		AudioLoaderQueue::Instance->calcLoadAvg(), AudioLoaderQueue::Instance->calcAvgIntegration(),
		AudioLoaderQueue::Instance->calcMinLoad(), AudioLoaderQueue::Instance->calcMinIntegration(),
		AudioLoaderQueue::Instance->calcMaxLoad(), AudioLoaderQueue::Instance->calcMaxIntegration(),
		tt
	);
}

ADD_STAT(audiothread)
{
	static int64_t lasttime = 0, lastUpdateTime = 0;
	static int maxLoading = 0, maxQueue = 0;
	static double maxUpdate = 0;

	if (maxQueue < AudioLoaderQueue::Instance->queueSize()) { maxQueue = AudioLoaderQueue::Instance->queueSize(); }
	if (maxLoading < AudioLoaderQueue::Instance->numActive()) { maxLoading = AudioLoaderQueue::Instance->numActive(); }
	if (maxUpdate < AudioLoaderQueue::Instance->updateTimeLast()) { maxUpdate = AudioLoaderQueue::Instance->updateTimeLast(); }

	auto t = I_msTime();
	if (t - lasttime > 400)
	{
		maxQueue = AudioLoaderQueue::Instance->queueSize();
		maxLoading = AudioLoaderQueue::Instance->numActive();
		lasttime = t;
	}

	if (t - lastUpdateTime > 2000) {
		maxUpdate = AudioLoaderQueue::Instance->updateTimeLast();
		lastUpdateTime = t;
	}

	FString out;
	AppendAudioThreadStats(maxQueue, maxLoading, maxUpdate, out);
	return out;
}


AudioLoaderThread::AudioLoaderThread(AudioQItem &item) {
	totalTime.Reset();
	threadTime.Reset();
	totalTime.Clock();

	auto rl = fileSystem.GetFileAt(item.sfx->lumpnum);
	auto reader = rl->Owner->GetReader();

	if (!reader) {
		FDirectory *fdir = dynamic_cast<FDirectory*>(rl->Owner);
		if (!fdir || !dynamic_cast<FDirectoryLump*>(rl)) {
			Printf(TEXTCOLOR_RED"AudioLoaderThread::No valid reader on owner for sfx : %s\n", item.sfx->name.GetChars());
			data = nullptr;
			mActive.store(false);
			return;
		}

		// Should be a valid file reader now
		readerCopy = rl->NewReader().CopyNew();
	}

	else { readerCopy = reader->CopyNew(); }

	qItem = item;

	std::thread sl(&AudioLoaderThread::startLoading, this);
	mThread = std::move(sl);	// This seems dumb, how do I do this properly? I'm rusty.
}

AudioLoaderThread::~AudioLoaderThread() {
	if (readerCopy) {
		delete readerCopy;
		readerCopy = nullptr;
	}

	// If for some reason we haven't taken ownership of the loaded data, destroy it here
	// to prevent memory leaks
	if (createdNewData && data != nullptr) {
		delete[] data;
	}

	//Printf(TEXTCOLOR_BLUE"Deleted an audio loader thread.\n");
}

void AudioLoaderThread::startLoading() {
	threadTime.Clock();

	auto rl = fileSystem.GetFileAt(qItem.sfx->lumpnum);
	auto reader = readerCopy;
	int size = rl->LumpSize;

	assert(readerCopy);

	if (!rl->Cache) {
		// This resource hasn't been cached yet, we need to read it manually
		if (!reader) {
			// TODO: Set up failure state
			return;
		}

		// This function is designed to be thread safe so we should be okay here
		data = new char[rl->LumpSize];
		size = rl->ReadData(*reader, data);
		createdNewData = true;

		// TODO: THis is currently a memory leak situation because the data created here will never be released
		// TODO: Find out how lump data is eventually freed and use the native process for that
	}
	else {
		data = rl->Cache;
	}


	//std::this_thread::sleep_for(std::chrono::milliseconds(500));

	// Try to interpret the data
	if (size > 8)
	{
		int32_t dmxlen = LittleLong(((int32_t *)data)[1]);

		// If the sound is voc, use the custom loader.
		if (strncmp(data, "Creative Voice File", 19) == 0)
		{
			loadSnd = GSnd->LoadSoundVoc((uint8_t *)data, size);
		}
		// If the sound is raw, just load it as such.
		else if (qItem.sfx->bLoadRAW)
		{
			loadSnd = GSnd->LoadSoundRaw((uint8_t *)data, size, qItem.sfx->RawRate, 1, 8, qItem.sfx->LoopStart);
		}
		// Otherwise, try the sound as DMX format.
		else if (((uint8_t *)data)[0] == 3 && ((uint8_t *)data)[1] == 0 && dmxlen <= size - 8)
		{
			int frequency = LittleShort(((uint16_t *)data)[1]);
			if (frequency == 0) frequency = 11025;
			loadSnd = GSnd->LoadSoundRaw((uint8_t *)data + 8, dmxlen, frequency, 1, 8, qItem.sfx->LoopStart);
		}
		// If that fails, let the sound system try and figure it out.
		else
		{
			loadSnd = GSnd->LoadSound((uint8_t *)data, size);
		}
	}

	threadTime.Unclock();
	mActive.store(false);
}

AudioLoaderQueue::AudioLoaderQueue() {
	updateCycles.Reset();
}


AudioLoaderQueue::~AudioLoaderQueue() {
	clear();
}


void AudioLoaderQueue::queue(sfxinfo_t *sfx, FSoundID soundID, const AudioQueuePlayInfo *playInfo) {
	if (sfx->lumpnum == sfx_empty) {
		//Printf("\c[RED]Tried to queue audio with no associated lump : %s\n", sfx->name.GetChars());
		return;
	}

	// Attempt to fold this play instance into an already queued or loading sound
	// This is to avoid loading the sound twice just because it's already queued
	if (playInfo != NULL) {
		for (AudioQItem &mm : mQueue) {
			// If we already have this item queued, add a playInfo to the queued item
			if (mm.soundID == soundID || mm.sfx == sfx) {
				mm.playInfo.Push(*playInfo);
				return;
			}
		}

		for (AudioLoaderThread *tr : mRunning) {
			// If we already have this item in the process of loading, add a playInfo to the loading item
			if (tr->qItem.soundID == soundID || tr->qItem.sfx == sfx) {
				tr->qItem.playInfo.Push(*playInfo);
				return;
			}
		}
	}

	AudioQItem m = {
		soundID, sfx
	};
	
	if (playInfo != NULL) {
		m.playInfo.Push(*playInfo);
	}

	AActor *a = playInfo && playInfo->type == SOURCE_Actor ? (AActor *)playInfo->source : NULL;


	if (mQueue.Size() == 0 && (int)mRunning.Size() < audio_max_threads) {
		//Printf(TEXTCOLOR_GREEN"STARTING BG THREAD %d : %s : %s\n", soundID, sfx->name.GetChars(), a ? a->GetCharacterName() : "<None>");
		start(m);
		return;
	}

	mQueue.Push(m);

	
	//Printf(TEXTCOLOR_YELLOW"Queued %d : %s : %s\n", soundID, sfx->name.GetChars(), a ? a->GetCharacterName() : "<None>");
}


void AudioLoaderQueue::relinkSound(int sourcetype, const void *from, const void *to, const FVector3 *optpos) {
	for (AudioQItem &mm : mQueue) {
		relinkSound(mm, sourcetype, from, to, optpos);
	}

	// This should be safe, the threads never actually write to qItem or use playInfo, it's just along for the ride
	// TODO: Synchronize access to the qItem just in case changes are made and I forget 
	for (AudioLoaderThread *tt : mRunning) {
		relinkSound(tt->qItem, sourcetype, from, to, optpos);
	}
}

void AudioLoaderQueue::relinkSound(AudioQItem &item, int sourcetype, const void *from, const void *to, const FVector3 *optpos) {
	for (unsigned int x = 0; x < item.playInfo.Size(); x++) {
		AudioQueuePlayInfo &pi = item.playInfo[x];

		if (pi.type == sourcetype && pi.source == from) {
			Printf(TEXTCOLOR_BRICK"Relinking an instance of : %s\n", item.sfx->name.GetChars());

			pi.source = to;

			if (to == NULL) {
				if (!(pi.flags & CHANF_LOOP) && optpos) {
					pi.pos = *optpos;
					pi.type = SOURCE_Unattached;
				}
				else {
					// Delete the play instruction, we can't do a looped sound with no source
					Printf("Cancelling play of looped sound because it no longer has a source: %s\n", item.sfx->name.GetChars());
					item.playInfo.Delete(x);
					x--;
					continue;
				}
			}
		}
	}
}


void AudioLoaderQueue::stopSound(FSoundID soundID) {
	for (AudioQItem &mm : mQueue) {
		if (mm.soundID == soundID) {
			Printf("Stopping play of sound in queue by request: %s\n", mm.sfx->name.GetChars());
			mm.playInfo.Clear();	// Remove all play instances of this sound
		}
	}

	// This should be safe, the threads never actually write to qItem or use playInfo, it's just along for the ride
	// TODO: Synchronize access to the qItem just in case changes are made and I forget 
	for (AudioLoaderThread *tt : mRunning) {
		if (tt->qItem.soundID == soundID) {
			Printf("Stopping play of sound currently being loaded by request: %s\n", tt->qItem.sfx->name.GetChars());
			tt->qItem.playInfo.Clear();
		}
	}
}


void AudioLoaderQueue::stopSound(int channel, FSoundID soundID) {
	for (AudioQItem &mm : mQueue) {
		for (unsigned int x = 0; x < mm.playInfo.Size(); x++) {
			if (mm.playInfo[x].type == SOURCE_None && 
				(mm.soundID == soundID || soundID == -1) && 
				(channel == CHAN_AUTO || channel == mm.playInfo[x].channel)) {
				Printf("Stopping play of sound in queue by chan request: %s\n", mm.sfx->name.GetChars());
				mm.playInfo.Delete(x);
				x--;
			}
		}
	}

	// This should be safe, the threads never actually write to qItem or use playInfo, it's just along for the ride
	// TODO: Synchronize access to the qItem just in case changes are made and I forget 
	for (AudioLoaderThread *tt : mRunning) {
		for (unsigned int x = 0; x < tt->qItem.playInfo.Size(); x++) {
			if (tt->qItem.playInfo[x].type == SOURCE_None &&
				(tt->qItem.soundID == soundID || soundID == -1) && 
				(channel == CHAN_AUTO || channel == tt->qItem.playInfo[x].channel)) {
				Printf("Stopping play of sound being loaded by chan request: %s\n", tt->qItem.sfx->name.GetChars());
				tt->qItem.playInfo.Delete(x);
				x--;
			}
		}
	}
}


void AudioLoaderQueue::stopSound(int sourcetype, const void* actor, int channel, int soundID) {
	AActor *a = sourcetype == SOURCE_Actor ? (AActor *)actor : NULL;
	//Printf(TEXTCOLOR_RED"TRYING TO STOP : %d for %s\n", soundID, a ? a->GetCharacterName() : "<None>");

	for (AudioQItem &mm : mQueue) {
		for (unsigned int x = 0; x < mm.playInfo.Size(); x++) {
			if (mm.playInfo[x].source == actor && 
				mm.playInfo[x].type == sourcetype && 
				(soundID == -1 ? (mm.playInfo[x].channel == channel || channel < 0) : (mm.soundID == soundID))) {
				Printf(TEXTCOLOR_RED"Stopping play of sound in queue by actor:chan request: %s (%s)\n", mm.sfx->name.GetChars(), a ? a->GetCharacterName() : "<None>");
				mm.playInfo.Delete(x);
				x--;
			}
		}
	}

	// This should be safe, the threads never actually write to qItem or use playInfo, it's just along for the ride
	// TODO: Synchronize access to the qItem just in case changes are made and I forget 
	for (AudioLoaderThread *tt : mRunning) {
		for (unsigned int x = 0; x < tt->qItem.playInfo.Size(); x++) {
			if (tt->qItem.playInfo[x].source == actor && 
				tt->qItem.playInfo[x].type == sourcetype && 
				(soundID == -1 ? (tt->qItem.playInfo[x].channel == channel || channel < 0) : (tt->qItem.soundID == soundID))) {
				Printf(TEXTCOLOR_RED"Stopping play of sound being loaded by actor:chan request: %s (%s)\n", tt->qItem.sfx->name.GetChars(), a ? a->GetCharacterName() : "<None>");
				tt->qItem.playInfo.Delete(x);
				x--;
			}
		}
	}
}


void AudioLoaderQueue::stopActorSounds(int sourcetype, const void* actor, int chanmin, int chanmax) {
	AActor *a = (AActor *)actor;
	//Printf(TEXTCOLOR_RED"TRYING TO STOP ALL ACTOR SOUNDS for %s\n", a ? a->GetCharacterName() : "<None>");

	const bool all = (chanmin == 0 && chanmax == 0);

	for (AudioQItem &mm : mQueue) {
		for (unsigned int x = 0; x < mm.playInfo.Size(); x++) {
			if (mm.playInfo[x].source == actor && 
				mm.playInfo[x].type == sourcetype && 
				(all || (mm.playInfo[x].channel >= chanmin && mm.playInfo[x].channel <= chanmax))) {
				Printf(TEXTCOLOR_RED"Stopping play of sound in queue by all actor request: %s (%s)\n", mm.sfx->name.GetChars(), a ? a->GetCharacterName() : "<None>");
				mm.playInfo.Delete(x);
				x--;
			}
		}
	}

	// This should be safe, the threads never actually write to qItem or use playInfo, it's just along for the ride
	// TODO: Synchronize access to the qItem just in case changes are made and I forget 
	for (AudioLoaderThread *tt : mRunning) {
		for (unsigned int x = 0; x < tt->qItem.playInfo.Size(); x++) {
			if (tt->qItem.playInfo[x].source == actor && 
				tt->qItem.playInfo[x].type == sourcetype && 
				(all || (tt->qItem.playInfo[x].channel >= chanmin && tt->qItem.playInfo[x].channel <= chanmax))) {
				Printf(TEXTCOLOR_RED"Stopping play of sound being loaded by all actor request: %s (%s)\n", tt->qItem.sfx->name.GetChars(), a ? a->GetCharacterName() : "<None>");
				tt->qItem.playInfo.Delete(x);
				x--;
			}
		}
	}
}


void AudioLoaderQueue::stopAllSounds() {
	for (AudioQItem &mm : mQueue) {
		Printf("Stopping play of sound in queue by All Sounds request: %s\n", mm.sfx->name.GetChars());
		mm.playInfo.Clear();
	}

	for (AudioLoaderThread *tt : mRunning) {
		Printf("Stopping play of sound currently loading by All Sounds request: %s\n", tt->qItem.sfx->name.GetChars());
		tt->qItem.playInfo.Clear();
	}
}


void AudioLoaderQueue::update() {
	updateCycles.Reset();
	updateCycles.Clock();
	
	// Check if any of the load operations are complete
	for (unsigned int x = 0; x < mRunning.Size(); x++) {
		if (!mRunning[x]->active()) {
			cycle_t integrationTime = cycle_t();
			integrationTime.Reset();
			integrationTime.Clock();

			mRunning[x]->join();

			// Did this thread fail?
			if (mRunning[x]->data == nullptr) { 
				totalFailed++;
				continue;
			}
			
			// Move audio data references
			AudioQItem &i = mRunning[x]->qItem;
			if (!i.sfx->data.isValid()) {
				if (mRunning[x]->getSoundHandle().isValid()) {
					//Printf("Moving loaded audio for : %s\n", i.sfx->name.GetChars());
					i.sfx->data = mRunning[x]->getSoundHandle();
				} else {
					i.sfx->lumpnum = sfx_empty;
					//Printf("Invalid audio data loaded, marking sound : %s\n", i.sfx->name.GetChars());
				}
			}

			totalLoaded++;

			// TODO: Set the cache pointer if there is data loaded and there is not already a cache
			// Not sure if this is necessary as the cache is probably not needed now that the sound is buffered OpenAL


			// Play audio(s) if specified
			// TOODO: Play audio from this data!
			if (i.sfx->data.isValid()) {
				//Printf("Finished loading; now playing : %s (%d copies)\n", i.sfx->name.GetChars(), i.playInfo.Size());

				for (auto snd : i.playInfo) {
					// TODO: Get rolloff info and pass it along properly
					soundEngine->StartSoundER(i.sfx, snd.type, snd.source, snd.pos, snd.vel, snd.channel, snd.flags, i.soundID, snd.orgSoundID, snd.volume, snd.attenuation, &snd.rolloff, snd.pitch, snd.startTime);
				}
			}

			mRunning[x]->totalTime.Unclock();
			integrationTime.Unclock();

			// Create a stat entry for this item
			QStat qs = {
				mRunning[x]->totalTime.TimeMS(),
				mRunning[x]->threadTime.TimeMS(),
				integrationTime.TimeMS()
			};

			mStats.Insert(0, qs);
			mStats.Clamp(100);

			auto prDistaster = mRunning[x];
			mRunning.Delete(x);
			delete prDistaster;
			x--;
		}
	}

	// Start any queued loads
	for (unsigned int x = 0; x < audio_max_threads - mRunning.Size() && mQueue.Size() > 0; x++) {
		start(mQueue[0]);
		mQueue.Delete(0);
	}

	updateCycles.Unclock();

	if (updateCycles.TimeMS() > 1.0) {
		Printf(TEXTCOLOR_YELLOW"AudioLoaderQueue::Warning Update() took more than 1ms (%0.24fms), this might be a little high!\n", updateCycles.TimeMS());
	}
}


void AudioLoaderQueue::clear() {
	mQueue.Clear();

	// We can't abort the current jobs yet, we'll have to let them finish
	for (unsigned int x = 0; x < mRunning.Size(); x++) {
		mRunning[x]->join();
	}

	// Move audio data references
	for (unsigned int x = 0; x < mRunning.Size(); x++) {
		AudioQItem &i = mRunning[x]->qItem;
		if (!i.sfx->data.isValid()) {
			if (mRunning[x]->getSoundHandle().isValid()) {
				//Printf("Moving loaded audio for : %s\n", i.sfx->name.GetChars());
				i.sfx->data = mRunning[x]->getSoundHandle();
			}
			else {
				i.sfx->lumpnum = sfx_empty;
				//Printf("Invalid audio data loaded, marking sound : %s\n", i.sfx->name.GetChars());
			}
		}
	}

	mRunning.Clear();
}


void AudioLoaderQueue::start(AudioQItem &item) {
	//Printf("Starting BG load of : %s\n", item.sfx->name.GetChars());

	mRunning.Push(new AudioLoaderThread(item));
}

void AudioLoaderQueue::startup() {
	Printf("Booting Audio Loader Queue...");
}

void AudioLoaderQueue::shutdown() {
	clear();

	Printf("Shut down Audio Loader Queue.");
}