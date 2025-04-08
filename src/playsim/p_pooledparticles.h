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

enum EParticleDefinitionFlags
{
	PDF_KILLSTOP				= 1 << 0,	// Kill the particle when it stops moving
	PDF_DIRFROMMOMENTUM			= 1 << 1,	// Always face the direction of travel
	PDF_INSTANTBOUNCE			= 1 << 2,	// Clear interpolation on bounce, useful when using DirFromMomentum
	PDF_VELOCITYFADE			= 1 << 3,	// Fade out between min and max fadeVel
	PDF_LIFEFADE				= 1 << 4,	// Fade out between min and max fadeLife
	PDF_ROLLSTOP				= 1 << 5,	// Stop rolling when coming to rest
	PDF_SLEEPSTOP				= 1 << 6,	// Use Sleep() when the particle comes to rest for the rest of it's idle lifetime. Warning: Will probably float if on a moving surface
	PDF_NOTHINK					= 1 << 7,	// Don't call ThinkParticle
	PDF_FADEATBOUNCELIMIT		= 1 << 8,	// Once the bounce limit is reached, start fading out using lifetime settings
	PDF_IMPORTANT				= 1 << 9,	// If not flagged as important, particles will not spawn when reaching the limits
	PDF_SQUAREDSCALE			= 1 << 10,	// Random scale is always square (Match X and Y scale)
	PDF_ISBLOOD					= 1 << 11,	// Treat this particle as blood. Use different spawn variables such as r_BloodQuality instead of r_particleIntensity
	PDF_LIFESCALE				= 1 << 12,	// Follow min/max scale over lifetime
	PDF_BOUNCEONFLOORS			= 1 << 13,	// Bounce on floors
};

enum EDefinedParticleFlags
{
    DPF_FIRSTUPDATE             = 1 << 15,  // Particle has just been created
    DPF_ANIMATING               = 1 << 16,  // Particle is animating
	DPF_ATREST					= 1 << 17,	// Particle has settled and isn't moving
	DPF_NOPROCESS				= 1 << 18,	// This is set when killing the particle, so it may be used after for sprite things
	DPF_CLEANEDUP				= 1 << 19,	// Used by the particle limiter, this value is set once the particle has been marked for cleaning
	DPF_DESTROYED				= 1 << 20,	// Particle has been destroyed, and should be removed from the particle list next update
	DPF_ISBLOOD					= 1 << 21,	// Treat this particle as blood, same as bIsBlood on SelacoParticle
	DPF_FORCETRANSPARENT		= 1 << 22,	// Force this particle to render as transparent if it's set to None or Normal
};

struct particledata_t
{
	DParticleDefinition* definition;			// +8
	uint8_t renderStyle;						// +1
	int16_t life;								// +2 
	int16_t startLife;							// +2 
	DVector3 prevpos;							// +24
	DVector3 pos;								// +24
	FVector3 vel;								// +12
	float alpha, alphaStep;						// +8 
	FVector2 scale, scaleStep, startScale;		// +12
	float roll, rollStep;						// +8 
	float pitch, pitchStep;						// +8
	int16_t bounces, maxBounces;				// +4
	float floorz, ceilingz;						// +8
	int color;									// +4
	FTextureID texture;							// +4 
	uint8_t animFrame, animTick;				// +2 
	uint8_t invalidateTicks;					// +1
	uint16_t sleepFor;							// +2
	uint32_t flags;								// +4 
	int user1, user2, user3, user4;				// +16
	uint16_t tnext, tprev;						// +4 

	subsector_t* subsector;						// +8 
	uint16_t snext;								// +2 

	bool HasFlag(int flag) const { return flags & flag; }
	void SetFlag(int flag) { flags |= flag; }
	void ClearFlag(int flag) { flags &= ~flag; }
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
	ERenderStyle DefaultRenderStyle;

	static const float INVALID;
	static const float BOUNCE_SOUND_ATTENUATION;

