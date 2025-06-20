/*
** gl_hwtexture.cpp
** GL texture abstraction
**
**---------------------------------------------------------------------------
** Copyright 2019 Christoph Oelckers
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

#include "gl_system.h"

#include "c_cvars.h"
#include "hw_material.h"

#include "gl_interface.h"
#include "hw_cvars.h"
#include "gl_debug.h"
#include "gl_renderer.h"
#include "gl_renderstate.h"
#include "gl_samplers.h"
#include "gl_hwtexture.h"

#include "image.h"
#include "filesystem.h"

namespace OpenGLRenderer
{


TexFilter_s TexFilter[] = {
	{GL_NEAREST,					GL_NEAREST,		false},
	{GL_NEAREST_MIPMAP_NEAREST,		GL_NEAREST,		true},
	{GL_LINEAR,						GL_LINEAR,		false},
	{GL_LINEAR_MIPMAP_NEAREST,		GL_LINEAR,		true},
	{GL_LINEAR_MIPMAP_LINEAR,		GL_LINEAR,		true},
	{GL_NEAREST_MIPMAP_LINEAR,		GL_NEAREST,		true},
	{GL_LINEAR_MIPMAP_LINEAR,		GL_NEAREST,		true},
};

//===========================================================================
// 
//	Static texture data
//
//===========================================================================
unsigned int FHardwareTexture::lastbound[FHardwareTexture::MAX_TEXTURES];

//===========================================================================
// 
//	Loads the texture image into the hardware
//
// NOTE: For some strange reason I was unable to find the source buffer
// should be one line higher than the actual texture. I got extremely
// strange crashes deep inside the GL driver when I didn't do it!
//
//===========================================================================

unsigned int FHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, const char *name)
{
	int rh,rw;
	int texformat = GL_RGBA8;// TexFormat[gl_texture_format];
	bool deletebuffer=false;

	/*
	if (forcenocompression)
	{
		texformat = GL_RGBA8;
	}
	*/
	bool firstCall = glTexID == 0;
	if (firstCall)
	{
		glGenTextures(1, &glTexID);
	}

	int textureBinding = UINT_MAX;
	if (texunit == -1)	glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding);
	if (texunit > 0) glActiveTexture(GL_TEXTURE0+texunit);
	if (texunit >= 0) lastbound[texunit] = glTexID;
	glBindTexture(GL_TEXTURE_2D, glTexID);

	FGLDebug::LabelObject(GL_TEXTURE, glTexID, name);

	rw = GetTexDimension(w);
	rh = GetTexDimension(h);
	if (glBufferID > 0)
	{
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		buffer = nullptr;
	}
	else if (!buffer)
	{
		// The texture must at least be initialized if no data is present.
		mipmapped = false;
		buffer=(unsigned char *)calloc(4,rw * (rh+1));
		deletebuffer=true;
		//texheight=-h;	
	}
	else
	{
		if (rw < w || rh < h)
		{
			// The texture is larger than what the hardware can handle so scale it down.
			unsigned char * scaledbuffer=(unsigned char *)calloc(4,rw * (rh+1));
			if (scaledbuffer)
			{
				Resize(w, h, rw, rh, buffer, scaledbuffer);
				deletebuffer=true;
				buffer=scaledbuffer;
			}
		}
	}
	// store the physical size.

	int sourcetype;
	if (glTextureBytes > 0)
	{
		if (glTextureBytes < 4) glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		static const int ITypes[] = { GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 };
		static const int STypes[] = { GL_RED, GL_RG, GL_BGR, GL_BGRA };

		texformat = ITypes[glTextureBytes - 1];
		sourcetype = STypes[glTextureBytes - 1];
	}
	else
	{
		sourcetype = GL_BGRA;
	}
#ifdef __MOBILE__
    texformat = sourcetype = GL_BGRA;
#endif
	if (!firstCall && glBufferID > 0)
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rw, rh, sourcetype, GL_UNSIGNED_BYTE, buffer);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, texformat, rw, rh, 0, sourcetype, GL_UNSIGNED_BYTE, buffer);

	if (deletebuffer && buffer) free(buffer);
	else if (glBufferID)
	{
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}

	if (mipmap && TexFilter[gl_texture_filter].mipmapping && !forcenofilter)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
		mipmapped = true;
	}

	if (texunit > 0) glActiveTexture(GL_TEXTURE0);
	else if (texunit == -1) glBindTexture(GL_TEXTURE_2D, textureBinding);

	SetHardwareState(HardwareState::READY, texunit);

	return glTexID;
}


