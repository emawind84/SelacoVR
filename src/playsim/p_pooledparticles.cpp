#include "doomtype.h"
#include "doomstat.h"

#include "p_pooledparticles.h"
#include "g_levellocals.h"
#include "m_argv.h"
#include "vm.h"
#include "types.h"
#include "texturemanager.h"

CVAR(Bool, r_pooledparticles, true, 0);

/*
IMPLEMENT_CLASS(DParticleDefinition, false, false)
DEFINE_FIELD(DParticleDefinition, PoolSize)
DEFINE_FIELD(DParticleDefinition, Lifetime)
DEFINE_FIELD(DParticleDefinition, LifetimeRange)
DEFINE_FIELD(DParticleDefinition, Acceleration)
DEFINE_FIELD(DParticleDefinition, Scale)
DEFINE_FIELD(DParticleDefinition, ScaleStep)
DEFINE_FIELD(DParticleDefinition, Alpha)
DEFINE_FIELD(DParticleDefinition, FadeStep)
DEFINE_FIELD(DParticleDefinition, Roll)
DEFINE_FIELD(DParticleDefinition, RollVel)
DEFINE_FIELD(DParticleDefinition, RollAcc)
DEFINE_FIELD(DParticleDefinition, Color)
DEFINE_FIELD(DParticleDefinition, Texture)
DEFINE_FIELD(DParticleDefinition, Style)

DParticleDefinition::DParticleDefinition()
	: PoolSize(100)
	, Lifetime(35)
	, LifetimeRange(0)
	, Scale(1)
	, ScaleStep(0)
	, Alpha(1)
	, FadeStep(0)
	, Roll(0)
	, RollVel(0)
	, RollAcc(0)
	, Color(0xffffff)
	, Style(STYLE_Normal)
{

}
*/

#define FADEFROMTTL(a)	(1.f/(a))

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

// HACK: This is probably nasty, talk to Cock about whether there's a better way of doing this
template<typename T>
void GetDefaultValue(PClass* cls, FName name, T* outValue)
{
	uint8_t* addr = cls->Defaults;

	PField* pField = dyn_cast<PField>(cls->FindSymbol(name, true));
	assert(pField && "Couldn't find field for default value");
	assert(pField->Type->Size == sizeof(T) && "Field is incorrect size");

	addr += pField->Offset;

	assert(((intptr_t)addr & (pField->Type->Align - 1)) == 0 && "unaligned address");

	*outValue = *(T*)(addr);
}

template<typename T>
T GetDefaultValue(PClass* cls, FName name)
{
	T value = {};
	GetDefaultValue(cls, name, &value);
	return value;
}

#define GET_DEFAULT_FROM_DEFINITION(name) GetDefaultValue(cls, #name, &definition->name);

void P_InitDefinitionFromClass(particledefinition_t* definition, PClass* cls)
{
	GET_DEFAULT_FROM_DEFINITION(PoolSize);
	GET_DEFAULT_FROM_DEFINITION(Lifetime);
	GET_DEFAULT_FROM_DEFINITION(LifetimeVariance);
	GET_DEFAULT_FROM_DEFINITION(Acceleration);
	GET_DEFAULT_FROM_DEFINITION(ScaleMin);
	GET_DEFAULT_FROM_DEFINITION(ScaleMax);
	GET_DEFAULT_FROM_DEFINITION(ScaleStep);
	GET_DEFAULT_FROM_DEFINITION(AlphaMin);
	GET_DEFAULT_FROM_DEFINITION(AlphaMax);
	GET_DEFAULT_FROM_DEFINITION(FadeStep);
	GET_DEFAULT_FROM_DEFINITION(RollMin);
	GET_DEFAULT_FROM_DEFINITION(RollMax);
	GET_DEFAULT_FROM_DEFINITION(RollVelMin);
	GET_DEFAULT_FROM_DEFINITION(RollVelMax);
	GET_DEFAULT_FROM_DEFINITION(RollAccMin);
	GET_DEFAULT_FROM_DEFINITION(RollAccMax);
	GET_DEFAULT_FROM_DEFINITION(Color);
	GET_DEFAULT_FROM_DEFINITION(Style);

	FString textureName = GetDefaultValue<FString>(cls, "Texture");
	if (!textureName.IsEmpty())
	{
		definition->Texture = TexMan.CheckForTexture(textureName.GetChars(), ETextureType::Any);
	}
}

