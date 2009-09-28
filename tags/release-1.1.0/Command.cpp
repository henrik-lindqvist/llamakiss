/*
 * Command.cpp
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

#include "Command.h"
#include "Connection.h"
#include "Common.h"
#include "RuntimeException.h"

//#define CREATE_FILE_FLAGS	(FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED)
#define CREATE_FILE_FLAGS	(FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_OVERLAPPED)

extern LogFunction Log;
extern _TCHAR ConfigFilename[];

void Command::SetConnection (::Connection* connection)
{
	Connection = connection;
}

Command* Command::Reset ()
{
	return this;
}

Command* Command::Release ()
{
	Reset();
	Connection = NULL;
	return this;
}

BOOL Command::AllowPlayer (_TCHAR* playerID)
{
	_TCHAR tmp[8];
	GetPrivateProfileString("general", "DefaultPlayerPermission", "ALLOW", 
							(LPSTR)&tmp, sizeof(tmp), ConfigFilename);
	GetPrivateProfileString("players", playerID, tmp, 
							(LPSTR)&tmp, sizeof(tmp), ConfigFilename);
	return strcmp(tmp, "ALLOW") == 0;
}

BOOL Command::AllowFile (_TCHAR* media, _TCHAR* filename)
{
	_TCHAR tmp[MEDIA_LENGTH];
	_TCHAR* ext;
	int i;
	GetPrivateProfileString("general", "FilterMedia", "NO",
							(LPSTR)&tmp, sizeof(tmp), ConfigFilename);
	if (strcmp(tmp, "NO") == 0)
	{
		return TRUE;
	}
	if ((ext = PathFindExtension(filename)) == NULL)
	{
		return FALSE;
	}
	ZeroMemory(tmp, sizeof(tmp));
	for (i = 0, ext++; *ext; tmp[i++] = tolower(*ext++))
	{
		if (i == sizeof(tmp)-1)
		{
			return FALSE;
		}
	}
	GetPrivateProfileString("extensions", tmp, "",
							(LPSTR)&tmp, sizeof(tmp), ConfigFilename);
	return strcmp(tmp, media) == 0;
}

VOID Command::GetMediaPath (_TCHAR* path, _TCHAR* media, _TCHAR* location, _TCHAR* filename)
{
	_TCHAR tmp[_MAX_PATH];
	_TCHAR* ptr;
	_TCHAR* end;
	GetPrivateProfileSection(media, tmp, _MAX_PATH, ConfigFilename);
	for (ptr = tmp; *ptr; ptr = end + strlen(end) + 1) 
	{
		if ((end = strchr(ptr, '=')) == NULL)
		{
			end = ptr;
		}
		else 
		{
			*end++ = '\0';
			if (_stricmp(ptr, location) == 0)
			{
				ptr = end;
				break;
			}
		}
	}
	if (*ptr == '\0')
	{
		throw RuntimeException("GetMediaPath", ERROR_PATH_NOT_FOUND);
	}
	if (ExpandEnvironmentStrings(ptr, path, _MAX_PATH-2) == 0)
	{
		throw RuntimeException("ExpandEnvironmentStrings");
	}
	/*
	if (*ptr)
	{
		strcpy_s(path, _MAX_PATH, ptr);
	}
	else 
	{
		strcpy_s(path, _MAX_PATH, RootDirectory);
	}
	if (strncmp("%USERPROFILE%", path, 13) == 0)
	{
		if (GetEnvironmentVariable("USERPROFILE", tmp, _MAX_PATH) == 0)
		{
			throw RuntimeException("GetEnvironmentVariable");
		}
		if (PathAppend(tmp, &path[13]) == NULL)
		{
			throw RuntimeException("PathAppend");
		}
		strcpy_s(path, _MAX_PATH, tmp);
	}
	*/
	if (filename && *filename)
	{
		if (PathCanonicalize(tmp, filename) == FALSE)
		{
			throw RuntimeException("PathCanonicalize");
		}
		if (PathAppend(path, tmp) == NULL)
		{
			throw RuntimeException("PathAppend");
		}
	}
}

