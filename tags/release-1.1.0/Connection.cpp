/*
 * Connection.cpp
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

#include "Connection.h"
#include "Command.h"
#include "Common.h"
#include "SockAddrSet.h"
#include "RuntimeException.h"

extern LogFunction Log;
extern CommandInitFunction AvailableCommands[];
extern _TCHAR ConfigFilename[];
extern SockAddrSet* DetectedPlayers;
extern HANDLE CommandDumpFile;
#ifdef USE_TRANSMITPACKETS
extern LPFN_TRANSMITPACKETS _TransmitPackets;
#endif // USE_TRANSMITPACKETS

void Connection::Completed (BOOL status, DWORD transferred, Connection* connection)
{
	try
	{
		if (status == 0)
		{
			switch (GetLastError())
			{
			case ERROR_ABANDONED_WAIT_0:
			case ERROR_NETNAME_DELETED:
			//case ERROR_HANDLE_EOF:
				if (connection)
				{
					connection->IssueDisconnect();
					return;
				}
				break;
			default:
				throw RuntimeException("GetQueuedCompletionStatus");
			}
		}
		if (connection)
		{
			switch (connection->Pending)
			{
			case PENDING_ACCEPT:
				connection->CompletedAccept();
				break;
			case PENDING_RECEIVE:
				connection->CompletedReceive(transferred);
				break;
			case PENDING_TRANSMIT:
				connection->CompletedTransmit(transferred);
				break;
			case PENDING_DISCONNECT:
				connection->CompletedDisconnect();
				break;
			}
		}
	}
	catch (...)
	{
		if (connection)
		{
			connection->IssueDisconnect(); 
		}
		throw;
	}
}

Connection::Connection (DWORD id, SOCKET listener, HANDLE port) 
	: Listener(listener), Socket(INVALID_SOCKET), 
	  Id(id), Command(NULL), Pending(PENDING_NOTHING),
	  ReceiveBuffer(NULL), ReceiveCapacity(DEFAULT_RECEIVE_BUFFER_SIZE), ReceiveSize(0)
{
#ifdef USE_COMMTIMEOUTS
	ZeroMemory(&Timeouts, sizeof(Timeouts));
#endif // !USE_COMMTIMEOUTS
	if ((ReceiveBuffer = (_TCHAR*)VirtualAlloc(NULL, ReceiveCapacity, MEM_COMMIT, PAGE_READWRITE)) == NULL)
	{
		throw RuntimeException("VirtualAlloc");
	}
	if ((Socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
	{
		throw RuntimeException("WSASocket", WSAGetLastError());
	}
    if (CreateIoCompletionPort((HANDLE)Socket, port, COMPLETED_CONNECTION, 0) == NULL)
	{
		throw RuntimeException("CreateIoCompletionPort");
	}
    IssueAccept();
}

Connection::~Connection ()
{
	if (Command != NULL)
	{
		delete Command;
	}
	if (Socket != INVALID_SOCKET)
	{
		shutdown(Socket, SD_BOTH);
		closesocket(Socket);
	}
	if (ReceiveBuffer != NULL)
	{
		VirtualFree(ReceiveBuffer, 0, MEM_RELEASE);
	}
}

void Connection::Reset ()
{
	Pending = PENDING_NOTHING;
	ClearOverlapped();
	ReceiveSize = 0;
    ZeroMemory(ReceiveBuffer, ReceiveCapacity);
	if (Command)
	{
		Command->Release();
		Command = NULL;
	}
}

Command* Connection::SetCommand (::Command* command)
{
	if (Command)
	{
		Command->Release();
	}
	Command = command;
	Command->SetConnection(this);
	return command;
}

Command* Connection::GetCommand ()
{
	return Command;
}


void Connection::ClearOverlapped ()
{
	ZeroMemory(this, sizeof(OVERLAPPED));
}

/*
void Connection::SetTimeout (DWORD ms)
{
#ifdef USE_COMMTIMEOUTS
	Timeouts.ReadIntervalTimeout = ms;
	Timeouts.ReadTotalTimeoutMultiplier = ms;
	Timeouts.ReadTotalTimeoutConstant = ms;
	if (SetCommTimeouts((HANDLE)Socket, &Timeouts) != 0)
	{
		throw RuntimeException("SetCommTimeouts");
	}
#else // !USE_COMMTIMEOUTS
	if (setsockopt(Socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&ms, sizeof(ms)) == SOCKET_ERROR)
	{
		throw RuntimeException("setsockopt", WSAGetLastError());
	}
#endif // !USE_COMMTIMEOUTS
}
*/

