/*
 * Debug.h
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
#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "stdafx.h"

extern LONG __stdcall GenerateMiniDump (struct _EXCEPTION_POINTERS* ep);

#endif // __DEBUG_H__