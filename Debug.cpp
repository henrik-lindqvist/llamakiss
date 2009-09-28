/*
 * Debug.cpp
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

#include "Common.h"
#include "Debug.h"

typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(
	HANDLE hProcess,
	DWORD ProcessId,
	HANDLE hFile,
	MINIDUMP_TYPE DumpType,
	PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
	PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	PMINIDUMP_CALLBACK_INFORMATION CallbackParam
);

extern LogFunction Log;
extern _TCHAR ConfigFilename[];

LONG __stdcall GenerateMiniDump (struct _EXCEPTION_POINTERS* ep)
{
	_TCHAR filename[_MAX_PATH] = {0};
	LONG rc = EXCEPTION_CONTINUE_SEARCH;
 
	GetPrivateProfileString("debug", "MiniDumpFile", "", 
							(LPSTR)&filename, sizeof(filename), ConfigFilename);
	if (strlen(filename) == 0)
	{
		return rc;
	}

	HMODULE dll = NULL;
	MINIDUMPWRITEDUMP dump;
	MINIDUMP_EXCEPTION_INFORMATION mei;
	HANDLE file;
	_TCHAR dllfile[_MAX_PATH] = {0};

	// dll in exe path
	if (GetModuleFileName(NULL, dllfile, _MAX_PATH))
	{
		if (PathAppend(dllfile, "DBGHELP.DLL") != NULL)
		{
			dll = LoadLibrary(dllfile);
		}
	}
	// dll anywhere
	if (dll == NULL)
	{
		dll = LoadLibrary("DBGHELP.DLL");
	}
	if (dll != NULL)
	{
		if ((dump = (MINIDUMPWRITEDUMP)GetProcAddress(dll, "MiniDumpWriteDump")) != NULL)
		{
			if ((file = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
								   FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE)
			{
				ZeroMemory(&mei, sizeof(MINIDUMP_EXCEPTION_INFORMATION));
				mei.ThreadId = GetCurrentThreadId();
				mei.ExceptionPointers = ep;
				mei.ClientPointers = NULL;

				if (dump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, &mei, NULL, NULL))
				{
					rc = EXCEPTION_EXECUTE_HANDLER;
				}
				else 
				{
					Log(LOG_WARN, _T("GenerateMiniDump failed: couldn't write file: %s"), filename);
				}
				CloseHandle(file);
			}
			else
			{
				Log(LOG_WARN, _T("GenerateMiniDump failed: couldn't create file: %s"), filename);
			}
		}
		else
		{
			Log(LOG_WARN, _T("GenerateMiniDump failed: DBGHELP.DLL to old"));
		}
		FreeLibrary(dll);
	}
	else
	{
		Log(LOG_WARN, _T("GenerateMiniDump failed: DBGHELP.DLL not found"));
	}
	return rc;
}