bool FHardwareTexture::SwapToLoadedImage() {
	if (glInfo.glTexID > 0) {
		assert(glTexID == 0 && glBufferID == 0);

		glTexID = glInfo.glTexID;
		glTextureBytes = glInfo.glTextureBytes;
		mipmapped = glInfo.mipmapped;
		forcenofilter = glInfo.forcenofilter;

		glInfo = GLLoadInfo();	// Reset
		
		return true;
	}

	return false;
}

// Remove background loaded image, probably because it finished loading too late
void FHardwareTexture::DestroyLoadedImage() {
	if (glInfo.glTexID > 0 && glInfo.glTexID != glTexID) {
		glDeleteTextures(1, &glInfo.glTexID);
	}
	glInfo = GLLoadInfo();
}

// Add a BC7 compressed texture and mipmaps
unsigned int FHardwareTexture::BackgroundCreateCompressedTexture(unsigned char* buffer, uint32_t dataSize, uint32_t totalSize, int w, int h, int texunit, int numMips, const char* name, bool forceNoMips, bool allowQualityReduction) {
	int rh, rw;

	glGetError();
	bool firstCall = glInfo.glTexID == 0;
	if (firstCall)
	{
		glGenTextures(1, &glInfo.glTexID);
	}
	auto err = glGetError();
	assert(err == GL_NO_ERROR);
	assert(firstCall);
	assert(texunit >= 0);

	if (texunit > 0) glActiveTexture(GL_TEXTURE0 + texunit);
	glBindTexture(GL_TEXTURE_2D, glInfo.glTexID);

	FGLDebug::LabelObject(GL_TEXTURE, glInfo.glTexID, name);

	rw = GetTexDimension(w);
	rh = GetTexDimension(h);

	if (rw < w || rh < h) {
		return 0;	// We can't resize the texture, 
	}

	bool mipmap = numMips > 1 && !forceNoMips;

	uint32_t mipWidth = w, mipHeight = h;
	size_t mipSize = dataSize, dataPos = 0;
	const int startMip = allowQualityReduction ? min((int)gl_texture_quality, (int)numMips - 1) : 0;
	int mipCnt = 0, maxMips = mipmap ? numMips - startMip : 1;

	for (uint32_t x = 0; x < numMips && mipCnt < maxMips; x++) {
		if (mipSize == 0 || totalSize - dataPos < mipSize)
			break;

		if (x >= startMip) {
			mipCnt++;
			if (x == startMip) {
				// Base texture
				glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, mipWidth, mipHeight, 0, mipSize, buffer + dataPos);
			}
			else {
				glInfo.mipmapped = true;
				glInfo.forcenofilter = false;

				// Mip
				CreateCompressedMipmap(glInfo.glTexID, buffer + dataPos, x - startMip, mipWidth, mipHeight, mipSize, texunit);
			}
		}

		dataPos += mipSize;

		mipWidth = std::max(1u, (mipWidth >> 1));
		mipHeight = std::max(1u, (mipHeight >> 1));
		mipSize = std::max(1u, ((mipWidth + 3) / 4)) * std::max(1u, ((mipHeight + 3) / 4)) * 16;
	}

	/*glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, rw, rh, 0, dataSize, buffer);

	// Always load mips when available, since loading them later is impossible without another disk fetch and huge waste of time
	if (numMips > 0 && !forceNoMips) {
		glInfo.mipmapped = true;
		glInfo.forcenofilter = false;

		// Create each mip level if possible
		uint32_t mipWidth = w, mipHeight = h;
		uint32_t mipSize = (uint32_t)dataSize, dataPos = (uint32_t)dataSize;

		for (int x = 1; x < numMips; x++) {
			mipWidth = std::max(1u, (mipWidth >> 1));
			mipHeight = std::max(1u, (mipHeight >> 1));
			mipSize = std::max(1u, ((mipWidth + 3) / 4)) * std::max(1u, ((mipHeight + 3) / 4)) * 16;
			if (mipSize == 0 || totalSize - dataPos < mipSize)
				break;
			CreateCompressedMipmap(glInfo.glTexID, buffer + dataPos, x, mipWidth, mipHeight, mipSize, texunit);
			dataPos += mipSize;
		}
	}
	else if (forceNoMips) {
		glInfo.forcenofilter = true;
	}*/

	if (forceNoMips) {
		glInfo.forcenofilter = true;
	}

	if (texunit > 0) glActiveTexture(GL_TEXTURE0);

	glFinish();

	return glTexID;
}


