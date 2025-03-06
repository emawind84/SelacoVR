#pragma once

#include "vectors.h"
#include "doomdef.h"
#include "renderstyle.h"
#include "dthinker.h"
#include "palettecontainer.h"
#include "animations.h"
#include "p_effect.h"

struct pooledparticledefinition_t
{
	uint32_t poolsize;
	FVector3 acceleration;
	float startscale, scalestep;
	float startalpha, fadestep;
	float startroll, startrollvel, rollacc;
	int32_t lifetimemin, lifetimemax;
	int color;
	FTextureID texture;
	ERenderStyle style;
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
	pooledparticledefinition_t* Definition;
	uint32_t					OldestParticle; // Oldest particle for replacing with SPF_REPLACE
	uint32_t					ActiveParticles;
	uint32_t					InactiveParticles;
	TArray<pooledparticle_t>	Particles;
	TArray<uint16_t>			ParticlesInSubsec;
};
