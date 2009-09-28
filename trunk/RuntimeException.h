/*
 * RuntimeException.h
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
#ifndef __RUNTIMEEXCEPTION_H__
#define __RUNTIMEEXCEPTION_H__

#include "stdafx.h"

class RuntimeException : public std::exception {
public:
// functions:
	RuntimeException (const _TCHAR* function, DWORD code = GetLastError());
	~RuntimeException ();
	virtual const char* what () const
	{
		return What;
	}
	const _TCHAR* function () const
	{
		return Function;
	}
	DWORD code ()
	{
		return Code;
	}
private:
	DWORD Code;
	const _TCHAR* Function;
	_TCHAR* What;
};

#endif // !__RUNTIMEEXCEPTION_H__
