#pragma once

#include "vectors.h"
#include "doomdef.h"
#include "renderstyle.h"
#include "dthinker.h"
#include "palettecontainer.h"
#include "animations.h"
#include "p_local.h"
#include "p_effect.h"
#include "actor.h"
#include "dobject.h"
#include "serializer.h"

typedef uint32_t pooledparticleid;

// Used to iterate over particles in a subsector
struct pooledparticlessit_t
{
	uint16_t particleIndex; // Next sector index
	uint16_t poolIndex; // Next pool index
};

struct pooledparticle_t
{
	int32_t time;
	int32_t lifetime;
	DVector3 prevpos;
	DVector3 pos;
	FVector3 vel;
	float alpha, alphaStep;
	float scale, scaleStep;
	float roll, rollStep;
	int color;
	FTextureID texture;
	uint16_t flags;
	uint16_t tnext, tprev;

	subsector_t* subsector;
	pooledparticlessit_t snext;
};

class DParticleDefinition : public DObject
{
	DECLARE_CLASS(DParticleDefinition, DObject);

public:
	DParticleDefinition();
	virtual ~DParticleDefinition();

	uint32_t PoolSize;
	FTextureID DefaultTexture;
	ERenderStyle Style;

	void Init();
	void OnCreateParticle(pooledparticle_t* particle);
	void ThinkParticle(pooledparticle_t* particle);
};

struct particlelevelpool_t
{
	DParticleDefinition*		Definition;
	uint32_t					OldestParticle; // Oldest particle for replacing with SPF_REPLACE
	uint32_t					ActiveParticles;
	uint32_t					InactiveParticles;
	TArray<pooledparticle_t>	Particles;
};

inline pooledparticle_t* NewPooledParticle(FLevelLocals* Level, pooledparticleid particleDefinitionID, bool replace = false);
inline pooledparticle_t* NewPooledParticle(FLevelLocals* Level, particlelevelpool_t* pool, bool replace = false);
void P_InitPooledParticles(FLevelLocals* Level);
void P_ClearAllPooledParticles(FLevelLocals* Level);
void P_ClearPooledParticles(particlelevelpool_t* pool);

void P_FindPooledParticleSubsectors(FLevelLocals* Level);
void P_ThinkAllPooledParticles(FLevelLocals* Level);
void P_ThinkPooledParticles(FLevelLocals* Level, particlelevelpool_t* pool);
void P_SpawnPooledParticle(FLevelLocals* Level, particlelevelpool_t* pool, const DVector3& pos, const DVector3& vel, double scale, int flags);

void P_LoadParticlePools(FSerializer& arc, FLevelLocals* Level, const char* key);

FSerializer& Serialize(FSerializer& arc, const char* key, particlelevelpool_t& lp, particlelevelpool_t* def);
FSerializer& Serialize(FSerializer& arc, const char* key, pooledparticle_t& p, pooledparticle_t* def);