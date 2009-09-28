/*
 * Win32Exception.h
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
#ifndef __WIN32EXCEPTION_H__
#define __WIN32EXCEPTION_H__

#include "stdafx.h"

#ifndef NTSTATUS

typedef DWORD NTSTATUS;

#define NT_INFORMATION(s)	 ((s) >= 0x40000000 && (s) <= 0x7FFFFFFF)
#define NT_WARNING(s)		 ((s) >= 0x80000000 && (s) <= 0xBFFFFFFF)
#define NT_ERROR(s)          ((s) >= 0xC0000000 && (s) <= 0xFFFFFFFF)
#define NT_SUCCESS(s)		(((s) >= 0x00000000 && (s) <= 0x3FFFFFFF) || NT_INFORMATION(s))

#endif // !NTSTATUS

class Win32Exception : public std::exception {
public:
	static void InstallTranslator ()
	{
		_set_se_translator(Win32Exception::Translate);
	}
	virtual const char* what() const
	{
		return What;
	}
	unsigned int code ()
	{
		return Code;
	}
	PVOID where ()
	{
		return Where;
	}
	bool continuable ()
	{
		return Continuable;
	}
protected:
	unsigned int Code;
	const _TCHAR* What;
	PVOID Where;
	bool Continuable;
// functions:
	Win32Exception (const EXCEPTION_RECORD& record, const _TCHAR* what = "Unknown")
		: std::exception(NULL), Code(record.ExceptionCode), What(what), Where(record.ExceptionAddress), 
		  Continuable(record.ExceptionFlags != EXCEPTION_NONCONTINUABLE)
 	{
	};
	static void Translate (unsigned int code, EXCEPTION_POINTERS* pointers);
};

class IllegalInstructionException : public Win32Exception
{
protected:
	IllegalInstructionException (const EXCEPTION_RECORD& record, const _TCHAR* what = "Illegal instruction")
		: Win32Exception(record, what) {};
private:
	friend void Win32Exception::Translate(unsigned int code, EXCEPTION_POINTERS* pointers);
};

class IllegalAccessException : public Win32Exception
{
protected:
	IllegalAccessException (const EXCEPTION_RECORD& record, const char* what)
		: Win32Exception(record, what) {};
private:
	friend void Win32Exception::Translate(unsigned int code, EXCEPTION_POINTERS* pointers);
};

class AccessViolationException : public IllegalAccessException
{
public:
    bool write () const
	{
		return Write;
	}
    PVOID address() const
	{
		return Address;
	}
protected:
    bool Write;
    PVOID Address;
	// functions:
	AccessViolationException (const EXCEPTION_RECORD& record, const char* what)
		: IllegalAccessException(record, what), Write(record.ExceptionInformation[0] == 1), 
		  Address((PVOID)record.ExceptionInformation[1]) {};
private:
	friend void Win32Exception::Translate(unsigned int code, EXCEPTION_POINTERS* pointers);
};

class InPageErrorException : public IllegalAccessException
{
public:
	NTSTATUS status ()
	{
		return Status;
	}
	bool success ()
	{
		return NT_SUCCESS(Status);
	}
	bool informal ()
	{
		return NT_INFORMATION(Status);
	}
	bool warning ()
	{
		return NT_WARNING(Status);
	}
	bool error ()
	{
		return NT_ERROR(Status);
	}
protected:
    NTSTATUS Status;
	// functions:
	InPageErrorException (const EXCEPTION_RECORD& record, const char* what)
		: IllegalAccessException(record, what), Status((NTSTATUS)record.ExceptionInformation[2]) {};
private:
	friend void Win32Exception::Translate(unsigned int code, EXCEPTION_POINTERS* pointers);
};

class ArithmeticException : public Win32Exception
{
protected:
	ArithmeticException (const EXCEPTION_RECORD& record, const char* what)
		: Win32Exception(record, what) {};
private:
	friend void Win32Exception::Translate(unsigned int code, EXCEPTION_POINTERS* pointers);
};

class OutOfMemoryException : public Win32Exception, std::bad_alloc
{
protected:
	OutOfMemoryException (const EXCEPTION_RECORD& record, const char* what = "Out of memory")
		: Win32Exception(record, what), std::bad_alloc(what) {};
private:
	friend void Win32Exception::Translate(unsigned int code, EXCEPTION_POINTERS* pointers);
};

#endif // !__WIN32EXCEPTION_H__
