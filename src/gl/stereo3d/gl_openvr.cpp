//
//---------------------------------------------------------------------------
//
// Copyright(C) 2016-2017 Christopher Bruns
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_openvr.cpp
** Stereoscopic virtual reality mode for the HTC Vive headset
**
*/

#ifdef USE_OPENVR

#include "gl_openvr.h"
#include "openvr_capi.h"
#include <string>
#include "gl/system/gl_system.h"
#include "doomtype.h" // Printf
#include "d_player.h"
#include "g_game.h" // G_Add...
#include "p_local.h" // P_TryMove
#include "r_utility.h" // viewpitch
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/renderer/gl_2ddrawer.h" // crosshair
#include "g_levellocals.h" // pixelstretch
#include "g_statusbar/sbar.h"
#include "math/cmath.h"
#include "c_cvars.h"
#include "cmdlib.h"
#include "LSMatrix.h"

#ifdef DYN_OPENVR
// Dynamically load OpenVR

#include "i_module.h"
FModule OpenVRModule{ "OpenVR" };

/** Pointer-to-function type, useful for dynamically getting OpenVR entry points. */
// Derived from global entry at the bottom of openvr_capi.h, plus a few other functions
typedef intptr_t (*LVR_InitInternal)(EVRInitError *peError, EVRApplicationType eType);
typedef void (*LVR_ShutdownInternal)();
typedef bool (*LVR_IsHmdPresent)();
typedef intptr_t (*LVR_GetGenericInterface)(const char *pchInterfaceVersion, EVRInitError *peError);
typedef bool (*LVR_IsRuntimeInstalled)();
typedef const char * (*LVR_GetVRInitErrorAsSymbol)(EVRInitError error);
typedef const char * (*LVR_GetVRInitErrorAsEnglishDescription)(EVRInitError error);
typedef bool (*LVR_IsInterfaceVersionValid)(const char * version);
typedef uint32_t (*LVR_GetInitToken)();

#define DEFINE_ENTRY(name) static TReqProc<OpenVRModule, L##name> name{#name};
DEFINE_ENTRY(VR_InitInternal)
DEFINE_ENTRY(VR_ShutdownInternal)
DEFINE_ENTRY(VR_IsHmdPresent)
DEFINE_ENTRY(VR_GetGenericInterface)
DEFINE_ENTRY(VR_IsRuntimeInstalled)
DEFINE_ENTRY(VR_GetVRInitErrorAsSymbol)
DEFINE_ENTRY(VR_GetVRInitErrorAsEnglishDescription)
DEFINE_ENTRY(VR_IsInterfaceVersionValid)
DEFINE_ENTRY(VR_GetInitToken)

#ifdef _WIN32
#define OPENVRLIB "openvr_api.dll"
#elif defined(__APPLE__)
#define OPENVRLIB "libopenvr_api.dylib"
#else
#define OPENVRLIB "libopenvr_api.so"
#endif

#else
// Non-dynamic loading of OpenVR

// OpenVR Global entry points
S_API intptr_t VR_InitInternal(EVRInitError *peError, EVRApplicationType eType);
S_API void VR_ShutdownInternal();
S_API bool VR_IsHmdPresent();
S_API intptr_t VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError);
S_API bool VR_IsRuntimeInstalled();
S_API const char * VR_GetVRInitErrorAsSymbol(EVRInitError error);
S_API const char * VR_GetVRInitErrorAsEnglishDescription(EVRInitError error);
S_API bool VR_IsInterfaceVersionValid(const char * version);
S_API uint32_t VR_GetInitToken();

#endif

// For conversion between real-world and doom units
#define VERTICAL_DOOM_UNITS_PER_METER 27.0f

EXTERN_CVAR(Int, screenblocks);
EXTERN_CVAR(Float, movebob);
EXTERN_CVAR(Bool, gl_billboard_faces_camera);
EXTERN_CVAR(Int, gl_multisample);

bool IsOpenVRPresent()
{
#ifndef USE_OPENVR
	return false;
#elif !defined DYN_OPENVR
	return true;
#else
	static bool cached_result = false;
	static bool done = false;

	if (!done)
	{
		done = true;
		cached_result = OpenVRModule.Load({ NicePath("$PROGDIR/" OPENVRLIB), OPENVRLIB });
	}
	return cached_result;
#endif
}

