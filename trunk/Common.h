/*
 * Common.h
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
#ifndef __COMMON_H__
#define __COMMON_H__

#include "stdafx.h"

// Default config
#define DEFAULT_CONFIG_FILENAME			"LlamaKISS.ini"
#define DEFAULT_ADDRESS					"0.0.0.0"
#define	DEFAULT_PORT					8000
#define WORKER_THREAD_COUNT				4
#define MAX_CONCURRENT_CONNECTIONS		32
#define DEFAULT_LISTEN_QUEUE_SIZE		64		// SOMAXCONN?
#define DEFAULT_RECEIVE_BUFFER_SIZE		4096	// Not less than ACCEPT_ADDRESS_LENGTH * 2.
#define MAX_CONCURRENT_DETECTORS		4
#define DEFAULT_DATAGRAM_BUFFER_SIZE	1024	// Not less than MAX_COMPUTERNAME_LENGTH + 1.
#define USE_CREATETHREAD				1		// use CreateThread instead of _beginthreadex
//#define USE_TRANSMITPACKETS				1		// Use TransmitPackets.
//#define USE_COMMTIMEOUTS				1		// use SetCommTimeouts instead of setsockopt(SO_RCVTIMEO)
#define USE_SERVICE						1

// Misc
#define ACCEPT_ADDRESS_LENGTH	(sizeof(struct sockaddr_in)+16)
#define MEDIA_LENGTH			16
#define LOCATION_LENGTH			64
#define PLAYERID_LENGTH			16

#ifndef ERROR_ABANDONED_WAIT_0
#define ERROR_ABANDONED_WAIT_0	735		
#endif

// IoCompletionPort keys
enum {
	COMPLETED_NOTHING		= 0,
	COMPLETED_CONNECTION	= 1,
	COMPLETED_DETECTOR		= 2,
	COMPLETED_SHUTDOWN		= 3,
};

// Logging
#define LOG_DEBUG		EVENTLOG_INFORMATION_TYPE
#define LOG_INFO		EVENTLOG_INFORMATION_TYPE
#define LOG_WARN		EVENTLOG_WARNING_TYPE
#define LOG_ERROR		EVENTLOG_ERROR_TYPE

typedef void (*LogFunction)(WORD, _TCHAR* format, ...);

#endif // !__COMMON_H__
