#include "doomtype.h"
#include "doomstat.h"

#include "p_pooledparticles.h"
#include "g_levellocals.h"
#include "m_argv.h"
#include "vm.h"
#include "types.h"
#include "texturemanager.h"

const float DParticleDefinition::INVALID = -99999;
const float DParticleDefinition::BOUNCE_SOUND_ATTENUATION = 1.5f;

IMPLEMENT_CLASS(DParticleDefinition, false, false)
DEFINE_FIELD(DParticleDefinition, DefaultTexture)
DEFINE_FIELD(DParticleDefinition, Style)
DEFINE_FIELD(DParticleDefinition, AnimationFrames)
DEFINE_FIELD(DParticleDefinition, StopSpeed)
DEFINE_FIELD(DParticleDefinition, InheritVelocity)
DEFINE_FIELD(DParticleDefinition, MinLife) DEFINE_FIELD(DParticleDefinition, MaxLife)
DEFINE_FIELD(DParticleDefinition, MinAng) DEFINE_FIELD(DParticleDefinition, MaxAng)
DEFINE_FIELD(DParticleDefinition, MinPitch) DEFINE_FIELD(DParticleDefinition, MaxPitch)
DEFINE_FIELD(DParticleDefinition, MinSpeed) DEFINE_FIELD(DParticleDefinition, MaxSpeed)
DEFINE_FIELD(DParticleDefinition, MinFadeVel) DEFINE_FIELD(DParticleDefinition, MaxFadeVel)
DEFINE_FIELD(DParticleDefinition, MinFadeLife) DEFINE_FIELD(DParticleDefinition, MaxFadeLife)
DEFINE_FIELD(DParticleDefinition, MinScale) DEFINE_FIELD(DParticleDefinition, MaxScale)
DEFINE_FIELD(DParticleDefinition, MinFadeScale) DEFINE_FIELD(DParticleDefinition, MaxFadeScale)
DEFINE_FIELD(DParticleDefinition, MinScaleLife) DEFINE_FIELD(DParticleDefinition, MaxScaleLife)
DEFINE_FIELD(DParticleDefinition, MinScaleVel) DEFINE_FIELD(DParticleDefinition, MaxScaleVel)
DEFINE_FIELD(DParticleDefinition, MinRandomBounces) DEFINE_FIELD(DParticleDefinition, MaxRandomBounces)
DEFINE_FIELD(DParticleDefinition, Drag)
DEFINE_FIELD(DParticleDefinition, MinRoll) DEFINE_FIELD(DParticleDefinition, MaxRoll)
DEFINE_FIELD(DParticleDefinition, MinRollSpeed) DEFINE_FIELD(DParticleDefinition, MaxRollSpeed)
DEFINE_FIELD(DParticleDefinition, RollDamping) DEFINE_FIELD(DParticleDefinition, RollDampingBounce)
DEFINE_FIELD(DParticleDefinition, RestingPitchMin) DEFINE_FIELD(DParticleDefinition, RestingPitchMax) DEFINE_FIELD(DParticleDefinition, RestingPitchSpeed)
DEFINE_FIELD(DParticleDefinition, RestingRollMin) DEFINE_FIELD(DParticleDefinition, RestingRollMax) DEFINE_FIELD(DParticleDefinition, RestingRollSpeed)
DEFINE_FIELD(DParticleDefinition, MaxStepHeight)
DEFINE_FIELD(DParticleDefinition, Gravity)
DEFINE_FIELD(DParticleDefinition, BounceFactor)
DEFINE_FIELD(DParticleDefinition, BounceSound)
DEFINE_FIELD(DParticleDefinition, BounceSoundChance)
DEFINE_FIELD(DParticleDefinition, BounceSoundMinSpeed)
DEFINE_FIELD(DParticleDefinition, BounceSoundPitchMin) DEFINE_FIELD(DParticleDefinition, BounceSoundPitchMax)
DEFINE_FIELD(DParticleDefinition, BounceFudge)
DEFINE_FIELD(DParticleDefinition, MinBounceDeflect) DEFINE_FIELD(DParticleDefinition, MaxBounceDeflect)
DEFINE_FIELD(DParticleDefinition, QualityChanceLow)
DEFINE_FIELD(DParticleDefinition, QualityChanceMed)
DEFINE_FIELD(DParticleDefinition, QualityChanceHigh)
DEFINE_FIELD(DParticleDefinition, QualityChanceUlt)
DEFINE_FIELD(DParticleDefinition, QualityChanceInsane)
DEFINE_FIELD(DParticleDefinition, LifeMultLow)
DEFINE_FIELD(DParticleDefinition, LifeMultMed)
DEFINE_FIELD(DParticleDefinition, LifeMultHigh)
DEFINE_FIELD(DParticleDefinition, LifeMultUlt)
DEFINE_FIELD(DParticleDefinition, LifeMultInsane)
DEFINE_FIELD(DParticleDefinition, Flags)
DEFINE_ACTION_FUNCTION(DParticleDefinition, ThinkParticle)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);
	PARAM_POINTER(ParticleData, particledata_t);

	return 0;
}

