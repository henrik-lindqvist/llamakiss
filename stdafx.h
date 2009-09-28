// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// Windows 2000
#define WINVER 0x0500
#define _WIN32_WINNT 0x0500
/*
// Windows XP
#define WINVER 0x0500
#define _WIN32_WINNT 0x0500
*/
/*
#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						
*/

#define _CRT_RAND_S
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <tchar.h>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <shlwapi.h>
#include <windows.h>
#include <typeinfo.h>
#include <new.h>
#include <exception>

#define _NO_CVCONST_H
#include <dbghelp.h>


#pragma comment(lib,"ws2_32")   // Standard socket API.
#pragma comment(lib,"mswsock")  // AcceptEx, TransmitFile, etc,.
#pragma comment(lib,"shlwapi")  // PathAppend, etc,..
