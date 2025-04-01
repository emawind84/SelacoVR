struct ParticleData
{
    native int16        Time;       // Time elapsed
    native int16        Lifetime;   // How long this particle lives for
    native vector3      Pos;
    native FVector3     Vel;
    native float        Alpha;
    native float        AlphaStep;
    native float        Scale;
    native float        ScaleStep;
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
