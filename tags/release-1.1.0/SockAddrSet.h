/*
 * SockAddrSet.h
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
#ifndef __SOCKADDRSET_H__
#define __SOCKADDRSET_H__

#include "stdafx.h"

class SockAddrSet 
{
public:
// functions:
	SockAddrSet (size_t initialCapacity = 512);
	~SockAddrSet ();
	void Add (SOCKADDR_STORAGE* addr);
	BOOL Contains (SOCKADDR_STORAGE* addr);
//	void Debug ();
	size_t Size ()
	{
		return Used;
	}
private:
	size_t Used;
	size_t NumBins;
	DWORD Seed1;
	DWORD Seed2;
	u_long* Bins;
	HANDLE MutexLock;
// functions:
	void Rehash (size_t newNumBins);
	BOOL WaitForLock ();
	void ReleaseLock ();
};

#endif // !__SOCKADDRSET_H__
