#pragma once
#include "menu.h"
#include "savegamemanager.h"

struct FNewGameStartup
{
	bool hasPlayerClass;
	FString PlayerClass;
	int Episode;
	int Skill;
};

extern FNewGameStartup NewGameStartupInfo;
void M_StartupEpisodeMenu(FNewGameStartup *gs);
void M_StartupSkillMenu(FNewGameStartup *gs);
void M_CreateGameMenus();
void SetDefaultMenuColors();
void OnMenuOpen(bool makeSound);

class FSavegameManager : public FSavegameManagerBase
{
	void PerformSaveGame(const char *fn, const char *sgdesc) override;
	void PerformLoadGame(const char *fn, bool) override;
	FString ExtractSaveComment(FSerializer &arc) override;
	void ReadSaveStrings() override;
public:
	FString BuildSaveName(const char* prefix, int slot) override;
};

extern FSavegameManager savegameManager;