// feature toggles, for testing and debugging
static const bool doTrackHmdYaw = true;
static const bool doTrackHmdPitch = true;
static const bool doTrackHmdRoll = true;
static const bool doLateScheduledRotationTracking = true;
static const bool doStereoscopicViewpointOffset = true;
static const bool doRenderToDesktop = true; // mirroring to the desktop is very helpful for debugging
static const bool doRenderToHmd = true;
static const bool doTrackHmdVerticalPosition = true;
static const bool doTrackHmdHorizontalPosition = true;
static const bool doTrackVrControllerPosition = false; // todo:

namespace s3d 
{

/* static */
const Stereo3DMode& OpenVRMode::getInstance()
{
		static OpenVRMode instance;
		if (! instance.hmdWasFound)
			return  MonoView::getInstance();
		return instance;
}

static HmdVector3d_t eulerAnglesFromQuat(HmdQuaternion_t quat) {
	double q0 = quat.w;
	// permute axes to make "Y" up/yaw
	double q2 = quat.x;
	double q3 = quat.y;
	double q1 = quat.z;

	// http://stackoverflow.com/questions/18433801/converting-a-3x3-matrix-to-euler-tait-bryan-angles-pitch-yaw-roll
	double roll = atan2(2 * (q0*q1 + q2*q3), 1 - 2 * (q1*q1 + q2*q2));
	double pitch = asin(2 * (q0*q2 - q3*q1));
	double yaw = atan2(2 * (q0*q3 + q1*q2), 1 - 2 * (q2*q2 + q3*q3));

	return HmdVector3d_t{ yaw, pitch, roll };
}

static HmdQuaternion_t quatFromMatrix(HmdMatrix34_t matrix) {
	HmdQuaternion_t q;
	typedef float f34[3][4];
	f34& a = matrix.m;
	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
	float trace = a[0][0] + a[1][1] + a[2][2]; // I removed + 1.0f; see discussion with Ethan
	if (trace > 0) {// I changed M_EPSILON to 0
		float s = 0.5f / sqrtf(trace + 1.0f);
		q.w = 0.25f / s;
		q.x = (a[2][1] - a[1][2]) * s;
		q.y = (a[0][2] - a[2][0]) * s;
		q.z = (a[1][0] - a[0][1]) * s;
	}
	else {
		if (a[0][0] > a[1][1] && a[0][0] > a[2][2]) {
			float s = 2.0f * sqrtf(1.0f + a[0][0] - a[1][1] - a[2][2]);
			q.w = (a[2][1] - a[1][2]) / s;
			q.x = 0.25f * s;
			q.y = (a[0][1] + a[1][0]) / s;
			q.z = (a[0][2] + a[2][0]) / s;
		}
		else if (a[1][1] > a[2][2]) {
			float s = 2.0f * sqrtf(1.0f + a[1][1] - a[0][0] - a[2][2]);
			q.w = (a[0][2] - a[2][0]) / s;
			q.x = (a[0][1] + a[1][0]) / s;
			q.y = 0.25f * s;
			q.z = (a[1][2] + a[2][1]) / s;
		}
		else {
			float s = 2.0f * sqrtf(1.0f + a[2][2] - a[0][0] - a[1][1]);
			q.w = (a[1][0] - a[0][1]) / s;
			q.x = (a[0][2] + a[2][0]) / s;
			q.y = (a[1][2] + a[2][1]) / s;
			q.z = 0.25f * s;
		}
	}

	return q;
}

static HmdVector3d_t eulerAnglesFromMatrix(HmdMatrix34_t mat) {
	return eulerAnglesFromQuat(quatFromMatrix(mat));
}

OpenVREyePose::OpenVREyePose(int eye)
	: ShiftedEyePose( 0.0f )
	, eye(eye)
	, eyeTexture(nullptr)
	, currentPose(nullptr)
{
}


/* virtual */
OpenVREyePose::~OpenVREyePose() 
{
	dispose();
}

static void vSMatrixFromHmdMatrix34(VSMatrix& m1, const HmdMatrix34_t& m2)
{
	float tmp[16];
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 4; ++j) {
			tmp[4 * i + j] = m2.m[i][j];
		}
	}
	int i = 3;
	for (int j = 0; j < 4; ++j) {
		tmp[4 * i + j] = 0;
	}
	tmp[15] = 1;
	m1.loadMatrix(&tmp[0]);
}