DEFINE_ACTION_FUNCTION(DParticleDefinition, OnParticleBounce)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);
	PARAM_POINTER(ParticleData, particledata_t);

	self->OnParticleBounce(ParticleData);

	return 0;
}

DEFINE_ACTION_FUNCTION(DParticleDefinition, OnParticleDeath)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);
	PARAM_POINTER(ParticleData, particledata_t);

	ACTION_RETURN_BOOL(true);
}

static int DParticleDefinition_AddAnimationSequence(DParticleDefinition* self)
{
	if (self->AnimationSequences.Size() >= 255)
	{
		ThrowAbortException(X_OTHER, "Exceeded maximum number of sequences (256) for ParticleDefinition: %s", self->GetClass()->TypeName.GetChars());
	}

	int firstFrame = max((int)self->AnimationFrames.Size() - 1, 0);
	self->AnimationSequences.Push({ (uint8_t)firstFrame, (uint8_t)firstFrame, (uint8_t)self->AnimationSequences.Size() });

	return (int)self->AnimationSequences.size() - 1;
}

DEFINE_ACTION_FUNCTION_NATIVE(DParticleDefinition, AddAnimationSequence, DParticleDefinition_AddAnimationSequence)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);

	int sequenceIndex = DParticleDefinition_AddAnimationSequence(self);
	
	ACTION_RETURN_INT(sequenceIndex);
}

static void CheckSequence(DParticleDefinition* self, int sequence)
{
	if (sequence > self->AnimationSequences.size())
	{
		ThrowAbortException(X_OTHER, "Animation sequence %d does not exist (Did you call forget to call AddAnimationSequence?)", sequence);
	}
}

static void DParticleDefinition_AddAnimationFrame(DParticleDefinition* self, int sequence, const FString& textureName, int ticks)
{
	CheckSequence(self, sequence);

	if (self->AnimationFrames.Size() >= 255)
	{
		ThrowAbortException(X_OTHER, "Exceeded maximum number of frames for a Particle animation (256) for ParticleDefinition: %s", self->GetClass()->TypeName.GetChars());
	}

	particleanimsequence_t& animSequence = self->AnimationSequences[sequence];
	FTextureID frame = TexMan.CheckForTexture(textureName.GetChars(), ETextureType::Any);

	// Don't add invalid frames
	if (!frame.isValid())
	{
		return;
	}

	self->AnimationFrames.Insert(animSequence.endFrame, { frame, (uint8_t)ticks, (uint8_t)sequence });

	animSequence.endFrame++;
	animSequence.lengthInTicks += ticks;

	// Shift the start and endframes down for any sequences that follow this one
	for (size_t i = sequence + 1; i < self->AnimationSequences.size(); i++)
	{
		self->AnimationSequences[i].startFrame++;
		self->AnimationSequences[i].endFrame++;
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(DParticleDefinition, AddAnimationFrame, DParticleDefinition_AddAnimationFrame)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);
	PARAM_UINT(sequence);
	PARAM_STRING(textureName);
	PARAM_UINT(ticks);
	DParticleDefinition_AddAnimationFrame(self, sequence, textureName, ticks);
	return 0;
}

static void DParticleDefinition_AddAnimationFrames(DParticleDefinition* self, int sequence, const FString& spriteName, const FString& frames, int ticks)
{
	for (size_t i = 0; i < frames.Len(); i++)
	{
		std::string textureName = std::string(spriteName.GetChars()) + frames[i] + "0";
		DParticleDefinition_AddAnimationFrame(self, sequence, textureName, (uint8_t)ticks);
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(DParticleDefinition, AddAnimationFrames, DParticleDefinition_AddAnimationFrames)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);
	PARAM_UINT(sequence);
	PARAM_STRING(spriteName);
	PARAM_STRING(frames);
	PARAM_UINT(ticksPerFrame);

	DParticleDefinition_AddAnimationFrames(self, sequence, spriteName, frames, ticksPerFrame);

	return 0;
}

static int DParticleDefinition_GetAnimationFrameCount(DParticleDefinition* self, int sequence)
{
	CheckSequence(self, sequence);

	particleanimsequence_t& animSequence = self->AnimationSequences[sequence];
	return animSequence.endFrame - animSequence.startFrame;
}

DEFINE_ACTION_FUNCTION_NATIVE(DParticleDefinition, GetAnimationFrameCount, DParticleDefinition_GetAnimationFrameCount)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);
	PARAM_UINT(sequence);

	ACTION_RETURN_INT(DParticleDefinition_GetAnimationFrameCount(self, sequence));
}

