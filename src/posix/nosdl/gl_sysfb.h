#ifndef __POSIX_SDL_GL_SYSFB_H__
#define __POSIX_SDL_GL_SYSFB_H__

//#include <SDL.h>

#include "v_video.h"

void QzDoom_GetScreenRes(uint32_t *width, uint32_t *height);

class SystemGLFrameBuffer : public DFrameBuffer
{
	typedef DFrameBuffer Super;

public:
	// this must have the same parameters as the Windows version, even if they are not used!
	SystemGLFrameBuffer (void *hMonitor, bool fullscreen);
	~SystemGLFrameBuffer ();

	void ForceBuffering (bool force);

	bool IsFullscreen ();

	virtual void SetVSync( bool vsync );
	void SwapBuffers();
	
	void NewRefreshRate ();

	friend class SDLGLVideo;

	int GetClientWidth();
	int GetClientHeight();

	//SDL_Window *GetSDLWindow() { return Screen; }

	void setNULLContext();
	void setMainContext();
	void setAuxContext(int index);
	int numAuxContexts();

	virtual int GetTrueHeight() { return GetClientHeight(); }

protected:
	void SetGammaTable(uint16_t *tbl);
	void ResetGammaTable();
	void InitializeState();

	SystemGLFrameBuffer () {}
	uint8_t GammaTable[3][256];
	bool UpdatePending;

	//SDL_Window *Screen;

	//SDL_GLContext GLContext;

	void UpdateColors ();

	//Uint16 m_origGamma[3][256];
	bool m_supportsGamma;
};

#endif // __POSIX_SDL_GL_SYSFB_H__