/* virtual */
void OpenVREyePose::GetViewShift(FLOATTYPE yaw, FLOATTYPE outViewShift[3]) const
{
	outViewShift[0] = outViewShift[1] = outViewShift[2] = 0;

	if (currentPose == nullptr)
		return;
	const TrackedDevicePose_t& hmd = *currentPose;
	if (! hmd.bDeviceIsConnected)
		return;
	if (! hmd.bPoseIsValid)
		return;

	if (! doStereoscopicViewpointOffset)
		return;

	const HmdMatrix34_t& hmdPose = hmd.mDeviceToAbsoluteTracking;

	// Pitch and Roll are identical between OpenVR and Doom worlds.
	// But yaw can differ, depending on starting state, and controller movement.
	float doomYawDegrees = yaw;
	float openVrYawDegrees = RAD2DEG(-eulerAnglesFromMatrix(hmdPose).v[0]);
	float deltaYawDegrees = doomYawDegrees - openVrYawDegrees;
	while (deltaYawDegrees > 180)
		deltaYawDegrees -= 360;
	while (deltaYawDegrees < -180)
		deltaYawDegrees += 360;

	// extract rotation component from hmd transform
	LSMatrix44 openvr_X_hmd(hmdPose);
	LSMatrix44 hmdRot = openvr_X_hmd.getWithoutTranslation(); // .transpose();

	/// In these eye methods, just get local inter-eye stereoscopic shift, not full position shift ///

	// compute local eye shift
	LSMatrix44 eyeShift2;
	eyeShift2.loadIdentity();
	eyeShift2 = eyeShift2 * eyeToHeadTransform; // eye to head
	eyeShift2 = eyeShift2 * hmdRot; // head to openvr

	LSVec3 eye_EyePos = LSVec3(0, 0, 0); // eye position in eye frame
	LSVec3 hmd_EyePos = LSMatrix44(eyeToHeadTransform) * eye_EyePos;
	LSVec3 hmd_HmdPos = LSVec3(0, 0, 0); // hmd position in hmd frame
	LSVec3 openvr_EyePos = openvr_X_hmd * hmd_EyePos;
	LSVec3 openvr_HmdPos = openvr_X_hmd * hmd_HmdPos;
	LSVec3 hmd_OtherEyePos = LSMatrix44(otherEyeToHeadTransform) * eye_EyePos;
	LSVec3 openvr_OtherEyePos = openvr_X_hmd * hmd_OtherEyePos;
	LSVec3 openvr_EyeOffset = openvr_EyePos - openvr_HmdPos;

	VSMatrix doomInOpenVR = VSMatrix();
	doomInOpenVR.loadIdentity();
	// permute axes
	float permute[] = { // Convert from OpenVR to Doom axis convention, including mirror inversion
		-1,  0,  0,  0, // X-right in OpenVR -> X-left in Doom
			0,  0,  1,  0, // Z-backward in OpenVR -> Y-backward in Doom
			0,  1,  0,  0, // Y-up in OpenVR -> Z-up in Doom
			0,  0,  0,  1};
	doomInOpenVR.multMatrix(permute);
	doomInOpenVR.scale(VERTICAL_DOOM_UNITS_PER_METER, VERTICAL_DOOM_UNITS_PER_METER, VERTICAL_DOOM_UNITS_PER_METER); // Doom units are not meters
	double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
	doomInOpenVR.scale(pixelstretch, pixelstretch, 1.0); // Doom universe is scaled by 1990s pixel aspect ratio
	doomInOpenVR.rotate(deltaYawDegrees, 0, 0, 1);

	LSVec3 doom_EyeOffset = LSMatrix44(doomInOpenVR) * openvr_EyeOffset;

	if (doTrackHmdVerticalPosition) {
		// In OpenVR, the real world floor level is at y==0
		// In Doom, the virtual player foot level is viewheight below the current viewpoint (on the Z axis)
		// We want to align those two heights here
		const player_t & player = players[consoleplayer];
		double vh = player.viewheight; // Doom thinks this is where you are
		double hh = openvr_X_hmd[1][3] * VERTICAL_DOOM_UNITS_PER_METER; // HMD is actually here
		doom_EyeOffset[2] += hh - vh;
		// TODO: optionally allow player to jump and crouch by actually jumping and crouching
	}

	if (doTrackHmdHorizontalPosition) {
		// shift viewpoint when hmd position shifts
		static bool is_initial_origin_set = false;
		static LSVec3 openvr_origin(0, 0, 0);
		if (! is_initial_origin_set) {
			// initialize origin to first noted HMD location
			// TODO: implement recentering based on a CCMD
			openvr_origin = openvr_HmdPos;
			is_initial_origin_set = true;
		}
		LSVec3 openvr_dpos = openvr_HmdPos - openvr_origin;
		{
			// Suddenly recenter if deviation gets too large
			const double max_shift = 0.30; // meters
			if (std::abs(openvr_dpos[0]) + std::abs(openvr_dpos[2]) > max_shift) {
				openvr_origin += 1.0 * openvr_dpos; // recenter MOST of the way to the new position
				openvr_dpos = openvr_HmdPos - openvr_origin;
			}
		}
		LSVec3 doom_dpos = LSMatrix44(doomInOpenVR) * openvr_dpos;
		doom_EyeOffset[0] += doom_dpos[0];
		doom_EyeOffset[1] += doom_dpos[1];

		// TODO: update player playsim position based on HMD position changes
	}

	outViewShift[0] = doom_EyeOffset[0];
	outViewShift[1] = doom_EyeOffset[1];
	outViewShift[2] = doom_EyeOffset[2];
}