	float StopSpeed = 0.4f;
	float InheritVelocity = 0;
	int MinLife = -1, MaxLife = -1;
	float MinAng = 0, MaxAng = 0;
	float MinPitch = 0, MaxPitch = 0;
	float MinSpeed = INVALID, MaxSpeed = INVALID;
	float MinFadeVel = 0.4f, MaxFadeVel = 2;
	int MinFadeLife = 0, MaxFadeLife = 25;
	FVector2 MinScale = FVector2(-1, -1), MaxScale = FVector2(-1, -1);
	FVector2 MinFadeScale = FVector2(1, 1), MaxFadeScale = FVector2(0.2f, 0.2f);
	float MinScaleLife = 0, MaxScaleLife = 0.1f;
	float MinScaleVel = 0, MaxScaleVel = 0;
	int MinRandomBounces = -1, MaxRandomBounces = -1;
	float Drag = 0;
	float MinRoll = 0, MaxRoll = 0;
	float MinRollSpeed = 0, MaxRollSpeed = 0;
	float RollDamping = 0, RollDampingBounce = 0.3f;
	float RestingPitchMin = 0, RestingPitchMax = 0, RestingPitchSpeed = -1;
	float RestingRollMin = 0, RestingRollMax = 0, RestingRollSpeed = -1;

	float MaxStepHeight = 8;
	float Gravity = 0;
	float BounceFactor = 0.7f;
	int BounceSound = 0;
	float BounceSoundChance = 0.2f;
	float BounceSoundMinSpeed = 2;
	float BounceSoundPitchMin = 0.95f, BounceSoundPitchMax = 1.05f;
	float BounceFudge = 0.15f;
	float MinBounceDeflect = -25, MaxBounceDeflect = 25;

	float QualityChanceLow = 0.1f, QualityChanceMed = 0.25f, QualityChanceHigh = 0.6f, QualityChanceUlt = 1, QualityChanceInsane = 9999;
	float LifeMultLow = 0.07f, LifeMultMed = 0.1f, LifeMultHigh = 0.3f, LifeMultUlt = 1, LifeMultInsane = 3;

	uint32_t Flags = 
		PDF_BOUNCEONFLOORS |
		PDF_INSTANTBOUNCE |
		PDF_SQUAREDSCALE;

	TArray<particleanimsequence_t> AnimationSequences;
	TArray<particleanimframe_t> AnimationFrames;

	void CallInit();
	void CallOnCreateParticle(particledata_t* particle, AActor* refActor);
	bool CallOnParticleDeath(particledata_t* particle);
	void CallThinkParticle(particledata_t* particle);
	void CallOnParticleBounce(particledata_t* particle);

	void HandleFading(particledata_t* particle);
	void HandleScaling(particledata_t* particle);
	void OnParticleBounce(particledata_t* particle);

	static int GetParticleLimit();
	static int GetParticleCullLimit();

	void RestParticle(particledata_t* particle);
	void CullParticle(particledata_t* particle);
	void CleanupParticle(particledata_t* particle);

	bool HasFlag(EParticleDefinitionFlags flag) const { return Flags & flag; }
	void SetFlag(EParticleDefinitionFlags flag) { Flags |= flag; }
	void ClearFlag(EParticleDefinitionFlags flag) { Flags &= ~flag; }

	FLevelLocals* Level;
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
bool P_DestroyDefinedParticle(FLevelLocals* Level, int particleIndex);
void P_ThinkDefinedParticles(FLevelLocals* Level);
void P_SpawnDefinedParticle(FLevelLocals* Level, DParticleDefinition* definition, const DVector3& pos, const DVector3& vel, double scale, int flags, AActor* refActor);

void P_LoadDefinedParticles(FSerializer& arc, FLevelLocals* Level, const char* key);
FSerializer& Serialize(FSerializer& arc, const char* key, particlelevelpool_t& lp, particlelevelpool_t* def);
FSerializer& Serialize(FSerializer& arc, const char* key, particledata_t& p, particledata_t* def);