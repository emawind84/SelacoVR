#ifndef __FILE_ZIP_H
#define __FILE_ZIP_H

#include "resourcefile.h"

namespace FileSys {
//==========================================================================
//
// Zip Lump
//
//==========================================================================

struct FZipLump : public FResourceLump
{
	uint16_t	GPFlags;
	uint8_t	Method;
	bool	NeedFileStart;
	int		CompressedSize;
	int64_t		Position;
	unsigned CRC32;

	virtual FileReader *GetReader();
	//virtual FileReader CloneReader();
	virtual int FillCache() override;
	virtual long ReadData(FileReader &reader, char *buffer);

private:
	void SetLumpAddress();
	void SetLumpAddress(FileReader &reader);
	int GetLumpAddressOffset(FileReader &reader);
	virtual int GetFileOffset();
	FCompressedBuffer GetRawData();
};


//==========================================================================
//
// Zip file
//
//==========================================================================

class FZipFile : public FResourceFile
{
	FZipLump *Lumps;

public:
	FZipFile(const char * filename, FileReader &file, StringPool* sp);
	virtual ~FZipFile();
	bool Open(LumpFilterInfo* filter, FileSystemMessageFunc Printf);
	virtual FResourceLump *GetLump(int no) { return ((unsigned)no < NumLumps)? &Lumps[no] : NULL; }
};

}

#endif