/* virtual */
VSMatrix OpenVREyePose::GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const
{
	// Ignore those arguments and get the projection from the SDK
	// VSMatrix vs1 = ShiftedEyePose::GetProjection(fov, aspectRatio, fovRatio);
	return projectionMatrix;
}

void OpenVREyePose::initialize(VR_IVRSystem_FnTable * vrsystem)
{
	float zNear = 5.0;
	float zFar = 65536.0;
	HmdMatrix44_t projection = vrsystem->GetProjectionMatrix(
			EVREye(eye), zNear, zFar);
	HmdMatrix44_t proj_transpose;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			proj_transpose.m[i][j] = projection.m[j][i];
		}
	}
	projectionMatrix.loadIdentity();
	projectionMatrix.multMatrix(&proj_transpose.m[0][0]);

	HmdMatrix34_t eyeToHead = vrsystem->GetEyeToHeadTransform(EVREye(eye));
	vSMatrixFromHmdMatrix34(eyeToHeadTransform, eyeToHead);
	HmdMatrix34_t otherEyeToHead = vrsystem->GetEyeToHeadTransform(eye == EVREye_Eye_Left ? EVREye_Eye_Right : EVREye_Eye_Left);
	vSMatrixFromHmdMatrix34(otherEyeToHeadTransform, otherEyeToHead);

	if (eyeTexture == nullptr)
		eyeTexture = new Texture_t();
	eyeTexture->handle = nullptr; // TODO: populate this at resolve time
	eyeTexture->eType = ETextureType_TextureType_OpenGL;
	eyeTexture->eColorSpace = EColorSpace_ColorSpace_Linear;
}

void OpenVREyePose::dispose()
{
	if (eyeTexture) {
		delete eyeTexture;
		eyeTexture = nullptr;
	}
}

bool OpenVREyePose::submitFrame(VR_IVRCompositor_FnTable * vrCompositor) const
{
	if (eyeTexture == nullptr)
		return false;
	if (vrCompositor == nullptr)
		return false;
 
	// Copy HDR game texture to local vr LDR framebuffer, so gamma correction could work
	if (eyeTexture->handle == nullptr) {
		glGenFramebuffers(1, &framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

		GLuint handle;
		glGenTextures(1, &handle);
		eyeTexture->handle = (void *)(std::ptrdiff_t)handle;
		glBindTexture(GL_TEXTURE_2D, handle);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, GLRenderer->mSceneViewport.width,
			GLRenderer->mSceneViewport.height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, handle, 0);
		GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT0};
		glDrawBuffers(1, drawBuffers);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			// TODO: react to error if it ever happens
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	GLRenderer->mBuffers->BindEyeTexture(eye, 0);
	GL_IRECT box = {0, 0, GLRenderer->mSceneViewport.width, GLRenderer->mSceneViewport.height};
	GLRenderer->DrawPresentTexture(box, true);

	vrCompositor->Submit(EVREye(eye), eyeTexture, nullptr, EVRSubmitFlags_Submit_Default);
	return true;
}

