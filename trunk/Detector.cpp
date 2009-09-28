/*
 * Detector.cpp
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

#include "Detector.h"
#include "Common.h"
#include "SockAddrSet.h"
#include "RuntimeException.h"

extern LogFunction Log;
extern SockAddrSet* DetectedPlayers;

// public:
void Detector::Completed (BOOL status, DWORD transferred, Detector* detector)
{
	try
	{
		if (status == 0)
		{
			throw RuntimeException("GetQueuedCompletionStatus");
		}
		if (detector)
		{
			switch (detector->Pending)
			{
			case PENDING_RECEIVE:
				detector->CompletedReceive(transferred);
				break;
			case PENDING_TRANSMIT:
				detector->CompletedTransmit(transferred);
				break;
			}
		}
	}
	catch (...)
	{
		detector->Pending = PENDING_NOTHING;
		detector->IssueReceive();
		throw;
	}
}

Detector::Detector (DWORD id, SOCKET receiver, HANDLE port) 
	: Id(id), Pending(PENDING_NOTHING), Socket(receiver)
{
	// OVERLAPPED
	Internal = 0;
	InternalHigh = 0;
	Offset = 0;
	OffsetHigh = 0;
	hEvent = 0;

	// Detector
	ZeroMemory(&Datagram, sizeof(Datagram));
	Datagram.buf = DatagramBuffer;
	IssueReceive();
}

void Detector::IssueReceive ()
{
	SetPending(PENDING_RECEIVE);
	DatagramFlags = 0;
	ZeroMemory(&DatagramBuffer, sizeof(DatagramBuffer));
	Datagram.len = sizeof(DatagramBuffer);
	ZeroMemory(&Address, sizeof(Address));
	AddressSize = sizeof(Address);
	if (WSARecvFrom(Socket, &Datagram, 1, NULL, &DatagramFlags, 
					(SOCKADDR*)&Address, &AddressSize, this, NULL) == SOCKET_ERROR
		&& WSAGetLastError() != ERROR_IO_PENDING)
	{
		throw RuntimeException("WSARecvFrom", WSAGetLastError());
	}
}

void Detector::CompletedReceive (DWORD received)
{
	SetPending(PENDING_NOTHING);
	if (received == 27 && strncmp(DatagramBuffer, "ARE_YOU_KISS_PCLINK_SERVER?", 27) == 0)
	{
#ifdef _DEBUG
		Log(LOG_DEBUG, _T("\nDetector #%d: Detected Player: %s"),
			Id, inet_ntoa(((SOCKADDR_IN*)&Address)->sin_addr));
#endif // _DEBUG
		DetectedPlayers->Add(&Address);
		Datagram.len = sizeof(DatagramBuffer);
		if (GetComputerName(DatagramBuffer, &Datagram.len) == 0)
		{
			throw RuntimeException("GetComputerName");
		}
		IssueTransmit();
	}
	else
	{
		IssueReceive();
	}
}

void Detector::IssueTransmit ()
{
	SetPending(PENDING_TRANSMIT);
	if (WSASendTo(Socket, &Datagram, 1, NULL, 0, (SOCKADDR*)&Address, 
				  sizeof(Address), this, NULL) == SOCKET_ERROR
		&& WSAGetLastError() != ERROR_IO_PENDING)
	{
		throw RuntimeException("WSASendTo", WSAGetLastError());
	}
}

void Detector::CompletedTransmit (DWORD transmitted)
{
	SetPending(PENDING_NOTHING);
	IssueReceive();
}

// private:
void Detector::SetPending (PENDING pending)
{
	if (Pending && pending)
	{
		throw RuntimeException("SetPending", ERROR_IO_PENDING);
	}
	Pending = pending;
}
