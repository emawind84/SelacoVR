#include "doomtype.h"
#include "doomstat.h"

#include "p_pooledparticles.h"
#include "g_levellocals.h"
#include "m_argv.h"
#include "vm.h"
#include "types.h"
#include "texturemanager.h"

IMPLEMENT_CLASS(DParticleDefinition, false, false)
DEFINE_FIELD(DParticleDefinition, PoolSize)
DEFINE_FIELD(DParticleDefinition, DefaultTexture)
DEFINE_FIELD(DParticleDefinition, Style)
DEFINE_FIELD(DParticleDefinition, AnimationFrames)
DEFINE_ACTION_FUNCTION(DParticleDefinition, ThinkParticle)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(ParticleData, pooledparticle_t);

	return 0;
}

static void DParticleDefinition_AddAnimationFrame(DParticleDefinition* self, const FString& textureName, int ticks)
{
	if (self->AnimationFrames.Size() >= 255)
	{
		ThrowAbortException(X_OTHER, "Exceeded maximum number of frames for a Particle animation (256) for ParticleDefinition: %s", self->GetClass()->TypeName.GetChars());
	}

	self->AnimationFrames.Push({ TexMan.CheckForTexture(textureName.GetChars(), ETextureType::Any), (uint8_t)ticks });
	self->AnimationLengthInTicks += ticks;
}

DEFINE_ACTION_FUNCTION_NATIVE(DParticleDefinition, AddAnimationFrame, DParticleDefinition_AddAnimationFrame)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);
	PARAM_STRING(textureName);
	PARAM_UINT(ticks);
	DParticleDefinition_AddAnimationFrame(self, textureName, ticks);
	return 0;
}

static void DParticleDefinition_AddAnimationFrames(DParticleDefinition* self, const FString& spriteName, const FString& frames, int ticks)
{
	for (size_t i = 0; i < frames.Len(); i++)
	{
		std::string textureName = std::string(spriteName.GetChars()) + frames[i] + "0";
		DParticleDefinition_AddAnimationFrame(self, textureName, (uint8_t)ticks);
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(DParticleDefinition, AddAnimationFrames, DParticleDefinition_AddAnimationFrames)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);
	PARAM_STRING(spriteName);
	PARAM_STRING(frames);
	PARAM_UINT(ticksPerFrame);

	DParticleDefinition_AddAnimationFrames(self, spriteName, frames, ticksPerFrame);

	return 0;
}

DEFINE_ACTION_FUNCTION(DParticleDefinition, GetAnimationFrameCount)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);

	ACTION_RETURN_INT(self->AnimationFrames.Size());
}

DEFINE_ACTION_FUNCTION(DParticleDefinition, GetAnimationLengthInTicks)
{
	PARAM_SELF_PROLOGUE(DParticleDefinition);

	ACTION_RETURN_INT(self->AnimationLengthInTicks);
}

DParticleDefinition::DParticleDefinition()
	: PoolSize(100)
	, DefaultTexture()
	, Style(STYLE_Normal)
	, AnimationLengthInTicks(0)
{
	// We don't want to save ParticleDefinitions, since the definition could have changed since the game was saved.
	// This way we always use the most up-to-date ParticleDefinition
	ObjectFlags |= OF_Transient;
}

DParticleDefinition::~DParticleDefinition()
{

}

DEFINE_FIELD_X(ParticleData, pooledparticle_t, time);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, lifetime);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, pos);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, vel);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, alpha);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, alphaStep);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, scale);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, scaleStep);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, roll);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, rollStep);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, color);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, texture);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, animFrame);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, animTick);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, flags);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, user1);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, user2);
DEFINE_FIELD_X(ParticleData, pooledparticle_t, user3);

DEFINE_FIELD_X(ParticleAnimFrame, pooledparticleanimframe_t, frame);
DEFINE_FIELD_X(ParticleAnimFrame, pooledparticleanimframe_t, duration);

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