VSMatrix OpenVREyePose::getQuadInWorld(
	float distance, // meters
	float width, // meters 
	bool doFixPitch,
	float pitchOffset) const 
{
	VSMatrix new_projection;
	new_projection.loadIdentity();

	// doom_units from meters
	new_projection.scale(
		-VERTICAL_DOOM_UNITS_PER_METER,
		VERTICAL_DOOM_UNITS_PER_METER,
		-VERTICAL_DOOM_UNITS_PER_METER);
	double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
	new_projection.scale(pixelstretch, pixelstretch, 1.0); // Doom universe is scaled by 1990s pixel aspect ratio

	const OpenVREyePose * activeEye = this;

	// eye coordinates from hmd coordinates
	LSMatrix44 e2h(activeEye->eyeToHeadTransform);
	new_projection.multMatrix(e2h.transpose());

	// Follow HMD orientation, EXCEPT for roll angle (keep weapon upright)
	if (activeEye->currentPose) {
		float openVrRollDegrees = RAD2DEG(-eulerAnglesFromMatrix(activeEye->currentPose->mDeviceToAbsoluteTracking).v[2]);
		new_projection.rotate(-openVrRollDegrees, 0, 0, 1);

		if (doFixPitch) {
			float openVrPitchDegrees = RAD2DEG(-eulerAnglesFromMatrix(activeEye->currentPose->mDeviceToAbsoluteTracking).v[1]);
			new_projection.rotate(-openVrPitchDegrees, 1, 0, 0);
		}
		if (pitchOffset != 0)
			new_projection.rotate(-pitchOffset, 1, 0, 0);
	}

	// hmd coordinates (meters) from ndc coordinates
	// const float weapon_distance_meters = 0.55f;
	// const float weapon_width_meters = 0.3f;
	const float aspect = SCREENWIDTH / float(SCREENHEIGHT);
	new_projection.translate(0.0, 0.0, distance);
	new_projection.scale(
		-width,
		width / aspect,
		-width);

	// ndc coordinates from pixel coordinates
	new_projection.translate(-1.0, 1.0, 0);
	new_projection.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);

	VSMatrix proj(activeEye->projectionMatrix);
	proj.multMatrix(new_projection);
	new_projection = proj;

	return new_projection;
}

void OpenVREyePose::AdjustHud() const
{
	// Draw crosshair on a separate quad, before updating HUD matrix
	const Stereo3DMode * mode3d = &Stereo3DMode::getCurrentMode();
	if (mode3d->IsMono())
		return;
	const OpenVRMode * openVrMode = static_cast<const OpenVRMode *>(mode3d);
	if (openVrMode 
		&& openVrMode->crossHairDrawer
		// Don't draw the crosshair if there is none
		&& CrosshairImage != NULL 
		&& gamestate != GS_TITLELEVEL 
		&& r_viewpoint.camera->health > 0)
	{
		const float crosshair_distance_meters = 10.0f; // meters
		const float crosshair_width_meters = 0.2f * crosshair_distance_meters;
		gl_RenderState.mProjectionMatrix = getQuadInWorld(
			crosshair_distance_meters,
			crosshair_width_meters,
			false,
			0.0);
		gl_RenderState.ApplyMatrices();
		openVrMode->crossHairDrawer->Draw();
	}

	// Update HUD matrix to render on a separate quad
	const float menu_distance_meters = 1.0f;
	const float menu_width_meters = 0.4f * menu_distance_meters;
	const float pitch_offset = -8.0;
	gl_RenderState.mProjectionMatrix = getQuadInWorld(
		menu_distance_meters, 
		menu_width_meters, 
		true,
		pitch_offset);
	gl_RenderState.ApplyMatrices();
}

void OpenVREyePose::AdjustBlend() const
{
	VSMatrix& proj = gl_RenderState.mProjectionMatrix;
	proj.loadIdentity();
	proj.translate(-1, 1, 0);
	proj.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);
	gl_RenderState.ApplyMatrices();
}

