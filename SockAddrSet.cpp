/*
 * SockAddrSet.cpp
 * Copyright (C) 2007-2008 Henrik Lindqvist <henrik.lindqvist@llamalab.com> 
 *
 * This file is part of LlamaKISS.
 *
 * LlamaKISS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LlamaKISS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LlamaKISS.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "stdafx.h"

#include "SockAddrSet.h"
#include "RuntimeException.h"

#define HASH1(nb, s1, s2, h)	(_Mix((s1), (s2), (h)) % (nb))
#define HASH2(nb, s1, s2, h)	(_Mix((s2), (s1), (h)) % (nb))
#define MEANSIZE(nb)			((nb) * 5 / 12)
#define MINSIZE(nb)				((nb) / 5)
#define MAXCHAIN(nb)			(2 + (size_t)(log((double)(nb)) * 2.0 / log((double)2) + 0.5))
#define SOCKADDR_ULONG(x)		(((SOCKADDR_IN*)(x))->sin_addr.S_un.S_addr)

static inline DWORD _Mix (DWORD a, DWORD b, DWORD c) {
  a -= b; a -= c; a ^= (c >> 13);
  b -= c; b -= a; b ^= (a <<  8);
  c -= a; c -= b; c ^= (b >> 13);
  a -= b; a -= c; a ^= (c >> 12);
  b -= c; b -= a; b ^= (a << 16);
  c -= a; c -= b; c ^= (b >>  5);
  a -= b; a -= c; a ^= (c >>  3);
  b -= c; b -= a; b ^= (a << 10);
  c -= a; c -= b; c ^= (b >> 15);
  return c;
}

static u_long _Insert (u_long* &bins, size_t &numBins, size_t &used, 
					   DWORD &seed1, DWORD &seed2, u_long &value)
{
	u_long tmp;
	size_t j, l, i = HASH1(numBins, seed1, seed2, value), limit = MAXCHAIN(numBins);
	for (l = 0; l < limit; l++)
	{
		tmp = bins[i];
		bins[i] = value;
		if (tmp == 0L)
		{ 
			used++;
			return 0;
		}
		value = tmp;
		j = HASH1(numBins, seed1, seed2, value);
		i = (i == j) ? HASH2(numBins, seed1, seed2, value) : j;
	}
	return value;
}


SockAddrSet::SockAddrSet (size_t initialCapacity)
	: Used(0), NumBins(initialCapacity), Seed1((DWORD)rand()), Seed2((DWORD)rand())
{
	if ((Bins = new u_long[NumBins]) == NULL)
	{
		// shouldn't happend
		throw new std::bad_alloc();
	}
	ZeroMemory(Bins, NumBins * sizeof(u_long));
	if ((MutexLock = CreateMutex(NULL, FALSE, "SockAddrSetLock")) == NULL)
	{
		throw new RuntimeException("CreateMutex");
	}
}

SockAddrSet::~SockAddrSet ()
{
	if (Bins != NULL)
	{
		delete Bins;
	}
	if (MutexLock != NULL)
	{
		CloseHandle(MutexLock);
	}
}

BOOL SockAddrSet::Contains (SOCKADDR_STORAGE* addr)
{
	u_long value = SOCKADDR_ULONG(addr);
	size_t i = HASH1(NumBins, Seed1, Seed2, value);
	WaitForLock();
	__try
	{
		if (Bins[i] == value)
		{
			return TRUE;
		}
		i = HASH2(NumBins, Seed1, Seed2, value);
		if (Bins[i] == value)
		{
			return TRUE;
		}
		return FALSE;
	}
	__finally
	{
		ReleaseLock();
	}
}

void SockAddrSet::Add (SOCKADDR_STORAGE* addr)
{
	u_long value = SOCKADDR_ULONG(addr);
	size_t i = HASH1(NumBins, Seed1, Seed2, value);
	WaitForLock();
	__try
	{
		if (Bins[i] == value)
		{
			return;
		}
		i = HASH2(NumBins, Seed1, Seed2, value);
		if (Bins[i] == value)
		{
			return;
		}
		while ((value = _Insert(Bins, NumBins, Used, Seed1, Seed2, value)) != 0L) 
		{
			Rehash((Used < MEANSIZE(NumBins)) ? NumBins : (NumBins << 1));
		}
	}
	__finally
	{
		ReleaseLock();
	}
}

void SockAddrSet::Rehash (size_t numBins)
{
	u_long* bins;
	DWORD seed1;
	DWORD seed2;
	size_t i, used;
init:
//	Debug();
	if ((bins = new u_long[numBins]) == NULL)
	{
		throw new std::bad_alloc();
	}
	try 
	{
		ZeroMemory(bins, numBins * sizeof(u_long));
		seed1 = (DWORD)rand();
		seed2 = (DWORD)rand();
		used = 0;
		for (i = 0; i < NumBins; i++)
		{
			if (Bins[i] != 0L && _Insert(bins, numBins, used, seed1, seed2, Bins[i])) 
			{
				delete bins;
				numBins <<= 1;
				goto init;
			}
		}
		delete Bins;
		Bins = bins;
		NumBins = numBins;
		Seed1 = seed1;
		Seed2 = seed2;
	}
	catch (...)
	{
		delete bins;
		throw;
	}
}

BOOL SockAddrSet::WaitForLock ()
{
	switch (WaitForSingleObject(MutexLock, INFINITE))
	{
		case WAIT_OBJECT_0:
			return TRUE;
		case WAIT_ABANDONED: 
			return FALSE;
//		case WAIT_FAILED:
		default:
			throw new RuntimeException("WaitForSingleObject");
	}
}

void SockAddrSet::ReleaseLock ()
{
	if (ReleaseMutex(MutexLock) == 0)
	{
		throw new RuntimeException("ReleaseMutex");
	}
}

/*
void SockAddrSet::Debug ()
{
	_tprintf(_T("Usage: %d of %d (%f%%)\n"), Used, NumBins, 100.0 * (((double)Used) / ((double)NumBins)));
}
*/