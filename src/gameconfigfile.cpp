/*
** gameconfigfile.cpp
** An .ini parser specifically for zdoom.ini
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OFf
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <stdio.h>

#include "gameconfigfile.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "c_bind.h"
#include "m_argv.h"
#include "cmdlib.h"
#include "version.h"
#include "m_misc.h"
#include "v_font.h"
#include "a_pickups.h"
#include "doomstat.h"
#include "gi.h"
#include "d_main.h"
#include "v_video.h"
#if !defined _MSC_VER && !defined __APPLE__
#include "i_system.h"  // for SHARE_DIR
#endif // !_MSC_VER && !__APPLE__

EXTERN_CVAR (Bool, con_centernotify)
EXTERN_CVAR (Int, msg0color)
EXTERN_CVAR (Color, color)
EXTERN_CVAR (Float, dimamount)
EXTERN_CVAR (Int, msgmidcolor)
EXTERN_CVAR (Int, msgmidcolor2)
EXTERN_CVAR (Bool, snd_pitched)
EXTERN_CVAR (Color, am_wallcolor)
EXTERN_CVAR (Color, am_fdwallcolor)
EXTERN_CVAR (Color, am_cdwallcolor)
EXTERN_CVAR (Bool, wi_percents)
EXTERN_CVAR (Int, gl_texture_hqresizemode)
EXTERN_CVAR (Int, gl_texture_hqresizemult)
EXTERN_CVAR (Int, vid_preferbackend)
EXTERN_CVAR (Float, vid_scale_custompixelaspect)
EXTERN_CVAR (Bool, vid_scale_linear)
EXTERN_CVAR(Float, m_sensitivity_x)
EXTERN_CVAR(Float, m_sensitivity_y)
EXTERN_CVAR(Int, adl_volume_model)
EXTERN_CVAR (Int, gl_texture_hqresize_targets)
EXTERN_CVAR(Int, wipetype)
EXTERN_CVAR(Bool, i_pauseinbackground)
EXTERN_CVAR(Bool, i_soundinbackground)

#ifdef _WIN32
EXTERN_CVAR(Int, in_mouse)
#endif

FGameConfigFile::FGameConfigFile ()
{
#ifdef __APPLE__
	FString user_docs, user_app_support, local_app_support;
	M_GetMacSearchDirectories(user_docs, user_app_support, local_app_support);
#endif

	FString pathname;

	OkayToWrite = false;	// Do not allow saving of the config before DoKeySetup()
	bModSetup = false;
	bRequiresReset = false;
	pathname = GetConfigPath (true);
	ChangePathName (pathname.GetChars());
	LoadConfigFile ();

	// If zdoom.ini was read from the program directory, switch
	// to the user directory now. If it was read from the user
	// directory, this effectively does nothing.
	pathname = GetConfigPath (false);
	ChangePathName (pathname.GetChars());

	// Set default IWAD search paths if none present
	if (!SetSection ("IWADSearch.Directories"))
	{
		SetSection ("IWADSearch.Directories", true);
		SetValueForKey ("Path", ".", true);
		//SetValueForKey ("Path", "$SELACOWADDIR", true);
#ifdef __APPLE__
		SetValueForKey ("Path", user_docs.GetChars(), true);
		SetValueForKey ("Path", user_app_support.GetChars(), true);
		SetValueForKey ("Path", "$PROGDIR", true);
		SetValueForKey ("Path", local_app_support.GetChars(), true);
#elif __ANDROID__
		SetValueForKey ("Path", "./wads", true);
#elif !defined(__unix__)
		SetValueForKey ("Path", "$HOME", true);
		SetValueForKey ("Path", "$PROGDIR", true);
#else
		SetValueForKey ("Path", "$HOME/" GAME_DIR, true);
		SetValueForKey ("Path", "$HOME/.local/share/games/doom", true);
		// Arch Linux likes them in /usr/share/doom
		// Debian likes them in /usr/share/games/doom
		// I assume other distributions don't do anything radically different
		SetValueForKey ("Path", "/usr/local/share/doom", true);
		SetValueForKey ("Path", "/usr/local/share/games/doom", true);
		SetValueForKey ("Path", "/usr/share/doom", true);
		SetValueForKey ("Path", "/usr/share/games/doom", true);
#endif
	}

	// Set default search paths if none present
	if (!SetSection ("FileSearch.Directories"))
	{
		SetSection ("FileSearch.Directories", true);
		SetValueForKey ("Path", ".", true);
#ifdef __APPLE__
		SetValueForKey ("Path", user_docs.GetChars(), true);
		SetValueForKey ("Path", user_app_support.GetChars(), true);
		SetValueForKey ("Path", "$PROGDIR", true);
		SetValueForKey ("Path", local_app_support.GetChars(), true);
#elif __ANDROID__
		SetValueForKey ("Path", "./mods", true);
#elif !defined(__unix__)
		SetValueForKey ("Path", "$PROGDIR", true);
#else
		SetValueForKey ("Path", "$HOME/" GAME_DIR, true);
		SetValueForKey ("Path", "$HOME/.local/share/games/doom", true);
		SetValueForKey ("Path", SHARE_DIR, true);
		SetValueForKey ("Path", "/usr/local/share/doom", true);
		SetValueForKey ("Path", "/usr/local/share/games/doom", true);
		SetValueForKey ("Path", "/usr/share/doom", true);
		SetValueForKey ("Path", "/usr/share/games/doom", true);
#endif
		//SetValueForKey ("Path", "$SELACOWADDIR", true);
	}

	// Set default search paths if none present
	if (!SetSection("SoundfontSearch.Directories"))
	{
		SetSection("SoundfontSearch.Directories", true);
#ifdef __APPLE__
		SetValueForKey("Path", (user_docs + "/soundfonts").GetChars(), true);
		SetValueForKey("Path", (user_docs + "/fm_banks").GetChars(), true);
		SetValueForKey("Path", (user_app_support + "/soundfonts").GetChars(), true);
		SetValueForKey("Path", (user_app_support + "/fm_banks").GetChars(), true);
		SetValueForKey("Path", "$PROGDIR/soundfonts", true);
		SetValueForKey("Path", "$PROGDIR/fm_banks", true);
		SetValueForKey("Path", (local_app_support + "/soundfonts").GetChars(), true);
		SetValueForKey("Path", (local_app_support + "/fm_banks").GetChars(), true);
#elif __ANDROID__
		SetValueForKey("Path", "./soundfonts", true);
		SetValueForKey("Path", "./fm_banks", true);
#elif !defined(__unix__)
		SetValueForKey("Path", "$PROGDIR/soundfonts", true);
		SetValueForKey("Path", "$PROGDIR/fm_banks", true);
#else
		SetValueForKey("Path", "$HOME/" GAME_DIR "/soundfonts", true);
		SetValueForKey("Path", "$HOME/" GAME_DIR "/fm_banks", true);
		SetValueForKey("Path", "$HOME/.local/share/games/doom/soundfonts", true);
		SetValueForKey("Path", "$HOME/.local/share/games/doom/fm_banks", true);
		SetValueForKey("Path", "/usr/local/share/doom/soundfonts", true);
		SetValueForKey("Path", "/usr/local/share/doom/fm_banks", true);
		SetValueForKey("Path", "/usr/local/share/games/doom/soundfonts", true);
		SetValueForKey("Path", "/usr/local/share/games/doom/fm_banks", true);
		SetValueForKey("Path", "/usr/share/doom/soundfonts", true);
		SetValueForKey("Path", "/usr/share/doom/fm_banks", true);
		SetValueForKey("Path", "/usr/share/games/doom/soundfonts", true);
		SetValueForKey("Path", "/usr/share/games/doom/fm_banks", true);
#endif
	}

	// Add some self-documentation.
	SetSectionNote("IWADSearch.Directories",
		"# These are the directories to automatically search for IWADs.\n"
		"# Each directory should be on a separate line, preceded by Path=\n");
	SetSectionNote("FileSearch.Directories",
		"# These are the directories to search for wads added with the -file\n"
		"# command line parameter, if they cannot be found with the path\n"
		"# as-is. Layout is the same as for IWADSearch.Directories\n");
	SetSectionNote("SoundfontSearch.Directories",
		"# These are the directories to search for soundfonts that let listed in the menu.\n"
		"# Layout is the same as for IWADSearch.Directories\n");
}

FGameConfigFile::~FGameConfigFile ()
{
}

void FGameConfigFile::WriteCommentHeader (FileWriter *file) const
{
	file->Printf ("# This file was generated by " GAMENAME " %s on %s\n", GetVersionString(), myasctime());
}

void FGameConfigFile::DoAutoloadSetup (FIWadManager *iwad_man)
{
	// Create auto-load sections, so users know what's available.
	// Note that this totem pole is the reverse of the order that
	// they will appear in the file.

	double last = 0;
	if (SetSection ("LastRun"))
	{
		const char *lastver = GetValueForKey ("Version");
		if (lastver != NULL) last = atof(lastver);
	}

	const FString *pAuto;
	for (int num = 0; (pAuto = iwad_man->GetAutoname(num)) != NULL; num++)
	{
		if (!(iwad_man->GetIWadFlags(num) & GI_SHAREWARE))	// we do not want autoload sections for shareware IWADs (which may have an autoname for resource filtering)
		{
			FString workname = *pAuto;

			while (workname.IsNotEmpty())
			{
				FString section = workname + ".Autoload";
				CreateSectionAtStart(section.GetChars());
				auto dotpos = workname.LastIndexOf('.');
				if (dotpos < 0) break;
				workname.Truncate(dotpos);
			}
		}
	}
	CreateSectionAtStart("Global.Autoload");

	// The same goes for auto-exec files.
	CreateStandardAutoExec("Selaco.AutoExec", true);

	// Move search paths back to the top.
	MoveSectionToStart("SoundfontSearch.Directories");
	MoveSectionToStart("FileSearch.Directories");
	MoveSectionToStart("IWADSearch.Directories");

	SetSectionNote("Selaco.AutoExec",
		"# Files to automatically execute when running the corresponding game.\n"
		"# Each file should be on its own line, preceded by Path=\n\n");
	SetSectionNote("Global.Autoload",
		"# WAD files to always load. These are loaded after the IWAD but before\n"
		"# any files added with -file. Place each file on its own line, preceded\n"
		"# by Path=\n");
}

void FGameConfigFile::DoGlobalSetup ()
{
	if (SetSection ("GlobalSettings.Unknown"))
	{
		ReadCVars (CVAR_GLOBALCONFIG);
	}
	if (SetSection ("GlobalSettings"))
	{
		ReadCVars (CVAR_GLOBALCONFIG);
	}
	if (SetSection ("LastRun"))
	{
		const char *lastver = GetValueForKey ("Version");
		double last = 0, target = atof(LASTRUNVERSION);
		if (lastver != NULL)
		{
			last = atof(lastver);
		}

		if (last < target) {
			
			if (last < 224) {
				bRequiresReset = true;
			}
			
			if (last < 225)
			if (last < 223)
			{
				FBaseCVar *var = FindCVar("storesavepic", NULL);
				if (var != NULL) var->ResetToDefault();
				var = FindCVar("gl_max_lights", NULL);
				if (var != NULL) var->ResetToDefault();
				var = FindCVar("gl_light_shadowmap", NULL);
				if (var != NULL) var->ResetToDefault();
				var = FindCVar("gl_sprite_distance_cull", NULL);
				if (var != NULL) var->ResetToDefault();
				var = FindCVar("gl_line_distance_cull", NULL);
				if (var != NULL) var->ResetToDefault();
				var = FindCVar("r_sprite_distance_cull", NULL);
				if (var != NULL) var->ResetToDefault();
				var = FindCVar("r_sprite_distance_cull", NULL);
				if (var != NULL) var->ResetToDefault();
			}
			if (last < 224)
			{
				if (const auto var = FindCVar("m_sensitivity_x", NULL))
				{
					UCVarValue v = var->GetGenericRep(CVAR_Float);
					v.Float *= 0.5f;
					var->SetGenericRep(v, CVAR_Float);
				}
			}
			if (last < 224)
			{
				FBaseCVar *var = FindCVar("vr_move_use_offhand", NULL);
				if (var != NULL) var->ResetToDefault();
			}
			if (last < 225)
			{
				if (const auto var = FindCVar("gl_lightmode", NULL))
				{
					UCVarValue v = var->GetGenericRep(CVAR_Int);
					v.Int = v.Int == 16 ? 2 : v.Int == 8 ? 1 : 0;
					var->SetGenericRep(v, CVAR_Int);
				}
			}
			if (last < 225)
			{
				FBaseCVar *var = FindCVar("fluid_patchset", NULL);
				if (var != NULL) var->ResetToDefault();
				var = FindCVar("timidity_config", NULL);
				if (var != NULL) var->ResetToDefault();
				var = FindCVar("midi_config", NULL);
				if (var != NULL) var->ResetToDefault();
			}
			if (last < 226)
			{
				FBaseCVar *var = FindCVar("gl_billboard_faces_camera", NULL);
				if (var != NULL) var->ResetToDefault();
				var = FindCVar("storesavepic", NULL);
				if (var != NULL) var->ResetToDefault();
				var = FindCVar("gl_max_lights", NULL);
				if (var != NULL) var->ResetToDefault();
			}
		}
	}
}

void FGameConfigFile::DoGameSetup (const char *gamename)
{
	const char *key;
	const char *value;

	sublen = countof(section) - 1 - mysnprintf (section, countof(section), "%s.", gamename);
	subsection = section + countof(section) - sublen - 1;
	section[countof(section) - 1] = '\0';
	
	strncpy (subsection, "UnknownConsoleVariables", sublen);
	if (SetSection (section))
	{
		ReadCVars (0);
	}

	strncpy (subsection, "ConfigOnlyVariables", sublen);
	if (SetSection (section))
	{
		ReadCVars (0);
	}

	strncpy (subsection, "ConsoleVariables", sublen);
	if (SetSection (section))
	{
		ReadCVars (0);
	}

	if (gameinfo.gametype & GAME_Raven)
	{
		SetRavenDefaults (gameinfo.gametype == GAME_Hexen);
	}

	if (gameinfo.gametype & GAME_Strife)
	{
		SetStrifeDefaults ();
	}

	// The NetServerInfo section will be read and override anything loaded
	// here when it's determined that a netgame is being played.
	strncpy (subsection, "LocalServerInfo", sublen);
	if (SetSection (section))
	{
		ReadCVars (0);
	}

	strncpy (subsection, "Player", sublen);
	if (SetSection (section))
	{
		ReadCVars (0);
	}

	strncpy (subsection, "ConsoleAliases", sublen);
	if (SetSection (section))
	{
		const char *name = NULL;
		while (NextInSection (key, value))
		{
			if (stricmp (key, "Name") == 0)
			{
				name = value;
			}
			else if (stricmp (key, "Command") == 0 && name != NULL)
			{
				C_SetAlias (name, value);
				name = NULL;
			}
		}
	}
}

// Moved from DoGameSetup so that it can happen after wads are loaded
void FGameConfigFile::DoKeySetup(const char *gamename)
{
	static const struct { const char *label; FKeyBindings *bindings; } binders[] =
	{
		{ "Bindings", &Bindings },
		{ "DoubleBindings", &DoubleBindings },
		{ "AutomapBindings", &AutomapBindings },
		{ NULL, NULL }
	};
	const char *key, *value;

	sublen = countof(section) - 1 - mysnprintf(section, countof(section), "%s.", gamename);
	subsection = section + countof(section) - sublen - 1;
	section[countof(section) - 1] = '\0';

	C_SetDefaultBindings ();

	for (int i = 0; binders[i].label != NULL; ++i)
	{
		strncpy(subsection, binders[i].label, sublen);
		if (SetSection(section))
		{
			FKeyBindings *bindings = binders[i].bindings;
			bindings->UnbindAll();
			while (NextInSection(key, value))
			{
				bindings->DoBind(key, value);
			}
		}
	}
	OkayToWrite = true;
}

void FGameConfigFile::FinishStartup() {
	if (bRequiresReset) {
		C_SetCVarsToDefaults();
		bRequiresReset = false;
	}
}

// Like DoGameSetup(), but for mod-specific cvars.
// Called after CVARINFO has been parsed.
void FGameConfigFile::DoModSetup(const char *gamename)
{
	mysnprintf(section, countof(section), "%s.Player.Mod", gamename);
	if (SetSection(section))
	{
		ReadCVars(CVAR_MOD|CVAR_USERINFO|CVAR_IGNORE);
	}
	mysnprintf(section, countof(section), "%s.LocalServerInfo.Mod", gamename);
	if (SetSection (section))
	{
		ReadCVars (CVAR_MOD|CVAR_SERVERINFO|CVAR_IGNORE);
	}
	mysnprintf(section, countof(section), "%s.ConfigOnlyVariables.Mod", gamename);
	if (SetSection (section))
	{
		ReadCVars (CVAR_MOD|CVAR_CONFIG_ONLY|CVAR_IGNORE);
	}
	// Signal that these sections should be rewritten when saving the config.
	bModSetup = true;
}

void FGameConfigFile::ReadNetVars ()
{
	strncpy (subsection, "NetServerInfo", sublen);
	if (SetSection (section))
	{
		ReadCVars (0);
	}
	if (bModSetup)
	{
		mysnprintf(subsection, sublen, "NetServerInfo.Mod");
		if (SetSection(section))
		{
			ReadCVars(CVAR_MOD|CVAR_SERVERINFO|CVAR_IGNORE);
		}
	}
}

// Read cvars from a cvar section of the ini. Flags are the flags to give
// to newly-created cvars that were not already defined.
void FGameConfigFile::ReadCVars (uint32_t flags)
{
	const char *key, *value;
	FBaseCVar *cvar;
	UCVarValue val;

	flags |= CVAR_ARCHIVE|CVAR_UNSETTABLE|CVAR_AUTO;
	while (NextInSection (key, value))
	{
		cvar = FindCVar (key, NULL);
		if (cvar == NULL)
		{
			cvar = new FStringCVar (key, NULL, flags);
		}
		val.String = const_cast<char *>(value);
		cvar->SetGenericRep (val, CVAR_String);
	}
}

void FGameConfigFile::ArchiveGameData (const char *gamename)
{
	char section[32*3], *subsection;

	sublen = countof(section) - 1 - mysnprintf (section, countof(section), "%s.", gamename);
	subsection = section + countof(section) - 1 - sublen;

	strncpy (subsection, "Player", sublen);
	SetSection (section, true);
	ClearCurrentSection ();
	C_ArchiveCVars (this, CVAR_ARCHIVE|CVAR_USERINFO);

	if (bModSetup)
	{
		strncpy (subsection + 6, ".Mod", sublen - 6);
		SetSection (section, true);
		ClearCurrentSection ();
		C_ArchiveCVars (this, CVAR_MOD|CVAR_ARCHIVE|CVAR_AUTO|CVAR_USERINFO);
	}

	strncpy (subsection, "ConsoleVariables", sublen);
	SetSection (section, true);
	ClearCurrentSection ();
	C_ArchiveCVars (this, CVAR_ARCHIVE);

	// Do not overwrite the serverinfo section if playing a netgame, and
	// this machine was not the initial host.
	if (!netgame || consoleplayer == 0)
	{
		strncpy (subsection, netgame ? "NetServerInfo" : "LocalServerInfo", sublen);
		SetSection (section, true);
		ClearCurrentSection ();
		C_ArchiveCVars (this, CVAR_ARCHIVE|CVAR_SERVERINFO);

		if (bModSetup)
		{
			strncpy (subsection, netgame ? "NetServerInfo.Mod" : "LocalServerInfo.Mod", sublen);
			SetSection (section, true);
			ClearCurrentSection ();
			C_ArchiveCVars (this, CVAR_MOD|CVAR_ARCHIVE|CVAR_AUTO|CVAR_SERVERINFO);
		}
	}

	strncpy (subsection, "ConfigOnlyVariables", sublen);
	SetSection (section, true);
	ClearCurrentSection ();
	C_ArchiveCVars (this, CVAR_ARCHIVE|CVAR_AUTO|CVAR_CONFIG_ONLY);

	if (bModSetup)
	{
		strncpy (subsection, "ConfigOnlyVariables.Mod", sublen);
		SetSection (section, true);
		ClearCurrentSection ();
		C_ArchiveCVars (this, CVAR_ARCHIVE|CVAR_AUTO|CVAR_MOD|CVAR_CONFIG_ONLY);
	}

	strncpy (subsection, "UnknownConsoleVariables", sublen);
	SetSection (section, true);
	ClearCurrentSection ();
	C_ArchiveCVars (this, CVAR_ARCHIVE|CVAR_AUTO);

	strncpy (subsection, "ConsoleAliases", sublen);
	SetSection (section, true);
	ClearCurrentSection ();
	C_ArchiveAliases (this);

	M_SaveCustomKeys (this, section, subsection, sublen);

	strcpy (subsection, "Bindings");
	SetSection (section, true);
	Bindings.ArchiveBindings (this);

	strncpy (subsection, "DoubleBindings", sublen);
	SetSection (section, true);
	DoubleBindings.ArchiveBindings (this);

	strncpy (subsection, "AutomapBindings", sublen);
	SetSection (section, true);
	AutomapBindings.ArchiveBindings (this);
}

void FGameConfigFile::ArchiveGlobalData ()
{
	SetSection ("LastRun", true);
	ClearCurrentSection ();
	SetValueForKey ("Version", LASTRUNVERSION);

	SetSection ("GlobalSettings", true);
	ClearCurrentSection ();
	C_ArchiveCVars (this, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

	SetSection ("GlobalSettings.Unknown", true);
	ClearCurrentSection ();
	C_ArchiveCVars (this, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_AUTO);
}

FString FGameConfigFile::GetConfigPath (bool tryProg)
{
	const char *pathval;

	if (Args != NULL)
	{
		pathval = Args->CheckValue ("-config");
		if (pathval != NULL)
		{
			return FString(pathval);
		}
	}
	return M_GetConfigPath(tryProg);
}

void FGameConfigFile::CreateStandardAutoExec(const char *section, bool start)
{
	if (!SetSection(section))
	{
		FString path = M_GetAutoexecPath();
		SetSection (section, true);
		SetValueForKey ("Path", path.GetChars());
	}
	if (start)
	{
		MoveSectionToStart(section);
	}
}

void FGameConfigFile::AddAutoexec (FArgs *list, const char *game)
{
	char section[64];
	const char *key;
	const char *value;

	mysnprintf (section, countof(section), "%s.AutoExec", game);

	// If <game>.AutoExec section does not exist, create it
	// with a default autoexec.cfg file present.
	CreateStandardAutoExec(section, false);
	// Run any files listed in the <game>.AutoExec section
	if (!SectionIsEmpty())
	{
		while (NextInSection (key, value))
		{
			if (stricmp (key, "Path") == 0 && *value != '\0')
			{
				FString expanded_path = ExpandEnvVars(value);
				if (FileExists(expanded_path))
				{
					list->AppendArg (ExpandEnvVars(value));
				}
			}
		}
	}
}

void FGameConfigFile::SetRavenDefaults (bool isHexen)
{
	UCVarValue val;

	val.Bool = false;
	wi_percents->SetGenericRepDefault (val, CVAR_Bool);
	val.Bool = true;
	con_centernotify->SetGenericRepDefault (val, CVAR_Bool);
	snd_pitched->SetGenericRepDefault (val, CVAR_Bool);
	val.Int = 9;
	msg0color->SetGenericRepDefault (val, CVAR_Int);
	val.Int = CR_WHITE;
	msgmidcolor->SetGenericRepDefault (val, CVAR_Int);
	val.Int = CR_YELLOW;
	msgmidcolor2->SetGenericRepDefault (val, CVAR_Int);

	val.Int = 0x543b17;
	am_wallcolor->SetGenericRepDefault (val, CVAR_Int);
	val.Int = 0xd0b085;
	am_fdwallcolor->SetGenericRepDefault (val, CVAR_Int);
	val.Int = 0x734323;
	am_cdwallcolor->SetGenericRepDefault (val, CVAR_Int);

	val.Int = 0;
	wipetype->SetGenericRepDefault(val, CVAR_Int);

	// Fix the Heretic/Hexen automap colors so they are correct.
	// (They were wrong on older versions.)
	if (*am_wallcolor == 0x2c1808 && *am_fdwallcolor == 0x887058 && *am_cdwallcolor == 0x4c3820)
	{
		am_wallcolor->ResetToDefault ();
		am_fdwallcolor->ResetToDefault ();
		am_cdwallcolor->ResetToDefault ();
	}

	if (!isHexen)
	{
		val.Int = 0x3f6040;
		color->SetGenericRepDefault (val, CVAR_Int);
	}
}

void FGameConfigFile::SetStrifeDefaults ()
{
	UCVarValue val;
	val.Int = 3;
	wipetype->SetGenericRepDefault(val, CVAR_Int);
}

CCMD (whereisini)
{
	FString path = M_GetConfigPath(false);
	Printf ("%s\n", path.GetChars());
}
