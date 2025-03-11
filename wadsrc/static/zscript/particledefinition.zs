class ParticleDefinition : Actor
{
    uint            PoolSize;
    int             Lifetime;
    int             LifetimeVariance;
    vector3         Acceleration;
    float           ScaleMin;
    float           ScaleMax;
    float           ScaleStep;
    float           AlphaMin;
    float           AlphaMax;
    float           FadeStep;
    float           RollMin;
    float           RollMax;
    float           RollVelMin;
    float           RollVelMax;
    float           RollAccMin;
    float           RollAccMax;
    color           Color;
    string          Texture;
    ERenderStyle    Style;

    property        PoolSize : PoolSize;
    property        Lifetime : Lifetime;
    property        LifetimeVariance : LifetimeVariance;
    property        Acceleration : Acceleration;
    property        RandomScale : ScaleMin, ScaleMax;
    property        ScaleStep : ScaleStep;
    property        RandomAlpha : AlphaMin, AlphaMax;
    property        FadeStep : FadeStep;
    property        RandomRoll : RollMin, RollMax;
    property        RandomRollVel : RollVelMin, RollVelMax;
    property        RandomRollAcc : RollAccMin, RollAccMax;
    property        Color : Color;
    property        Texture : Texture;
    property        Style : Style;

    default
    {
        ParticleDefinition.PoolSize 100;
        ParticleDefinition.Lifetime 35;
        ParticleDefinition.RandomScale 1, 1;
        ParticleDefinition.RandomAlpha 1, 1;
        ParticleDefinition.Color "white";
        ParticleDefinition.Style STYLE_Normal;
    }
}