BOOL Command::Tokenize (DWORD& offset, TOK tok1, ...)
{
	va_list va;
	_TCHAR* ptr = &Connection->ReceiveBuffer[offset];
	_TCHAR* end = &Connection->ReceiveBuffer[Connection->ReceiveSize];
	_TCHAR* str = NULL;
	_TCHAR* tmp;
	TOK tok;
	size_t size;
	void* data;

	va_start(va, tok1);
	tok = tok1;
	for (;;)
	{
		switch (tok) 
		{
		case TOK_MATCH:
			if (!str) 
				str = va_arg(va, _TCHAR*);
			switch (va_arg(va, int))
			{
			case '=':
				//_tprintf("TOK_MATCH (=): \"%s\"\n", str);
				while (*str)
					if (ptr >= end || *str++ != *ptr++) 
						goto bad;
				break;
			case '1':
				//_tprintf("TOK_MATCH (1): \"%s\"\n", str);
				if (ptr >= end || !strchr(str, *ptr++)) 
					goto bad;
				break;
			case '?':
				//_tprintf("TOK_MATCH (?): \"%s\"\n", str);
				if (ptr < end && strchr(str, *ptr)) ptr++;
				break;
			case '+':
				//_tprintf("TOK_MATCH (+): \"%s\" 0x%02x\n", str, *ptr);
				tmp = ptr;
				while (ptr < end && strchr(str, *ptr)) ptr++;
				if (tmp == ptr) 
					goto bad;
				break;
			case '*':
				//_tprintf("TOK_MATCH (*): \"%s\"\n", str);
				while (ptr < end && strchr(str, *ptr)) ptr++;
				break;
			default:
				throw RuntimeException("Tokenize", ERROR_INVALID_PARAMETER);
			}
			tok = va_arg(va, TOK);
			str = NULL;
			break;
		case TOK_STRING:
			//_tprintf("TOK_STRING %c", *ptr);
			data = va_arg(va, _TCHAR*);
			size = va_arg(va, size_t);
			tok = va_arg(va, TOK);
			ZeroMemory(data, size);
			tmp = ptr;
			switch (tok)
			{
			case TOK_MATCH:
				str = va_arg(va, _TCHAR*);
				while (ptr < end && !strchr(str, *ptr)) ptr++;
				break;
			case TOK_NUMBER:
				while (ptr < end && *ptr < '0' && *ptr > '9') ptr++;
				break;
			case TOK_END:
				ptr = end;
				break;
			default:
				throw RuntimeException("Tokenize", ERROR_INVALID_PARAMETER);
			}
			if (tmp == ptr)
				goto bad;
			memcpy_s(data, size - 1, tmp, ptr - tmp);
			break;
		case TOK_NUMBER:
			data  = va_arg(va, _TCHAR*);
			tmp = ptr;
			tok = va_arg(va, TOK);
#define TONUMBER(type)								\
ZeroMemory(data, sizeof(type));						\
while (ptr < end && *ptr >= '0' && *ptr <= '9')	\
{													\
	*((type*)data) *= 10;							\
	*((type*)data) += (*ptr++ - '0');				\
}
			switch (tok & TOK_TYPE_MASK)
			{
			case TOK_DWORD:
				TONUMBER(DWORD);
				break;
			case TOK_LONGLONG:
				TONUMBER(long long);
				break;
			default:
				throw RuntimeException("Tokenize", ERROR_INVALID_PARAMETER);
			}
#undef TONUMBER
			if (tmp == ptr)
				goto bad;
			tok = va_arg(va, TOK);
			break;
		case TOK_END:
			goto good;
		default:
			throw RuntimeException("Tokenize", ERROR_INVALID_PARAMETER);
		}
	}
bad:
	va_end(va);
	return FALSE;
good:
	offset += (DWORD)(ptr - &Connection->ReceiveBuffer[offset]);
	va_end(va);
	return TRUE;
}

