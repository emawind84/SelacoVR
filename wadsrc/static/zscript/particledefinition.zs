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
    native int16        Flags;
    native int          User1;
    native int          User2;
    native int          User3;

}

class ParticleDefinition native
{
    native uint            PoolSize;
    native TextureID       DefaultTexture;
    native ERenderStyle    Style;

    virtual void Init() { }
    virtual void OnCreateParticle(in out ParticleData data) { }
    
    native virtual void ThinkParticle(in out ParticleData data);

    native void AddAnimationFrame(string textureName, int ticks = 1);
    native void AddAnimationFrames(string spriteName, string frames, int ticksPerFrame);
    native int GetAnimationFrameCount();
    native int GetAnimationLengthInTicks();
}
