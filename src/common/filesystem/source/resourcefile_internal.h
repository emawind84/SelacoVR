#pragma once

#include "resourcefile.h"
#include "zstring.h"

namespace FileSys {
struct FUncompressedLump : public FResourceLump
{
	int				Position;

	virtual FileReader *GetReader();
	virtual int FillCache() override;
	virtual int GetFileOffset() { return Position; }
	virtual long ReadData(FileReader &reader, char *buffer);
};

// Base class for uncompressed resource files (WAD, GRP, PAK and single lumps)
class FUncompressedFile : public FResourceFile
{
protected:
	TArray<FUncompressedLump> Lumps;

	FUncompressedFile(const char *filename, StringPool* sp);
	FUncompressedFile(const char *filename, FileReader &r, StringPool* sp);
	virtual FResourceLump *GetLump(int no) { return ((unsigned)no < NumLumps)? &Lumps[no] : NULL; }
};


// should only be used internally.
struct FExternalLump : public FResourceLump
{
	FString FileName;

	FExternalLump(const char *_filename, int filesize, StringPool* sp);
	virtual int FillCache() override;
	virtual long ReadData(FileReader &reader, char *buffer);
};

}