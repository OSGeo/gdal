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
 * consequential damages, arising out of the use of the Software.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2002/05/29 20:36:24  warmerda
 * New
 *
 * Revision 1.3  2001/07/27 17:24:45  warmerda
 * First phase rewrite for MapGuide
 *
 * Revision 1.2  1999/11/23 15:39:51  warmerda
 * tab expantion
 *
 * Revision 1.1  1999/11/23 15:22:58  warmerda
 * New
 *
 * Revision 1.2  1999/11/10 14:04:44  warmerda
 * updated to new fmeobjects kit
 *
 * Revision 1.1  1999/09/09 20:40:56  warmerda
 * New
 *
 */

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


                  
