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
 * gdal_base.h
 *
 * Include file providing low level portability services for GDAL.  This
 * should be the first include file for any GDAL code.  It provides the
 * following:
 *
 * o Includes some standard system include files, such as stdio, and stdlib.
 *
 * o Defines GDAL_C_START, GDAL_C_END macros.
 *
 * o Ensures that some other standard macros like NULL are defined.
 *
 * o Defines some portability stuff like GDAL_MSB, or GDAL_LSB.
 *
 * o Ensures that core types such as GBool, GInt32, GInt16, GUInt32, 
 *   GUInt16, and GByte are defined.
 *
 * $Log$
 * Revision 1.1  1998/10/18 06:15:11  warmerda
 * Initial implementation.
 *
 */

#ifndef GDAL_BASE_H_INCLUDED
#define GDAL_BASE_H_INCLUDED

/* ==================================================================== */
/*      Base portability stuff ... this stuff may need to be            */
/*      modified for new platforms.                                     */
/* ==================================================================== */

#define GDAL_LSB

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

/* ==================================================================== */
/*      Other standard services.                                        */
/* ==================================================================== */
#ifdef __cplusplus
#  define GDAL_C_START		extern "C" {
#  define GDAL_C_END		}
#else
#  define GDAL_C_START
#  define GDAL_C_END
#endif

/* #  define GDAL_DLL     __declspec(dllexport) */

#define GDAL_DLL

#ifndef NULL
#  define NULL	0
#endif

/* ==================================================================== */
/*      Error handling.                                                 */
/* ==================================================================== */
GDAL_C_START

typedef enum
{
    GE_None = 0,
    GE_Warning = 1,
    GE_Failure = 2,
    GE_Fatal = 3
  
} GBSErr;

void GDAL_DLL GBSError( GBSErr, const char *, ... );
void GDAL_DLL GBSClearError( void );
GBSErr GDAL_DLL GBSGetError( char ** );

GDAL_C_END


#endif /* ndef GDAL_BASE_H_INCLUDED */

