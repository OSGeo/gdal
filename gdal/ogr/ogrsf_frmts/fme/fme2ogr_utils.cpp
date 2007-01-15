/******************************************************************************
 * $Id$
 *
 * Project:  FMEObjects Translator
 * Purpose:  Various FME related support functions.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001 Safe Software Inc.
 * All Rights Reserved
 *
 * This software may not be copied or reproduced, in all or in part, 
 * without the prior written consent of Safe Software Inc.
 *
 * The entire risk as to the results and performance of the software,
 * supporting text and other information contained in this file
 * (collectively called the "Software") is with the user.  Although
 * Safe Software Incorporated has used considerable efforts in preparing 
 * the Software, Safe Software Incorporated does not warrant the
 * accuracy or completeness of the Software. In no event will Safe Software 
 * Incorporated be liable for damages, including loss of profits or 
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


                  
