/******************************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Convenience functions for parsing with Xerces-C library
 *           Functions for translating back and forth between XMLCh and char.
 *           We assume that XMLCh is a simple numeric type that we can
 *           correspond 1:1 with char values, but that it likely is larger
 *           than a char.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
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

// Must be first for DEBUG_BOOL case
#include "ogr_xerces.h"

#include "cpl_port.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_error.h"

CPL_CVSID("$Id$");

#ifdef HAVE_XERCES

static CPLMutex* hMutex = NULL;
static int nCounter = 0;

/************************************************************************/
/*                        OGRInitializeXerces()                         */
/************************************************************************/

bool OGRInitializeXerces(void)
{
    CPLMutexHolderD(&hMutex);
    if( nCounter > 0 )
    {
        nCounter ++;
        return true;
    }

    try
    {
        CPLDebug("OGR", "XMLPlatformUtils::Initialize()"); 
        XMLPlatformUtils::Initialize();
        nCounter ++;
        return true;
    }
    catch (const XMLException& toCatch)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Exception initializing Xerces: %s",
                  tr_strdup(toCatch.getMessage()) );
        return false;
    }
}

/************************************************************************/
/*                       OGRDeinitializeXerces()                        */
/************************************************************************/

void OGRDeinitializeXerces(void)
{
    CPLMutexHolderD(&hMutex);
    if( nCounter == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unpaired OGRInitializeXerces / OGRDeinitializeXerces calls");
        return;
    }
    nCounter --;
    if( nCounter == 0 )
    {
        if( CPLTestBool(CPLGetConfigOption("OGR_XERCES_TERMINATE", "YES")) )
        {
            CPLDebug("OGR", "XMLPlatformUtils::Terminate()");
            XMLPlatformUtils::Terminate();
        }
    }
}

/************************************************************************/
/*                       OGRCleanupXercesMutex()                        */
/************************************************************************/

void OGRCleanupXercesMutex(void)
{
    if( hMutex != NULL )
        CPLDestroyMutex(hMutex);
    hMutex = NULL;
}

/************************************************************************/
/*                             tr_isascii()                             */
/************************************************************************/

static int tr_isascii( const char * pszCString )

{
    while( *pszCString != '\0' )
    {
        if( *((unsigned char *) pszCString) > 127 )
            return FALSE;

        pszCString++;
    }

    return TRUE;
}

