/*
** hw_cvars.cpp
**
** most of the hardware renderer's CVARs.
**
**---------------------------------------------------------------------------
** Copyright 2005-2020 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
**
*/ 



#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "hw_cvars.h"
#include "menu.h"
#include "printf.h"
#include "version.h"

CUSTOM_CVAR(Int, gl_fogmode, 2, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	if (self > 2) self = 2;
	if (self < 0) self = 0;
}

// @Cockatrice - Toggle background texture fetching when supported
CVAR(Bool, gl_texture_thread, true, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)

// @Cockatrice - Enable upload inside the texture thread (when available), or force upload to happen in main thread (debugging, old hardware etc)
CVAR(Bool, gl_texture_thread_upload, true, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)

// @Cockatrice - Controls how many background loaded textures are re-integrated every tick
// Especially on cards that have to create mipmaps in the main thread, this number can't be too high
// or we get choppy when too many things are loading at once
// Keeping this number too low will result in visible sprite popping
CUSTOM_CVAR(Int, gl_background_flush_count, 100, CVAR_GLOBALCONFIG | CVAR_ARCHIVE) {
	if (self < 25) self = 25;
}

// OpenGL stuff moved here
// GL related CVARs
CVAR(Bool, gl_portals, true, 0)
CVAR(Bool, gl_mirrors, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR(Bool, gl_mirror_player, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR(Bool,gl_mirror_envmap, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR(Bool, gl_seamless, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Int, r_mirror_recursions,2,CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	if (self<0) self=0;
	if (self>10) self=10;
}
bool gl_plane_reflection_i;	// This is needed in a header that cannot include the CVAR stuff...
CUSTOM_CVAR(Bool, gl_plane_reflection, false, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	gl_plane_reflection_i = self;
}

CUSTOM_CVARD(Float, vid_gamma, 1.2f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "adjusts gamma component of gamma ramp")
{
	if (self < 0) self = 1;
	else if (self > 4) self = 4;
}

CUSTOM_CVARD(Float, vid_contrast, 1.1f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "adjusts contrast component of gamma ramp")
{
	if (self < 0) self = 0;
	else if (self > 5) self = 5;
}

CUSTOM_CVARD(Float, vid_brightness, 0.05f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "adjusts brightness component of gamma ramp")
{
	if (self < -2) self = -2;
	else if (self > 2) self = 2;
}

CUSTOM_CVARD(Float, vid_saturation, 1.2f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "adjusts saturation component of gamma ramp")
{
	if (self < -3) self = -3;
	else if (self > 3) self = 3;
}

CCMD (bumpgamma)
{
	// [RH] Gamma correction tables are now generated on the fly for *any* gamma level
	// Q: What are reasonable limits to use here?

	float newgamma = vid_gamma + 0.1f;

	if (newgamma > 4.0)
		newgamma = 1.0;

	vid_gamma = newgamma;
	Printf ("Gamma correction level %g\n", newgamma);
}


CVAR(Int, gl_satformula, 1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

//==========================================================================
//
// Texture CVARs
//
//==========================================================================
CUSTOM_CVARD(Float, gl_texture_filter_anisotropic, 4.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL, "changes the OpenGL texture anisotropy setting")
{
	screen->SetTextureFilterMode();
}

CUSTOM_CVARD(Int, gl_texture_filter, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL, "changes the texture filtering settings")
{
	if (self < 0 || self > 6) self=4;
	screen->SetTextureFilterMode();
}

extern void hw_unloadQualitySprites();
CUSTOM_CVARD(Int, gl_texture_quality, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL, "changes texture quality. 0 = full, 2 = low")
{
	if (self < 0 || self > 4) self = 0;
	hw_unloadQualitySprites();
	screen->SetTextureFilterMode();	// For Vulkan, rebuild descriptors
}

CVAR(Bool, gl_precache, true, CVAR_ARCHIVE)
CVAR(Bool, gl_precache_actors, true, CVAR_ARCHIVE)

CUSTOM_CVAR(Int, gl_shadowmap_filter, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0 || self > 8) self = 1;
}

CVAR(Bool, gl_global_fade, false, CVAR_ARCHIVE)

CUSTOM_CVAR(Float, gl_global_fade_density, 0.001f, CVAR_ARCHIVE)
{
	if (self < 0.0001f) self = 0.0001f;
	if (self > 0.005f) self = 0.005f;
}
CUSTOM_CVAR(Float, gl_global_fade_gradient, 1.5f, CVAR_ARCHIVE)
{
	if (self < 0.1f) self = 0.1f;
	if (self > 2.f) self = 2.f;
}
CVAR(Color, gl_global_fade_color, 0x3f3f3f, CVAR_ARCHIVE)
CVAR(Bool, gl_global_fade_debug, false, 0)

CUSTOM_CVAR (Int, gl_storage_buffer_type, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("You must restart " GAMENAME " for this change to take effect.\n");
}

CVARD(Bool, gl_no_persistent_buffer, false, 0, "Disable persistent buffer storage support")
CVARD(Bool, gl_no_clip_planes, false, 0, "Disable clip planes support")
CVARD(Bool, gl_no_ssbo, false, 0, "Disable SSBO support")