OpenVRMode::OpenVRMode() 
	: vrSystem(nullptr)
	, leftEyeView(EVREye_Eye_Left)
	, rightEyeView(EVREye_Eye_Right)
	, hmdWasFound(false)
	, sceneWidth(0), sceneHeight(0)
	, vrCompositor(nullptr)
	, vrToken(0)
	, crossHairDrawer(new F2DDrawer)
	, cached2DDrawer(nullptr)
{
	eye_ptrs.Push(&leftEyeView); // initially default behavior to Mono non-stereo rendering

	if ( ! IsOpenVRPresent() ) return; // failed to load openvr API dynamically

	if ( ! VR_IsRuntimeInstalled() ) return; // failed to find OpenVR implementation

	if ( ! VR_IsHmdPresent() ) return; // no VR headset is attached

	EVRInitError eError;
	// Code below recapitulates the effects of C++ call vr::VR_Init()
	VR_InitInternal(&eError, EVRApplicationType_VRApplication_Scene);
	if (eError != EVRInitError_VRInitError_None) {
		std::string errMsg = VR_GetVRInitErrorAsEnglishDescription(eError);
		return;
	}
	if (! VR_IsInterfaceVersionValid(IVRSystem_Version))
	{
		VR_ShutdownInternal();
		return;
	}
	vrToken = VR_GetInitToken();
	const std::string sys_key = std::string("FnTable:") + std::string(IVRSystem_Version);
	vrSystem = (VR_IVRSystem_FnTable*) VR_GetGenericInterface(sys_key.c_str() , &eError);
	if (vrSystem == nullptr)
		return;

	vrSystem->GetRecommendedRenderTargetSize(&sceneWidth, &sceneHeight);

	leftEyeView.initialize(vrSystem);
	rightEyeView.initialize(vrSystem);

	const std::string comp_key = std::string("FnTable:") + std::string(IVRCompositor_Version);
	vrCompositor = (VR_IVRCompositor_FnTable*)VR_GetGenericInterface(comp_key.c_str(), &eError);
	if (vrCompositor == nullptr)
		return;

	eye_ptrs.Push(&rightEyeView); // NOW we render to two eyes
	hmdWasFound = true;

	crossHairDrawer->Clear();
}

/* virtual */
// AdjustViewports() is called from within FLGRenderer::SetOutputViewport(...)
void OpenVRMode::AdjustViewports() const
{
	// Draw the 3D scene into the entire framebuffer
	GLRenderer->mSceneViewport.width = sceneWidth;
	GLRenderer->mSceneViewport.height = sceneHeight;
	GLRenderer->mSceneViewport.left = 0;
	GLRenderer->mSceneViewport.top = 0;

	GLRenderer->mScreenViewport.width = sceneWidth;
	GLRenderer->mScreenViewport.height = sceneHeight;
}

void OpenVRMode::AdjustPlayerSprites() const
{
	Stereo3DMode::AdjustPlayerSprites();

	// Prepare to temporarily modify view size
	cachedViewwidth = viewwidth;
	cachedViewheight = viewheight;
	cachedViewwindowx = viewwindowx;
	cachedViewwindowy = viewwindowy;

	// Avoid rescaling weapon when status bar shown (screenblocks <= 10)
	viewwidth = SCREENWIDTH;
	viewheight = SCREENHEIGHT;
	viewwindowx = 0;
	viewwindowy = 0;

	const OpenVREyePose * activeEye = &rightEyeView;
	if (!activeEye->isActive())
		activeEye = &leftEyeView;
	const float weapon_distance_meters = 0.55f; // meters
	const float weapon_width_meters = 0.3f; // meters
	const float pitch_offset = 0.0; // degrees
	gl_RenderState.mProjectionMatrix = activeEye->getQuadInWorld(
		weapon_distance_meters, 
		weapon_width_meters, 
		false,
		pitch_offset);
	gl_RenderState.ApplyMatrices();


}

void OpenVRMode::UnAdjustPlayerSprites() const {
	viewwidth = cachedViewwidth;
	viewheight = cachedViewheight;
	viewwindowx = cachedViewwindowx;
	viewwindowy = cachedViewwindowy;
}

