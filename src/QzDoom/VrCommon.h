#if !defined(vrcommon_h)
#define vrcommon_h

#include "c_cvars.h"
#include "mathlib.h"

EXTERN_CVAR(Int, vr_control_scheme)
EXTERN_CVAR(Bool, vr_move_use_offhand)
EXTERN_CVAR(Float, vr_weaponRotate);
EXTERN_CVAR(Float, vr_snapTurn);
EXTERN_CVAR(Float, vr_ipd);
EXTERN_CVAR(Float, vr_weaponScale);
EXTERN_CVAR(Bool, vr_teleport);
EXTERN_CVAR(Bool, vr_switch_sticks);
EXTERN_CVAR(Bool, vr_secondary_button_mappings);
EXTERN_CVAR(Bool, vr_two_handed_weapons);
EXTERN_CVAR(Bool, vr_crouch_use_button);

extern bool qzdoom_initialised;

extern bool cinemamode;
extern float cinemamodeYaw;
extern float cinemamodePitch;

extern float playerYaw;
extern bool resetDoomYaw;
extern float doomYaw;
extern bool resetPreviousPitch;
extern float previousPitch;

extern float vrFOV;

extern vec3_t worldPosition;

extern vec3_t hmdPosition;
extern vec3_t hmdorientation;
extern vec3_t positionDeltaThisFrame;

extern vec3_t weaponangles;
extern vec3_t weaponoffset;

extern vec3_t offhandangles;
extern vec3_t offhandoffset;

extern bool player_moving;

extern bool ready_teleport;
extern bool trigger_teleport;

extern bool shutdown;

//Called from engine code
void QzDoom_setUseScreenLayer(bool use);
void QzDoom_Restart();


#endif //vrcommon_h