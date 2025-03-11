#pragma once

#include "vectors.h"
#include "doomdef.h"
#include "renderstyle.h"
#include "dthinker.h"
#include "palettecontainer.h"
#include "animations.h"
#include "p_local.h"
#include "p_effect.h"

struct particledefinition_t
{
	uint32_t PoolSize;
	int32_t Lifetime, LifetimeVariance;
	DVector3 Acceleration;
	float ScaleMin, ScaleMax, ScaleStep;
	float AlphaMin, AlphaMax, FadeStep;
	float RollMin, RollMax, RollVelMin, RollVelMax, RollAccMin, RollAccMax;
	PalEntry Color;
	FTextureID Texture;
	ERenderStyle Style;
};

typedef uint32_t pooledparticleid;

struct pooledparticle_t
{
	subsector_t* subsector;
	DVector3 pos;
	FVector3 vel;
	float roll;
	float rollvel;
	float scale;
	float alpha, fadestep;
	int32_t ttl;
	int color;
	uint16_t tnext, snext, tprev;
	uint16_t flags;
	FStandaloneAnimation animData;
};

struct particlelevelpool_t
{
	particledefinition_t		Definition;
	uint32_t					OldestParticle; // Oldest particle for replacing with SPF_REPLACE
	uint32_t					ActiveParticles;
	uint32_t					InactiveParticles;
	TArray<pooledparticle_t>	Particles;
	TArray<uint16_t>			ParticlesInSubsec;
};

inline pooledparticle_t* NewPooledParticle(FLevelLocals* Level, pooledparticleid particleDefinitionID, bool replace = false);
inline pooledparticle_t* NewPooledParticle(FLevelLocals* Level, particlelevelpool_t* pool, bool replace = false);
void P_InitPooledParticles(FLevelLocals* Level);
void P_ClearAllPooledParticles(FLevelLocals* Level);
void P_ClearPooledParticles(particlelevelpool_t* pool);

void P_FindAllPooledParticleSubsectors(FLevelLocals* Level);
void P_FindPooledParticleSubsectors(FLevelLocals* Level, particlelevelpool_t* pool);
void P_ThinkAllPooledParticles(FLevelLocals* Level);
void P_ThinkPooledParticles(FLevelLocals* Level, particlelevelpool_t* pool);
void P_SpawnPooledParticle(FLevelLocals* Level, particlelevelpool_t* pool, const DVector3& pos, const DVector3& vel, double scale, int flags);
