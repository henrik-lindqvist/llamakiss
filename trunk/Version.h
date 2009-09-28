/*
 * Version.h
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
#ifndef __VERSION_H__
#define __VERSION_H__

#define VERSION_MAJOR				1
#define VERSION_MINOR				1
#define VERSION_REVISION			1
#define VERSION_BUILD				0

#define APPLICATION_NAME			"LlamaKISS"
#define APPLICATION_DESCRIPTION		"KISS digital video recorder media streaming server"
#define APPLICATION_COMPANY			"LlamaLab"
#define APPLICATION_COPYRIGHT		"Copyright (C) 2007-2009 Henrik Lindqvist"

#define __QUOT2(x)	#x
#define __QUOT(x)	__QUOT2(x)

#ifdef _DEBUG
#define VERSION_STRING	\
	__QUOT(VERSION_MAJOR) "." __QUOT(VERSION_MINOR) "." __QUOT(VERSION_REVISION) " DEBUG"
#else // !_DEBUG
#define VERSION_STRING	\
	__QUOT(VERSION_MAJOR) "." __QUOT(VERSION_MINOR) "." __QUOT(VERSION_REVISION)
#endif // !_DEBUG

#endif // !__VERSION_H__
