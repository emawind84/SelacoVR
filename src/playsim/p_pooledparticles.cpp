#include "doomtype.h"
#include "doomstat.h"

#include "p_pooledparticles.h"
#include "g_levellocals.h"
#include "m_argv.h"

CVAR(Bool, r_pooledparticles, true, 0);

#define FADEFROMTTL(a)	(1.f/(a))

int ParticleRandom(int min, int max)
{
	if (min > max)
	{
		std::swap(min, max);
	}

	return min + M_Random(max - min);
}

inline pooledparticle_t* NewPooledParticle(FLevelLocals* Level, pooledparticleid particleDefinitionID, bool replace = false)
{
	if (particleDefinitionID >= Level->ParticlePools.size())
	{
		return nullptr;
	}

	particlelevelpool_t* pool = &Level->ParticlePools[particleDefinitionID];

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

void P_InitPooledParticles(FLevelLocals* Level, pooledparticleid particleDefinitionID)
{
	if (particleDefinitionID < Level->ParticlePools.size())
	{
		particlelevelpool_t* pool = &Level->ParticlePools[particleDefinitionID];

		pool->Particles.Resize(pool->Definition->poolsize);
		P_ClearPooledParticles(pool);
	}
}

void P_ClearAllPooledParticles(FLevelLocals* Level)
{
	for (particlelevelpool_t& pool : Level->ParticlePools)
	{
		P_ClearPooledParticles(&pool);
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

// Group particles by subsectors. Because particles are always
// in motion, there is little benefit to caching this information
// from one frame to the next.
// [MC] VisualThinkers hitches a ride here

void P_FindPooledParticleSubsectors(FLevelLocals* Level, particlelevelpool_t* pool)
{
	// [NL] What is this VisualThinker stuff for, exactly? They mention it's 'hitching a ride', so it may be irrelevant to particles
/*
	// [MC] Hitch a ride on particle subsectors since VisualThinkers are effectively using the same kind of system.
	for (uint32_t i = 0; i < Level->subsectors.Size(); i++)
	{
		Level->subsectors[i].sprites.Clear();
	}
	// [MC] Not too happy about using an iterator for this but I can't think of another way to handle it.
	// At least it's on its own statnum for maximum efficiency.
	auto it = Level->GetThinkerIterator<DVisualThinker>(NAME_None, STAT_VISUALTHINKER);
	DVisualThinker* sp;
	while (sp = it.Next())
	{
		if (!sp->PT.subsector) sp->PT.subsector = Level->PointInRenderSubsector(sp->PT.Pos);

		sp->PT.subsector->sprites.Push(sp);
	}
	// End VisualThinker hitching. Now onto the particles. 
*/

	if (pool->ParticlesInSubsec.Size() < Level->subsectors.Size())
	{
		pool->ParticlesInSubsec.Reserve(Level->subsectors.Size() - pool->ParticlesInSubsec.Size());
	}

	fillshort(&pool->ParticlesInSubsec[0], Level->subsectors.Size(), NO_PARTICLE);

	if (!r_pooledparticles)
	{
		return;
	}
	for (uint16_t i = pool->ActiveParticles; i != NO_PARTICLE; i = pool->Particles[i].tnext)
	{
		// Try to reuse the subsector from the last portal check, if still valid.
		if (pool->Particles[i].subsector == nullptr) pool->Particles[i].subsector = Level->PointInRenderSubsector(pool->Particles[i].pos);
		int ssnum = pool->Particles[i].subsector->Index();
		pool->Particles[i].snext = pool->ParticlesInSubsec[ssnum];
		pool->ParticlesInSubsec[ssnum] = i;
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
	pooledparticledefinition_t* definition = pool->Definition;
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

		particle->alpha -= definition->fadestep;
		particle->scale += definition->scalestep;
		if (particle->alpha <= 0 || --particle->ttl <= 0 || (particle->scale <= 0))
		{ // The particle has expired, so free it
			*particle = {};
			if (prev)
				prev->tnext = i;
			else
				Level->ActiveParticles = i;

			if (i != NO_PARTICLE)
			{
				particle_t* next = &Level->Particles[i];
				next->tprev = particle->tprev;
			}
			particle->tnext = Level->InactiveParticles;
			Level->InactiveParticles = (int)(particle - Level->Particles.Data());
			continue;
		}

		// Handle crossing a line portal
		DVector2 newxy = Level->GetPortalOffsetPosition(particle->Pos.X, particle->Pos.Y, particle->Vel.X, particle->Vel.Y);
		particle->pos.X = newxy.X;
		particle->pos.Y = newxy.Y;
		particle->pos.Z += particle->vel.Z;
		particle->vel += definition->acceleration;

		if (particle->flags & SPF_ROLL)
		{
			particle->roll += particle->rollvel;
			particle->rollvel += definition->rollacc;
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

void P_SpawnPooledParticle(FLevelLocals* Level, pooledparticleid particleDefinitionID, const DVector3& pos, const DVector3& vel, double scale, int flags)
{
	pooledparticle_t* particle = NewPooledParticle(Level, particleDefinitionID, (bool)(flags & SPF_REPLACE));

	if (particle)
	{
		pooledparticledefinition_t* definition; // TODO: Figure out where we're storing these definitions

		particle->pos = pos;
		particle->vel = FVector3(vel);
		particle->color = definition->color;
		particle->alpha = float(definition->startalpha);
		particle->ttl = ParticleRandom(definition->lifetimemin, definition->lifetimemax);
		
		if ((definition->fadestep < 0 && !(flags & SPF_NEGATIVE_FADESTEP)) || definition->fadestep <= -1.0) 
		{
			particle->fadestep = FADEFROMTTL(particle->ttl);
		}
		else 
		{
			particle->fadestep = float(definition->fadestep);
		}

		particle->scale = definition->startscale;
		particle->roll = definition->startroll;
		particle->rollvel = definition->startrollvel;
		particle->flags = flags;
		if (flags & SPF_LOCAL_ANIM)
		{
			TexAnim.InitStandaloneAnimation(particle->animData, definition->texture, Level->maptime);
		}
	}
}

