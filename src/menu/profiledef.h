#pragma once

struct FCommandLineInfo
{
    FString mName;
	FString mTitle;
    FString mPath;
};

class ProfileManager
{
    TArray<FCommandLineInfo> cmdlineProfiles;
    
    void ProcessOneProfileFile(const FString &filePath);
    
public:
    void CollectProfiles();
    const auto &GetList() const { return cmdlineProfiles; } // This is for the menu
    const FCommandLineInfo *GetProfileInfo(const char *profileName);
};

extern ProfileManager profileManager;