BOOL Connection::AllowClient ()
{
	_TCHAR tmp[8];
	SOCKADDR_STORAGE addr;
	int size = sizeof(addr);

	ZeroMemory(&addr, sizeof(addr));
	if (getpeername(Socket, (SOCKADDR*)&addr, &size) == SOCKET_ERROR)
	{
		throw RuntimeException("getpeername", WSAGetLastError());
	}
	GetPrivateProfileString("general", "DefaultClientPermission", "ALLOW", 
							(LPSTR)&tmp, sizeof(tmp), ConfigFilename);
	if (strcmp(tmp, "DETECT") == 0)
	{
		return DetectedPlayers->Contains(&addr);
	}
	GetPrivateProfileString("clients", inet_ntoa(((SOCKADDR_IN*)&addr)->sin_addr), tmp, 
							(LPSTR)&tmp, sizeof(tmp), ConfigFilename);
	return strcmp(tmp, "ALLOW") == 0;
}

void Connection::IssueAccept ()
{   
	Pending = PENDING_ACCEPT;
	ClearOverlapped();
	ReceiveSize = 0;
    if (AcceptEx(Listener, Socket, ReceiveBuffer, 0, ACCEPT_ADDRESS_LENGTH, 
				 ACCEPT_ADDRESS_LENGTH, &ReceiveSize, this) == FALSE
		&& WSAGetLastError() != ERROR_IO_PENDING)
	{
		throw RuntimeException("AcceptEx", WSAGetLastError());
	}
}

void Connection::CompletedAccept ()
{
	Pending = PENDING_NOTHING;
	ReceiveSize = 0;
	if (setsockopt(Socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&Listener, sizeof(SOCKET)) != 0)
	{
		throw RuntimeException("setsockopt", WSAGetLastError());
	}
#ifdef _DEBUG
	Log(LOG_DEBUG, _T("\nConnection #%d: Connected"), Id);
#endif // _DEBUG
	if (AllowClient())
	{
		//SetTimeout(0);
		IssueReceive();
	}
	else
	{
		IssueDisconnect();
	}
}

void Connection::IssueReceive ()
{
	Pending = PENDING_RECEIVE;
	ClearOverlapped();
	if (ReadFile((HANDLE)Socket, &ReceiveBuffer[ReceiveSize], ReceiveCapacity - ReceiveSize, NULL, this) == 0)
	{
		switch (GetLastError())
		{
		case ERROR_IO_PENDING:
			// async operation initialized successfully.
			break;
		case ERROR_HANDLE_EOF:
			IssueDisconnect();
			break;
		default:
			throw RuntimeException("ReadFile");
		}
	}
}

void Connection::CompletedReceive (DWORD received)
{
#ifdef _DEBUG
	Log(LOG_DEBUG, _T("\nConnection #%d: Received (%d + %d bytes)"), Id, ReceiveSize, received);
	//fwrite(&ReceiveBuffer[ReceiveSize], received, 1, stdout);
#endif // _DEBUG
	Pending = PENDING_NOTHING;
	if (CommandDumpFile != INVALID_HANDLE_VALUE)
	{
		DWORD written = 0;
		if (WriteFile(CommandDumpFile, &ReceiveBuffer[ReceiveSize], received, &written, NULL) == 0)
		{
			throw RuntimeException("WriteFile");
		}
	}
	ReceiveSize += received;
	if (Parse())
	{
		Command->Execute();
	}
	else if (received)
	{
		//SetTimeout(500);
		IssueReceive();
	}
	else
	{
		IssueDisconnect();
	}
}

