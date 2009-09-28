/*
 * Win32Exception.cpp
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

#include "Win32Exception.h"
#include "Debug.h"

void Win32Exception::Translate (unsigned int code, EXCEPTION_POINTERS* ep)
{
	GenerateMiniDump(ep);
	switch (code)
	{
	case EXCEPTION_STACK_OVERFLOW:
		throw OutOfMemoryException(*(ep->ExceptionRecord), "Stack overflow");
	case EXCEPTION_ACCESS_VIOLATION:
		throw AccessViolationException(*(ep->ExceptionRecord), "Access violation");
	case EXCEPTION_IN_PAGE_ERROR:
		throw InPageErrorException(*(ep->ExceptionRecord), "In page error");
	case EXCEPTION_DATATYPE_MISALIGNMENT:
		throw IllegalAccessException(*(ep->ExceptionRecord), "Datatype misalignment");
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		throw IllegalAccessException(*(ep->ExceptionRecord), "Array bounds exceeded");
	case EXCEPTION_ILLEGAL_INSTRUCTION:
		throw IllegalInstructionException(*(ep->ExceptionRecord), "Illegal instruction");
	case EXCEPTION_PRIV_INSTRUCTION:
		throw IllegalInstructionException(*(ep->ExceptionRecord), "Priv instruction");
	case EXCEPTION_FLT_DENORMAL_OPERAND:
		throw ArithmeticException(*(ep->ExceptionRecord), "Denormal operand");
	case EXCEPTION_FLT_INEXACT_RESULT:
		throw ArithmeticException(*(ep->ExceptionRecord), "Inexact result");
	case EXCEPTION_FLT_INVALID_OPERATION:
		throw ArithmeticException(*(ep->ExceptionRecord), "Invalid operation");
	case EXCEPTION_FLT_STACK_CHECK:
		throw ArithmeticException(*(ep->ExceptionRecord), "Stack check");
	case EXCEPTION_FLT_UNDERFLOW:
		throw ArithmeticException(*(ep->ExceptionRecord), "Underflow");
	case EXCEPTION_FLT_OVERFLOW:
	case EXCEPTION_INT_OVERFLOW:
		throw ArithmeticException(*(ep->ExceptionRecord), "Overflow");
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		throw ArithmeticException(*(ep->ExceptionRecord), "Divide by zero");
	case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		throw Win32Exception(*(ep->ExceptionRecord), "Non-continuable exception");
	case EXCEPTION_BREAKPOINT:
	case EXCEPTION_SINGLE_STEP:
		// ignore
		break;
	case EXCEPTION_INVALID_DISPOSITION:
		// fallthru
	default:
		throw Win32Exception(*(ep->ExceptionRecord));
	}
}