unsigned int FHardwareTexture::CreateCompressedTexture(unsigned char* buffer, uint32_t dataSize, uint32_t totalSize, int w, int h, int texunit, int numMips, const char* name, bool forceNoMips, bool allowQualityReduction) {
	int rh, rw;

	glGetError();
	bool firstCall = glTexID == 0;
	if (firstCall)
	{
		glGenTextures(1, &glTexID);
	}
	auto err = glGetError();
	assert(err == GL_NO_ERROR);
	assert(firstCall);

	int textureBinding = UINT_MAX;
	if (texunit == -1)	glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding);
	if (texunit > 0) glActiveTexture(GL_TEXTURE0 + texunit);
	if (texunit >= 0) lastbound[texunit] = glTexID;
	glBindTexture(GL_TEXTURE_2D, glTexID);

	FGLDebug::LabelObject(GL_TEXTURE, glTexID, name);

	rw = GetTexDimension(w);
	rh = GetTexDimension(h);

	if (rw < w || rh < h) {
		return 0;	// We can't resize the texture, 
	}

	/*glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, rw, rh, 0, dataSize, buffer);

	// Always load mips when available, since loading them later is impossible without another disk fetch and huge waste of time
	if (numMips > 0 && !forceNoMips) {
		// Create each mip level if possible
		uint32_t mipWidth = w, mipHeight = h;
		uint32_t mipSize = (uint32_t)dataSize, dataPos = (uint32_t)dataSize;

		for (int x = 1; x < numMips; x++) {
			mipWidth = std::max(1u, (mipWidth >> 1));
			mipHeight = std::max(1u, (mipHeight >> 1));
			mipSize = std::max(1u, ((mipWidth + 3) / 4)) * std::max(1u, ((mipHeight + 3) / 4)) * 16;
			if (mipSize == 0 || totalSize - dataPos < mipSize)
				break;
			CreateCompressedMipmap(glTexID, buffer + dataPos, x, mipWidth, mipHeight, mipSize, texunit);
			dataPos += mipSize;
		}
		mipmapped = true;
	}
	else if (forceNoMips) {
		forcenofilter = true;
	}*/
	bool mipmap = numMips > 1 && !forceNoMips;

	uint32_t mipWidth = w, mipHeight = h;
	size_t mipSize = dataSize, dataPos = 0;
	const int startMip = allowQualityReduction ? min((int)gl_texture_quality, (int)numMips - 1) : 0;
	int mipCnt = 0, maxMips = mipmap ? numMips - startMip : 1;

	for (uint32_t x = 0; x < numMips && mipCnt < maxMips; x++) {
		if (mipSize == 0 || totalSize - dataPos < mipSize)
			break;

		if (x >= startMip) {
			mipCnt++;
			if (x == startMip) {
				// Base texture
				glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, mipWidth, mipHeight, 0, mipSize, buffer + dataPos);
			}
			else {
				glInfo.mipmapped = true;
				glInfo.forcenofilter = false;

				// Mip
				CreateCompressedMipmap(glInfo.glTexID, buffer + dataPos, x - startMip, mipWidth, mipHeight, mipSize, texunit);
			}
		}

		dataPos += mipSize;

		mipWidth = std::max(1u, (mipWidth >> 1));
		mipHeight = std::max(1u, (mipHeight >> 1));
		mipSize = (size_t)std::max(1u, ((mipWidth + 3) / 4)) * std::max(1u, ((mipHeight + 3) / 4)) * 16;
	}

	if (forceNoMips) {
		glInfo.forcenofilter = true;
	}

	if (texunit > 0) glActiveTexture(GL_TEXTURE0);
	else if (texunit == -1) glBindTexture(GL_TEXTURE_2D, textureBinding);

	return glTexID;
}


