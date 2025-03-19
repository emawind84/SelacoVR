struct ParticleData
{
    native int      Time;       // Time elapsed
    native int      Lifetime;   // How long this particle lives for
    native vector3  Pos;
    native FVector3 Vel;
    native float    Alpha;
    native float    AlphaStep;
    native float    Scale;
    native float    ScaleStep;
    native float    Roll;
    native float    RollStep;
    native color    Color;
    native int16    Flags;
}

class ParticleDefinition native
{
    native uint            PoolSize;
    native TextureID       DefaultTexture;
    native ERenderStyle    Style;

    virtual void Init() { }
    virtual void OnCreateParticle(in out ParticleData data) { }
    
    native virtual void ThinkParticle(in out ParticleData data);
}
