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
	int16_t time;					// +2  = 2
	int16_t lifetime;				// +2  = 4
	DVector3 prevpos;				// +24 = 28
	DVector3 pos;					// +24 = 52
	FVector3 vel;					// +12 = 64
	float alpha, alphaStep;			// +8  = 72
	float scale, scaleStep;			// +8  = 80
	float roll, rollStep;			// +8  = 88
	int color;						// +4  = 92
	FTextureID texture;				// +4  = 96
	uint8_t animFrame, animTick;	// +2  = 98
	uint16_t flags;					// +2  = 100
	int user1, user2, user3;		// +12 = 112
	uint16_t tnext, tprev;			// +4  = 116

	subsector_t* subsector;			// +8  = 124
	pooledparticlessit_t snext;		// +4  = 128
};

struct pooledparticleanimframe_t
{
	FTextureID frame;
	uint8_t duration;
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

	TArray<pooledparticleanimframe_t> AnimationFrames;
	uint32_t AnimationLengthInTicks;

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