struct ParticleData
{
    native int16        Time;       // Time elapsed
    native int16        Lifetime;   // How long this particle lives for
    native vector3      Pos;
    native FVector3     Vel;
    native float        Alpha;
    native float        AlphaStep;
    native FVector2     Scale;
    native FVector2     ScaleStep;
    native float        Roll;
    native float        RollStep;
    native color        Color;
    native TextureID    Texture;
    native uint8        AnimFrame;
    native uint8        AnimTick;
    native int          Flags;
    native int          User1;
    native int          User2;
    native int          User3;

    // Helper so we don't need to set the X and Y separately
    void SetScale(float s)
    {
        Scale.X = s;
        Scale.Y = s;
    }

    // Helper so we don't need to set the X and Y separately
    void SetScaleStep(float s)
    {
        ScaleStep.X = s;
        ScaleStep.Y = s;
    }

    void ChangeVelocity(Actor ref, double x = 0, double y = 0, double z = 0, int flags = 0)
    {
		if (ref == NULL)
		{
			return;
		}

		let newvel = (x, y, z);

        vector3 dir;
        if (Vel.LengthSquared() != 0)
        {
            dir = Vel.Unit();
        }

		double sina = sin(ref.Angle);
		double cosa = cos(ref.Angle);

		if (flags & CVF_RELATIVE)	// relative axes - make x, y relative to particle's current angle
		{
			newvel.X = x * cosa - y * sina;
			newvel.Y = x * sina + y * cosa;
		}

		if (flags & CVF_REPLACE)	// discard old velocity - replace old velocity with new velocity
		{
			Vel = newvel;
		}
		else	// add new velocity to old velocity
		{
			Vel += newvel;
		}
    }
}

class ParticleDefinition native
{
    native TextureID       DefaultTexture;
    native ERenderStyle    Style;
    
    native float StopSpeed;
	native float InheritVelocity;
	native int MinLife, MaxLife;
	native float MinAng, MaxAng;
	native float MinPitch, MaxPitch;
	native float MinSpeed, MaxSpeed;
	native float MinFadeVel, MaxFadeVel;
	native float MinFadeLife, MaxFadeLife;
	native FVector2 MinScale, MaxScale;
	native FVector2 MinFadeScale, MaxFadeScale;
	native float MinScaleLife, MaxScaleLife;
	native float MinScaleVel, MaxScaleVel;
	native int MinRandomBounces, MaxRandomBounces;
	native float MinRoll, MaxRoll;
	native float MinRollSpeed, MaxRollSpeed;
	native float RollDamping, RollDampingBounce;
	native float RestingPitchMin, RestingPitchMax, RestingPitchSpeed;
	native float RestingRollMin, RestingRollMax, RestingRollSpeed;

	native Sound BounceSound;
	native float BounceSoundChance;
	native float BounceSoundMinSpeed;
	native float BounceSoundPitchMin, BounceSoundPitchMax;
	native int BounceAccuracy;
	native float BounceFudge;
	native float MinBounceDeflect, MaxBounceDeflect;

	float QualityChanceLow, QualityChanceMed, QualityChanceHigh, QualityChanceUlt, QualityChanceInsane;
	float LifeMultLow, LifeMultMed, LifeMultHigh, LifeMultUlt, LifeMultInsane;

    void RandomAngle(float min, float max)                  { MinAng = min; MaxAng = max; }
    void RandomPitch(float min, float max)                  { MinPitch = min; MaxPitch = max; }
    void RandomSpeed(float min, float max)                  { MinSpeed = min; MaxSpeed = max; }
    void RandomLife(float min, float max)                   { MinLife = min; MaxLife = max; }
    void RandomRoll(float min, float max)                   { MinRoll = min; MaxRoll = max; }
    void RandomRollSpeed(float min, float max)              { MinRollSpeed = min; MaxRollSpeed = max;}
    void RandomScaleX(float min, float max)                 { MinScale.X = min; MaxScale.X = max; }
    void RandomScaleY(float min, float max)                 { MinScale.Y = min; MaxScale.Y = max; }
    void RandomScale(float xMin, float xMax, float yMin, float yMax) { MinScale.x = xMin; MaxScale.x = xMax; MinScale.y = yMin; MaxScale.y = yMax; }
    void BounceDeflect(float min, float max)                { MinBounceDeflect = min; MaxBounceDeflect = max; }
    void FadeVelRange(float min, float max)                 { MinFadeVel = min; MaxFadeVel = max; }
    void FadeLifeRange(float min, float max)                { MinFadeLife = min; MaxFadeLife = max; }
    void ScaleLifeRange(float min, float max)               { MinScaleLife = min; MaxScaleLife = max; }
    void ScaleRangeX(float min, float max)                  { MinFadeScale.X = min; MaxFadeScale.X = max; }
    void ScaleRangeY(float min, float max)                  { MinFadeScale.Y = min; MaxFadeScale.Y = max; }
    void ScaleRange(float xMin, float xMax, float yMin, float yMax) { MinFadeScale.x = xMin; MaxFadeScale.x = xMax; MinFadeScale.y = yMin; MaxFadeScale.y = yMax; }
    void BouncesRange(int min, int max)                     { MinRandomBounces = min; MaxRandomBounces = max; }
    void BounceSoundPitchRange(float min, float max)        { BounceSoundPitchMin = min; BounceSoundPitchMax = max; }
    void RestorePitch(float min, float max, float speed)    { RestingPitchMin = min; RestingPitchMax = max; RestingPitchSpeed = speed; }
    void RestoreRoll(float min, float max, float speed)     { RestingRollMin = min; RestingRollMax = max; RestingRollSpeed = speed; }


    virtual void Init() { }
    virtual void OnCreateParticle(in out ParticleData data, Actor refActor) { }
    
    native virtual void ThinkParticle(in out ParticleData data);

    native int AddAnimationSequence();
    native void AddAnimationFrame(int sequence, string textureName, int ticks = 1);
    native void AddAnimationFrames(int sequence, string spriteName, string frames, int ticksPerFrame);
    native int GetAnimationFrameCount(int sequence);
    native int GetAnimationLengthInTicks(int sequence);

    native int GetAnimationSequenceCount();
    native int GetAnimationStartFrame(int sequence);
    native int GetAnimationEndFrame(int sequence);

    // Particles are removed immediately when going over this number
    static const int particleLimits[] = 
    { 
        500,    // Low
        800,    // Med
        1600,   // High
        2000,   // Ultra    
        6000    // Insane
    };

    // Particles start to fade out where possible when going over this number
    static const int cullLimits[] = 
    { 
        250,    // Low
        400,    // Med
        800,    // High
        1000,   // Ultra   
        3000    // Insane
    };

    static int GetParticleLimits()
    {
        int currentParticleSetting = clamp(CVar.FindCVar("r_particleIntensity").GetInt() - 1, 0, 5);
        return ParticleDefinition.particleLimits[currentParticleSetting];
    }
}