static int DParticleDefinition_GetAnimationLengthInTicks(DParticleDefinition* self, int sequence)
{
	CheckSequence(self, sequence);

	return self->AnimationSequences[sequence].lengthInTicks;
}

DEFINE_ACTION_FUNCTION_NATIVE(DParticleDefinition, GetAnimationLengthInTicks, DParticleDefinition_GetAnimationLengthInTicks)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);
	PARAM_UINT(sequence);

	ACTION_RETURN_INT(DParticleDefinition_GetAnimationLengthInTicks(self, sequence));
}

DEFINE_ACTION_FUNCTION(DParticleDefinition, GetAnimationSequenceCount)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);

	ACTION_RETURN_INT((int)self->AnimationSequences.size());
}

DEFINE_ACTION_FUNCTION(DParticleDefinition, GetAnimationStartFrame)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);
	PARAM_UINT(sequence);

	CheckSequence(self, sequence);

	ACTION_RETURN_INT(self->AnimationSequences[sequence].startFrame);
}

DEFINE_ACTION_FUNCTION(DParticleDefinition, GetAnimationEndFrame)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);
	PARAM_UINT(sequence);

	CheckSequence(self, sequence);

	ACTION_RETURN_INT(self->AnimationSequences[sequence].endFrame);
}

DParticleDefinition::DParticleDefinition()
	: DefaultTexture()
	, Style(STYLE_Normal)
{
	// We don't want to save ParticleDefinitions, since the definition could have changed since the game was saved.
	// This way we always use the most up-to-date ParticleDefinition
	ObjectFlags |= OF_Transient;
}

DParticleDefinition::~DParticleDefinition()
{

}

DEFINE_FIELD_X(ParticleData, particledata_t, life);
DEFINE_FIELD_X(ParticleData, particledata_t, startLife);
DEFINE_FIELD_X(ParticleData, particledata_t, pos);
DEFINE_FIELD_X(ParticleData, particledata_t, vel);
DEFINE_FIELD_X(ParticleData, particledata_t, alpha);
DEFINE_FIELD_X(ParticleData, particledata_t, alphaStep);
DEFINE_FIELD_X(ParticleData, particledata_t, scale);
DEFINE_FIELD_X(ParticleData, particledata_t, scaleStep);
DEFINE_FIELD_X(ParticleData, particledata_t, roll);
DEFINE_FIELD_X(ParticleData, particledata_t, rollStep);
DEFINE_FIELD_X(ParticleData, particledata_t, pitch);
DEFINE_FIELD_X(ParticleData, particledata_t, pitchStep);
DEFINE_FIELD_X(ParticleData, particledata_t, bounces);
DEFINE_FIELD_X(ParticleData, particledata_t, maxBounces);
DEFINE_FIELD_X(ParticleData, particledata_t, floorz);
DEFINE_FIELD_X(ParticleData, particledata_t, ceilingz);
DEFINE_FIELD_X(ParticleData, particledata_t, color);
DEFINE_FIELD_X(ParticleData, particledata_t, texture);
DEFINE_FIELD_X(ParticleData, particledata_t, animFrame);
DEFINE_FIELD_X(ParticleData, particledata_t, animTick);
DEFINE_FIELD_X(ParticleData, particledata_t, invalidateTicks);
DEFINE_FIELD_X(ParticleData, particledata_t, sleepFor);
DEFINE_FIELD_X(ParticleData, particledata_t, flags);
DEFINE_FIELD_X(ParticleData, particledata_t, user1);
DEFINE_FIELD_X(ParticleData, particledata_t, user2);
DEFINE_FIELD_X(ParticleData, particledata_t, user3);

DEFINE_FIELD_X(ParticleAnimFrame, particleanimframe_t, frame);
DEFINE_FIELD_X(ParticleAnimFrame, particleanimframe_t, duration);

int ParticleRandom(int min, int max)
{
	if (min == max)
	{
		return min;
	}

	if (min > max)
	{
		std::swap(min, max);
	}

	return min + M_Random(max - min);
}

float ParticleRandom(float min, float max)
{
	if (min == max)
	{
		return min;
	}

	if (min > max)
	{
		std::swap(min, max);
	}

	return (float)(min + M_Random.GenRand_Real1() * (max - min));
}

