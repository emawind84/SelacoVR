#pragma once

#include <stdint.h>
#include "tarray.h"

typedef TMap<int, bool> SpriteHits;
class FTexture;

class IHardwareTexture
{
public:
	enum
	{
		MAX_TEXTURES = 16
	};

	enum HardwareState
	{
		NONE		= 0,	// Uninitialized
		LOADING		= 1,	// Waiting on texture load op
		UPLOADING	= 2,	// Waiting on transfer op
		READY		= 3		// Fully ready to render
	};

	IHardwareTexture() = default;
	virtual ~IHardwareTexture() = default;

	HardwareState GetState() { return hwState; }
	void SetHardwareState(HardwareState hws) { hwState = hws; }

	virtual void AllocateBuffer(int w, int h, int texelsize) = 0;
	virtual uint8_t *MapBuffer() = 0;
	virtual unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, const char *name) = 0;

	// @Cockatrice - Used to determine if the texture is available to used in rendering (loaded/uploaded)
	virtual bool IsValid() { return hwState == HardwareState::READY; }

	void Resize(int swidth, int sheight, int width, int height, unsigned char *src_data, unsigned char *dst_data);

	int GetBufferPitch() const { return bufferpitch; }

protected:
	int bufferpitch = -1;
	HardwareState hwState = NONE;
};
