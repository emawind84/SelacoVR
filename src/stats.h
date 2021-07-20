/*
** stats.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
*/

#ifndef __STATS_H__
#define __STATS_H__

#include "zstring.h"
#include "i_time.h"


class cycle_t
{
public:
	void Reset()
	{
		Counter = 0;
	}
	
	void Clock()
	{
		int64_t time = I_nsTime();
		Counter -= time;
	}
	
	void Unclock()
	{
		int64_t time = I_nsTime();
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

	int64_t GetRawCounter()
	{
		return Counter;
	}

private:
	int64_t Counter;
};

class FStat
{
public:
	FStat (const char *name);
	virtual ~FStat ();

	virtual FString GetStats () = 0;

	void ToggleStat ();
	bool isActive() const
	{
		return m_Active;
	}

	static void PrintStat ();
	static FStat *FindStat (const char *name);
	static void ToggleStat (const char *name);
	static void DumpRegisteredStats ();

private:
	FStat *m_Next;
	const char *m_Name;
	bool m_Active;

	static FStat *FirstStat;
};

#define ADD_STAT(n) \
	static class Stat_##n : public FStat { \
		public: \
			Stat_##n () : FStat (#n) {} \
		FString GetStats (); } Istaticstat##n; \
	FString Stat_##n::GetStats ()

#endif //__STATS_H__
