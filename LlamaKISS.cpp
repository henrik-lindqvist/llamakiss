/*
 * LlamaKISS.cpp
 * Copyright (C) 2007-2008 Henrik Lindqvist <henrik.lindqvist@llamalab.com> 
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. 
 */

/*
 * KISS Developer Portal:
 * http://dev.kiss-technology.com
 */
#include "stdafx.h"

#include "Common.h"
#include "Connection.h"
#include "Detector.h"
#include "Debug.h"
#include "SockAddrSet.h"
#include "Win32Exception.h"
#include "RuntimeException.h"
#include "Version.h"

// Global variables
LogFunction Log = NULL;
_TCHAR RootDirectory[_MAX_PATH] = {0};
_TCHAR ConfigFilename[_MAX_PATH] = {0};
SockAddrSet* DetectedPlayers = NULL;
HANDLE CommandDumpFile = INVALID_HANDLE_VALUE;

#ifdef USE_TRANSMITPACKETS
LPFN_TRANSMITPACKETS _TransmitPackets = NULL;
#endif // USE_TRANSMITPACKETS

// Local variables
static HANDLE ShutdownEvent = INVALID_HANDLE_VALUE;
static _TCHAR* ServiceName = _T(APPLICATION_NAME);
static SERVICE_DESCRIPTION ServiceDescription = {
	_T(APPLICATION_DESCRIPTION)
};
static SERVICE_STATUS_HANDLE ServiceStatusHandle = NULL;
static SERVICE_STATUS ServiceStatus = {
	SERVICE_WIN32,
	SERVICE_STOPPED,
	0,
	NO_ERROR,
	NO_ERROR,
	0,
	3000	// ms it takes to start/stop server
};

static void EventLog (WORD type, _TCHAR* format, ...) 
{ 
    HANDLE source;
    LPCTSTR ptrs[2];
    _TCHAR tmp[512];
	va_list va;

	va_start(va, format);
	if ((source = RegisterEventSource(NULL, ServiceName)) != NULL)
	{
		_vstprintf_s(tmp, sizeof(tmp), format, va);
        ptrs[0] = ServiceName;
        ptrs[1] = tmp;
        ReportEvent(source, type, 0, 0, NULL, 2, 0, ptrs, NULL);
        DeregisterEventSource(source);
    }
}

static void StdioLog (WORD type, _TCHAR* format, ...) 
{
	va_list va;

	va_start(va, format);
	//_vtprintf(format, va);
	_vftprintf(stderr, format, va);
	fputc('\n', stderr);
}

static void WINAPI ServiceControlHandler (DWORD control)
{
	switch (control)
	{
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
		SetEvent(ShutdownEvent);
		return;
	case SERVICE_CONTROL_INTERROGATE:
		// fallthru
	default:
		SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
	}
}

// Ctrl-C handler
static BOOL WINAPI ConsoleCtrlHandler (DWORD ctrl)
{
    switch (ctrl)
	{
	case CTRL_C_EVENT:
		// fallthru
	case CTRL_CLOSE_EVENT:
		ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
		SetEvent(ShutdownEvent);
		return TRUE;
	default:
		return FALSE;
    }
}

#ifdef USE_CREATETHREAD
static DWORD WINAPI WorkerThread (LPVOID port)
#else // !USE_CREATETRHEAD
unsigned int __stdcall WorkerThread (void* port)
#endif // !USE_CREATETRHEAD
{
	BOOL status;
	DWORD transferred;
	ULONG_PTR completed;
	LPOVERLAPPED overlapped;

	Win32Exception::InstallTranslator();
	for (;;)
    {
		transferred = 0;
		completed = COMPLETED_NOTHING;
		overlapped = NULL;
		status = GetQueuedCompletionStatus((HANDLE)port, &transferred, &completed, &overlapped, INFINITE);
		try 
		{
			switch (completed)
			{
			case COMPLETED_CONNECTION:
				Connection::Completed(status, transferred, (Connection*)overlapped);
				break;
			case COMPLETED_DETECTOR:
				Detector::Completed(status, transferred, (Detector*)overlapped);
				break;
			case COMPLETED_SHUTDOWN:
				return 0;
			}
		}
		catch (RuntimeException& e)
		{
			Log(LOG_ERROR, _T("Worker %d: %s[%s]#%d: %s"),
				GetCurrentThreadId(), typeid(e).name(), e.function(), e.code(), e.what());
		}
		catch (Win32Exception& e)
		{
			Log(LOG_ERROR, _T("Worker %d: %s@0x%p: %s"),
				GetCurrentThreadId(), typeid(e).name(), e.where(), e.what());
			if (!e.continuable())
			{
				if (ServiceStatusHandle != NULL)
				{
					ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
					SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
				}
				SetEvent(ShutdownEvent);
				return e.code();
			}
		}
		catch (std::exception& e)
		{
			Log(LOG_ERROR, _T("Worker %d: %s: %s"),
				GetCurrentThreadId(), typeid(e).name(),e.what());
		}
	}
    return 0;
}