void OpenVRMode::AdjustCrossHair() const
{
	cached2DDrawer = GLRenderer->m2DDrawer;
	// Remove effect of screenblocks setting on crosshair position
	cachedViewheight = viewheight;
	cachedViewwindowy = viewwindowy;
	viewheight = SCREENHEIGHT;
	viewwindowy = 0;

	if (crossHairDrawer != nullptr) {
		// Hijack 2D drawing to our local crosshair drawer
		crossHairDrawer->Clear();
		GLRenderer->m2DDrawer = crossHairDrawer;
	}
}

void OpenVRMode::UnAdjustCrossHair() const
{
	viewheight = cachedViewheight;
	viewwindowy = cachedViewwindowy;
	if (cached2DDrawer)
		GLRenderer->m2DDrawer = cached2DDrawer;
	cached2DDrawer = nullptr;
}

/* virtual */
void OpenVRMode::Present() const {
	// TODO: For performance, don't render to the desktop screen here
	if (doRenderToDesktop) {
		GLRenderer->mBuffers->BindOutputFB();
		GLRenderer->ClearBorders();

		// Compute screen regions to use for left and right eye views
		int leftWidth = GLRenderer->mOutputLetterbox.width / 2;
		int rightWidth = GLRenderer->mOutputLetterbox.width - leftWidth;
		GL_IRECT leftHalfScreen = GLRenderer->mOutputLetterbox;
		leftHalfScreen.width = leftWidth;
		GL_IRECT rightHalfScreen = GLRenderer->mOutputLetterbox;
		rightHalfScreen.width = rightWidth;
		rightHalfScreen.left += leftWidth;

		GLRenderer->mBuffers->BindEyeTexture(0, 0);
		GLRenderer->DrawPresentTexture(leftHalfScreen, true);
		GLRenderer->mBuffers->BindEyeTexture(1, 0);
		GLRenderer->DrawPresentTexture(rightHalfScreen, true);
	}

	if (doRenderToHmd) 
	{
		leftEyeView.submitFrame(vrCompositor);
		rightEyeView.submitFrame(vrCompositor);
	}
}

static int mAngleFromRadians(double radians) 
{
	double m = std::round(65535.0 * radians / (2.0 * M_PI));
	return int(m);
}

void OpenVRMode::updateHmdPose(
	double hmdYawRadians, 
	double hmdPitchRadians, 
	double hmdRollRadians) const 
{
	hmdYaw = hmdYawRadians;
	double hmdpitch = hmdPitchRadians;
	double hmdroll = hmdRollRadians;

	double hmdYawDelta = 0;
	if (doTrackHmdYaw) {
		// Set HMD angle game state parameters for NEXT frame
		static double previousHmdYaw = 0;
		static bool havePreviousYaw = false;
		if (!havePreviousYaw) {
			previousHmdYaw = hmdYaw;
			havePreviousYaw = true;
		}
		hmdYawDelta = hmdYaw - previousHmdYaw;
		G_AddViewAngle(mAngleFromRadians(-hmdYawDelta));
		previousHmdYaw = hmdYaw;
	}

	/* */
	// Pitch
	if (doTrackHmdPitch) {
		double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
		double hmdPitchInDoom = -atan(tan(hmdpitch) / pixelstretch);
		double viewPitchInDoom = GLRenderer->mAngles.Pitch.Radians();
		double dPitch = 
			// hmdPitchInDoom
			-hmdpitch
			- viewPitchInDoom;
		G_AddViewPitch(mAngleFromRadians(-dPitch));
	}

	// Roll can be local, because it doesn't affect gameplay.
	if (doTrackHmdRoll)
		GLRenderer->mAngles.Roll = RAD2DEG(-hmdroll);

	// Late-schedule update to renderer angles directly, too
	if (doLateScheduledRotationTracking) {
		if (doTrackHmdPitch)
			GLRenderer->mAngles.Pitch = RAD2DEG(-hmdpitch);
		if (doTrackHmdYaw) {
			static double yawOffset = 0;


			// Late scheduled update of yaw angle reduces motion sickness
			//  * by lowering latency of view update after head motion
			//  * by ignoring lag and interpolation of the game view angle
			// Mostly rely on HMD to provide yaw angle, unless the discrepency gets large.
			// I'm not sure how to reason about which angle changes are from the HMD and
			// which are from the controllers. So here I'm assuming every discrepancy larger
			// than some cutoff comes from elsewhere. This is how we acheive rock solid
			// head tracking, at the expense of jerky controller turning.
			double hmdYawDegrees = RAD2DEG(hmdYaw);
			double gameYawDegrees = r_viewpoint.Angles.Yaw.Degrees;
			double currentOffset = gameYawDegrees - hmdYawDegrees;
			if ((gamestate == GS_LEVEL)
				&& (menuactive == MENU_Off))
			{
				// Predict current game view direction using hmd yaw change from previous time step
				static double previousGameYawDegrees = 0;
				static double previousHmdYawDelta = 0;
				double predictedGameYawDegrees = previousGameYawDegrees + RAD2DEG(previousHmdYawDelta);
				double predictionError = predictedGameYawDegrees - gameYawDegrees;
				while (predictionError > 180.0) predictionError -= 360.0;
				while (predictionError < -180.0) predictionError += 360.0;
				predictionError = std::abs(predictionError);
				if (predictionError > 0.1) {
					// looks like someone is turning using the controller, not just the HMD, so reset offset now
					yawOffset = currentOffset;
				}

				// 
				double discrepancy = yawOffset - currentOffset;
				while (discrepancy > 180.0) discrepancy -= 360.0;
				while (discrepancy < -180.0) discrepancy += 360.0;
				discrepancy = std::abs(discrepancy);
				if (discrepancy > 5.0) 
				{
					yawOffset = currentOffset;
				}

				previousGameYawDegrees = gameYawDegrees;
				previousHmdYawDelta = hmdYawDelta;
			}
			double viewYaw = hmdYawDegrees + yawOffset;
			while (viewYaw <= -180.0) 
				viewYaw += 360.0;
			while (viewYaw > 180.0) 
				viewYaw -= 360.0;
			r_viewpoint.Angles.Yaw = viewYaw;
		}
	}
}

