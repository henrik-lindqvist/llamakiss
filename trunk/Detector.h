/*
 * Detector.h
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
#ifndef __DETECTOR_H__
#define __DETECTOR_H__

#include "stdafx.h"

#include "Common.h"

/* Client (player) detector.
 * Input:
 *   ARE_YOU_KISS_PCLINK_SERVER?
 * Output:
 *   ServerName
 */
class Detector : public OVERLAPPED
{
public:
	DWORD Id;
// functions:
	static void Completed (BOOL status, DWORD transferred, Detector* detector);
	Detector (DWORD id, SOCKET receiver, HANDLE port);
	~Detector () {}
	void IssueReceive ();
	void CompletedReceive (DWORD received);
	void IssueTransmit ();
	void CompletedTransmit (DWORD transmitted);
private:
	enum PENDING {
		PENDING_NOTHING		= 0,
		PENDING_RECEIVE		= 1,
		PENDING_TRANSMIT	= 2,
	} Pending;
	SOCKET Socket;
	_TCHAR DatagramBuffer[DEFAULT_DATAGRAM_BUFFER_SIZE];
	DWORD DatagramFlags;
	WSABUF Datagram;
	SOCKADDR_STORAGE Address;
	int AddressSize;
// functions:
	void SetPending (PENDING pending);
};

#endif // !__DETECTOR_H__
