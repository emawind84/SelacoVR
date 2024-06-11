#pragma once

#include "resourcefile.h"

namespace FileSys {

//==========================================================================
//
// Zip Lump
//
//==========================================================================

struct FDirectoryLump : public FResourceLump
{
	FileReader NewReader() override;
	int FillCache() override;
	long ReadData(FileReader &reader, char *buffer) override;

	const char* mFullPath;
};


//==========================================================================
//
// Zip file
//
//==========================================================================

class FDirectory : public FResourceFile
{
	TArray<FDirectoryLump> Lumps;
	const bool nosubdir;

	int AddDirectory(const char* dirpath, LumpFilterInfo* filter, FileSystemMessageFunc Printf);
	void AddEntry(const char *fullpath, const char* relpath, int size);

public:
	FDirectory(const char * dirname, StringPool* sp, bool nosubdirflag = false);
	bool Open(LumpFilterInfo* filter, FileSystemMessageFunc Printf);
	virtual FResourceLump *GetLump(int no) { return ((unsigned)no < NumLumps)? &Lumps[no] : NULL; }
};

}