void P_InitPooledParticles(FLevelLocals* Level)
{
	FName className = "ParticleDefinition";

	for (unsigned int i = 0; i < PClass::AllClasses.Size(); i++)
	{
		PClass* cls = PClass::AllClasses[i];

		if (cls->TypeName != className && cls->IsDescendantOf(className))
		{
			uint32_t poolSize = GetDefaultValue<uint32_t>(cls, "PoolSize");

			if (poolSize > 0)
			{
				int index = Level->ParticlePools.Push({});

				particlelevelpool_t* pool = &Level->ParticlePools[index];
				particledefinition_t* definition = &pool->Definition;

				Level->ParticlePoolsByType.Insert(cls->TypeName.GetIndex(), pool);

				P_InitDefinitionFromClass(definition, cls);

				pool->Particles.Resize(poolSize);
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
	if (!r_pooledparticles)
	{
		return;
	}

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

	for (int poolIndex = 0; poolIndex < Level->ParticlePools.Size(); poolIndex++)
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
	particledefinition_t* definition = &pool->Definition;
	pooledparticle_t* particle = nullptr, *prev = nullptr;
	while (i != NO_PARTICLE)
	{
		particle = &pool->Particles[i];
		i = particle->tnext;
		if (Level->isFrozen() && !(particle->flags & SPF_NOTIMEFREEZE))
		{
			if (particle->flags & SPF_LOCAL_ANIM)
			{
				particle->animData.SwitchTic++;
			}

			prev = particle;
			continue;
		}

		particle->alpha -= definition->FadeStep;
		particle->scale += definition->ScaleStep;
		if (particle->alpha <= 0 || --particle->ttl <= 0 || (particle->scale <= 0))
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

		// Handle crossing a line portal
		DVector2 newxy = Level->GetPortalOffsetPosition(particle->pos.X, particle->pos.Y, particle->vel.X, particle->vel.Y);
		particle->pos.X = newxy.X;
		particle->pos.Y = newxy.Y;
		particle->pos.Z += particle->vel.Z;
		particle->vel += (FVector3)definition->Acceleration;

		if (particle->flags & SPF_ROLL)
		{
			particle->roll += particle->rollvel;
			particle->rollvel += ParticleRandom(definition->RollAccMin, definition->RollAccMax);
		}

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
		prev = particle;
	}
}

void P_SpawnPooledParticle(FLevelLocals* Level, particlelevelpool_t* pool, const DVector3& pos, const DVector3& vel, double scale, int flags)
{
	particledefinition_t* definition = &pool->Definition;
	pooledparticle_t* particle = NewPooledParticle(Level, pool, (bool)(flags & SPF_REPLACE));

	if (particle)
	{
		particle->pos = pos;
		particle->vel = FVector3(vel);
		particle->color = definition->Color;
		particle->alpha = ParticleRandom(definition->AlphaMin, definition->AlphaMax);
		particle->ttl = definition->Lifetime + M_Random(definition->LifetimeVariance);
		
		if ((definition->FadeStep < 0 && !(flags & SPF_NEGATIVE_FADESTEP)) || definition->FadeStep <= -1.0) 
		{
			particle->fadestep = FADEFROMTTL(particle->ttl);
		}
		else 
		{
			particle->fadestep = float(definition->FadeStep);
		}

		particle->scale = ParticleRandom(definition->ScaleMin, definition->ScaleMax);
		particle->roll = ParticleRandom(definition->RollMin, definition->RollMax);
		particle->rollvel = ParticleRandom(definition->RollVelMin, definition->RollVelMax);
		particle->flags = flags;
		if (flags & SPF_LOCAL_ANIM)
		{
			TexAnim.InitStandaloneAnimation(particle->animData, definition->Texture, Level->maptime);
		}
	}
}