// @Cockatrice - For now I am naively assuming that sprite textures do not ever use more than one texunit.
// I do not yet understand why we use different texture units for material layers, perhaps it's because the tex unit
// is used as a slot in the shader? 
// This code will have to be significantly refactored for thread safety if it turns out there will be more than one texture load per glTexID
unsigned int FHardwareTexture::BackgroundCreateTexture(unsigned char* buffer, int w, int h, int texunit, bool mipmap, bool indexed, const char* name, bool forceNoMips)
{
	// See todotodo.txt
	int rh, rw;
	int texformat = GL_RGBA8;
	bool deletebuffer = false;

	glGetError();
	bool firstCall = glInfo.glTexID == 0;
	if (firstCall)
	{
		glGenTextures(1, &glInfo.glTexID);
	}
	auto err = glGetError();
	assert(err == GL_NO_ERROR);
	assert(firstCall);

	if (texunit > 0) glActiveTexture(GL_TEXTURE0 + texunit);
	glBindTexture(GL_TEXTURE_2D, glInfo.glTexID);

	FGLDebug::LabelObject(GL_TEXTURE, glInfo.glTexID, name);

	rw = GetTexDimension(w);
	rh = GetTexDimension(h);
	
	if (rw < w || rh < h)
	{
		// The texture is larger than what the hardware can handle so scale it down.
		unsigned char* scaledbuffer = (unsigned char*)calloc(4, rw * (rh + 1));
		if (scaledbuffer)
		{
			Resize(w, h, rw, rh, buffer, scaledbuffer);
			deletebuffer = true;
			buffer = scaledbuffer;
		}
	}

	// store the physical size.
	int sourcetype;
	if (indexed)
	{
		mipmap = false;
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		texformat = GL_R8;
		sourcetype = GL_RED;
	}
	else
	{
		sourcetype = GL_BGRA;
	}


	glTexImage2D(GL_TEXTURE_2D, 0, texformat, rw, rh, 0, sourcetype, GL_UNSIGNED_BYTE, buffer);

	if (deletebuffer && buffer) free(buffer);

	if (mipmap && TexFilter[gl_texture_filter].mipmapping && !forceNoMips)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
		glInfo.mipmapped = true;
	}
	else if (forceNoMips) {
		glInfo.forcenofilter = true;
	}

	if (texunit > 0) glActiveTexture(GL_TEXTURE0);

	glFinish();

	return glTexID;
}


bool FHardwareTexture::CreateCompressedMipmap(unsigned int texID, unsigned char* buffer, int mipLevel, int w, int h, int32_t size, int texunit) {
	int rh, rw;

	if (texID == 0)
		return false;

	// Commented out for now, the few times this is called, the texture unit and texID are already bound
	//if (texunit > 0) glActiveTexture(GL_TEXTURE0 + texunit);
	//glBindTexture(GL_TEXTURE_2D, texID);

	rw = GetTexDimension(w);
	rh = GetTexDimension(h);

	if (rw < w || rh < h)
		return false;	// We can't create a mipmap of this size, so just bail since we can't work with the pixel data to shrink it

	glCompressedTexImage2D(GL_TEXTURE_2D, mipLevel, GL_COMPRESSED_RGBA_BPTC_UNORM, w, h, 0, size, buffer);

	//if (texunit > 0) glActiveTexture(GL_TEXTURE0);

	return true;
}


//===========================================================================
// 
//
//
//===========================================================================
void FHardwareTexture::AllocateBuffer(int w, int h, int texelsize)
{
	int rw = GetTexDimension(w);
	int rh = GetTexDimension(h);
	if (texelsize < 1 || texelsize > 4) texelsize = 4;
	glTextureBytes = texelsize;
	bufferpitch = w;
	if (rw == w || rh == h)
	{
		glGenBuffers(1, &glBufferID);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, glBufferID);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, w*h*texelsize, nullptr, GL_STREAM_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#ifdef __MOBILE__
        size = w*h*texelsize;
#endif
	}
}


uint8_t *FHardwareTexture::MapBuffer()
{
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, glBufferID);
#ifdef __MOBILE__
    return (uint8_t*)glMapBufferRange (GL_PIXEL_UNPACK_BUFFER, 0, size, GL_MAP_WRITE_BIT );
#else
	return (uint8_t*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
#endif
}

//===========================================================================
// 
//	Destroys the texture
//
//===========================================================================
FHardwareTexture::~FHardwareTexture() 
{ 
	DestroyLoadedImage();
	if (glTexID != 0) glDeleteTextures(1, &glTexID);
	if (glBufferID != 0) glDeleteBuffers(1, &glBufferID);
}


