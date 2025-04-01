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

class DParticleDefinition;

struct particledata_t
{
	DParticleDefinition* definition;	// +8  = 8
	int16_t time;						// +2  = 10
	int16_t lifetime;					// +2  = 12
	DVector3 prevpos;					// +24 = 36
	DVector3 pos;						// +24 = 60
	FVector3 vel;						// +12 = 72
	float alpha, alphaStep;				// +8  = 80
	float scale, scaleStep;				// +8  = 88
	float roll, rollStep;				// +8  = 96
	int color;							// +4  = 100
	FTextureID texture;					// +4  = 104
	uint8_t animFrame, animTick;		// +2  = 108
	uint32_t flags;						// +4  = 112
	int user1, user2, user3;			// +12 = 124
	uint16_t tnext, tprev;				// +4  = 128

	subsector_t* subsector;				// +8  = 136
	uint16_t snext;						// +2  = 138
};

struct particleanimsequence_t
{
	uint8_t startFrame;
	uint8_t endFrame;
	uint8_t lengthInTicks;
};

struct particleanimframe_t
{
	FTextureID frame;
	uint8_t duration;
	uint8_t sequence;
};

class DParticleDefinition : public DObject
{
	DECLARE_CLASS(DParticleDefinition, DObject);

public:
	DParticleDefinition();
	virtual ~DParticleDefinition();

	FTextureID DefaultTexture;
	ERenderStyle Style;

	TArray<particleanimsequence_t> AnimationSequences;
	TArray<particleanimframe_t> AnimationFrames;

	void Init();
	void OnCreateParticle(particledata_t* particle, AActor* refActor);
	void ThinkParticle(particledata_t* particle);

	static int GetParticleLimits();
};

struct particlelevelpool_t
{
	uint32_t					OldestParticle; // Oldest particle for replacing with SPF_REPLACE
	uint32_t					ActiveParticles;
	uint32_t					InactiveParticles;
	TArray<particledata_t>		Particles;
};

inline particledata_t* NewDefinedParticle(FLevelLocals* Level, DParticleDefinition* definition, bool replace = false);
void P_InitParticleDefinitions(FLevelLocals* Level);
void P_ClearAllDefinedParticles(FLevelLocals* Level);

void P_FindDefinedParticleSubsectors(FLevelLocals* Level);
void P_ThinkDefinedParticles(FLevelLocals* Level);
void P_SpawnDefinedParticle(FLevelLocals* Level, DParticleDefinition* definition, const DVector3& pos, const DVector3& vel, double scale, int flags, AActor* refActor);

void P_LoadDefinedParticles(FSerializer& arc, FLevelLocals* Level, const char* key);
FSerializer& Serialize(FSerializer& arc, const char* key, particlelevelpool_t& lp, particlelevelpool_t* def);
FSerializer& Serialize(FSerializer& arc, const char* key, particledata_t& p, particledata_t* def);