/* LIST Media Command
 * Input: 
 *   LIST VIDEO ||\r\n\r\n
 *   LIST VIDEO |Path|\r\n\r\n
 * Output:
 *   DisplayName|FullPathToDirectory|1|\n
 *   DisplayName|FullPathToFile|0|\n
 *   ...
 *   EOL\n
 */
class ListCommand : public Command 
{
public:
	static Command* Init (::Connection* connection)
	{
		if (connection->ReceiveSize > 4 && strncmp("LIST", connection->ReceiveBuffer, 4) == 0)
		{
			ListCommand* command = dynamic_cast<ListCommand*>(connection->GetCommand());
			if (command)
			{
				return command->Reset();
			}
			return connection->SetCommand(new ListCommand());			
		}
		return NULL;
	}
	DWORD Parse ()
	{
		DWORD offset = 0;
		if (Tokenize(offset,
			TOK_MATCH,	"LIST",		'=',
			TOK_MATCH,	" ",		'+',
			TOK_STRING, &Media,		sizeof(Media),
			TOK_MATCH,	" ",		'+',
			TOK_MATCH,	"|",		'1',
			TOK_END)
			&& (
			Tokenize(offset,
			TOK_STRING, &Location,	sizeof(Location),
			TOK_MATCH,	"\\/",		'1',
			TOK_STRING, &SubPath,	sizeof(SubPath),
			TOK_MATCH,	"|",		'1',
			TOK_MATCH,	" \r\n",	'*',
			TOK_END)
			||
			Tokenize(offset,
			TOK_STRING, &Location,	sizeof(Location),
			TOK_MATCH,	"|",		'1',
			TOK_MATCH,	" \r\n",	'*',
			TOK_END)
			||
			Tokenize(offset,
			TOK_MATCH,	"|",		'1',
			TOK_MATCH,	" \r\n",	'*',
			TOK_END)
			))
		{
			/*
			StrTrim(Media, " \t");
			StrTrim(Location, " \t");
			StrTrim(SubPath, " \t");
			*/
#ifdef _DEBUG
			Log(LOG_DEBUG, _T("\nConnection #%d: LIST: Media=%s, Location=%s, SubPath=%s"), 
				Connection->Id, Media, Location, SubPath);
#endif // _DEBUG
			return offset;
		}
		return 0;
	}
	VOID Execute () {
		_TCHAR path[_MAX_PATH];
		if (Location[0])
		{
			HANDLE find;
			WIN32_FIND_DATA data;
			//char *filename;

			GetMediaPath(path, Media, Location, SubPath);
			if (PathAppend(path, "*") == NULL)
			{
				throw RuntimeException("PathAppend");
			}
			//PathQuoteSpaces(path);
			ZeroMemory(&data, sizeof(data));
			if ((find = FindFirstFile(path, &data)) == INVALID_HANDLE_VALUE) 
			{
				throw RuntimeException("FindFirstFile");
			}
			__try
			{
				do
				{
					if (   strcmp(data.cFileName, ".")
						&& strcmp(data.cFileName, "..")
						&& (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0
						&& ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || AllowFile(Media, data.cFileName)))
					{
						//filename = PathFindFileName(data.cFileName);
						
						// Display part:
						Append(data.cFileName)->Append("|", 1);

						// Path and type part:
						if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
						{
							// Just list the directory name, 
							// the player will build the absolute path.
							Append(data.cFileName)->Append("|1|\n", 4);
						}
						else 
						{
							// For files, list the absolute path as GET command require.
							Append(Media)->Append("/", 1);
							Append(Location)->Append("/", 1);
							if (SubPath[0])
							{
								Append(SubPath)->Append("/", 1);
							}
							Append(data.cFileName)->Append("|0|\n", 4);
						}
						/*
						if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
						{
							// it's a file
							Append(Media)->Append("/", 1);
							Append(Location)->Append("/", 1);
							if (SubPath[0])
							{
								Append(SubPath)->Append("/", 1);
							}
						}
						Append(data.cFileName)->Append("|", 1);
						Append((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "1" : "0", 1);
						Append("|\n", 2);
						*/
					}
				}
				while (FindNextFile(find, &data) != 0);
				if (GetLastError() != ERROR_NO_MORE_FILES)
				{
					throw RuntimeException("FindNextFile");
				}
			}
			__finally 
			{
				FindClose(find);
			}
		}
		else 
		{
			// list Location's
			_TCHAR* ptr;
			_TCHAR* end;
			GetPrivateProfileSection(Media, path, _MAX_PATH, ConfigFilename);
			for (ptr = path; *ptr; ptr = end + strlen(end) + 1) 
			{
				if ((end = strchr(ptr, '=')) == NULL)
				{
					end = ptr;
				}
				else 
				{
					*end++ = '\0';
					Append(ptr)->Append("|", 1);
					Append(ptr)->Append("|1|\n", 4);
				}
			}
		}
		Append("EOL\n", 4);
#ifdef _DEBUG
		Log(LOG_DEBUG, _T("\nConnection #%d: LIST: Transmitting (%d bytes)..."), 
			Connection->Id, TransmitBuffers.HeadLength);
//		fwrite(TransmitBuffers.Head, TransmitBuffers.HeadLength, 1, stdout);
#endif // _DEBUG
#ifdef WINXP
		Packet.dwElFlags = TP_ELEMENT_MEMORY;
		Packet.cLength = (ULONG)Response.size();
		Packet.pBuffer = (void*)(Response.data());
		Connection->IssueTransmit(&Packet);
#else
		Connection->IssueTransmit(TransmitBuffers);
#endif
	}
	Command* Reset ()
	{
		ZeroMemory(Media, sizeof(Media));
		ZeroMemory(Location, sizeof(Location));
		ZeroMemory(SubPath, sizeof(SubPath));
		TransmitBuffers.HeadLength = 0;
		return Command::Reset();
	}
private:
	ListCommand () 
	{
		ZeroMemory(Media, sizeof(Media));
		ZeroMemory(Location, sizeof(Location));
		ZeroMemory(SubPath, sizeof(SubPath));
#ifdef WINXP
		ZeroMemory(&Packet, sizeof(TRANSMIT_PACKETS_ELEMENT));
#else
		ZeroMemory(&TransmitBuffers, sizeof(TRANSMIT_FILE_BUFFERS));
#endif
	}
	~ListCommand ()
	{
		if (TransmitBuffers.Head != NULL)
		{
			free(TransmitBuffers.Head);
		}
	}
	ListCommand* Append (_TCHAR* data)
	{
		return Append(data, (DWORD)strlen(data));
	}
	ListCommand* Append (_TCHAR* data, DWORD length)
	{
		size_t size = TransmitBuffers.Head ? _msize(TransmitBuffers.Head) : 0;
		if (size < TransmitBuffers.HeadLength + length)
		{
			if (size <= 0)
			{
				size = 1024;
			}
			while (size < TransmitBuffers.HeadLength + length) 
			{
				size *= 2;
			}
			if ((TransmitBuffers.Head = (_TCHAR*)realloc(TransmitBuffers.Head, size)) == NULL)
			{
				// shouldn't happend
				throw new std::bad_alloc();
			}
		}
		memcpy_s((_TCHAR*)TransmitBuffers.Head + TransmitBuffers.HeadLength, 
				 size - TransmitBuffers.HeadLength, data, length);
		TransmitBuffers.HeadLength += length;
		return this;
	}
	_TCHAR Media[MEDIA_LENGTH];
	_TCHAR Location[LOCATION_LENGTH];
	_TCHAR SubPath[_MAX_PATH];
#ifdef WINXP
	TRANSMIT_PACKETS_ELEMENT Packet;
#else
    TRANSMIT_FILE_BUFFERS TransmitBuffers;
#endif
};

