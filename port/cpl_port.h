/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * cpl_port.h
 *
 * Include file providing low level portability services for CPL.  This
 * should be the first include file for any CPL based code.  It provides the
 * following:
 *
 * o Includes some standard system include files, such as stdio, and stdlib.
 *
 * o Defines CPL_C_START, CPL_C_END macros.
 *
 * o Ensures that some other standard macros like NULL are defined.
 *
 * o Defines some portability stuff like CPL_MSB, or CPL_LSB.
 *
 * o Ensures that core types such as GBool, GInt32, GInt16, GUInt32, 
 *   GUInt16, and GByte are defined.
 *
 * $Log$
 * Revision 1.2  1998/12/04 21:38:40  danmo
 * Changed str*casecmp() to str*icmp() for WIN32
 *
 * Revision 1.1  1998/12/03 18:26:02  warmerda
 * New
 *
 */

#ifndef CPL_BASE_H_INCLUDED
#define CPL_BASE_H_INCLUDED

/* ==================================================================== */
/*      Base portability stuff ... this stuff may need to be            */
/*      modified for new platforms.                                     */
/* ==================================================================== */

#define CPL_LSB

typedef int		GInt32;
typedef unsigned int 	GUInt32;
typedef short		GInt16;
typedef unsigned short	GUInt16;
typedef unsigned char	GByte;
typedef int		GBool;

/* ==================================================================== */
/*	Standard include files.						*/
/* ==================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

/* ==================================================================== */
/*      Other standard services.                                        */
/* ==================================================================== */
#ifdef __cplusplus
#  define CPL_C_START		extern "C" {
#  define CPL_C_END		}
#else
#  define CPL_C_START
#  define CPL_C_END
#endif

/* #  define CPL_DLL     __declspec(dllexport) */

#define CPL_DLL

#ifndef NULL
#  define NULL	0
#endif

#ifndef FALSE
#  define FALSE	0
#endif

#ifndef TRUE
#  define TRUE	1
#endif

#ifndef MAX
#  define MIN(a,b)      ((a<b) ? a : b)
#  define MAX(a,b)      ((a>b) ? a : b)
#endif

#ifndef ABS
#  define ABS(x)        ((x<0) ? (-1*(x)) : x)
#endif

#ifndef EQUAL
#ifdef WIN32
#  define EQUALN(a,b,n)           (strnicmp(a,b,n)==0)
#  define EQUAL(a,b)              (stricmp(a,b)==0)
#else
#  define EQUALN(a,b,n)           (strncasecmp(a,b,n)==0)
#  define EQUAL(a,b)              (strcasecmp(a,b)==0)
#endif
#endif

#endif /* ndef CPL_BASE_H_INCLUDED */