//===========================================================================
// 
//	Binds this patch
//
//===========================================================================
unsigned int FHardwareTexture::Bind(int texunit, bool needmipmap)
{
	if (glTexID != 0)
	{
		if (lastbound[texunit] == glTexID) return glTexID;
		lastbound[texunit] = glTexID;
		if (texunit != 0) glActiveTexture(GL_TEXTURE0 + texunit);
		glBindTexture(GL_TEXTURE_2D, glTexID);
		// Check if we need mipmaps on a texture that was creted without them.
		if (needmipmap && !mipmapped && TexFilter[gl_texture_filter].mipmapping && !forcenofilter)
		{
			glGenerateMipmap(GL_TEXTURE_2D);
			mipmapped = true;
		}
		if (texunit != 0) glActiveTexture(GL_TEXTURE0);
		return glTexID;
	}
	return 0;
}

void FHardwareTexture::Unbind(int texunit)
{
	if (lastbound[texunit] != 0)
	{
		if (texunit != 0) glActiveTexture(GL_TEXTURE0+texunit);
		glBindTexture(GL_TEXTURE_2D, 0);
		if (texunit != 0) glActiveTexture(GL_TEXTURE0);
		lastbound[texunit] = 0;
	}
}

void FHardwareTexture::UnbindAll()
{
	for(int texunit = 0; texunit < 16; texunit++)
	{
		Unbind(texunit);
	}
}

//===========================================================================
// 
//	Creates a depth buffer for this texture
//
//===========================================================================

int FHardwareTexture::GetDepthBuffer(int width, int height)
{
	if (glDepthID == 0)
	{
		glGenRenderbuffers(1, &glDepthID);
		glBindRenderbuffer(GL_RENDERBUFFER, glDepthID);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 
			GetTexDimension(width), GetTexDimension(height));
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	return glDepthID;
}


//===========================================================================
// 
//	Binds this texture's surfaces to the current framrbuffer
//
//===========================================================================

void FHardwareTexture::BindToFrameBuffer(int width, int height)
{
	width = GetTexDimension(width);
	height = GetTexDimension(height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glTexID, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, GetDepthBuffer(width, height));
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, GetDepthBuffer(width, height));
}


//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

bool FHardwareTexture::BindOrCreate(FTexture *tex, int texunit, int clampmode, int translation, int flags)
{
	bool needmipmap = (clampmode <= CLAMP_XY) && !forcenofilter;

	// Bind it to the system.
	if (!Bind(texunit, needmipmap))
	{
		if (flags & CTF_Indexed)
		{
			glTextureBytes = 1;
			forcenofilter = true;
			needmipmap = false;
		}
		int w = 0, h = 0;

		// Create this texture
		FImageSource* src = tex->GetImage();
		if (src && src->IsGPUOnly()) {
			assert(glBufferID == 0);

			// Create a reader
			FileReader reader = fileSystem.OpenFileReader(src->LumpNum(), FileSys::EReaderType::READER_SHARED, 0);
			if (!reader.isOpen()) {
				Printf(TEXTCOLOR_RED "Lump: %s cannot be read: Uninitialized reader!\n", fileSystem.GetFileFullName(src->LumpNum(), false));
				SetHardwareState(HardwareState::READY, texunit);
				return false;
			}

			// Read and upload texture
			int numMipLevels;
			size_t dataSize = 0, totalSize = 0;
			unsigned char* pixelData;
			src->ReadCompressedPixels(&reader, &pixelData, totalSize, dataSize, numMipLevels);
			CreateCompressedTexture(pixelData, (uint32_t)dataSize, (uint32_t)totalSize, tex->GetWidth(), tex->GetHeight(), texunit, numMipLevels, "::BindOrCreate(Compressed)", !forcenofilter, flags & CTF_ReduceQuality);

			SetHardwareState(HardwareState::READY, texunit);
		}
		else {
			FTextureBuffer texbuffer;

			if (!tex->isHardwareCanvas())
			{
				texbuffer = tex->CreateTexBuffer(translation, flags | CTF_ProcessData);
				w = texbuffer.mWidth;
				h = texbuffer.mHeight;
			}
			else
			{
				w = tex->GetWidth();
				h = tex->GetHeight();
			}
			if (!CreateTexture(texbuffer.mBuffer, w, h, texunit, needmipmap, "FHardwareTexture.BindOrCreate"))
			{
				// could not create texture
				return false;
			}
		}
	}
	if (forcenofilter && clampmode <= CLAMP_XY) clampmode += CLAMP_NOFILTER - CLAMP_NONE;
	GLRenderer->mSamplerManager->Bind(texunit, clampmode, 255);
	return true;
}

}
