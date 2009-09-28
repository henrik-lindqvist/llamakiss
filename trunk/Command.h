/*
 * Command.h
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
#ifndef __COMMAND_H__
#define __COMMAND_H__

#include "stdafx.h"

class Connection;

/* Client input and output command handler
 */
class Command
{
public:
	// functions:
	virtual DWORD Parse () = 0;
	virtual VOID Execute () = 0;
	void SetConnection (Connection* connection);
	Command* Reset ();
	Command* Release ();

protected:
	enum TOK {
		// match
		TOK_END			= 0x0000,
		TOK_MATCH		= 0x0001,
		TOK_STRING		= 0x0002,
		TOK_NUMBER		= 0x0003,
		// type			
		TOK_DWORD		= 0x0010,
		TOK_LONGLONG	= 0x0020,
		TOK_TYPE_MASK   = 0x00F0,
		// flag
//		TOK_OPTIONAL	= 0x0100, 
	};
	Connection* Connection;
// functions:
	Command () {}
	BOOL Tokenize (DWORD& offset, TOK tok1, ...);
	BOOL AllowPlayer (_TCHAR* playerID);
	BOOL AllowFile (_TCHAR* filename, _TCHAR* media);
	VOID GetMediaPath (_TCHAR* path, _TCHAR* media, _TCHAR* location, _TCHAR* filename);
};

typedef Command* (*CommandInitFunction)(Connection*);

#endif // !__COMMAND_H__
