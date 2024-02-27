#include "VrCommon.h"

EXTERN_CVAR(Float, fov)

//Define all variables here that were externs in the VrCommon.h
float playerYaw;
bool resetDoomYaw;
bool resetPreviousPitch;
float doomYaw;
float previousPitch;
vec3_t worldPosition;
vec3_t hmdPosition;
vec3_t hmdorientation;
vec3_t weaponangles;
vec3_t weaponoffset;
bool weaponStabilised;

vec3_t offhandangles;
vec3_t offhandoffset;
bool shutdown;
bool ready_teleport;
bool trigger_teleport;
bool cinemamode;

float snapTurn;
float cinemamodeYaw;
float cinemamodePitch;

float remote_movementSideways;
float remote_movementForward;

/*
================================================================================

QuestZDoom Stuff

================================================================================
*/

void QzDoom_setUseScreenLayer(bool use)
{
}

int QzDoom_SetRefreshRate(int refreshRate)
{
    return 0;
}

void QzDoom_GetScreenRes(uint32_t *width, uint32_t *height)
{
}

float QzDoom_GetFOV()
{
	return fov;
}

void TBXR_submitFrame()
{
}

void VR_ExternalHapticEvent(const char* event, int position, int flags, int intensity, float angle, float yHeight )
{
}

void VR_HapticStopEvent(const char* event)
{
}

void VR_HapticEnable()
{
}

void VR_HapticDisable()
{
}

void VR_HapticEvent(const char* event, int position, int intensity, float angle, float yHeight )
{
}

void QzDoom_Vibrate(float duration, int channel, float intensity )
{
}

void VR_Shutdown()
{
}

void QzDoom_Restart()
{
}

void VR_GetMove(float *joy_forward, float *joy_side, float *hmd_forward, float *hmd_side, float *up,
				float *yaw, float *pitch, float *roll)
{
    *joy_forward = remote_movementForward;
    *joy_side = remote_movementSideways;
    *hmd_forward = 0.f;
    *hmd_side = 0.f;
    *up = 0.f;
    *yaw = 0.f;
    *pitch = 0.f;
    *roll = 0.f;
}

bool VR_GetVRProjection(int eye, float zNear, float zFar, float* projection)
{
    return true;
}

void TBXR_prepareEyeBuffer(int eye )
{
}

void TBXR_finishEyeBuffer(int eye )
{
}

bool TBXR_FrameSetup()
{
    return true;
}