static int RunServer (BOOL service)
{
	WSADATA wsa = {0};
	HANDLE port = NULL;
	SOCKET listener = INVALID_SOCKET;
	SOCKET receiver = INVALID_SOCKET;
    Connection* connections[MAX_CONCURRENT_CONNECTIONS];
    Detector* detectors[MAX_CONCURRENT_DETECTORS];
#ifdef USE_CREATETHREAD
    HANDLE workers[WORKER_THREAD_COUNT];
    DWORD workerIds[WORKER_THREAD_COUNT];
#else // !USE_CREATETRHEAD
    HANDLE workers[WORKER_THREAD_COUNT];
    unsigned int workerIds[WORKER_THREAD_COUNT];
#endif // !USE_CREATETRHEAD
	DWORD workersRunning = 0;
	DWORD i = 0;
	int rc = NO_ERROR;

	ZeroMemory(detectors, sizeof(detectors));
	ZeroMemory(connections, sizeof(connections));
	ZeroMemory(workers, sizeof(workers));
	ZeroMemory(workerIds, sizeof(workerIds));

	try
	{
		// Service
		if (service) 
		{
			if ((ServiceStatusHandle = RegisterServiceCtrlHandler(ServiceName, ServiceControlHandler)) == NULL)
			{
				throw RuntimeException("RegisterServiceCtrlHandler");
			}
			ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
			ServiceStatus.dwCheckPoint = 1;
			if (SetServiceStatus(ServiceStatusHandle, &ServiceStatus) == 0)
			{
				throw RuntimeException("GetCurrentDirectory");
			}
		}
		// Config
		if (service) 
		{
			if (GetModuleFileName(NULL, RootDirectory, sizeof(RootDirectory)) == 0)
			{
				throw RuntimeException("GetModuleFileName");
			}
			if (PathRemoveFileSpec(RootDirectory) == FALSE)
			{
				throw RuntimeException("PathRemoveFileSpec");
			}
		}
		else 
		{
			if (GetCurrentDirectory(sizeof(RootDirectory), RootDirectory) == 0)
			{
				throw RuntimeException("GetCurrentDirectory");
			}
		}
		if (PathCombine(ConfigFilename, RootDirectory, DEFAULT_CONFIG_FILENAME) == NULL)
		{
			throw RuntimeException("PathCombine");
		}
		// CommandDumpFile
		{
			_TCHAR filename[_MAX_PATH] = {0};
			GetPrivateProfileString("debug", "CommandDumpFile", "", 
									(LPSTR)&filename, sizeof(filename), ConfigFilename);
			if (strlen(filename) != 0)
			{
				if ((CommandDumpFile = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
												  FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
				{
					Log(LOG_WARN, _T("CommandDumpFile disabled: couldn't create file: %s"), filename);
				}
			}
		}

		// Init
		if ((i = WSAStartup(MAKEWORD(2,2), &wsa)) != NO_ERROR)
		{
			throw RuntimeException("WSAStartup", i);
		}
		if ((DetectedPlayers = new SockAddrSet(7)) == NULL)
		{
			throw RuntimeException("DetectedPlayers");
		}
		_TCHAR ip[32];
		GetPrivateProfileString("general", "Interface", DEFAULT_ADDRESS, 
								(LPSTR)&ip, sizeof(ip), ConfigFilename);
		SOCKADDR_IN addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(ip);	// INADDR_ANY;
		addr.sin_port = htons(DEFAULT_PORT);

		// Setup
		if ((ShutdownEvent = CreateEvent(NULL, FALSE, FALSE, "ShutdownEvent")) == INVALID_HANDLE_VALUE)
		{
			throw RuntimeException("CreateEvent");
		}
		if (ServiceStatusHandle == NULL 
			&& SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE) == 0)
		{
			throw RuntimeException("SetConsoleCtrlHandler");
		}
		if ((port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, WORKER_THREAD_COUNT)) == NULL)
		{
			throw RuntimeException("CreateIoCompletionPort");
		}
		// TCP Setup
		if ((listener = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
		{
			throw RuntimeException("WSASocket", WSAGetLastError());
		}
		BOOL yes = 1;
		if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) == SOCKET_ERROR)
		{
			throw RuntimeException("setsockopt", WSAGetLastError());
		}
		if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
		{
			throw RuntimeException("bind", WSAGetLastError());
		}
		if (listen(listener, DEFAULT_LISTEN_QUEUE_SIZE) == SOCKET_ERROR)
		{
			throw RuntimeException("listen", WSAGetLastError());
		}
#ifdef USE_TRANSMITPACKETS
		GUID guidTransmitPackets = WSAID_TRANSMITPACKETS;
		if (WSAIoctl(listener, 
			SIO_GET_EXTENSION_FUNCTION_POINTER, 
			&guidTransmitPackets, sizeof(guidTransmitPackets),
			&_TransmitPackets, sizeof(_TransmitPackets), 
			&i, NULL, NULL) == SOCKET_ERROR)
		{
			throw RuntimeException("WSAIoctl", WSAGetLastError());
		}
#endif // USE_TRANSMITPACKETS

		// UDP Setup
		if ((receiver = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, 0, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
		{
			throw RuntimeException("WSASocket", WSAGetLastError());
		}
		if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) == SOCKET_ERROR)
		{
			throw RuntimeException("setsockopt", WSAGetLastError());
		}
		if (bind(receiver, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
		{
			throw RuntimeException("bind", WSAGetLastError());
		}

		// Workers
		while (workersRunning < WORKER_THREAD_COUNT)
		{
#ifdef USE_CREATETHREAD
			HANDLE worker = (HANDLE)CreateThread(NULL, 0, WorkerThread, port, 0, &workerIds[workersRunning]);
			if (worker == 0)
			{
				throw RuntimeException("CreateThread");
			}
#else // !USE_CREATETHREAD
			HANDLE worker = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, port, 0, &workerIds[workersRunning]);
			if (worker == 0)
			{
				throw RuntimeException("_beginthreadex");
			}
#endif	// !USE_CREATETHREAD
			workers[workersRunning++] = worker;
		}

		if (CreateIoCompletionPort((HANDLE)listener, port, COMPLETED_CONNECTION, WORKER_THREAD_COUNT) == NULL)
		{
			throw RuntimeException("CreateIoCompletionPort");
		}
		for (i = 0; i < MAX_CONCURRENT_CONNECTIONS; i++)
		{
			connections[i] = new Connection(i + 1, listener, port);
		}

        if (CreateIoCompletionPort((HANDLE)receiver, port, COMPLETED_DETECTOR, WORKER_THREAD_COUNT) == NULL)
		{
			throw RuntimeException("CreateIoCompletionPort");
		}
		for (i = 0; i < MAX_CONCURRENT_DETECTORS; i++)
		{
			detectors[i] = new Detector(i + 1, receiver, port);
		}

		// Running
		if (ServiceStatusHandle != NULL)
		{
			ServiceStatus.dwControlsAccepted |= (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
			ServiceStatus.dwCurrentState = SERVICE_RUNNING;
			ServiceStatus.dwCheckPoint = 0;
			if (SetServiceStatus(ServiceStatusHandle, &ServiceStatus) == 0)
			{
				throw RuntimeException("SetServiceStatus");
			}
		}
		Log(LOG_INFO, _T("Server running"));
		WaitForSingleObject(ShutdownEvent, INFINITE);
	}
	catch (RuntimeException& e)
	{
		Log(LOG_ERROR, _T("%s[%s]#%d: %s"), typeid(e).name(), e.function(), e.code(), e.what());
		rc = e.code();
	}
	catch (Win32Exception& e)
	{
		Log(LOG_ERROR, _T("%s@0x%p: %s"), typeid(e).name(), e.where(), e.what());
		rc = e.code();
	}
	catch (std::exception& e)
	{
		Log(LOG_ERROR, _T("%s: %s"), typeid(e).name(), e.what());
		//rc = ERROR_CAN_NOT_COMPLETE; // ERROR_INVALID_FUNCTION 
		rc = GetLastError();
	}

	// Shutdown
	if (ServiceStatusHandle != NULL)
	{
		ServiceStatus.dwControlsAccepted &= (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
		ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		ServiceStatus.dwCheckPoint = 1;
		SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
	}
	Log(LOG_INFO, _T("Server shutting down"));

	// Clean up
	SetConsoleCtrlHandler(NULL, FALSE);
	if (workersRunning > 0)
	{
		for (i = 0; i < workersRunning; i++)
		{
			PostQueuedCompletionStatus(port, 0, COMPLETED_SHUTDOWN, 0);
		}
		WaitForMultipleObjects(workersRunning, workers, TRUE, INFINITE);
		for (i = 0; i < workersRunning; i++)
		{
			if (workers[i] != 0)
			{
				CloseHandle(workers[i]);
				workers[i] = 0;
			}
		}
		workersRunning = 0;
	}
	for (i = 0; i < MAX_CONCURRENT_DETECTORS; i++)
	{
		if (detectors[i] != NULL)
		{
			delete detectors[i];
			detectors[i] = NULL;
		}
	}
	if (receiver != INVALID_SOCKET)
	{
		shutdown(receiver, SD_BOTH);
		closesocket(receiver);
		receiver = NULL;
	}
	for (i = 0; i < MAX_CONCURRENT_CONNECTIONS; i++)
	{
		if (connections[i] != NULL)
		{
			delete connections[i];
			connections[i] = NULL;
		}
	}
	if (listener != INVALID_SOCKET)
	{
		shutdown(listener, SD_BOTH);
		closesocket(listener);
		listener = NULL;
	}
	if (port != NULL)
	{
		CloseHandle(port);
		port = NULL;
	}
	if (ShutdownEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(ShutdownEvent);
		ShutdownEvent = INVALID_HANDLE_VALUE;
	}
	if (DetectedPlayers != NULL)
	{
		delete DetectedPlayers;
		DetectedPlayers = NULL;
	}
	if (CommandDumpFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(CommandDumpFile);
		CommandDumpFile = INVALID_HANDLE_VALUE;
	}
	WSACleanup();
	if (ServiceStatusHandle != NULL)
	{
		ServiceStatus.dwControlsAccepted &= ~(SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		ServiceStatus.dwCheckPoint = 0;
		ServiceStatus.dwWin32ExitCode = rc;
		SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
	}
	return rc;
}

static void WINAPI ServiceMain (DWORD argc, TCHAR* argv[])
{
	Log = EventLog;
	_set_new_mode(1);
	Win32Exception::InstallTranslator();
	RunServer(TRUE);
}

static void ServiceStateWait (SC_HANDLE service, SERVICE_STATUS& status, DWORD state)
{
	DWORD start;
	DWORD ms;

	start = GetTickCount();
	for (;;)
	{
		if (QueryServiceStatus(service, &status) == 0)
		{
			throw RuntimeException("QueryServiceStatus");
		}
		if (status.dwCurrentState == state)
		{
			break;
		}
		ms = status.dwWaitHint / 10;
		Sleep(ms < 1000 ? 1000 : ms > 10000 ? 10000 : ms);
		if (GetTickCount() - start > 30000)
		{
			throw RuntimeException("ServiceStateWait", WAIT_TIMEOUT);
		}
	}
}

static void StopService (SC_HANDLE service, SERVICE_STATUS& status)
{
	if (QueryServiceStatus(service, &status) == 0)
	{
		throw RuntimeException("QueryServiceStatus");
	}
	if (status.dwCurrentState == SERVICE_STOPPED)
	{
		return;
	}
	if (status.dwCurrentState != SERVICE_STOP_PENDING)
	{
		if (ControlService(service, SERVICE_CONTROL_STOP, &status) == 0)
		{
			throw RuntimeException("ControlService");
		}
	}
	ServiceStateWait(service, status, SERVICE_STOPPED);
}

//int _tmain(int argc, _TCHAR* argv[])
int __cdecl _tmain (int argc, _TCHAR **argv, _TCHAR **envp)
{
	SC_HANDLE manager = NULL;
	SC_HANDLE service = NULL;
	SERVICE_STATUS status;
	int rc = NO_ERROR;

	ZeroMemory(&status, sizeof(status));

	Log = StdioLog;
	_set_new_mode(1);
	Win32Exception::InstallTranslator();
	try
	{
		Log(LOG_INFO, _T("%s v%s (%s)"), APPLICATION_NAME, VERSION_STRING, __DATE__);

		// Run Server
		if (argc == 1)
		{
			Log(LOG_INFO, _T("Running server interactively, Ctrl-C to stop."));
			rc = RunServer(FALSE);
		}

		// Install Server
		else if (lstrcmpi(argv[1], _T("install")) == 0)
		{
			TCHAR filename[_MAX_PATH + 8] = {0};

			Log(LOG_INFO, _T("Installing %s service"), ServiceName);
			if ((manager = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE)) == NULL)
			{
				throw RuntimeException("OpenSCManager");
			}
			if (GetModuleFileName(NULL, filename, sizeof(filename)) == 0)
			{
				throw RuntimeException("GetModuleFileName");
			}
			strcat_s(filename, sizeof(filename), " service"); // see below
			if ((service = CreateService(
				manager,
				ServiceName,
				ServiceName,
				SERVICE_CHANGE_CONFIG, //SERVICE_ALL_ACCESS,
				SERVICE_WIN32_OWN_PROCESS,
				SERVICE_AUTO_START, // SERVICE_DEMAND_START
				SERVICE_ERROR_NORMAL, //SERVICE_ERROR_IGNORE,
				filename,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL)) == NULL)
			{
				throw RuntimeException("CreateService");
			}
			if (ChangeServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, &ServiceDescription) == 0)
			{
				throw RuntimeException("ChangeServiceConfig2");
			}
			Log(LOG_INFO, _T("%s service installed successfully"), ServiceName);
		}

		// Uninstall Service
		else if (lstrcmpi(argv[1], _T("uninstall")) == 0)
		{
			Log(LOG_INFO, _T("Uninstalling %s service"), ServiceName);
			if ((manager = OpenSCManager(0, 0, SC_MANAGER_CONNECT)) == NULL)
			{
				throw RuntimeException("OpenSCManager");
			}
			if ((service = OpenService(manager, ServiceName, SERVICE_QUERY_STATUS | SERVICE_STOP | DELETE)) == NULL) 
			{
				throw RuntimeException("OpenService");
			}
			StopService(service, status);
			if (DeleteService(service) == 0)
			{
				throw RuntimeException("DeleteService");
			}
			Log(LOG_INFO, _T("%s service uninstalled successfully"), ServiceName);
		}

		// Start Service
		else if (lstrcmpi(argv[1], _T("start")) == 0)
		{
			Log(LOG_INFO, _T("Starting %s service"), ServiceName);
			if ((manager = OpenSCManager(0, 0, SC_MANAGER_CONNECT)) == NULL)
			{
				throw RuntimeException("OpenSCManager");
			}
			if ((service = OpenService(manager, ServiceName, SERVICE_QUERY_STATUS | SERVICE_START)) == NULL) 
			{
				throw RuntimeException("OpenService");
			}
			if (StartService(service, 0, NULL) == 0)
			{
				throw RuntimeException("StartService");
			}
			ServiceStateWait(service, status, SERVICE_RUNNING);
			Log(LOG_INFO, _T("%s service started successfully"), ServiceName);
		}

		// Stop Service
		else if (lstrcmpi(argv[1], _T("stop")) == 0)
		{
			Log(LOG_INFO, _T("Stopping %s Service"), ServiceName);
			if ((manager = OpenSCManager(0, 0, SC_MANAGER_CONNECT)) == NULL)
			{
				throw RuntimeException("OpenSCManager");
			}
			if ((service = OpenService(manager, ServiceName, SERVICE_QUERY_STATUS | SERVICE_STOP)) == NULL) 
			{
				throw RuntimeException("OpenService");
			}
			StopService(service, status);
			Log(LOG_INFO, _T("%s Service stopped successfully"), ServiceName);
		}

		// Run as Service
		else if (lstrcmpi(argv[1], _T("service")) == 0)
		{
			SERVICE_TABLE_ENTRY serviceTable[] = {
				{ ServiceName, ServiceMain },
				{ NULL, NULL }
			};

			if (StartServiceCtrlDispatcher(serviceTable) == 0)
			{
				throw RuntimeException("StartServiceCtrlDispatcher");
			}
		}
	}
	catch (RuntimeException& e)
	{
		Log(LOG_ERROR, _T("%s[%s]#%d: %s"), typeid(e).name(), e.function(), e.code(), e.what());
		rc = e.code();
	}
	catch (Win32Exception& e)
	{
		Log(LOG_ERROR, _T("%s@0x%p: %s"), typeid(e).name(), e.where(), e.what());
		rc = e.code();
	}
	catch (std::exception& e)
	{
		Log(LOG_ERROR, _T("%s: %s"), typeid(e).name(), e.what());
		rc = GetLastError();
	}
	if (service != NULL)
	{
		CloseServiceHandle(service);
	}
	if (manager != NULL)
	{
		CloseServiceHandle(manager);
	}
#ifdef _DEBUG
	Sleep(3000);
#endif // _DEBUG
	return rc;
}