#ifdef USE_TRANSMITPACKETS
void Connection::IssueTransmit (TRANSMIT_PACKETS_ELEMENT* packets)
{
	Pending = PENDING_TRANSMIT;
	if (_TransmitPackets(Socket, packets, 1, 0, this, TF_USE_KERNEL_APC) == FALSE
		&& WSAGetLastError() != ERROR_IO_PENDING)
	{
		throw RuntimeException("TransmitFile", WSAGetLastError());
	}
}
#else // !USE_TRANSMITPACKETS
void Connection::IssueTransmit (TRANSMIT_FILE_BUFFERS& buffers)
{
#ifdef _DEBUG
	Log(LOG_DEBUG, _T("\nConnection #%d: Transmitting Buffer (%d bytes)..."), 
		Id, buffers.HeadLength);
#endif // _DEBUG
	Pending = PENDING_TRANSMIT;
	ClearOverlapped();
	if (TransmitFile(Socket, 0, 0, 0, this, &buffers, 0) == FALSE
		&& WSAGetLastError() != WSA_IO_PENDING)
	{
		throw RuntimeException("TransmitFile", WSAGetLastError());
	}
}
#endif // !USE_TRANSMITPACKETS

void Connection::IssueTransmit (HANDLE file, LARGE_INTEGER& offset, LARGE_INTEGER& length)
{
#ifdef _DEBUG
	Log(LOG_DEBUG, _T("\nConnection #%d: Transmitting File (%lld @ %lld bytes)..."), 
		Id, length.QuadPart, offset.QuadPart);
#endif // _DEBUG
	if (length.HighPart)
	{
		throw RuntimeException("IssueTransmit", WSAEINVAL);
	}
	Pending = PENDING_TRANSMIT;
	ClearOverlapped();
	Offset = offset.LowPart;
	OffsetHigh = offset.HighPart;
	if (TransmitFile(Socket, file, length.LowPart, 0, this, NULL, 0) == FALSE
		&& WSAGetLastError() != WSA_IO_PENDING)
	{
		throw RuntimeException("TransmitFile", WSAGetLastError());
	}
	/*
	if (FlushFileBuffers((HANDLE)Socket) == 0)
	{
		throw RuntimeException("FlushFileBuffers");
	}
	*/
}

void Connection::CompletedTransmit (DWORD transmitted)
{
#ifdef _DEBUG
	Log(LOG_DEBUG, _T("\nConnection #%d: Transmitted (%d bytes)"), Id, transmitted);
#endif // _DEBUG
	Pending = PENDING_NOTHING;
	//SetTimeout(0);
	IssueReceive();
}

void Connection::IssueDisconnect()
{
	Pending = PENDING_DISCONNECT;
	ClearOverlapped();
	if (TransmitFile(Socket, NULL, 0, 0, this, NULL, TF_DISCONNECT | TF_REUSE_SOCKET) == FALSE
	    && WSAGetLastError() != WSA_IO_PENDING)
	{
		throw RuntimeException("TransmitFile", WSAGetLastError());
	}
}

void Connection::CompletedDisconnect()
{
#ifdef _DEBUG
	Log(LOG_DEBUG, _T("\nConnection #%d: Disconnected"), Id);
#endif // _DEBUG
	Pending = PENDING_NOTHING;
	Reset();
	IssueAccept();
}

BOOL Connection::Parse ()
{
	DWORD offset;
	size_t i = 0;
	while (AvailableCommands[i]) {
		if (Command = AvailableCommands[i++](this)) {
			if ((offset = Command->Parse()) == 0)
			{
				return FALSE;
			}
			ReceiveSize -= offset;
			memcpy_s(ReceiveBuffer, ReceiveCapacity, &ReceiveBuffer[offset], ReceiveSize);
			return TRUE;
		}
	}
	return FALSE;
}
