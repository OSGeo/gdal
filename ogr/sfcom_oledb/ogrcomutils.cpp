/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Utility functions for Geometry services COM implementation.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * $Log$
 * Revision 1.1  1999/06/21 21:08:17  warmerda
 * New
 *
 */

#include "cpl_conv.h"

/************************************************************************/
/*                            OGRComDebug()                             */
/************************************************************************/

void OGRComDebug( const char * pszDebugClass, const char * pszFormat, ... )

{
    va_list args;
    static FILE      *fpDebug = NULL;

/* -------------------------------------------------------------------- */
/*      Do we have a debug file?                                        */
/* -------------------------------------------------------------------- */
    if( fpDebug == NULL )
    {
        fpDebug = fopen( "f:\\gdal\\ogr\\sfcom_oledb\\Debug", "w" );
    }

/* -------------------------------------------------------------------- */
/*      Write message to stdout.                                        */
/* -------------------------------------------------------------------- */
    fprintf( stdout, "%s:", pszDebugClass );

    va_start(args, pszFormat);
    vfprintf( stdout, pszFormat, args );
    va_end(args);

    fflush( stdout );

/* -------------------------------------------------------------------- */
/*      Write message to debug file.                                    */
/* -------------------------------------------------------------------- */
    if( fpDebug != NULL )
    {
        fprintf( fpDebug, "%s:", pszDebugClass );

        va_start(args, pszFormat);
        vfprintf( fpDebug, pszFormat, args );
        va_end(args);

        fflush( fpDebug );
    }
}