/* virtual */
void OpenVRMode::SetUp() const
{
	super::SetUp();

	if (vrCompositor == nullptr)
		return;

	// Set VR-appropriate settings
	const bool doAdjustVrSettings = true;
	if (doAdjustVrSettings) {
		movebob = 0;
		gl_billboard_faces_camera = true;
		if (gl_multisample < 2)
			gl_multisample = 4;
	}

	if (gamestate == GS_LEVEL) {
		cachedScreenBlocks = screenblocks;
		screenblocks = 12; // always be full-screen during 3D scene render
	}
	else {
		// TODO: Draw a more interesting background behind the 2D screen
		for (int i = 0; i < 2; ++i) {
			GLRenderer->mBuffers->BindEyeFB(i);
			glClearColor(0.3f, 0.1f, 0.1f, 1.0f); // draw a dark red universe
			glClear(GL_COLOR_BUFFER_BIT);
		}
	}

	static TrackedDevicePose_t poses[k_unMaxTrackedDeviceCount];
	vrCompositor->WaitGetPoses(
		poses, k_unMaxTrackedDeviceCount, // current pose
		nullptr, 0 // future pose?
	);

	TrackedDevicePose_t& hmdPose0 = poses[k_unTrackedDeviceIndex_Hmd];

	if (hmdPose0.bPoseIsValid) {
		const HmdMatrix34_t& hmdPose = hmdPose0.mDeviceToAbsoluteTracking;
		HmdVector3d_t eulerAngles = eulerAnglesFromMatrix(hmdPose);
		// Printf("%.1f %.1f %.1f\n", eulerAngles.v[0], eulerAngles.v[1], eulerAngles.v[2]);
		updateHmdPose(eulerAngles.v[0], eulerAngles.v[1], eulerAngles.v[2]);
		leftEyeView.setCurrentHmdPose(&hmdPose0);
		rightEyeView.setCurrentHmdPose(&hmdPose0);
		// TODO: position tracking
	}
}

/* virtual */
void OpenVRMode::TearDown() const
{
	if (gamestate == GS_LEVEL) {
		screenblocks = cachedScreenBlocks;
	}
	super::TearDown();
}

/* virtual */
OpenVRMode::~OpenVRMode() 
{
	if (vrSystem != nullptr) {
		VR_ShutdownInternal();
		vrSystem = nullptr;
		vrCompositor = nullptr;
		leftEyeView.dispose();
		rightEyeView.dispose();
	}
	if (crossHairDrawer != nullptr) {
		delete crossHairDrawer;
		crossHairDrawer = nullptr;
	}
}

} /* namespace s3d */

#endif

