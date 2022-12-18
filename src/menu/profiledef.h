#pragma once

struct FCommandLineInfo
{
    FString mName;
	FString mTitle;
};

class ProfileManager
{
    TArray<FCommandLineInfo> cmdlineProfiles;
    
    void ProcessOneProfileFile(const FString &name);
    
public:
    void CollectProfiles();
    const auto &GetList() const { return cmdlineProfiles; } // This is for the menu
    
};

extern ProfileManager profileManager;