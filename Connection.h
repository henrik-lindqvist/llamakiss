/*
 * Connection.h
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
#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "stdafx.h"
#include "Common.h"

class Command;

/* A client connection
 */
class Connection : public OVERLAPPED
{
public:
	DWORD Id;
	_TCHAR* ReceiveBuffer;
	DWORD ReceiveSize;
	DWORD ReceiveCapacity;
// functions:
	static void Completed (BOOL status, DWORD transferred, Connection* connection);
	Connection (DWORD id, SOCKET listener, HANDLE port);
	~Connection ();
	void Reset ();
	Command* SetCommand (Command* command);
	Command* GetCommand ();
	void IssueAccept ();
	void IssueReceive ();
#ifdef WINXP
	void IssueTransmit (TRANSMIT_PACKETS_ELEMENT* packets);
#else
	void IssueTransmit (TRANSMIT_FILE_BUFFERS& buffers);
#endif
	void IssueTransmit (HANDLE file, LARGE_INTEGER& offset, LARGE_INTEGER& length);
	void IssueDisconnect ();
	BOOL Parse ();

protected:
	BOOL AllowClient ();
	void CompletedAccept ();
	void CompletedReceive (DWORD received);
	void CompletedTransmit (DWORD transmitted);
	void CompletedDisconnect ();

private:
	enum PENDING {
		PENDING_NOTHING		= 0,
		PENDING_ACCEPT		= 1,
		PENDING_RECEIVE		= 2,
		PENDING_TRANSMIT	= 3,
		PENDING_DISCONNECT	= 4,
	} Pending;
	SOCKET Listener;
	SOCKET Socket;
#ifdef USE_COMMTIMEOUTS
	COMMTIMEOUTS Timeouts;
#endif // !USE_COMMTIMEOUTS
	Command* Command;
// functions:
	void ClearOverlapped ();
	void SetTimeout (DWORD ms);
};

#endif // !__CONNECTION_H__