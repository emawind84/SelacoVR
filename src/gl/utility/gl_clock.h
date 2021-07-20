#ifndef __GL_CLOCK_H
#define __GL_CLOCK_H

#include "stats.h"
#include "i_time.h"

extern bool gl_benching;

class glcycle_t
{
public:
	glcycle_t &operator= (const glcycle_t &o)
	{
		Counter = o.Counter;
		return *this;
	}

	void Reset()
	{
		Counter = 0;
	}
	
	__forceinline void Clock()
	{
		// Not using QueryPerformanceCounter directly, so we don't need
		// to pull in the Windows headers for every single file that
		// wants to do some profiling.
		int64_t time = (gl_benching? I_nsTime() : 0);
		Counter -= time;
	}
	
	__forceinline void Unclock()
	{
		int64_t time = (gl_benching? I_nsTime() : 0);
		Counter += time;
	}
	
	double Time()
	{
		return double(Counter) / 1'000'000'000;
	}
	
	double TimeMS()
	{
		return double(Counter) / 1'000'000;
	}

private:
	int64_t Counter;
};

extern glcycle_t RenderWall,SetupWall,ClipWall;
extern glcycle_t RenderFlat,SetupFlat;
extern glcycle_t RenderSprite,SetupSprite;
extern glcycle_t All, Finish, PortalAll, Bsp;
extern glcycle_t ProcessAll, PostProcess;
extern glcycle_t RenderAll;
extern glcycle_t Dirty;
extern glcycle_t drawcalls;

extern int iter_dlightf, iter_dlight, draw_dlight, draw_dlightf;
extern int rendered_lines,rendered_flats,rendered_sprites,rendered_decals,render_vertexsplit,render_texsplit;
extern int rendered_portals;

extern int vertexcount, flatvertices, flatprimitives;

void ResetProfilingData();
void CheckBench();
void CheckBenchActive();


#endif