/* ACTION Command
 * Input: 
 *   ACTION 1 |PlayerId| Filename|
 *   ACTION 2 Filename|
 * WRONG  ACTION 1 Filename|PlayerId|
 * Output:
 *   200 or 404
 */
class ActionCommand : public Command 
{
public:
	static Command* Init (::Connection* connection)
	{
		if (connection->ReceiveSize > 6 && strncmp("ACTION", connection->ReceiveBuffer, 6) == 0)
		{
			ActionCommand* command = dynamic_cast<ActionCommand*>(connection->GetCommand());
			if (command)
			{
				return command->Reset();
			}
			return connection->SetCommand(new ActionCommand());			
		}
		return NULL;
	}
	DWORD Parse ()
	{
		DWORD offset = 0;
		if (Tokenize(offset,
			TOK_MATCH,	"ACTION",	'=',
			TOK_MATCH,	" ",		'+',
			TOK_NUMBER, &Number,	TOK_DWORD,
			TOK_MATCH,	" ",		'+',
			TOK_END)
			&& (Number == 2 || (Number == 1 
			&& Tokenize(offset,
			TOK_MATCH,	"|",		'1',
			TOK_STRING, &PlayerID,	sizeof(PlayerID),
			TOK_MATCH,	"|",		'1',
			TOK_MATCH,	" ",		'*',
			TOK_END)))
			&& Tokenize(offset,
			TOK_STRING, &Media,		sizeof(Media),
			TOK_MATCH,	"\\/",		'1',
			TOK_STRING, &Location,	sizeof(Location),
			TOK_MATCH,	"\\/",		'1',
			TOK_STRING, &Filename,	sizeof(Filename),
			TOK_MATCH,	"|",		'1',
			TOK_MATCH,	" \r\n",	'*',
			TOK_END))
		{
		/*
		if (Tokenize(offset,
			TOK_MATCH,	"ACTION",	'=',
			TOK_MATCH,	" ",		'+',
			TOK_NUMBER, &Number,	TOK_DWORD,
			TOK_MATCH,	" ",		'+',
			TOK_STRING, &Media,		sizeof(Media),
			TOK_MATCH,	"\\/",		'1',
			TOK_STRING, &Location,	sizeof(Location),
			TOK_MATCH,	"\\/",		'1',
			TOK_STRING, &Filename,	sizeof(Filename),
			TOK_MATCH,	"|",		'1',
			TOK_END)
			&& (
		(Number == 1 && Tokenize(offset,
			TOK_STRING, &PlayerID,	sizeof(PlayerID),
			TOK_MATCH,	"|",		'1',
			TOK_MATCH,	" \r\n",	'*',
			TOK_END))
			|| 
		(Number == 2 && Tokenize(offset,
			TOK_MATCH,	" \r\n",	'*',
			TOK_END))
			))
		{
		*/
			/*
			StrTrim(Media, " \t");
			StrTrim(Location, " \t");
			StrTrim(Filename, " \t");
			StrTrim(PlayerID, " \t");
			*/
#ifdef _DEBUG
			Log(LOG_DEBUG, _T("\nConnection #%d: ACTION: Number=%d, Media=%s, Location=%s, Filename=%s, PlayerID=%s"),
				Connection->Id, Number, Media, Location, Filename, PlayerID);
#endif // _DEBUG
			return offset;
		}
		return 0;
	}
	VOID Execute ()
	{
		if ((Number == 1 && !AllowPlayer(PlayerID)) || !AllowFile(Media, Filename))
		{
			Connection->IssueDisconnect();
			return;
		}

		_TCHAR path[_MAX_PATH];
		GetMediaPath(path, Media, Location, Filename);
#ifdef WINXP
		Packet.dwElFlags = TP_ELEMENT_MEMORY;
		Packet.cLength = 3;
		Packet.pBuffer = PathFileExists(path) ? "200" : "404";
		Connection->IssueTransmit(&Packet);
		...
#else // !WINXP
		TransmitBuffers.Head = PathFileExists(path) ? "200" : "404";
		TransmitBuffers.HeadLength = 3;
		switch (GetLastError())
		{
		case 0:
		case ERROR_FILE_NOT_FOUND:
			// ignore
			break;
		default:
			throw RuntimeException("PathFileExists");
		}
		Connection->IssueTransmit(TransmitBuffers);
#endif // !WINXP
	}
	Command* Reset ()
	{
		Number = 0;
		ZeroMemory(Media, sizeof(Media));
		ZeroMemory(Location, sizeof(Location));
		ZeroMemory(Filename, sizeof(Filename));
		ZeroMemory(PlayerID, sizeof(PlayerID));
		return Command::Reset();
	}
private:
	ActionCommand ()
		: Number(0)
	{
		ZeroMemory(Media, sizeof(Media));
		ZeroMemory(Location, sizeof(Location));
		ZeroMemory(Filename, sizeof(Filename));
		ZeroMemory(PlayerID, sizeof(PlayerID));
#ifdef WINXP
		ZeroMemory(&Packet, sizeof(TRANSMIT_PACKETS_ELEMENT));
#else
		ZeroMemory(&TransmitBuffers, sizeof(TRANSMIT_FILE_BUFFERS));
#endif
	}
	DWORD Number;
	_TCHAR Media[MEDIA_LENGTH];
	_TCHAR Location[LOCATION_LENGTH];
	_TCHAR Filename[_MAX_PATH];
	_TCHAR PlayerID[PLAYERID_LENGTH];
#ifdef WINXP
	TRANSMIT_PACKETS_ELEMENT Packet;
#else
    TRANSMIT_FILE_BUFFERS TransmitBuffers;
#endif
};