void DParticleDefinition::Init()
{
	IFVIRTUAL(DParticleDefinition, Init)
	{
		VMValue params[] = { this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

void DParticleDefinition::OnCreateParticle(pooledparticle_t* particle)
{
	IFVIRTUAL(DParticleDefinition, OnCreateParticle)
	{
		VMValue params[] = { this, particle };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DParticleDefinition::ThinkParticle(pooledparticle_t* particle)
{
	IFVIRTUAL(DParticleDefinition, ThinkParticle)
	{
		VMValue params[] = { this, particle };

		VMCall(func, params, 2, nullptr, 0);
	}
}

inline pooledparticle_t* NewPooledParticle(FLevelLocals* Level, pooledparticleid particleDefinitionID, bool replace /* = false */)
{
	if (particleDefinitionID >= Level->ParticlePools.Size())
	{
		return nullptr;
	}

	return NewPooledParticle(Level, &Level->ParticlePools[particleDefinitionID], replace);
}

pooledparticle_t* NewPooledParticle(FLevelLocals* Level, particlelevelpool_t* pool, bool replace /* = false */)
{
	pooledparticle_t* result = nullptr;

	// Array's filled up
	if (pool->InactiveParticles == NO_PARTICLE)
	{
		if (replace)
		{
			result = &pool->Particles[pool->OldestParticle];

			// There should be NO_PARTICLE for the oldest's tnext
			if (result->tprev != NO_PARTICLE)
			{
				// tnext: youngest to oldest
				// tprev: oldest to youngest

				// 2nd oldest -> oldest
				pooledparticle_t* nbottom = &pool->Particles[result->tprev];
				nbottom->tnext = NO_PARTICLE;

				// now oldest becomes youngest
				pool->OldestParticle = result->tprev;
				result->tnext = pool->ActiveParticles;
				result->tprev = NO_PARTICLE;
				pool->ActiveParticles = uint32_t(result - pool->Particles.Data());

				// youngest -> 2nd youngest
				pooledparticle_t* ntop = &pool->Particles[result->tnext];
				ntop->tprev = pool->ActiveParticles;
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
	uint32_t current = pool->ActiveParticles;
	result = &pool->Particles[pool->InactiveParticles];
	pool->InactiveParticles = result->tnext;
	result->tnext = current;
	result->tprev = NO_PARTICLE;
	pool->ActiveParticles = uint32_t(result - pool->Particles.Data());

	if (current != NO_PARTICLE) // More than one active particles
	{
		pooledparticle_t* next = &pool->Particles[current];
		next->tprev = pool->ActiveParticles;
	}
	else // Just one active particle
	{
		pool->OldestParticle = pool->ActiveParticles;
	}

	return result;
}

void P_InitPooledParticles(FLevelLocals* Level)
{
	PClass* baseClass = PClass::FindClass("ParticleDefinition");

	Level->ParticlePools.Clear();
	Level->ParticlePoolsByType.Clear();

	for (unsigned int i = 0; i < PClass::AllClasses.Size(); i++)
	{
		PClass* cls = PClass::AllClasses[i];

		if (cls != baseClass && cls->IsDescendantOf(baseClass))
		{
			DParticleDefinition* definition = (DParticleDefinition*)cls->CreateNew();
			definition->Init();

			if (definition->PoolSize > 0)
			{
				int index = Level->ParticlePools.Push({});
				particlelevelpool_t* pool = &Level->ParticlePools[index];

				pool->Definition = definition;
				Level->ParticlePoolsByType.Insert(cls->TypeName.GetIndex(), pool);

				pool->Particles.Resize(pool->Definition->PoolSize);
				P_ClearPooledParticles(pool);
			}
		}
	}
}

void P_ClearPooledParticles(particlelevelpool_t* pool)
{
	int i = 0;
	pool->OldestParticle = NO_PARTICLE;
	pool->ActiveParticles = NO_PARTICLE;
	pool->InactiveParticles = 0;
	for (auto& p : pool->Particles)
	{
		p = {};
		p.tprev = i - 1;
		p.tnext = ++i;
	}
	pool->Particles.Last().tnext = NO_PARTICLE;
	pool->Particles.Data()->tprev = NO_PARTICLE;
}

void P_ClearAllPooledParticles(FLevelLocals* Level)
{
	for (particlelevelpool_t& pool : Level->ParticlePools)
	{
		P_ClearPooledParticles(&pool);
	}
}

// Group particles by subsectors. Because particles are always
// in motion, there is little benefit to caching this information
// from one frame to the next.
// [MC] VisualThinkers hitches a ride here

void P_FindPooledParticleSubsectors(FLevelLocals* Level)
{
	if (Level->PooledParticlesInSubsec.Size() < Level->subsectors.Size())
	{
		Level->PooledParticlesInSubsec.Reserve(Level->subsectors.Size() - Level->PooledParticlesInSubsec.Size());
	}

	pooledparticlessit_t* b2 = &Level->PooledParticlesInSubsec[0];
	for (size_t i = 0; i < Level->PooledParticlesInSubsec.Size(); ++i)
	{
		b2[i] = { NO_PARTICLE, NO_PARTICLE };
	}

	fillshort(&Level->PooledParticlesInSubsec[0], Level->subsectors.Size(), NO_PARTICLE);

	for (uint32_t poolIndex = 0; poolIndex < Level->ParticlePools.Size(); poolIndex++)
	{
		particlelevelpool_t* pool = &Level->ParticlePools[poolIndex];

		for (uint16_t i = pool->ActiveParticles; i != NO_PARTICLE; i = pool->Particles[i].tnext)
		{
			// Try to reuse the subsector from the last portal check, if still valid.
			if (pool->Particles[i].subsector == nullptr) pool->Particles[i].subsector = Level->PointInRenderSubsector(pool->Particles[i].pos);
			int ssnum = pool->Particles[i].subsector->Index();
			pool->Particles[i].snext = Level->PooledParticlesInSubsec[ssnum];
			Level->PooledParticlesInSubsec[ssnum] = pooledparticlessit_t { i, (uint16_t)poolIndex };
		}
	}
}

void P_ThinkAllPooledParticles(FLevelLocals* Level)
{
	for (particlelevelpool_t& pool : Level->ParticlePools)
	{
		P_ThinkPooledParticles(Level, &pool);
	}
}

void P_ThinkPooledParticles(FLevelLocals* Level, particlelevelpool_t* pool)
{
	int i = pool->ActiveParticles;
	DParticleDefinition* definition = pool->Definition;
	pooledparticle_t* particle = nullptr, *prev = nullptr;
	while (i != NO_PARTICLE)
	{
		particle = &pool->Particles[i];
		i = particle->tnext;
		if (Level->isFrozen() && !(particle->flags & SPF_NOTIMEFREEZE))
		{
			prev = particle;
			continue;
		}

		particle->prevpos = particle->pos;

		definition->ThinkParticle(particle);

		if (particle->time++ > particle->lifetime)
		{ // The particle has expired, so free it
			*particle = {};
			if (prev)
				prev->tnext = i;
			else
				pool->ActiveParticles = i;

			if (i != NO_PARTICLE)
			{
				pooledparticle_t* next = &pool->Particles[i];
				next->tprev = particle->tprev;
			}
			particle->tnext = pool->InactiveParticles;
			pool->InactiveParticles = (int)(particle - pool->Particles.Data());
			continue;
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
		particle->pos.Z += particle->vel.Z;

		particle->subsector = Level->PointInRenderSubsector(particle->pos);
		sector_t* s = particle->subsector->sector;

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
		if (animFrameCount > 0)
		{
			const pooledparticleanimframe_t& animFrame = definition->AnimationFrames[particle->animFrame % animFrameCount];
			if (particle->animTick++ > animFrame.duration)
			{
				particle->animFrame++;
				particle->texture = animFrame.frame;
			}
		}

		prev = particle;
	}
}

void P_SpawnPooledParticle(FLevelLocals* Level, particlelevelpool_t* pool, const DVector3& pos, const DVector3& vel, double scale, int flags)
{
	DParticleDefinition* definition = pool->Definition;
	pooledparticle_t* particle = NewPooledParticle(Level, pool, (bool)(flags & SPF_REPLACE));

	if (particle)
	{
		particle->pos = particle->prevpos = pos;
		particle->vel = FVector3(vel);

		particle->time = 0;
		particle->lifetime = 35;
		particle->alpha = 1;
		particle->alphaStep = 0;
		particle->scale = 1;
		particle->scaleStep = 0;
		particle->roll = 0;
		particle->rollStep = 0;
		particle->color = 0xffffff;
		particle->texture = definition->AnimationFrames.Size() ? definition->AnimationFrames[0].frame : definition->DefaultTexture;
		particle->flags = flags;

		definition->OnCreateParticle(particle);
	}
}

void P_LoadParticlePools(FSerializer& arc, FLevelLocals* Level, const char* key)
{
	assert(arc.isReading());

	if (!arc.isReading())
	{
		return;
	}

	// Reinitialize the pools, since even if they've already been initialized, the ParticleDefinitions
	// would have been destroyed by the load, so we need to recreate them.
	P_InitPooledParticles(Level);

	if (arc.BeginArray(key))
	{
		for (unsigned int i = 0; i < arc.ArraySize(); i++)
		{
			if (arc.BeginObject(nullptr))
			{
				FName definitionName;
				arc("definitionname", definitionName);

				particlelevelpool_t** pool = Level->ParticlePoolsByType.CheckKey(definitionName.GetIndex());
				if (pool)
				{
					arc(nullptr, **pool);
				}

				arc.EndObject();
			}
		}

		arc.EndArray();
	}
}

FSerializer& Serialize(FSerializer& arc, const char* key, particlelevelpool_t& lp, particlelevelpool_t* def)
{
	if (arc.isReading() || arc.BeginObject(key))
	{
		if (arc.isWriting())
		{
			arc("definitionname", lp.Definition->GetClass()->TypeName);
		}

		if (arc.BeginArray("particles"))
		{
			if (arc.isWriting())
			{
				// Write out the particles from newest to oldest and then stop, so we only store the particles we *need*
				for (uint16_t i = lp.ActiveParticles; i != NO_PARTICLE; i = lp.Particles[i].tnext)
				{
					pooledparticle_t& p = lp.Particles[i];
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
					pooledparticle_t& p = lp.Particles[i];
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

		if (arc.isWriting())
		{
			arc.EndObject();
		}
	}
	return arc;
}

FSerializer& Serialize(FSerializer& arc, const char* key, pooledparticle_t& p, pooledparticle_t* def)
{
	if (arc.BeginObject(key))
	{
		arc ("time", p.time)
			("lifetime", p.lifetime)
			("prevpos", p.prevpos)
			("pos", p.pos)
			("vel", p.vel)
			("alpha", p.alpha)
			("alphastep", p.alphaStep)
			("scale", p.scale)
			("scalestep", p.scaleStep)
			("roll", p.roll)
			("rollstep", p.rollStep)
			("color", p.color)
			("texture", p.texture)
			("flags", p.flags)
			// Deliberately not saving tprev or tnext, since they're calculated during load
			// Deliberately not saving subsector or snext, since they're calculated every frame.
			.EndObject();
	}
	return arc;
}

