#include "cmdlib.h"
#include "i_system.h"
#include "gameconfigfile.h"
#include "c_cvars.h"
#include "fs_findfile.h"
#include "findfile.h"
#include "profiledef.h"
#include <algorithm>

extern bool wantToRestart;

CUSTOM_CVAR(String, cmdlineprofile, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	wantToRestart = true;
}

ProfileManager profileManager;

void ProfileManager::ProcessOneProfileFile(const FString &name)
{
	auto fb = ExtractFileBase(name.GetChars(), false);
	long clidx = fb.IndexOf("commandline_", 0);
	if (clidx == 0)
	{
		fb.Remove(clidx, 12);
		for (auto &profile : cmdlineProfiles)
		{
			// We already got a profile with this name. Do not add again.
			if (!profile.mName.CompareNoCase(fb)) return;
		}
		FileReader fr;
		if (fr.OpenFile(name.GetChars()))
		{
			char head[100] = { 0};
			fr.Read(head, 100);
			FString titleTag = "#TITLE";
			if (!memcmp(head, titleTag.GetChars(), titleTag.Len()))
			{
				const long titleMaxLength = 50;
				FString title = FString(&head[7], std::min<size_t>(strcspn(&head[7], "\r\n"), titleMaxLength));
				FCommandLineInfo sft = { fb, title };
				cmdlineProfiles.Push(sft);
			}
			else
			{
				FCommandLineInfo sft = { fb, fb };
				cmdlineProfiles.Push(sft);
			}
		}
	}
}

void ProfileManager::CollectProfiles()
{
	TArray<FString> mSearchPaths;
	cmdlineProfiles.Clear();
	cmdlineProfiles.Push({"", "No profile"});

	if (GameConfig != NULL && GameConfig->SetSection ("FileSearch.Directories"))
	{
		const char *key;
		const char *value;

		while (GameConfig->NextInSection (key, value))
		{
			if (stricmp (key, "Path") == 0)
			{
				FString dir = NicePath(value);
				if (dir.Len() > 0) mSearchPaths.Push(dir);
			}
		}
	}
	
	// Add program root folder to the search paths
	FString dir = NicePath("$PROGDIR");
	if (dir.Len() > 0) mSearchPaths.Push(dir);

	dir = NicePath("$PROGDIR/profiles/");
	mSearchPaths.Push(dir);

	// Unify and remove trailing slashes
	for (auto &str : mSearchPaths)
	{
		FixPathSeperator(str);
		if (str.Back() == '/') str.Truncate(str.Len() - 1);
	}

	// Collect all profiles in the search path
	for (auto &dir : mSearchPaths)
	{
		FileSys::FileList list;
		if (FileSys::ScanDirectory(list, dir.GetChars(), "*", true))
		{
			for (auto& entry : list)
			{
				if (!entry.isDirectory)
				{
					ProcessOneProfileFile(entry.FilePath.c_str());
				}
			}
		}
	}

	std::sort(cmdlineProfiles.begin(), cmdlineProfiles.end(), 
	[](const FCommandLineInfo left, const FCommandLineInfo right){
		return std::tolower(*left.mTitle.GetChars()) < std::tolower(*right.mTitle.GetChars());
	});
}

void I_InitProfiles()
{
	profileManager.CollectProfiles();
}