/* SIZE Command
 * Input: 
 *   SIZE Filename|\r\n\r\n
 * Output:
 *   000000000000015
 */
class SizeCommand : public Command 
{
public:
	static Command* Init (::Connection* connection)
	{
		if (connection->ReceiveSize > 4 && strncmp("SIZE", connection->ReceiveBuffer, 4) == 0)
		{
			SizeCommand* command = dynamic_cast<SizeCommand*>(connection->GetCommand());
			if (command)
			{
				return command->Reset();
			}
			return connection->SetCommand(new SizeCommand());
		}
		return NULL;
	}
	DWORD Parse ()
	{
		DWORD offset = 0;
		if (Tokenize(offset,
			TOK_MATCH,	"SIZE",		'=',
			TOK_MATCH,	" ",		'+',
			TOK_STRING, &Media,		sizeof(Media),
			TOK_MATCH,	"\\/",		'1',
			TOK_STRING, &Location,	sizeof(Location),
			TOK_MATCH,	"\\/",		'1',
			TOK_STRING, &Filename,	sizeof(Filename),
			TOK_MATCH,	"|",		'1',
			TOK_MATCH,	" \r\n",	'*',
			TOK_END))
		{
			/*
			StrTrim(Media, " \t");
			StrTrim(Location, " \t");
			StrTrim(Filename, " \t");
			*/
#ifdef _DEBUG
			Log(LOG_DEBUG, _T("\nConnection #%d: SIZE: Media=%s, Location=%s, Filename=%s"),
				Connection->Id, Media, Location, Filename);
#endif // DEBUG
			return offset;
		}
		return 0;
	}
	VOID Execute ()
	{
		if (!AllowFile(Media, Filename)) 
		{
			Connection->IssueDisconnect();
			return;
		}

		HANDLE file = INVALID_HANDLE_VALUE;
		LARGE_INTEGER size;
		_TCHAR path[_MAX_PATH];

		GetMediaPath(path, Media, Location, Filename);
		if ((file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 
							   CREATE_FILE_FLAGS, NULL)) == INVALID_HANDLE_VALUE)
		{
			throw RuntimeException("CreateFile");
		}
		__try 
		{
			if (GetFileSizeEx(file, &size) == 0)
			{
				throw RuntimeException("GetFileSizeEx");
			}
		}
		__finally
		{
			CloseHandle(file);
		}
		_stprintf_s(Response, sizeof(Response), _T("%015lld"), size.QuadPart);
#ifdef WINXP
		Packet.dwElFlags = TP_ELEMENT_MEMORY;
		Packet.cLength = 15;
		Packet.pBuffer = Response;
		Connection->IssueTransmit(&Packet);
#else // !WINXP
		TransmitBuffers.Head = Response;
		TransmitBuffers.HeadLength = 15;
		Connection->IssueTransmit(TransmitBuffers);
#endif // !WINXP
	}
	Command* Reset ()
	{
		ZeroMemory(Media, sizeof(Media));
		ZeroMemory(Location, sizeof(Location));
		ZeroMemory(Filename, sizeof(Filename));
		ZeroMemory(Response, sizeof(Response));
		return Command::Reset();
	}
