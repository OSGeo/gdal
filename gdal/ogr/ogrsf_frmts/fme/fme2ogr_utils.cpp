/******************************************************************************
 * $Id$
 *
 * Project:  FMEObjects Translator
 * Purpose:  Various FME related support functions.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001 Safe Software Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "fme2ogr.h"
#include "cpl_conv.h"
#include <stdarg.h>

/************************************************************************/
/*                            CPLFMEError()                             */
/*                                                                      */
/*      This function takes care of reporting errors through            */
/*      CPLError(), but appending the last FME error message.           */
/************************************************************************/

void CPLFMEError( IFMESession * poSession, const char *pszFormat, ... )

{
    va_list     hVaArgs;
    char        *pszErrorBuf = (char *) CPLMalloc(10000);

/* -------------------------------------------------------------------- */
/*      Format the error message into a buffer.                         */
/* -------------------------------------------------------------------- */
    va_start( hVaArgs, pszFormat );
    vsprintf( pszErrorBuf, pszFormat, hVaArgs );
    va_end( hVaArgs );

/* -------------------------------------------------------------------- */
/*      Get the last error message from FME.                            */
/* -------------------------------------------------------------------- */
    const char  *pszFMEErrorString = poSession->getLastErrorMsg();

    if( pszFMEErrorString == NULL )
    {
        pszFMEErrorString = "FME reports no error message.";
    }
   
/* -------------------------------------------------------------------- */
/*      Send composite error through CPL, and cleanup.                  */
/* -------------------------------------------------------------------- */
    CPLError( CE_Failure, CPLE_AppDefined,
              "%s\n%s",
              pszErrorBuf, pszFMEErrorString );

    CPLFree( pszErrorBuf );
}


                  
