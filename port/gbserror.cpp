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
 * gbs_error.cpp
 *
 * Supporting routines for GDAL Base System Error functions.
 *
 * $Log$
 * Revision 1.1  1998/12/02 19:33:09  warmerda
 * New
 *
 */

#include "gdal_port.h"

GBSErr	eCurrentError = GE_None;

/************************************************************************/
/*                              GBSError()                              */
/************************************************************************/

void GBSError( GBSErr eErr, const char * pszFormat, ... )

{
    va_list 	args;

    va_start( args, pszFormat );
    vfprintf( stderr, pszFormat, args );
    va_end( args );

    if( eErr == GE_Fatal )
        exit( 1 );

    eCurrentError = eErr;
}

/************************************************************************/
/*                            GBSGetError()                             */
/************************************************************************/

GBSErr GBSGetError( char ** ppszError )

{
    if( ppszError != NULL )
        *ppszError = "";

    return eCurrentError;
}

/************************************************************************/
/*                           GBSClearError()                            */
/************************************************************************/

void GBSClearError()

{
    eCurrentError = GE_None;
}