private:
	SizeCommand ()
	{
		ZeroMemory(Media, sizeof(Media));
		ZeroMemory(Location, sizeof(Location));
		ZeroMemory(Filename, sizeof(Filename));
		ZeroMemory(Response, sizeof(Response));
#ifdef WINXP
		ZeroMemory(&Packet, sizeof(TRANSMIT_PACKETS_ELEMENT));
#else
		ZeroMemory(&TransmitBuffers, sizeof(TRANSMIT_FILE_BUFFERS));
#endif
	}
	_TCHAR Media[MEDIA_LENGTH];
	_TCHAR Location[LOCATION_LENGTH];
	_TCHAR Filename[_MAX_PATH];
	_TCHAR Response[16];
#ifdef WINXP
	TRANSMIT_PACKETS_ELEMENT Packet;
#else
	TRANSMIT_FILE_BUFFERS TransmitBuffers;
#endif
};

/* GET Command
 * Input: 
 *   GET Filename| Offset Length |PlayerId|\r\n\r\n
 *   GET Filename| Offset Length ||\r\n\r\n
 *   GET Filename| Offset Length \r\n\r\n
 * Output:
 *   binary data
 */
class GetCommand : public Command 
{
public:
	static Command* Init (::Connection* connection)
	{
		if (connection->ReceiveSize > 3 && strncmp("GET", connection->ReceiveBuffer, 3) == 0)
		{
			GetCommand* command = dynamic_cast<GetCommand*>(connection->GetCommand());
			if (command)
			{
				return command->Reset();
			}
			return connection->SetCommand(new GetCommand());			
		}
		return NULL;
	}
	DWORD Parse ()
	{
		DWORD offset = 0;
		if (Tokenize(offset,
			TOK_MATCH,  "GET",				'=',
			TOK_MATCH,	" ",				'+',
			TOK_STRING, &Media,				sizeof(Media),
			TOK_MATCH,	"\\/",				'1',
			TOK_STRING, &Location,			sizeof(Location),
			TOK_MATCH,	"\\/",				'1',
			TOK_STRING, &Filename,			sizeof(Filename),
			TOK_MATCH,	"|",				'1',
			TOK_MATCH,	" ",				'*',
			TOK_NUMBER, &Offset.QuadPart,	TOK_LONGLONG,
			TOK_MATCH,	" ",				'+',
			TOK_NUMBER, &Length.QuadPart,	TOK_LONGLONG,
			TOK_MATCH,	" ",				'*',
			TOK_END)
			&& (
			Tokenize(offset,
			TOK_MATCH,	"|",				'1',
			TOK_STRING, &PlayerID,			sizeof(PlayerID),
			TOK_MATCH,	"|",				'1',
			TOK_MATCH,	" \r\n",			'*',
			TOK_END)
			|| 
			Tokenize(offset,
			TOK_MATCH,	"||",				'=',
			TOK_MATCH,	" \r\n",			'*',
			TOK_END)
			|| 
			Tokenize(offset,
			TOK_MATCH,	"\r\n",				'+',
			TOK_END)
			))
		{
			/*
			StrTrim(Media, " \t");
			StrTrim(Location, " \t");
			StrTrim(Filename, " \t");
			StrTrim(PlayerID, " \t");
			*/
#ifdef _DEBUG
			Log(LOG_DEBUG, _T("\nConnection #%d: GET: Media=%s, Location=%s, Filename=%s, Offset=%lld, Length=%lld, PlayerID=%s"),
				Connection->Id, Media, Location, Filename, Offset.QuadPart, Length.QuadPart, PlayerID);
#endif // DEBUG
			return offset;
		}
		return 0;
	}
	VOID Execute ()
	{
		if (!AllowPlayer(PlayerID) || !AllowFile(Media, Filename))
		{
			Connection->IssueDisconnect();
			return;
		}

		_TCHAR path[_MAX_PATH];

		GetMediaPath(path, Media, Location, Filename);
		if ((File = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
			CREATE_FILE_FLAGS, NULL)) == INVALID_HANDLE_VALUE)
		{
			throw RuntimeException("CreateFile");
		}
		/*
		if (Length.QuadPart == 0)
		{
			if (GetFileSizeEx(File, &Length) == 0)
			{
				throw RuntimeException("GetFileSizeEx");
			}
			if (Length.QuadPart < Offset.QuadPart)
			{
				Length.QuadPart -= Offset.QuadPart;
			}
			else 
			{
				Connection->IssueDisconnect();
				return;
			}
		}
		*/
		Connection->IssueTransmit(File, Offset, Length);
	}
	Command* Reset ()
	{
		if (File != INVALID_HANDLE_VALUE)
		{	
			CloseHandle(File);
			File = INVALID_HANDLE_VALUE;
		}
		ZeroMemory(Media, sizeof(Media));
		ZeroMemory(Location, sizeof(Location));
		ZeroMemory(Filename, sizeof(Filename));
		ZeroMemory(&Offset, sizeof(Offset));
		ZeroMemory(&Length, sizeof(Length));
		ZeroMemory(PlayerID, sizeof(PlayerID));
		return Command::Reset();
	}
private:
	GetCommand ()
		: File(INVALID_HANDLE_VALUE)
	{
		ZeroMemory(Media, sizeof(Media));
		ZeroMemory(Location, sizeof(Location));
		ZeroMemory(Filename, sizeof(Filename));
		ZeroMemory(&Offset, sizeof(Offset));
		ZeroMemory(&Length, sizeof(Length));
		ZeroMemory(PlayerID, sizeof(PlayerID));
	}
	~GetCommand ()
	{
		if (File != INVALID_HANDLE_VALUE)
		{	
			CloseHandle(File);
		}
	}
	LARGE_INTEGER Offset;
	LARGE_INTEGER Length;
	_TCHAR Media[MEDIA_LENGTH];
	_TCHAR Location[LOCATION_LENGTH];
	_TCHAR Filename[_MAX_PATH];
	_TCHAR PlayerID[PLAYERID_LENGTH];
	HANDLE File;
};

/* Registered Command's
 * Ordered by usage for fastest look up.
 */
CommandInitFunction AvailableCommands[] = {
	GetCommand::Init,
	ActionCommand::Init,
	SizeCommand::Init,
	ListCommand::Init,
	NULL,
};