void DParticleDefinition::CallInit()
{
	IFVIRTUAL(DParticleDefinition, Init)
	{
		VMValue params[] = { this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

void DParticleDefinition::CallOnCreateParticle(particledata_t* particle, AActor* refActor)
{
	IFVIRTUAL(DParticleDefinition, OnCreateParticle)
	{
		VMValue params[] = { this, particle, refActor };
		VMCall(func, params, 3, nullptr, 0);
	}
}

bool DParticleDefinition::CallOnParticleDeath(particledata_t* particle)
{
	int result = true;

	IFVIRTUAL(DParticleDefinition, OnParticleDeath)
	{
		VMValue params[] = { this, particle };
		VMReturn ret(&result);

		VMCall(func, params, 2, &ret, 1);
	}

	return result;
}

void DParticleDefinition::CallThinkParticle(particledata_t* particle)
{
	IFVIRTUAL(DParticleDefinition, ThinkParticle)
	{
		VMValue params[] = { this, particle };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DParticleDefinition::CallOnParticleBounce(particledata_t* particle)
{
	IFVIRTUAL(DParticleDefinition, OnParticleBounce)
	{
		VMValue params[] = { this, particle };
		VMCall(func, params, 2, nullptr, 0);
	}
	else
	{
		OnParticleBounce(particle);
	}
}

void DParticleDefinition::OnParticleBounce(particledata_t* particle)
{
	// Roll speed damping
	particle->rollStep *= 1.0f - RollDampingBounce;

	float speed = (float)particle->vel.Length();

	// Clean up if we reached the bounce limit
	if (particle->maxBounces >= 0) 
	{
		if (particle->bounces++ >= particle->maxBounces)
		{
			if (HasFlag(PDF_FADEATBOUNCELIMIT) && particle->bounces - 1 == particle->maxBounces)
			{
				// Shortcut life
				if (particle->life > MaxFadeLife)
				{
					particle->life = MaxFadeLife;
				}
			}
			else if (!HasFlag(PDF_FADEATBOUNCELIMIT))
			{
				CleanupParticle(particle);
				return;
			}
		}
	}

	if (speed < StopSpeed)
	{
		particle->vel.Zero();

		if (HasFlag(PDF_KILLSTOP))
		{
			CleanupParticle(particle);
			return;
		}
		else
		{
			RestParticle(particle);
		}
	}
	else if (BounceSound && speed > BounceSoundMinSpeed && ParticleRandom(0.0f, 1.0f) <= BounceSoundChance)
	{
		S_SoundPitch(Level, particle->pos, CHAN_AUTO, 0, FSoundID::fromInt(BounceSound), 1.0f, BOUNCE_SOUND_ATTENUATION, ParticleRandom(BounceSoundPitchMin, BounceSoundPitchMax));
	}

	if (RestingRollSpeed >= 0 && speed <= RestingRollSpeed) 
	{
		particle->roll += (((round(particle->roll / 180.0f) * 180.0f) + ParticleRandom(RestingRollMin, RestingRollMax)) - particle->roll) * 0.65f;
	}

	if (RestingPitchSpeed >= 0 && speed <= RestingPitchSpeed) 
	{
		particle->pitch += (((round(particle->pitch / 180.0f) * 180.0f) + ParticleRandom(RestingPitchMin, RestingPitchMax)) - particle->pitch) * 0.65f;
	}
}

int DParticleDefinition::GetParticleLimit()
{
	int numParticles = 1000; // Probably will never be hit, but let's just set it to some safe number just in case

	IFVM(ParticleDefinition, GetParticleLimit)
	{
		VMReturn ret(&numParticles);
		VMCall(func, nullptr, 0, &ret, 1);
	}

	return numParticles;
}

int DParticleDefinition::GetParticleCullLimit()
{
	int numParticles = 500; // Probably will never be hit, but let's just set it to some safe number just in case

	IFVM(ParticleDefinition, GetParticleCullLimit)
	{
		VMReturn ret(&numParticles);
		VMCall(func, nullptr, 0, &ret, 1);
	}

	return numParticles;
}

void DParticleDefinition::RestParticle(particledata_t* particle)
{
	particle->SetFlag(DPF_ATREST);

	if (HasFlag(PDF_ROLLSTOP)) 
	{
		particle->rollStep = 0;
		particle->pitchStep = 0;
	}

	if (RestingRollSpeed >= 0) 
	{
		particle->roll = (round(particle->roll / 180.0f) * 180.0f) + ParticleRandom(RestingRollMin, RestingRollMax);
	}

	if (RestingPitchSpeed >= 0) 
	{
		particle->pitch = (round(particle->pitch / 180.0f) * 180.0f) + ParticleRandom(RestingPitchMin, RestingPitchMax);
	}

	if (HasFlag(PDF_SLEEPSTOP) && particle->life > 10) 
	{
		int lifeLeft = particle->life - (MinFadeLife + MaxFadeLife);

		if (lifeLeft > 0) 
		{
			particle->sleepFor = lifeLeft;
			particle->life = MinFadeLife + MaxFadeLife;
		}
	}
}

// Clean up this particle as quickly as possible, triggered by the particle limiter
void DParticleDefinition::CullParticle(particledata_t* particle)
{
	if (HasFlag(PDF_LIFEFADE) && particle->life > 0)
	{
		if (particle->life > MaxFadeLife || particle->life > TICRATE)
		{
			// Impose a practical limit of 1 second for fading out
			particle->life = min(TICRATE, MaxFadeLife);
			particle->sleepFor = 0;    // Just in case we slept for whatever reason
		}
		// Otherwise this particle should be already on it's way out
	}
	else
	{
		particle->SetFlag(DPF_DESTROYED);
	}
}

void DParticleDefinition::CleanupParticle(particledata_t* particle)
{
	particle->SetFlag(DPF_NOPROCESS);

	if (!CallOnParticleDeath(particle))
	{
		// Mark this as destroyed so we can destroy it the next update
		particle->SetFlag(DPF_DESTROYED);
	}
}

particledata_t* NewDefinedParticle(FLevelLocals* Level, DParticleDefinition* definition, bool replace /* = false */)
{
	particledata_t* result = nullptr;

	particlelevelpool_t& pool = Level->DefinedParticlePool;

	// Array's filled up
	if (pool.InactiveParticles == NO_PARTICLE)
	{
		if (replace)
		{
			result = &pool.Particles[pool.OldestParticle];
			result->definition = definition;

			// There should be NO_PARTICLE for the oldest's tnext
			if (result->tprev != NO_PARTICLE)
			{
				// tnext: youngest to oldest
				// tprev: oldest to youngest

				// 2nd oldest -> oldest
				particledata_t* nbottom = &pool.Particles[result->tprev];
				nbottom->tnext = NO_PARTICLE;

				// now oldest becomes youngest
				pool.OldestParticle = result->tprev;
				result->tnext = pool.ActiveParticles;
				result->tprev = NO_PARTICLE;
				pool.ActiveParticles = uint32_t(result - pool.Particles.Data());

				// youngest -> 2nd youngest
				particledata_t* ntop = &pool.Particles[result->tnext];
				ntop->tprev = pool.ActiveParticles;
			}
			// [MC] Future proof this by resetting everything when replacing a particle.
			auto tnext = result->tnext;
			auto tprev = result->tprev;
			*result = {};
			result->tnext = tnext;
			result->tprev = tprev;
		}
		return result;
	}

	// Array isn't full.
	uint32_t current = pool.ActiveParticles;
	result = &pool.Particles[pool.InactiveParticles];
	result->definition = definition;
	pool.InactiveParticles = result->tnext;
	result->tnext = current;
	result->tprev = NO_PARTICLE;
	pool.ActiveParticles = uint32_t(result - pool.Particles.Data());

	if (current != NO_PARTICLE) // More than one active particles
	{
		particledata_t* next = &pool.Particles[current];
		next->tprev = pool.ActiveParticles;
	}
	else // Just one active particle
	{
		pool.OldestParticle = pool.ActiveParticles;
	}

	return result;
}

void P_InitParticleDefinitions(FLevelLocals* Level)
{
	PClass* baseClass = PClass::FindClass("ParticleDefinition");

	for (unsigned int i = 0; i < PClass::AllClasses.Size(); i++)
	{
		PClass* cls = PClass::AllClasses[i];

		if (cls != baseClass && cls->IsDescendantOf(baseClass))
		{
			DParticleDefinition* definition = (DParticleDefinition*)cls->CreateNew();
			definition->Level = Level;
			definition->CallInit();

			Level->ParticleDefinitionsByType.Insert(cls->TypeName.GetIndex(), definition);
		}
	}

	int numParticles = DParticleDefinition::GetParticleLimit();

	Level->DefinedParticlePool.Particles.Resize(numParticles);
	P_ClearAllDefinedParticles(Level);
}

void P_ClearAllDefinedParticles(FLevelLocals* Level)
{
	particlelevelpool_t& pool = Level->DefinedParticlePool;

	int i = 0;
	pool.OldestParticle = NO_PARTICLE;
	pool.ActiveParticles = NO_PARTICLE;
	pool.InactiveParticles = 0;
	for (auto& p : pool.Particles)
	{
		p = {};
		p.tprev = i - 1;
		p.tnext = ++i;
	}
	pool.Particles.Last().tnext = NO_PARTICLE;
	pool.Particles.Data()->tprev = NO_PARTICLE;
}

// Group particles by subsectors. Because particles are always
// in motion, there is little benefit to caching this information
// from one frame to the next.
// [MC] VisualThinkers hitches a ride here

void P_FindDefinedParticleSubsectors(FLevelLocals* Level)
{
	if (Level->DefinedParticlesInSubsec.Size() < Level->subsectors.Size())
	{
		Level->DefinedParticlesInSubsec.Reserve(Level->subsectors.Size() - Level->DefinedParticlesInSubsec.Size());
	}

	uint16_t* b2 = &Level->DefinedParticlesInSubsec[0];
	for (size_t i = 0; i < Level->DefinedParticlesInSubsec.Size(); ++i)
	{
		b2[i] = NO_PARTICLE;
	}

	fillshort(&Level->DefinedParticlesInSubsec[0], Level->subsectors.Size(), NO_PARTICLE);

	particlelevelpool_t& pool = Level->DefinedParticlePool;

	for (uint16_t i = pool.ActiveParticles; i != NO_PARTICLE; i = pool.Particles[i].tnext)
	{
		// Try to reuse the subsector from the last portal check, if still valid.
		if (pool.Particles[i].subsector == nullptr) pool.Particles[i].subsector = Level->PointInRenderSubsector(pool.Particles[i].pos);
		int ssnum = pool.Particles[i].subsector->Index();
		pool.Particles[i].snext = Level->DefinedParticlesInSubsec[ssnum];
		Level->DefinedParticlesInSubsec[ssnum] = i;
	}
}

bool P_DestroyDefinedParticle(FLevelLocals* Level, int particleIndex)
{
	particlelevelpool_t& pool = Level->DefinedParticlePool;
	particledata_t& particle = pool.Particles[particleIndex];

	if (!particle.HasFlag(DPF_DESTROYED) && !particle.definition->CallOnParticleDeath(&particle))
	{
		return false;
	}

	if (particle.tprev != NO_PARTICLE)
		pool.Particles[particle.tprev].tnext = particle.tnext;
	else
		pool.ActiveParticles = particle.tnext;

	if (particle.tnext != NO_PARTICLE)
	{
		particledata_t& next = pool.Particles[particle.tnext];
		next.tprev = particle.tprev;
	}

	particle = {};
	particle.tnext = pool.InactiveParticles;
	pool.InactiveParticles = particleIndex;

	return true;
}

void P_ThinkDefinedParticles(FLevelLocals* Level)
{
	particlelevelpool_t* pool = &Level->DefinedParticlePool;

	int particleCount = 0;
	int cullLimit = DParticleDefinition::GetParticleCullLimit();

	int i = pool->ActiveParticles;
	particledata_t* particle = nullptr;
	while (i != NO_PARTICLE)
	{
		particle = &pool->Particles[i];
		int particleIndex = i;
		i = particle->tnext;
		if (Level->isFrozen() && !(particle->flags & SPF_NOTIMEFREEZE))
		{
			continue;
		}

		if (particle->sleepFor > 0)
		{
			particle->sleepFor--;
			continue;
		}

		particle->prevpos = particle->pos;

		DParticleDefinition* definition = particle->definition;
		definition->CallThinkParticle(particle);

		if (particle->life > 0)
		{
			particle->life--;
		}

		if ((particle->life == 0) || particle->HasFlag(DPF_DESTROYED))
		{ // The particle has expired, so free it
			if (P_DestroyDefinedParticle(Level, particleIndex))
			{
				continue;
			}
		}

		particle->alpha += particle->alphaStep;
		particle->scale += particle->scaleStep;
		particle->roll += particle->rollStep;

		// Handle crossing a line portal
		double movex = (particle->pos.X - particle->prevpos.X) + particle->vel.X;
		double movey = (particle->pos.Y - particle->prevpos.Y) + particle->vel.Y;
		DVector2 newxy = Level->GetPortalOffsetPosition(particle->prevpos.X, particle->prevpos.Y, movex, movey);
		particle->pos.X = newxy.X;
		particle->pos.Y = newxy.Y;

		particle->subsector = Level->PointInRenderSubsector(particle->pos);
		sector_t* s = particle->subsector->sector;

		if (definition->Gravity != 0)
		{
			particle->vel *= 1.0f - definition->Drag;

			float gravity = (float)(Level->gravity * s->gravity * definition->Gravity * 0.00125);

			// TODO: If we need water checks, we're going to have to replicate AActor::FallAndSink
			particle->vel.Z -= gravity;
		}

		particle->pos.Z += particle->vel.Z;

		particle->floorz = (float)s->floorplane.ZatPoint(particle->pos);
		particle->ceilingz = (float)s->ceilingplane.ZatPoint(particle->pos);

		// Handle crossing a sector portal.
		if (!s->PortalBlocksMovement(sector_t::ceiling))
		{
			if (particle->pos.Z > s->GetPortalPlaneZ(sector_t::ceiling))
			{
				particle->pos += s->GetPortalDisplacement(sector_t::ceiling);
				particle->subsector = NULL;
			}
		}
		else if (!s->PortalBlocksMovement(sector_t::floor))
		{
			if (particle->pos.Z < s->GetPortalPlaneZ(sector_t::floor))
			{
				particle->pos += s->GetPortalDisplacement(sector_t::floor);
				particle->subsector = NULL;
			}
		}

		uint8_t animFrameCount = (uint8_t)definition->AnimationFrames.Size();
		if (definition->AnimationSequences.size() > 0 && particle->animFrame < animFrameCount)
		{
			const particleanimframe_t& animFrame = definition->AnimationFrames[particle->animFrame];
			
			uint8_t sequenceIndex = animFrame.sequence;
			const particleanimsequence_t& sequence = definition->AnimationSequences[sequenceIndex];

			if (++particle->animTick >= animFrame.duration)
			{
				particle->animTick = 0;
				particle->animFrame++;

				// Loop the animation if it's finished
				if (particle->animFrame >= sequence.endFrame)
				{
					particle->animFrame = sequence.startFrame;
				}

				particle->texture = definition->AnimationFrames[particle->animFrame].frame;
			}
		}

		if (!particle->HasFlag(DPF_NOPROCESS))
		{
			if (definition->Flags & PDF_BOUNCEONFLOORS)
			{
				bool bounced = false;

				if (particle->pos.Z < particle->floorz && particle->vel.Z < 0)
				{
					if (particle->pos.Z - particle->floorz >= -definition->MaxStepHeight)
					{
						particle->pos.Z = particle->floorz;
						particle->vel.Z *= -(definition->BounceFactor * ParticleRandom(1.0f - definition->BounceFudge, 1.0f));
						bounced = true;
						particle->invalidateTicks = 0;
					}
					else
					{
						particle->vel.Z = 0;
						particle->invalidateTicks++;
					}

					particle->vel.X *= definition->BounceFactor;
					particle->vel.Y *= definition->BounceFactor;

					FVector2 deflected = particle->vel.XY().Rotated(ParticleRandom(definition->MinBounceDeflect, definition->MaxBounceDeflect));
					particle->vel.X = deflected.X;
					particle->vel.Y = deflected.Y;
				}
				else if (particle->pos.Z > particle->ceilingz && particle->vel.Z > 0)
				{
					if (particle->pos.Z - particle->ceilingz <= -definition->MaxStepHeight)
					{
						particle->pos.Z = particle->floorz;
						particle->vel.Z *= -(definition->BounceFactor * ParticleRandom(1.0f - definition->BounceFudge, 1.0f));
						bounced = true;
						particle->invalidateTicks = 0;
					}
					else
					{
						particle->vel.Z = 0;
						particle->invalidateTicks++;
					}

					particle->vel.X *= definition->BounceFactor;
					particle->vel.Y *= definition->BounceFactor;

					FVector2 deflected = particle->vel.XY().Rotated(ParticleRandom(definition->MinBounceDeflect, definition->MaxBounceDeflect));
					particle->vel.X = deflected.X;
					particle->vel.Y = deflected.Y;
				}
				else
				{
					particle->invalidateTicks = 0;
				}

				if (bounced)
				{
					definition->CallOnParticleBounce(particle);
				}

				if (particle->invalidateTicks > 5 && P_DestroyDefinedParticle(Level, particleIndex))
				{
					continue;
				}
			}
		}

		if (particle->HasFlag(DPF_DESTROYED))
		{
			P_DestroyDefinedParticle(Level, particleIndex);
			continue;
		}

		if (particleCount > cullLimit)
		{
			definition->CullParticle(particle);
		}

		particleCount++;
	}
}

void P_SpawnDefinedParticle(FLevelLocals* Level, DParticleDefinition* definition, const DVector3& pos, const DVector3& vel, double scale, int flags, AActor* refActor)
{
	particledata_t* particle = NewDefinedParticle(Level, definition, (bool)(flags & SPF_REPLACE));

	if (particle)
	{
		particle->pos = particle->prevpos = pos;
		particle->vel = FVector3(vel);

		if (definition->MinSpeed != DParticleDefinition::INVALID && definition->MaxSpeed != DParticleDefinition::INVALID)
		{
			particle->vel = particle->vel.Unit() * ParticleRandom(definition->MinSpeed, definition->MaxSpeed);
		}

		particle->subsector = Level->PointInRenderSubsector(particle->pos);
		sector_t* s = particle->subsector->sector;

		particle->floorz = (float)s->floorplane.ZatPoint(particle->pos);
		particle->ceilingz = (float)s->ceilingplane.ZatPoint(particle->pos);

		particle->startLife = particle->life = ParticleRandom(definition->MinLife, definition->MaxLife);
		particle->alpha = 1;
		particle->alphaStep = 0;
		particle->scale.X = definition->MinScale.X == -1 && definition->MaxScale.X == -1 ? 1 : ParticleRandom(definition->MinScale.X, definition->MaxScale.X);
		particle->scale.Y = definition->MinScale.Y == -1 && definition->MaxScale.Y == -1 ? 1 : ParticleRandom(definition->MinScale.Y, definition->MaxScale.Y);
		particle->scaleStep = FVector2(0, 0);
		particle->roll = ParticleRandom(definition->MinRoll, definition->MaxRoll);
		particle->rollStep = 0;
		particle->pitch = ParticleRandom(definition->MinPitch, definition->MaxPitch);
		particle->pitchStep = 0;
		particle->bounces = 0;
		particle->maxBounces = ParticleRandom(definition->MinRandomBounces, definition->MaxRandomBounces);
		particle->invalidateTicks = 0;
		particle->color = 0xffffff;
		particle->texture = definition->AnimationFrames.Size() ? definition->AnimationFrames[0].frame : definition->DefaultTexture;
		particle->flags = flags;

		definition->CallOnCreateParticle(particle, refActor);

		// If we've set any roll values, make sure the roll flag is set
		if (particle->roll != 0 || particle->rollStep != 0)
		{
			particle->flags |= SPF_ROLL;
		}
	}
}

static FLevelLocals* ParticleDefinitionLoadingLevel = nullptr;

void P_LoadDefinedParticles(FSerializer& arc, FLevelLocals* Level, const char* key)
{
	assert(arc.isReading());

	if (!arc.isReading())
	{
		return;
	}

	// Reinitialize the pools, since even if they've already been initialized, the ParticleDefinitions
	// would have been destroyed by the load, so we need to recreate them.
	P_InitParticleDefinitions(Level);

	particlelevelpool_t& pool = Level->DefinedParticlePool;

	ParticleDefinitionLoadingLevel = Level;
	Serialize(arc, key, pool, &pool);
	ParticleDefinitionLoadingLevel = nullptr;

	// Go through all the particles and destroy any that are lacking a definition
	int i = pool.ActiveParticles;
	while (i != NO_PARTICLE)
	{
		particledata_t& particle = pool.Particles[i];
	
		int particleIndex = i;
		i = particle.tnext;
		
		if (!particle.definition)
		{
			P_DestroyDefinedParticle(Level, particleIndex);
		}
	}
}


FSerializer& Serialize(FSerializer& arc, const char* key, particlelevelpool_t& lp, particlelevelpool_t* def)
{
	if (arc.BeginObject(key))
	{
		if (arc.BeginArray("particles"))
		{
			if (arc.isWriting())
			{
				// Write out the particles from newest to oldest and then stop, so we only store the particles we *need*
				for (uint16_t i = lp.ActiveParticles; i != NO_PARTICLE; i = lp.Particles[i].tnext)
				{
					particledata_t& p = lp.Particles[i];
					arc(nullptr, p);
				}
			}
			else
			{
				unsigned int count = min(arc.ArraySize(), lp.Particles.Size());

				lp.OldestParticle = NO_PARTICLE;
				lp.ActiveParticles = count > 0 ? 0 : NO_PARTICLE;
				lp.InactiveParticles = 0;

				for (unsigned int i = 0; i < count; i++)
				{
					particledata_t& p = lp.Particles[i];
					arc(nullptr, p);

					// Since the particles are stored newest-to-oldest, we can figure out the tprev and tnext
					p.tprev = i > 0 ? i - 1 : NO_PARTICLE;
					p.tnext = i < count - 1 ? i + 1 : NO_PARTICLE;

					lp.OldestParticle = i;
					lp.InactiveParticles = p.tnext;
				}
			}

			arc.EndArray();
		}

		arc.EndObject();
	}
	return arc;
}

FSerializer& Serialize(FSerializer& arc, const char* key, particledata_t& p, particledata_t* def)
{
	if (arc.BeginObject(key))
	{
		if (arc.isWriting())
		{
			FName definitionName;
			if (p.definition)
			{
				definitionName = p.definition->GetClass()->TypeName;
			}

			arc("definition", definitionName);
		}
		else
		{
			FName definitionName;
			arc("definition", definitionName);

			if (DParticleDefinition** definition = ParticleDefinitionLoadingLevel->ParticleDefinitionsByType.CheckKey(definitionName.GetIndex()))
			{
				p.definition = *definition;
			}
			else
			{
				p.definition = nullptr;
			}
		}

		arc ("life", p.life)
			("startLife", p.startLife)
			("prevpos", p.prevpos)
			("pos", p.pos)
			("vel", p.vel)
			("alpha", p.alpha)
			("alphastep", p.alphaStep)
			("scale", p.scale)
			("scalestep", p.scaleStep)
			("roll", p.roll)
			("rollstep", p.rollStep)
			("pitch", p.pitch)
			("pitchstep", p.pitchStep)
			("bounces", p.bounces)
			("maxbounces", p.maxBounces)
			("floorz", p.floorz)
			("ceilingz", p.ceilingz)
			("color", p.color)
			("texture", p.texture)
			("animframe", p.animFrame)
			("animTick", p.animTick)
			("invalidateTicks", p.invalidateTicks)
			("flags", p.flags)
			("user1", p.user1)
			("user2", p.user2)
			("user3", p.user3)
			("user4", p.user4)
			// Deliberately not saving tprev or tnext, since they're calculated during load
			// Deliberately not saving subsector or snext, since they're calculated every frame.
			.EndObject();
	}
	return arc;
}