namespace OGR
{

/************************************************************************/
/*                 tr_strcpy(const char*,const XMLCh*)                  */
/************************************************************************/

void tr_strcpy( XMLCh *panXMLString, const char *pszCString )

{
/* -------------------------------------------------------------------- */
/*      Simple (ASCII) case.                                            */
/* -------------------------------------------------------------------- */
    if( tr_isascii( pszCString ) )
    {
        while( *pszCString != 0 )
            *(panXMLString++) = *(pszCString++);
        *panXMLString = 0;
        return;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we need to do a full UTC2 to UTF-8 conversion.        */
/* -------------------------------------------------------------------- */
    wchar_t *pwszUTF16;

    pwszUTF16 = CPLRecodeToWChar( pszCString, CPL_ENC_UTF8, "WCHAR_T" );

    int i = 0;
    for( ; pwszUTF16[i] != 0; i++ )
        panXMLString[i] = pwszUTF16[i];

    panXMLString[i] = 0;

    CPLFree( pwszUTF16 );
}

/************************************************************************/
/*                 tr_strcpy(const XMLCh*,const char*)                  */
/************************************************************************/

void tr_strcpy( char *pszCString, const XMLCh *panXMLString )

{
    int bSimpleASCII = TRUE;
    const XMLCh *panXMLStringOriginal = panXMLString;
    char *pszCStringOriginal = pszCString;

    while( *panXMLString != 0 )
    {
        if( *panXMLString > 127 )
            bSimpleASCII = FALSE;

        *(pszCString++) = (char) *(panXMLString++);
    }
    *pszCString = 0;

    if( bSimpleASCII )
        return;

    panXMLString = panXMLStringOriginal;
    pszCString = pszCStringOriginal;

/* -------------------------------------------------------------------- */
/*      The simple translation was wrong, because the source is not     */
/*      all simple ASCII characters.  Redo using the more expensive     */
/*      recoding API.                                                   */
/* -------------------------------------------------------------------- */
    wchar_t *pwszSource = (wchar_t *) CPLCalloc(sizeof(wchar_t),
                                                tr_strlen(panXMLStringOriginal)+1 );
    int i = 0;
    for( ; panXMLString[i] != 0; i++ )
        pwszSource[i] = panXMLString[i];
    pwszSource[i] = 0;

    char *pszResult = CPLRecodeFromWChar( pwszSource,
                                          "WCHAR_T", CPL_ENC_UTF8 );

    strcpy( pszCString, pszResult );

    CPLFree( pwszSource );
    CPLFree( pszResult );
}

/************************************************************************/
/*                             tr_strlen()                              */
/************************************************************************/

int tr_strlen( const XMLCh *panXMLString )

{
    int nLength = 0;

    while( *(panXMLString++) != 0 )
        nLength++;

    return nLength;
}

/************************************************************************/
/*                             tr_strdup()                              */
/************************************************************************/

char *tr_strdup( const XMLCh *panXMLString )

{
/* -------------------------------------------------------------------- */
/*      Compute maximum length.                                         */
/* -------------------------------------------------------------------- */
    int i, nMaxLen = 1;

    for( i = 0; panXMLString[i] != 0; i++ )
    {
        if( panXMLString[i] < 128 )
            nMaxLen++;
        else if( panXMLString[i] < 0x7ff )
            nMaxLen += 2;
        else
            nMaxLen += 4;
    }

/* -------------------------------------------------------------------- */
/*      Do the translation.                                             */
/* -------------------------------------------------------------------- */
    char        *pszResult = (char *) CPLMalloc(nMaxLen);
    tr_strcpy( pszResult, panXMLString );
    return pszResult;
}

/************************************************************************/
/*                            transcode()                               */
/************************************************************************/

CPLString transcode( const XMLCh *panXMLString, int nLimitingChars )
{
    CPLString osRet;
    transcode( panXMLString, osRet, nLimitingChars );
    return osRet;
}

CPLString& transcode( const XMLCh *panXMLString, CPLString& osRet, int nLimitingChars )
{
    bool bSimpleASCII = true;
    int nChars = 0;

    if( panXMLString == NULL )
    {
        osRet = "(null)";
        return osRet;
    }

    osRet.clear();
    if( nLimitingChars > 0 )
        osRet.reserve(nLimitingChars);
    for(int i = 0; panXMLString[i] != 0 &&
                        (nLimitingChars < 0 || i < nLimitingChars); i++ )
    {
        if( panXMLString[i] > 127 )
        {
            bSimpleASCII = false;
        }
        osRet += (char) panXMLString[i];
        nChars ++;
    }

    if( bSimpleASCII )
        return osRet;

/* -------------------------------------------------------------------- */
/*      The simple translation was wrong, because the source is not     */
/*      all simple ASCII characters.  Redo using the more expensive     */
/*      recoding API.                                                   */
/* -------------------------------------------------------------------- */
    wchar_t *pwszSource = (wchar_t *) CPLMalloc(sizeof(wchar_t) * (nChars+1) );
    for( int i = 0 ; i < nChars; i++ )
        pwszSource[i] = panXMLString[i];
    pwszSource[nChars] = 0;

    char *pszResult = CPLRecodeFromWChar( pwszSource,
                                          "WCHAR_T", CPL_ENC_UTF8 );

    osRet = pszResult;

    CPLFree( pwszSource );
    CPLFree( pszResult );

    return osRet;
}

}

#else // HAVE_XERCES

void OGRCleanupXercesMutex(void)
{
}

#endif // HAVE_XERCES
